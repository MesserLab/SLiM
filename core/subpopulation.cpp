//
//  subpopulation.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
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


#include "subpopulation.h"
#include "slim_sim.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"
#include "eidos_global.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <assert.h>
#include <string>
#include <map>


#pragma mark -
#pragma mark _SpatialMap
#pragma mark -

_SpatialMap::_SpatialMap(std::string p_spatiality_string, int p_spatiality, int64_t *p_grid_sizes, bool p_interpolate, double p_min_value, double p_max_value, int p_num_colors) :
	spatiality_string_(p_spatiality_string), spatiality_(p_spatiality), interpolate_(p_interpolate), min_value_(p_min_value), max_value_(p_max_value), n_colors_(p_num_colors)
{
	int64_t values_size = 0;
	
	switch (spatiality_)
	{
		case 1:
			grid_size_[0] = p_grid_sizes[0];
			grid_size_[1] = 0;
			grid_size_[2] = 0;
			values_size = grid_size_[0];
			break;
		case 2:
			grid_size_[0] = p_grid_sizes[0];
			grid_size_[1] = p_grid_sizes[1];
			grid_size_[2] = 0;
			values_size = grid_size_[0] * grid_size_[1];
			break;
		case 3:
			grid_size_[0] = p_grid_sizes[0];
			grid_size_[1] = p_grid_sizes[1];
			grid_size_[2] = p_grid_sizes[2];
			values_size = grid_size_[0] * grid_size_[1] * grid_size_[2];
			break;
	}
	
	values_ = (double *)malloc(values_size * sizeof(double));
	
	if (n_colors_ > 0)
	{
		red_components_ = (float *)malloc(n_colors_ * sizeof(float));
		green_components_ = (float *)malloc(n_colors_ * sizeof(float));
		blue_components_ = (float *)malloc(n_colors_ * sizeof(float));
	}
	else
	{
		red_components_ = nullptr;
		green_components_ = nullptr;
		blue_components_ = nullptr;
	}
	
	display_buffer_ = nullptr;
}

_SpatialMap::~_SpatialMap(void)
{
	if (values_)
		free(values_);
	
	if (red_components_)
		free(red_components_);
	if (green_components_)
		free(green_components_);
	if (blue_components_)
		free(blue_components_);
	if (display_buffer_)
		free(display_buffer_);
}

double _SpatialMap::ValueAtPoint(double *p_point)
{
	// This looks up the value at point, which is in coordinates that have been normalized and clamped to [0,1]
	switch (spatiality_)
	{
		case 1:
		{
			double x_fraction = p_point[0];
			int64_t xsize = grid_size_[0];
			
			if (interpolate_)
			{
				double x_map = x_fraction * (xsize - 1);
				int x1_map = (int)floor(x_map);
				int x2_map = (int)ceil(x_map);
				double fraction_x2 = x_map - x1_map;
				double fraction_x1 = 1.0 - fraction_x2;
				double value_x1 = values_[x1_map] * fraction_x1;
				double value_x2 = values_[x2_map] * fraction_x2;
				
				return value_x1 + value_x2;
			}
			else
			{
				int x_map = (int)round(x_fraction * (xsize - 1));
				
				return values_[x_map];
			}
			break;
		}
		case 2:
		{
			double x_fraction = p_point[0];
			double y_fraction = p_point[1];
			int64_t xsize = grid_size_[0];
			int64_t ysize = grid_size_[1];
			
			if (interpolate_)
			{
				double x_map = x_fraction * (xsize - 1);
				double y_map = y_fraction * (ysize - 1);
				int x1_map = (int)floor(x_map);
				int y1_map = (int)floor(y_map);
				int x2_map = (int)ceil(x_map);
				int y2_map = (int)ceil(y_map);
				double fraction_x2 = x_map - x1_map;
				double fraction_x1 = 1.0 - fraction_x2;
				double fraction_y2 = y_map - y1_map;
				double fraction_y1 = 1.0 - fraction_y2;
				double value_x1_y1 = values_[x1_map + y1_map * xsize] * fraction_x1 * fraction_y1;
				double value_x2_y1 = values_[x2_map + y1_map * xsize] * fraction_x2 * fraction_y1;
				double value_x1_y2 = values_[x1_map + y2_map * xsize] * fraction_x1 * fraction_y2;
				double value_x2_y2 = values_[x2_map + y2_map * xsize] * fraction_x2 * fraction_y2;
				
				return value_x1_y1 + value_x2_y1 + value_x1_y2 + value_x2_y2;
			}
			else
			{
				int x_map = (int)round(x_fraction * (xsize - 1));
				int y_map = (int)round(y_fraction * (ysize - 1));
				
				return values_[x_map + y_map * xsize];
			}
			break;
		}
		case 3:
		{
			double x_fraction = p_point[0];
			double y_fraction = p_point[1];
			double z_fraction = p_point[2];
			int64_t xsize = grid_size_[0];
			int64_t ysize = grid_size_[1];
			int64_t zsize = grid_size_[2];
			
			if (interpolate_)
			{
				double x_map = x_fraction * (xsize - 1);
				double y_map = y_fraction * (ysize - 1);
				double z_map = z_fraction * (zsize - 1);
				int x1_map = (int)floor(x_map);
				int y1_map = (int)floor(y_map);
				int z1_map = (int)floor(z_map);
				int x2_map = (int)ceil(x_map);
				int y2_map = (int)ceil(y_map);
				int z2_map = (int)ceil(z_map);
				double fraction_x2 = x_map - x1_map;
				double fraction_x1 = 1.0 - fraction_x2;
				double fraction_y2 = y_map - y1_map;
				double fraction_y1 = 1.0 - fraction_y2;
				double fraction_z2 = z_map - z1_map;
				double fraction_z1 = 1.0 - fraction_z2;
				double value_x1_y1_z1 = values_[x1_map + y1_map * xsize + z1_map * xsize * ysize] * fraction_x1 * fraction_y1 * fraction_z1;
				double value_x2_y1_z1 = values_[x2_map + y1_map * xsize + z1_map * xsize * ysize] * fraction_x2 * fraction_y1 * fraction_z1;
				double value_x1_y2_z1 = values_[x1_map + y2_map * xsize + z1_map * xsize * ysize] * fraction_x1 * fraction_y2 * fraction_z1;
				double value_x2_y2_z1 = values_[x2_map + y2_map * xsize + z1_map * xsize * ysize] * fraction_x2 * fraction_y2 * fraction_z1;
				double value_x1_y1_z2 = values_[x1_map + y1_map * xsize + z2_map * xsize * ysize] * fraction_x1 * fraction_y1 * fraction_z2;
				double value_x2_y1_z2 = values_[x2_map + y1_map * xsize + z2_map * xsize * ysize] * fraction_x2 * fraction_y1 * fraction_z2;
				double value_x1_y2_z2 = values_[x1_map + y2_map * xsize + z2_map * xsize * ysize] * fraction_x1 * fraction_y2 * fraction_z2;
				double value_x2_y2_z2 = values_[x2_map + y2_map * xsize + z2_map * xsize * ysize] * fraction_x2 * fraction_y2 * fraction_z2;
				
				return value_x1_y1_z1 + value_x2_y1_z1 + value_x1_y2_z1 + value_x2_y2_z1 + value_x1_y1_z2 + value_x2_y1_z2 + value_x1_y2_z2 + value_x2_y2_z2;
			}
			else
			{
				int x_map = (int)round(x_fraction * (xsize - 1));
				int y_map = (int)round(y_fraction * (ysize - 1));
				int z_map = (int)round(z_fraction * (zsize - 1));
				
				return values_[x_map + y_map * xsize + z_map * xsize * ysize];
			}
			break;
		}
	}
	
	return 0.0;
}

void _SpatialMap::ColorForValue(double p_value, double *p_rgb_ptr)
{
	if (n_colors_ == 0)
		EIDOS_TERMINATION << "ERROR (_SpatialMap::ColorForValue): no color map defined for spatial map." << EidosTerminate();
	
	double value_fraction = (p_value - min_value_) / (max_value_ - min_value_);
	double color_index = value_fraction * (n_colors_ - 1);
	int color_index_1 = (int)floor(color_index);
	int color_index_2 = (int)ceil(color_index);
	
	if (color_index_1 < 0) color_index_1 = 0;
	if (color_index_1 >= n_colors_) color_index_1 = n_colors_ - 1;
	if (color_index_2 < 0) color_index_2 = 0;
	if (color_index_2 >= n_colors_) color_index_2 = n_colors_ - 1;
	
	double color_2_weight = color_index - color_index_1;
	double color_1_weight = 1.0f - color_2_weight;
	
	double red1 = red_components_[color_index_1];
	double green1 = green_components_[color_index_1];
	double blue1 = blue_components_[color_index_1];
	double red2 = red_components_[color_index_2];
	double green2 = green_components_[color_index_2];
	double blue2 = blue_components_[color_index_2];
	
	p_rgb_ptr[0] = (red1 * color_1_weight + red2 * color_2_weight);
	p_rgb_ptr[1] = (green1 * color_1_weight + green2 * color_2_weight);
	p_rgb_ptr[2] = (blue1 * color_1_weight + blue2 * color_2_weight);
}

void _SpatialMap::ColorForValue(double p_value, float *p_rgb_ptr)
{
	if (n_colors_ == 0)
		EIDOS_TERMINATION << "ERROR (_SpatialMap::ColorForValue): no color map defined for spatial map." << EidosTerminate();
	
	double value_fraction = (p_value - min_value_) / (max_value_ - min_value_);
	double color_index = value_fraction * (n_colors_ - 1);
	int color_index_1 = (int)floor(color_index);
	int color_index_2 = (int)ceil(color_index);
	
	if (color_index_1 < 0) color_index_1 = 0;
	if (color_index_1 >= n_colors_) color_index_1 = n_colors_ - 1;
	if (color_index_2 < 0) color_index_2 = 0;
	if (color_index_2 >= n_colors_) color_index_2 = n_colors_ - 1;
	
	double color_2_weight = color_index - color_index_1;
	double color_1_weight = 1.0f - color_2_weight;
	
	double red1 = red_components_[color_index_1];
	double green1 = green_components_[color_index_1];
	double blue1 = blue_components_[color_index_1];
	double red2 = red_components_[color_index_2];
	double green2 = green_components_[color_index_2];
	double blue2 = blue_components_[color_index_2];
	
	p_rgb_ptr[0] = (float)(red1 * color_1_weight + red2 * color_2_weight);
	p_rgb_ptr[1] = (float)(green1 * color_1_weight + green2 * color_2_weight);
	p_rgb_ptr[2] = (float)(blue1 * color_1_weight + blue2 * color_2_weight);
}


#pragma mark -
#pragma mark Subpopulation
#pragma mark -

// given the subpop size and sex ratio currently set for the child generation, make new genomes to fit
void Subpopulation::GenerateChildrenToFit(const bool p_parents_also)
{
#ifdef DEBUG
	bool old_log = Genome::LogGenomeCopyAndAssign(false);
#endif
	Chromosome &chromosome = population_.sim_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	int32_t mutrun_length = chromosome.mutrun_length_;
	
	// throw out whatever used to be there
	child_genomes_.clear();
	cached_child_genomes_value_.reset();
	
	if (p_parents_also)
	{
		parent_genomes_.clear();
		cached_parent_genomes_value_.reset();
	}
	
	// make new stuff
	if (sex_enabled_)
	{
		// Figure out the first male index from the sex ratio, and exit if we end up with all one sex
		slim_popsize_t total_males = static_cast<slim_popsize_t>(lround(child_sex_ratio_ * child_subpop_size_));	// round in favor of males, arbitrarily
		
		child_first_male_index_ = child_subpop_size_ - total_males;
		
		if (child_first_male_index_ <= 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no females." << EidosTerminate();
		else if (child_first_male_index_ >= child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no males." << EidosTerminate();
		
		if (p_parents_also)
		{
			total_males = static_cast<slim_popsize_t>(lround(parent_sex_ratio_ * parent_subpop_size_));	// round in favor of males, arbitrarily
			
			parent_first_male_index_ = parent_subpop_size_ - total_males;
			
			if (parent_first_male_index_ <= 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no females." << EidosTerminate();
			else if (parent_first_male_index_ >= parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no males." << EidosTerminate();
		}
		
		switch (modeled_chromosome_type_)
		{
			case GenomeType::kAutosome:
			{
				// set up genomes of type GenomeType::kAutosome with a shared empty MutationRun for efficiency
				{
					child_genomes_.reserve(2 * child_subpop_size_);
					
					MutationRun *shared_empty_run = MutationRun::NewMutationRun();
					Genome aut_model = Genome(this, mutrun_count, mutrun_length, GenomeType::kAutosome, false, shared_empty_run);
					
					for (slim_popsize_t i = 0; i < child_subpop_size_; ++i)
					{
						child_genomes_.emplace_back(aut_model);
						child_genomes_.emplace_back(aut_model);
					}
				}
				
				if (p_parents_also)
				{
					parent_genomes_.reserve(2 * parent_subpop_size_);
					
					MutationRun *shared_empty_run_parental = MutationRun::NewMutationRun();
					Genome aut_model_parental = Genome(this, mutrun_count, mutrun_length, GenomeType::kAutosome, false, shared_empty_run_parental);
					
					for (slim_popsize_t i = 0; i < parent_subpop_size_; ++i)
					{
						parent_genomes_.emplace_back(aut_model_parental);
						parent_genomes_.emplace_back(aut_model_parental);
					}
				}
				break;
			}
			case GenomeType::kXChromosome:
			case GenomeType::kYChromosome:
			{
				// if we are not modeling a given chromosome type, then instances of it are null – they will log and exit if used
				{
					child_genomes_.reserve(2 * child_subpop_size_);
					
					MutationRun *shared_empty_run = MutationRun::NewMutationRun();
					Genome x_model = Genome(this, mutrun_count, mutrun_length, GenomeType::kXChromosome, modeled_chromosome_type_ != GenomeType::kXChromosome, shared_empty_run);
					Genome y_model = Genome(this, mutrun_count, mutrun_length, GenomeType::kYChromosome, modeled_chromosome_type_ != GenomeType::kYChromosome, shared_empty_run);
					
					// females get two Xs
					for (slim_popsize_t i = 0; i < child_first_male_index_; ++i)
					{
						child_genomes_.emplace_back(x_model);
						child_genomes_.emplace_back(x_model);
					}
					
					// males get an X and a Y
					for (slim_popsize_t i = child_first_male_index_; i < child_subpop_size_; ++i)
					{
						child_genomes_.emplace_back(x_model);
						child_genomes_.emplace_back(y_model);
					}
				}
				
				if (p_parents_also)
				{
					parent_genomes_.reserve(2 * parent_subpop_size_);
					
					MutationRun *shared_empty_run_parental = MutationRun::NewMutationRun();
					Genome x_model_parental = Genome(this, mutrun_count, mutrun_length, GenomeType::kXChromosome, modeled_chromosome_type_ != GenomeType::kXChromosome, shared_empty_run_parental);
					Genome y_model_parental = Genome(this, mutrun_count, mutrun_length, GenomeType::kYChromosome, modeled_chromosome_type_ != GenomeType::kYChromosome, shared_empty_run_parental);
					
					// females get two Xs
					for (slim_popsize_t i = 0; i < parent_first_male_index_; ++i)
					{
						parent_genomes_.emplace_back(x_model_parental);
						parent_genomes_.emplace_back(x_model_parental);
					}
					
					// males get an X and a Y
					for (slim_popsize_t i = parent_first_male_index_; i < parent_subpop_size_; ++i)
					{
						parent_genomes_.emplace_back(x_model_parental);
						parent_genomes_.emplace_back(y_model_parental);
					}
				}
				break;
			}
		}
	}
	else
	{
		// set up genomes of type GenomeType::kAutosome with a shared empty MutationRun for efficiency
		{
			child_genomes_.reserve(2 * child_subpop_size_);
			
			MutationRun *shared_empty_run = MutationRun::NewMutationRun();
			Genome aut_model = Genome(this, mutrun_count, mutrun_length, GenomeType::kAutosome, false, shared_empty_run);
			
			for (slim_popsize_t i = 0; i < child_subpop_size_; ++i)
			{
				child_genomes_.emplace_back(aut_model);
				child_genomes_.emplace_back(aut_model);
			}
		}
		
		if (p_parents_also)
		{
			parent_genomes_.reserve(2 * parent_subpop_size_);
			
			MutationRun *shared_empty_run_parental = MutationRun::NewMutationRun();
			Genome aut_model_parental = Genome(this, mutrun_count, mutrun_length, GenomeType::kAutosome, false, shared_empty_run_parental);
			
			for (slim_popsize_t i = 0; i < parent_subpop_size_; ++i)
			{
				parent_genomes_.emplace_back(aut_model_parental);
				parent_genomes_.emplace_back(aut_model_parental);
			}
		}
	}
	
#ifdef DEBUG
	Genome::LogGenomeCopyAndAssign(old_log);
#endif
	
	// This is also our cue to make sure that the individuals_ vectors are sufficiently large.  We just expand both to have as many individuals
	// as would be needed to hold either the child or the parental generation, and we only expand, never shrink.  Since SLiM does not actually
	// use these vectors for simulation dynamics, they do not need to be accurately sized, just sufficiently large.  By the way, if you're
	// wondering why we need separate individuals for the child and parental generations at all, it is because in modifyChild() callbacks and
	// similar situations, we need to be accessing tag values and genomes and such for the individuals in both generations at the same time.
	// They two generations therefore need to keep separate state, just as with Genome objects and other such state.
	slim_popsize_t max_subpop_size = std::max(child_subpop_size_, parent_subpop_size_);
	slim_popsize_t parent_individuals_size = (slim_popsize_t)parent_individuals_.size();
	slim_popsize_t child_individuals_size = (slim_popsize_t)child_individuals_.size();
	
#ifdef DEBUG
	old_log = Individual::LogIndividualCopyAndAssign(false);
#endif
	
	// BCH 7 November 2016: If we are going to resize a vector containing individuals, we have
	// to be quite careful, because the vector can resize, changing the addresses of all of the
	// Individual objects, which means that any cached pointers are invalid!  This is not an issue
	// with most SLiM objects since we don't store them in vectors, we allocate them.  We also
	// have to be careful with Genomes, which are also kept in vectors.
	if (parent_individuals_size < max_subpop_size)
	{
		// Clear any EidosValue cache of our individuals that we have made for the individuals property
		cached_parent_individuals_value_.reset();
		
		// Clear any cached EidosValue self-references inside the Individual objects themselves
		for (Individual &parent_ind : parent_individuals_)
			parent_ind.ClearCachedEidosValue();
		
		do
		{
			parent_individuals_.emplace_back(Individual(*this, parent_individuals_size));
			parent_individuals_size++;
		}
		while (parent_individuals_size < max_subpop_size);
	}
	
	if (child_individuals_size < max_subpop_size)
	{
		// Clear any EidosValue cache of our individuals that we have made for the individuals property
		cached_child_individuals_value_.reset();
		
		// Clear any cached EidosValue self-references inside the Individual objects themselves
		for (Individual &child_ind : child_individuals_)
			child_ind.ClearCachedEidosValue();
		
		do
		{
			child_individuals_.emplace_back(Individual(*this, child_individuals_size));
			child_individuals_size++;
		}
		while (child_individuals_size < max_subpop_size);
	}
	
#ifdef DEBUG
	Individual::LogIndividualCopyAndAssign(old_log);
#endif
}

Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size) : population_(p_population), subpopulation_id_(p_subpopulation_id), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size),
	self_symbol_(Eidos_GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class)))
{
	GenerateChildrenToFit(true);
	
	// Set up to draw random individuals, based initially on equal fitnesses
	cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
	cached_fitness_capacity_ = parent_subpop_size_;
	cached_fitness_size_ = parent_subpop_size_;
	
	double *fitness_buffer_ptr = cached_parental_fitness_;
	
	for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
		*(fitness_buffer_ptr++) = 1.0;
	
	lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
}

// SEX ONLY
Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, double p_sex_ratio, GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff) :
population_(p_population), subpopulation_id_(p_subpopulation_id), sex_enabled_(true), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size), parent_sex_ratio_(p_sex_ratio), child_sex_ratio_(p_sex_ratio), modeled_chromosome_type_(p_modeled_chromosome_type), x_chromosome_dominance_coeff_(p_x_chromosome_dominance_coeff),
	self_symbol_(Eidos_GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class)))
{
	GenerateChildrenToFit(true);
	
	// Set up to draw random females, based initially on equal fitnesses
	cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
	cached_male_fitness_ = (double *)realloc(cached_male_fitness_, sizeof(double) * parent_subpop_size_);
	cached_fitness_capacity_ = parent_subpop_size_;
	cached_fitness_size_ = parent_subpop_size_;
	
	double *fitness_buffer_ptr = cached_parental_fitness_;
	double *male_buffer_ptr = cached_male_fitness_;
	
	for (slim_popsize_t i = 0; i < parent_first_male_index_; i++)
	{
		*(fitness_buffer_ptr++) = 1.0;
		*(male_buffer_ptr++) = 0.0;				// this vector has 0 for all females, for mateChoice() callbacks
	}
	
	// Set up to draw random males, based initially on equal fitnesses
	slim_popsize_t num_males = parent_subpop_size_ - parent_first_male_index_;
	
	for (slim_popsize_t i = 0; i < num_males; i++)
	{
		*(fitness_buffer_ptr++) = 1.0;
		*(male_buffer_ptr++) = 1.0;
	}
	
	lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, cached_parental_fitness_);
	lookup_male_parent_ = gsl_ran_discrete_preproc(num_males, cached_parental_fitness_ + parent_first_male_index_);
}


Subpopulation::~Subpopulation(void)
{
	//std::cout << "Subpopulation::~Subpopulation" << std::endl;
	
	if (lookup_parent_)
		gsl_ran_discrete_free(lookup_parent_);
	
	if (lookup_female_parent_)
		gsl_ran_discrete_free(lookup_female_parent_);
	
	if (lookup_male_parent_)
		gsl_ran_discrete_free(lookup_male_parent_);
	
	if (cached_parental_fitness_)
		free(cached_parental_fitness_);
	
	if (cached_male_fitness_)
		free(cached_male_fitness_);
	
	for (const SpatialMapPair &map_pair : spatial_maps_)
	{
		SpatialMap *map_ptr = map_pair.second;
		
		if (map_ptr)
			delete map_ptr;
	}
}

void Subpopulation::UpdateFitness(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, std::vector<SLiMEidosBlock*> &p_global_fitness_callbacks)
{
	// The FitnessOfParent...() methods called by this method rely upon cached fitness values
	// kept inside the Mutation objects.  Those caches may need to be validated before we can
	// calculate fitness values.  We check for that condition and repair it first.
	const std::map<slim_objectid_t,MutationType*> &mut_types = population_.sim_.MutationTypes();
	bool needs_recache = false;
	
	for (auto &mut_type_iter : mut_types)
	{
		MutationType *mut_type = mut_type_iter.second;
		
		if (mut_type->dominance_coeff_changed_)
		{
			needs_recache = true;
			mut_type->dominance_coeff_changed_ = false;
			
			// don't break, because we want to clear all changed flags
		}
	}
	
	if (needs_recache)
		population_.ValidateMutationFitnessCaches();
	
	// This function calculates the population mean fitness as a side effect
	double totalFitness = 0.0;
	
	// Figure out our callback scenario: zero, one, or many?  See the comment below, above FitnessOfParentWithGenomeIndices_NoCallbacks(),
	// for more info on this complication.  Here we just figure out which version to call and set up for it.
	int fitness_callback_count = (int)p_fitness_callbacks.size();
	bool fitness_callbacks_exist = (fitness_callback_count > 0);
	bool single_fitness_callback = false;
	MutationType *single_callback_mut_type = nullptr;
	
	if (fitness_callback_count == 1)
	{
		slim_objectid_t mutation_type_id = p_fitness_callbacks[0]->mutation_type_id_;
		auto found_muttype_pair = mut_types.find(mutation_type_id);
		
		if (found_muttype_pair != mut_types.end())
		{
			if (mut_types.size() > 1)
			{
				// We have a single callback that applies to a known mutation type among more than one defined type; we can optimize that
				single_fitness_callback = true;
				single_callback_mut_type = found_muttype_pair->second;
			}
			// else there is only one mutation type, so the callback applies to all mutations in the simulation
		}
		else
		{
			// The only callback refers to a mutation type that doesn't exist, so we effectively have no callbacks; we probably never hit this
			fitness_callback_count = 0;
			(void)fitness_callback_count;		// tell the static analyzer that we know we just did a dead store
			fitness_callbacks_exist = false;
		}
	}
	
	// Can we skip chromosome-based fitness calculations altogether, and just call global fitness() callbacks if any?
	// We can do this if (a) all mutation types either use a neutral DFE, or have been made neutral with a "return 1.0;"
	// fitness callback that is active, (b) for the mutation types that use a neutral DFE, no mutation has had its
	// selection coefficient changed, and (c) no fitness() callbacks are active apart from "return 1.0;" type callbacks.
	// This is often the case for QTL-based models (such as Misha's coral model), and should produce a big speed gain,
	// so we do a pre-check here for this case.  Note that we can ignore global fitness callbacks in this situation,
	// because they are explicitly documented as potentially being executed after all non-global fitness callbacks, so
	// they are not allowed, as a matter of policy, to alter the operation of non-global fitness callbacks.
	bool skip_chromosomal_fitness = true;
	
	// first set a flag on all mut types indicating whether they are pure neutral according to their DFE
	for (auto &mut_type_iter : mut_types)
		mut_type_iter.second->is_pure_neutral_now_ = mut_type_iter.second->all_pure_neutral_DFE_;
	
	// then go through the fitness callback list and set the pure neutral flag for mut types neutralized by an active callback
	for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
	{
		if (fitness_callback->active_)
		{
			const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
			
			if (compound_statement_node->cached_value_)
			{
				// The script is a constant expression such as "{ return 1.1; }"
				EidosValue *result = compound_statement_node->cached_value_.get();
				
				if ((result->Type() == EidosValueType::kValueFloat) || (result->Count() == 1))
				{
					if (result->FloatAtIndex(0, nullptr) == 1.0)
					{
						// the callback returns 1.0, so it makes the mutation types to which it applies become neutral
						slim_objectid_t mutation_type_id = fitness_callback->mutation_type_id_;
						
						if (mutation_type_id == -1)
						{
							for (auto &mut_type_iter : mut_types)
								mut_type_iter.second->is_pure_neutral_now_ = true;
						}
						else
						{
							auto found_muttype_pair = mut_types.find(mutation_type_id);
							
							if (found_muttype_pair != mut_types.end())
								found_muttype_pair->second->is_pure_neutral_now_ = true;
						}
						
						continue;
					}
				}
			}
			
			// if we reach this point, we have an active callback that is not neutral-making, so we fail and we're done
			skip_chromosomal_fitness = false;
			break;
		}
	}
	
	// finally, tabulate the pure-neutral flags of all the mut types into an overall flag for whether we can skip
	if (skip_chromosomal_fitness)
	{
		for (auto &mut_type_iter : mut_types)
		{
			if (!mut_type_iter.second->is_pure_neutral_now_)
			{
				skip_chromosomal_fitness = false;
				break;
			}
		}
	}
	else
	{
		// At this point, there is an active callback that is not neutral-making, so we really can't reliably depend
		// upon is_pure_neutral_now_; that rogue callback could make other callbacks active/inactive, etc.  So we go
		// through and clear the is_pure_neutral_now_ flags to avoid any confusion.  This is actually redundant as
		// the code presently stands, but it avoids leaving the flag in a bad state and causing future bugs.
		for (auto &mut_type_iter : mut_types)
			mut_type_iter.second->is_pure_neutral_now_ = false;
	}
	
	// Figure out global callbacks; these are callbacks with NULL supplied for the mut-type id, which means that they are called
	// exactly once per individual, for every individual regardless of genetics, to provide an entry point for alternate fitness definitions
	int global_fitness_callback_count = (int)p_global_fitness_callbacks.size();
	bool global_fitness_callbacks_exist = (global_fitness_callback_count > 0);
	
	// We cache the calculated fitness values, for use in PopulationView and mateChoice() callbacks and such
	if (cached_fitness_capacity_ < parent_subpop_size_)
	{
		cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
		if (sex_enabled_)
			cached_male_fitness_ = (double *)realloc(cached_male_fitness_, sizeof(double) * parent_subpop_size_);
		cached_fitness_capacity_ = parent_subpop_size_;
	}
	
	cached_fitness_size_ = 0;	// while we're refilling, the fitness cache is invalid
	
	// We optimize the pure neutral case, as long as no fitness callbacks are defined; fitness values are then simply 1.0, for everybody.
	bool pure_neutral = (!fitness_callbacks_exist && !global_fitness_callbacks_exist && population_.sim_.pure_neutral_);
	
	// calculate fitnesses in parent population and create new lookup table
	if (sex_enabled_)
	{
		// SEX ONLY
		double totalMaleFitness = 0.0, totalFemaleFitness = 0.0;
		
		if (lookup_female_parent_)
		{
			gsl_ran_discrete_free(lookup_female_parent_);
			lookup_female_parent_ = nullptr;
		}
		
		if (lookup_male_parent_)
		{
			gsl_ran_discrete_free(lookup_male_parent_);
			lookup_male_parent_ = nullptr;
		}
		
		// Set up to draw random females
		if (pure_neutral)
		{
			for (slim_popsize_t i = 0; i < parent_first_male_index_; i++)
			{
				cached_parental_fitness_[i] = 1.0;
				cached_male_fitness_[i] = 0;
				
				totalFemaleFitness += 1.0;
			}
		}
		else if (skip_chromosomal_fitness)
		{
			for (slim_popsize_t i = 0; i < parent_first_male_index_; i++)
			{
				double fitness = 1.0;
				
				if (global_fitness_callbacks_exist)
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, i);
				
				cached_parental_fitness_[i] = fitness;
				cached_male_fitness_[i] = 0;
				
				totalFemaleFitness += fitness;
			}
		}
		else
		{
			// general case for females
			for (slim_popsize_t i = 0; i < parent_first_male_index_; i++)
			{
				double fitness;
				
				if (!fitness_callbacks_exist)
					fitness = FitnessOfParentWithGenomeIndices_NoCallbacks(i);
				else if (single_fitness_callback)
					fitness = FitnessOfParentWithGenomeIndices_SingleCallback(i, p_fitness_callbacks, single_callback_mut_type);
				else
					fitness = FitnessOfParentWithGenomeIndices_Callbacks(i, p_fitness_callbacks);
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, i);
				
				cached_parental_fitness_[i] = fitness;
				cached_male_fitness_[i] = 0;				// this vector has 0 for all females, for mateChoice() callbacks
				
				totalFemaleFitness += fitness;
			}
		}
		
		totalFitness += totalFemaleFitness;
		if (totalFemaleFitness <= 0.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of females is <= 0.0." << EidosTerminate(nullptr);
		
		// in pure neutral models we don't set up the discrete preproc
		if (!pure_neutral)
			lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, cached_parental_fitness_);
		
		// Set up to draw random males
		slim_popsize_t num_males = parent_subpop_size_ - parent_first_male_index_;
		
		if (pure_neutral)
		{
			for (slim_popsize_t i = 0; i < num_males; i++)
			{
				slim_popsize_t individual_index = (i + parent_first_male_index_);
				
				cached_parental_fitness_[individual_index] = 1.0;
				cached_male_fitness_[individual_index] = 1.0;
				
				totalMaleFitness += 1.0;
			}
		}
		else if (skip_chromosomal_fitness)
		{
			for (slim_popsize_t i = 0; i < num_males; i++)
			{
				slim_popsize_t individual_index = (i + parent_first_male_index_);
				double fitness = 1.0;
				
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, individual_index);
				
				cached_parental_fitness_[individual_index] = fitness;
				cached_male_fitness_[individual_index] = fitness;
				
				totalMaleFitness += fitness;
			}
		}
		else
		{
			// general case for males
			for (slim_popsize_t i = 0; i < num_males; i++)
			{
				slim_popsize_t individual_index = (i + parent_first_male_index_);
				double fitness;
				
				if (!fitness_callbacks_exist)
					fitness = FitnessOfParentWithGenomeIndices_NoCallbacks(individual_index);
				else if (single_fitness_callback)
					fitness = FitnessOfParentWithGenomeIndices_SingleCallback(individual_index, p_fitness_callbacks, single_callback_mut_type);
				else
					fitness = FitnessOfParentWithGenomeIndices_Callbacks(individual_index, p_fitness_callbacks);
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, individual_index);
				
				cached_parental_fitness_[individual_index] = fitness;
				cached_male_fitness_[individual_index] = fitness;
				
				totalMaleFitness += fitness;
			}
		}
		
		totalFitness += totalMaleFitness;
		(void)totalFitness;		// tell the static analyzer that we know we just did a dead store
		
		if (totalMaleFitness <= 0.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of males is <= 0.0." << EidosTerminate(nullptr);
		
		// in pure neutral models we don't set up the discrete preproc
		if (!pure_neutral)
			lookup_male_parent_ = gsl_ran_discrete_preproc(num_males, cached_parental_fitness_ + parent_first_male_index_);
	}
	else
	{
		double *fitness_buffer_ptr = cached_parental_fitness_;
		
		if (lookup_parent_)
		{
			gsl_ran_discrete_free(lookup_parent_);
			lookup_parent_ = nullptr;
		}
		
		if (pure_neutral)
		{
			for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
			{
				*(fitness_buffer_ptr++) = 1.0;
				
				totalFitness += 1.0;
			}
		}
		else if (skip_chromosomal_fitness)
		{
			for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
			{
				double fitness = 1.0;
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, i);
				
				*(fitness_buffer_ptr++) = fitness;
				
				totalFitness += fitness;
			}
		}
		else
		{
			// general case for hermaphrodites
			for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
			{
				double fitness;
				
				if (!fitness_callbacks_exist)
					fitness = FitnessOfParentWithGenomeIndices_NoCallbacks(i);
				else if (single_fitness_callback)
					fitness = FitnessOfParentWithGenomeIndices_SingleCallback(i, p_fitness_callbacks, single_callback_mut_type);
				else
					fitness = FitnessOfParentWithGenomeIndices_Callbacks(i, p_fitness_callbacks);
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, i);
				
				*(fitness_buffer_ptr++) = fitness;
				
				totalFitness += fitness;
			}
		}
		
		if (totalFitness <= 0.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of all individuals is <= 0.0." << EidosTerminate(nullptr);
		
		// in pure neutral models we don't set up the discrete preproc
		if (!pure_neutral)
			lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
	}
	
	cached_fitness_size_ = parent_subpop_size_;
	
#ifdef SLIMGUI
	parental_total_fitness_ = totalFitness;
#endif
}

double Subpopulation::ApplyFitnessCallbacks(MutationIndex p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, Individual *p_individual, Genome *p_genome1, Genome *p_genome2)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	slim_objectid_t mutation_type_id = (gSLiM_Mutation_Block + p_mutation)->mutation_type_ptr_->mutation_type_id_;
	SLiMSim &sim = population_.sim_;
	
	for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
	{
		if (fitness_callback->active_)
		{
			slim_objectid_t callback_mutation_type_id = fitness_callback->mutation_type_id_;
			
			if ((callback_mutation_type_id == -1) || (callback_mutation_type_id == mutation_type_id))
			{
				// The callback is active and matches the mutation type id of the mutation, so we need to execute it
				// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
				const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
				
				if (compound_statement_node->cached_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
					EidosValue_SP result_SP = compound_statement_node->cached_value_;
					EidosValue *result = result_SP.get();
					
					if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
						EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << EidosTerminate(fitness_callback->identifier_token_);
					
					p_computed_fitness = result->FloatAtIndex(0, nullptr);
					
					// the cached value is owned by the tree, so we do not dispose of it
					// there is also no script output to handle
				}
				else
				{
					// local variables for the callback parameters that we might need to allocate here, and thus need to free below
					EidosValue_Object_singleton local_mut(gSLiM_Mutation_Block + p_mutation, gSLiM_Mutation_Class);
					EidosValue_Float_singleton local_relFitness(p_computed_fitness);
					
					// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
					{
						EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &sim.SymbolTable());
						EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
						EidosFunctionMap &function_map = sim.FunctionMap();
						EidosInterpreter interpreter(fitness_callback->compound_statement_node_, client_symbols, function_map, &sim);
						
						if (fitness_callback->contains_self_)
							callback_symbols.InitializeConstantSymbolEntry(fitness_callback->SelfSymbolTableEntry());		// define "self"
						
						// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
						// We can use that method because we know the lifetime of the symbol table is shorter than that of
						// the value objects, and we know that the values we are setting here will not change (the objects
						// referred to by the values may change, but the values themselves will not change).
						if (fitness_callback->contains_mut_)
						{
							local_mut.StackAllocated();			// prevent Eidos_intrusive_ptr from trying to delete this
							callback_symbols.InitializeConstantSymbolEntry(gID_mut, EidosValue_SP(&local_mut));
						}
						if (fitness_callback->contains_relFitness_)
						{
							local_relFitness.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
							callback_symbols.InitializeConstantSymbolEntry(gID_relFitness, EidosValue_SP(&local_relFitness));
						}
						if (fitness_callback->contains_individual_)
							callback_symbols.InitializeConstantSymbolEntry(gID_individual, p_individual->CachedEidosValue());
						if (fitness_callback->contains_genome1_)
							callback_symbols.InitializeConstantSymbolEntry(gID_genome1, p_genome1->CachedEidosValue());
						if (fitness_callback->contains_genome2_)
							callback_symbols.InitializeConstantSymbolEntry(gID_genome2, p_genome2->CachedEidosValue());
						if (fitness_callback->contains_subpop_)
							callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
						
						// p_homozygous == -1 means the mutation is opposed by a NULL chromosome; otherwise, 0 means heterozyg., 1 means homozyg.
						// that gets translated into Eidos values of NULL, F, and T, respectively
						if (fitness_callback->contains_homozygous_)
						{
							if (p_homozygous == -1)
								callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, gStaticEidosValueNULL);
							else
								callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, (p_homozygous != 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
						}
						
						try
						{
							// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
							EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(fitness_callback->script_);
							EidosValue *result = result_SP.get();
							
							if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
								EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << EidosTerminate(fitness_callback->identifier_token_);
							
							p_computed_fitness = result->FloatAtIndex(0, nullptr);
							
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
	}
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(population_.sim_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessCallback)]);
#endif
	
	return p_computed_fitness;
}

// This calculates the effects of global fitness callbacks, i.e. those with muttype==NULL and which therefore do not reference any mutation
double Subpopulation::ApplyGlobalFitnessCallbacks(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, slim_popsize_t p_individual_index)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	double computed_fitness = 1.0;
	Individual *individual = &(parent_individuals_[p_individual_index]);
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	SLiMSim &sim = population_.sim_;
	
	for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
	{
		if (fitness_callback->active_)
		{
			// The callback is active, so we need to execute it
			// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
			const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
			
			if (compound_statement_node->cached_value_)
			{
				// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
				EidosValue_SP result_SP = compound_statement_node->cached_value_;
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyGlobalFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << EidosTerminate(fitness_callback->identifier_token_);
				
				computed_fitness *= result->FloatAtIndex(0, nullptr);
				
				// the cached value is owned by the tree, so we do not dispose of it
				// there is also no script output to handle
			}
			else if (fitness_callback->has_cached_optimization_)
			{
				// We can special-case particular simple callbacks for speed.  This is similar to the cached_value_
				// mechanism above, but it is done in SLiM, not in Eidos, and is specific to callbacks, not general.
				// The has_cached_optimization_ flag is the umbrella flag for all such optimizations; we then figure
				// out below which cached optimization is in effect for this callback.  See SLiMSim::AddScriptBlock()
				// for comments on the specific cases optimized here.
				if (fitness_callback->has_cached_opt_dnorm1_)
				{
					double A = fitness_callback->cached_opt_A_;
					double B = fitness_callback->cached_opt_B_;
					double C = fitness_callback->cached_opt_C_;
					double D = fitness_callback->cached_opt_D_;
					
					computed_fitness *= (D + (gsl_ran_gaussian_pdf(individual->TagFloat() - A, B) / C));
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyGlobalFitnessCallbacks): (internal error) cached optimization flag mismatch" << EidosTerminate(fitness_callback->identifier_token_);
				}
			}
			else
			{
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &sim.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = sim.FunctionMap();
					EidosInterpreter interpreter(fitness_callback->compound_statement_node_, client_symbols, function_map, &sim);
					
					if (fitness_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(fitness_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (fitness_callback->contains_mut_)
						callback_symbols.InitializeConstantSymbolEntry(gID_mut, gStaticEidosValueNULL);
					if (fitness_callback->contains_relFitness_)
						callback_symbols.InitializeConstantSymbolEntry(gID_relFitness, gStaticEidosValue_Float1);
					if (fitness_callback->contains_individual_)
						callback_symbols.InitializeConstantSymbolEntry(gID_individual, individual->CachedEidosValue());
					if (fitness_callback->contains_genome1_)
						callback_symbols.InitializeConstantSymbolEntry(gID_genome1, genome1->CachedEidosValue());
					if (fitness_callback->contains_genome2_)
						callback_symbols.InitializeConstantSymbolEntry(gID_genome2, genome2->CachedEidosValue());
					if (fitness_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
					if (fitness_callback->contains_homozygous_)
						callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, gStaticEidosValueNULL);
					
					try
					{
						// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(fitness_callback->script_);
						EidosValue *result = result_SP.get();
						
						if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyGlobalFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << EidosTerminate(fitness_callback->identifier_token_);
						
						computed_fitness *= result->FloatAtIndex(0, nullptr);
						
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
			
			// If any callback puts us at or below zero, we can short-circuit the rest
			if (computed_fitness <= 0.0)
			{
				computed_fitness = 0.0;
				break;
			}
		}
	}
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(population_.sim_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)]);
#endif
	
	return computed_fitness;
}

// FitnessOfParentWithGenomeIndices has three versions, for no callbacks, a single callback, and multiple callbacks.  This is for two reasons.  First,
// it allows the case without fitness() callbacks to run at full speed.  Second, the non-callback case short-circuits when the selection coefficient
// is exactly 0.0f, as an optimization; but that optimization would be invalid in the callback case, since callbacks can change the relative fitness
// of ostensibly neutral mutations.  For reasons of maintainability, the three versions should be kept in synch as closely as possible.
//
// When there is just a single callback, it usually refers to a mutation type that is relatively uncommon.  The model might have neutral mutations in most
// cases, plus a rare (or unique) mutation type that is subject to more complex selection, for example.  We can optimize that very common case substantially
// by making the callout to ApplyFitnessCallbacks() only for mutations of the mutation type that the callback modifies.  This pays off mostly when there
// are many common mutations with no callback, plus one rare mutation type that has a callback.  A model of neutral drift across a long chromosome with a
// high mutation rate, with an introduced beneficial mutation with a selection coefficient extremely close to 0, for example, would hit this case hard and
// see a speedup of as much as 25%, so the additional complexity seems worth it (since that's quite a realistic and common case).

// This version of FitnessOfParentWithGenomeIndices assumes no callbacks exist.  It tests for neutral mutations and skips processing them.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_NoCallbacks(slim_popsize_t p_individual_index)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	SLiMSim &sim = population_.sim_;
	int32_t nonneutral_change_counter = sim.nonneutral_change_counter_;
	int32_t nonneutral_regime = sim.last_nonneutral_regime_;
#endif
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		const int32_t mutrun_count = genome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun = genome->mutruns_[run_index].get();
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome_iter, *genome_max;
			
			mutrun->beginend_nonneutral_pointers(&genome_iter, &genome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome_iter = mutrun->begin_pointer_const();
			const MutationIndex *genome_max = mutrun->end_pointer_const();
#endif
			
			if (genome->Type() == GenomeType::kXChromosome)
			{
				// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
				// we don't cache this fitness effect because x_chromosome_dominance_coeff_ is not readily available to Mutation
				while (genome_iter != genome_max)
				{
					slim_selcoeff_t selection_coeff = (mut_block_ptr + *genome_iter)->selection_coeff_;
					
					if (selection_coeff != 0.0f)
					{
						w *= (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
						
						if (w <= 0.0)
							return 0.0;
					}
					
					genome_iter++;
				}
			}
			else
			{
				// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
				while (genome_iter != genome_max)
					w *= (mut_block_ptr + *genome_iter++)->cached_one_plus_sel_;
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		const int32_t mutrun_count = genome1->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun1 = genome1->mutruns_[run_index].get();
			MutationRun *mutrun2 = genome2->mutruns_[run_index].get();
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome1_iter, *genome2_iter, *genome1_max, *genome2_max;
			
			mutrun1->beginend_nonneutral_pointers(&genome1_iter, &genome1_max, nonneutral_change_counter, nonneutral_regime);
			mutrun2->beginend_nonneutral_pointers(&genome2_iter, &genome2_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome1_iter = mutrun1->begin_pointer_const();
			const MutationIndex *genome2_iter = mutrun2->begin_pointer_const();
			
			const MutationIndex *genome1_max = mutrun1->end_pointer_const();
			const MutationIndex *genome2_max = mutrun2->end_pointer_const();
#endif
			
			// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
			if (genome1_iter != genome1_max && genome2_iter != genome2_max)
			{
				MutationIndex genome1_mutation = *genome1_iter, genome2_mutation = *genome2_iter;
				slim_position_t genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_, genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
				
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
						
						if (++genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
						
						if (++genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						slim_position_t position = genome1_iter_position;
						const MutationIndex *genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome2_matchscan = genome2_iter; 
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
							{
								if (genome1_mutation == *genome2_matchscan) 		// note pointer equality test
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_;
									goto homozygousExit1;
								}
								
								genome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
							
						homozygousExit1:
							
							if (++genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome1_matchscan = genome1_start; 
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
							{
								if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
								{
									// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
									goto homozygousExit2;
								}
								
								genome1_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
							
						homozygousExit2:
							
							if (++genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
							}
						} while (genome2_iter_position == position);
						
						// break out if either genome has reached its end
						if (genome1_iter == genome1_max || genome2_iter == genome2_max)
							break;
					}
				} while (true);
			}
			
			// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
#ifdef DEBUG
			assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
#endif
			
			// if genome1 is unfinished, finish it
			while (genome1_iter != genome1_max)
				w *= (mut_block_ptr + *genome1_iter++)->cached_one_plus_dom_sel_;
			
			// if genome2 is unfinished, finish it
			while (genome2_iter != genome2_max)
				w *= (mut_block_ptr + *genome2_iter++)->cached_one_plus_dom_sel_;
		}
		
		return w;
	}
}

// This version of FitnessOfParentWithGenomeIndices assumes multiple callbacks exist.  It doesn't optimize neutral mutations since they might be modified by callbacks.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_Callbacks(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	SLiMSim &sim = population_.sim_;
	int32_t nonneutral_change_counter = sim.nonneutral_change_counter_;
	int32_t nonneutral_regime = sim.last_nonneutral_regime_;
#endif
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	Individual *individual = &(parent_individuals_[p_individual_index]);
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		const int32_t mutrun_count = genome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun = genome->mutruns_[run_index].get();
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome_iter, *genome_max;
			
			mutrun->beginend_nonneutral_pointers(&genome_iter, &genome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome_iter = mutrun->begin_pointer_const();
			const MutationIndex *genome_max = mutrun->end_pointer_const();
#endif
			
			if (genome->Type() == GenomeType::kXChromosome)
			{
				// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
				// we don't cache this fitness effect because x_chromosome_dominance_coeff_ is not readily available to Mutation
				while (genome_iter != genome_max)
				{
					MutationIndex genome_mutation = *genome_iter;
					slim_selcoeff_t selection_coeff = (mut_block_ptr + genome_mutation)->selection_coeff_;
					
					w *= ApplyFitnessCallbacks(genome_mutation, -1, 1.0 + x_chromosome_dominance_coeff_ * selection_coeff, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
					
					genome_iter++;
				}
			}
			else
			{
				// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
				while (genome_iter != genome_max)
				{
					MutationIndex genome_mutation = *genome_iter;
					
					w *= ApplyFitnessCallbacks(genome_mutation, -1, (mut_block_ptr + genome_mutation)->cached_one_plus_sel_, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
					
					genome_iter++;
				}
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		const int32_t mutrun_count = genome1->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun1 = genome1->mutruns_[run_index].get();
			MutationRun *mutrun2 = genome2->mutruns_[run_index].get();
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome1_iter, *genome2_iter, *genome1_max, *genome2_max;
			
			mutrun1->beginend_nonneutral_pointers(&genome1_iter, &genome1_max, nonneutral_change_counter, nonneutral_regime);
			mutrun2->beginend_nonneutral_pointers(&genome2_iter, &genome2_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome1_iter = mutrun1->begin_pointer_const();
			const MutationIndex *genome2_iter = mutrun2->begin_pointer_const();
			
			const MutationIndex *genome1_max = mutrun1->end_pointer_const();
			const MutationIndex *genome2_max = mutrun2->end_pointer_const();
#endif
			
			// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
			if (genome1_iter != genome1_max && genome2_iter != genome2_max)
			{
				MutationIndex genome1_mutation = *genome1_iter, genome2_mutation = *genome2_iter;
				slim_position_t genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_, genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
				
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						w *= ApplyFitnessCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
						
						if (++genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						w *= ApplyFitnessCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
						
						if (++genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						slim_position_t position = genome1_iter_position;
						const MutationIndex *genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome2_matchscan = genome2_iter; 
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
							{
								if (genome1_mutation == *genome2_matchscan)		// note pointer equality test
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									w *= ApplyFitnessCallbacks(genome1_mutation, true, (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_, p_fitness_callbacks, individual, genome1, genome2);
									
									goto homozygousExit3;
								}
								
								genome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= ApplyFitnessCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
							
						homozygousExit3:
							
							if (w <= 0.0)
								return 0.0;
							
							if (++genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome1_matchscan = genome1_start; 
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
							{
								if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
								{
									// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
									goto homozygousExit4;
								}
								
								genome1_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= ApplyFitnessCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
							
							if (w <= 0.0)
								return 0.0;
							
						homozygousExit4:
							
							if (++genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
							}
						} while (genome2_iter_position == position);
						
						// break out if either genome has reached its end
						if (genome1_iter == genome1_max || genome2_iter == genome2_max)
							break;
					}
				} while (true);
			}
			
			// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
			assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
			
			// if genome1 is unfinished, finish it
			while (genome1_iter != genome1_max)
			{
				MutationIndex genome1_mutation = *genome1_iter;
				
				w *= ApplyFitnessCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
				
				genome1_iter++;
			}
			
			// if genome2 is unfinished, finish it
			while (genome2_iter != genome2_max)
			{
				MutationIndex genome2_mutation = *genome2_iter;
				
				w *= ApplyFitnessCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
				
				genome2_iter++;
			}
		}
		
		return w;
	}
}

// This version of FitnessOfParentWithGenomeIndices assumes a single callback exists, modifying the given mutation type.  It is a hybrid of the previous two versions.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_SingleCallback(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, MutationType *p_single_callback_mut_type)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	SLiMSim &sim = population_.sim_;
	int32_t nonneutral_change_counter = sim.nonneutral_change_counter_;
	int32_t nonneutral_regime = sim.last_nonneutral_regime_;
#endif
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	Individual *individual = &(parent_individuals_[p_individual_index]);
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		const int32_t mutrun_count = genome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun = genome->mutruns_[run_index].get();
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome_iter, *genome_max;
			
			mutrun->beginend_nonneutral_pointers(&genome_iter, &genome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome_iter = mutrun->begin_pointer_const();
			const MutationIndex *genome_max = mutrun->end_pointer_const();
#endif
			
			if (genome->Type() == GenomeType::kXChromosome)
			{
				// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
				// we don't cache this fitness effect because x_chromosome_dominance_coeff_ is not readily available to Mutation
				while (genome_iter != genome_max)
				{
					MutationIndex genome_mutation = *genome_iter;
					slim_selcoeff_t selection_coeff = (mut_block_ptr + genome_mutation)->selection_coeff_;
					
					if ((mut_block_ptr + genome_mutation)->mutation_type_ptr_ == p_single_callback_mut_type)
					{
						w *= ApplyFitnessCallbacks(genome_mutation, -1, 1.0 + x_chromosome_dominance_coeff_ * selection_coeff, p_fitness_callbacks, individual, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
					}
					else
					{
						if (selection_coeff != 0.0f)
						{
							w *= (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
							
							if (w <= 0.0)
								return 0.0;
						}
					}
					
					genome_iter++;
				}
			}
			else
			{
				// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
				while (genome_iter != genome_max)
				{
					MutationIndex genome_mutation = *genome_iter;
					
					if ((mut_block_ptr + genome_mutation)->mutation_type_ptr_ == p_single_callback_mut_type)
					{
						w *= ApplyFitnessCallbacks(genome_mutation, -1, (mut_block_ptr + genome_mutation)->cached_one_plus_sel_, p_fitness_callbacks, individual, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
					}
					else
					{
						w *= (mut_block_ptr + genome_mutation)->cached_one_plus_sel_;
					}
					
					genome_iter++;
				}
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		const int32_t mutrun_count = genome1->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun1 = genome1->mutruns_[run_index].get();
			MutationRun *mutrun2 = genome2->mutruns_[run_index].get();
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome1_iter, *genome2_iter, *genome1_max, *genome2_max;
			
			mutrun1->beginend_nonneutral_pointers(&genome1_iter, &genome1_max, nonneutral_change_counter, nonneutral_regime);
			mutrun2->beginend_nonneutral_pointers(&genome2_iter, &genome2_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome1_iter = mutrun1->begin_pointer_const();
			const MutationIndex *genome2_iter = mutrun2->begin_pointer_const();
			
			const MutationIndex *genome1_max = mutrun1->end_pointer_const();
			const MutationIndex *genome2_max = mutrun2->end_pointer_const();
#endif
			
			// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
			if (genome1_iter != genome1_max && genome2_iter != genome2_max)
			{
				MutationIndex genome1_mutation = *genome1_iter, genome2_mutation = *genome2_iter;
				slim_position_t genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_, genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
				
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						MutationType *genome1_muttype = (mut_block_ptr + genome1_mutation)->mutation_type_ptr_;
						
						if (genome1_muttype == p_single_callback_mut_type)
						{
							w *= ApplyFitnessCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
							
							if (w <= 0.0)
								return 0.0;
						}
						else
						{
							w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
						}
						
						if (++genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						MutationType *genome2_muttype = (mut_block_ptr + genome2_mutation)->mutation_type_ptr_;
						
						if (genome2_muttype == p_single_callback_mut_type)
						{
							w *= ApplyFitnessCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
							
							if (w <= 0.0)
								return 0.0;
						}
						else
						{
							w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
						}
						
						if (++genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						slim_position_t position = genome1_iter_position;
						const MutationIndex *genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							MutationType *genome1_muttype = (mut_block_ptr + genome1_mutation)->mutation_type_ptr_;
							
							if (genome1_muttype == p_single_callback_mut_type)
							{
								const MutationIndex *genome2_matchscan = genome2_iter; 
								
								// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
								while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
								{
									if (genome1_mutation == *genome2_matchscan)		// note pointer equality test
									{
										// a match was found, so we multiply our fitness by the full selection coefficient
										w *= ApplyFitnessCallbacks(genome1_mutation, true, (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_, p_fitness_callbacks, individual, genome1, genome2);
										
										goto homozygousExit5;
									}
									
									genome2_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= ApplyFitnessCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
								
							homozygousExit5:
								
								if (w <= 0.0)
									return 0.0;
							}
							else
							{
								const MutationIndex *genome2_matchscan = genome2_iter; 
								
								// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
								while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
								{
									if (genome1_mutation == *genome2_matchscan) 		// note pointer equality test
									{
										// a match was found, so we multiply our fitness by the full selection coefficient
										w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_;
										goto homozygousExit6;
									}
									
									genome2_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
								
							homozygousExit6:
								;
							}
							
							if (++genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							MutationType *genome2_muttype = (mut_block_ptr + genome2_mutation)->mutation_type_ptr_;
							
							if (genome2_muttype == p_single_callback_mut_type)
							{
								const MutationIndex *genome1_matchscan = genome1_start; 
								
								// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
								while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
								{
									if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
									{
										// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
										goto homozygousExit7;
									}
									
									genome1_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= ApplyFitnessCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
								
								if (w <= 0.0)
									return 0.0;
								
							homozygousExit7:
								;
							}
							else
							{
								const MutationIndex *genome1_matchscan = genome1_start; 
								
								// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
								while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
								{
									if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
									{
										// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
										goto homozygousExit8;
									}
									
									genome1_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
								
							homozygousExit8:
								;
							}
							
							if (++genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
							}
						} while (genome2_iter_position == position);
						
						// break out if either genome has reached its end
						if (genome1_iter == genome1_max || genome2_iter == genome2_max)
							break;
					}
				} while (true);
			}
			
			// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
			assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
			
			// if genome1 is unfinished, finish it
			while (genome1_iter != genome1_max)
			{
				MutationIndex genome1_mutation = *genome1_iter;
				MutationType *genome1_muttype = (mut_block_ptr + genome1_mutation)->mutation_type_ptr_;
				
				if (genome1_muttype == p_single_callback_mut_type)
				{
					w *= ApplyFitnessCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
				}
				
				genome1_iter++;
			}
			
			// if genome2 is unfinished, finish it
			while (genome2_iter != genome2_max)
			{
				MutationIndex genome2_mutation = *genome2_iter;
				MutationType *genome2_muttype = (mut_block_ptr + genome2_mutation)->mutation_type_ptr_;
				
				if (genome2_muttype == p_single_callback_mut_type)
				{
					w *= ApplyFitnessCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
				}
				
				genome2_iter++;
			}
		}
		
		return w;
	}
}

void Subpopulation::SwapChildAndParentGenomes(void)
{
	bool will_need_new_children = false;
	
	// If there are any differences between the parent and child genome setups (due to change in subpop size, sex ratio, etc.), we will need to create new child genomes after swapping
	// This is because the parental genomes, which are based on the old parental values, will get swapped in to the children, but they will be out of date.
	if (parent_subpop_size_ != child_subpop_size_ || parent_sex_ratio_ != child_sex_ratio_ || parent_first_male_index_ != child_first_male_index_)
		will_need_new_children = true;
	
	// Execute the genome swap
	child_genomes_.swap(parent_genomes_);
	cached_child_genomes_value_.swap(cached_parent_genomes_value_);
	
	// Execute a swap of individuals as well; since individuals carry so little baggage, this is mostly important just for moving tag values
	child_individuals_.swap(parent_individuals_);
	cached_child_individuals_value_.swap(cached_parent_individuals_value_);
	
	// Clear out any dictionary values and color values stored in what are now the child individuals
	for (Individual &child : child_individuals_)
	{
		child.RemoveAllKeys();
		child.ClearColor();
	}
	
	// The parents now have the values that used to belong to the children.
	parent_subpop_size_ = child_subpop_size_;
	parent_sex_ratio_ = child_sex_ratio_;
	parent_first_male_index_ = child_first_male_index_;
	
	// mark the child generation as invalid, until it is generated
	child_generation_valid_ = false;
	
	// The parental genomes, which have now been swapped into the child genome vactor, no longer fit the bill.  We need to throw them out and generate new genome vectors.
	if (will_need_new_children)
		GenerateChildrenToFit(false);	// false means generate only new children, not new parents
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *Subpopulation::Class(void) const
{
	return gSLiM_Subpopulation_Class;
}

void Subpopulation::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<p" << subpopulation_id_ << ">";
}

EidosValue_SP Subpopulation::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:				// ACCELERATED
		{
			if (!cached_value_subpop_id_)
				cached_value_subpop_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subpopulation_id_));
			return cached_value_subpop_id_;
		}
		case gID_firstMaleIndex:	// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(child_generation_valid_ ? child_first_male_index_ : parent_first_male_index_));
		case gID_genomes:
		{
			if (child_generation_valid_)
			{
				if (!cached_child_genomes_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class);
					cached_child_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter = child_genomes_.begin(); genome_iter != child_genomes_.end(); genome_iter++)
						vec->PushObjectElement(&(*genome_iter));		// operator * can be overloaded by the iterator
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = (EidosValue_Object_vector *)cached_child_genomes_value_.get();
					const std::vector<EidosObjectElement *> *vec_direct = vec->ObjectElementVector();
					int vec_size = (int)vec_direct->size();
					
					if (vec_size == (int)child_genomes_.size())
					{
						for (int i = 0; i < vec_size; ++i)
							if ((*vec_direct)[i] != &(child_genomes_[i]))
								EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): value mismatch in cached_child_genomes_value_." << EidosTerminate();
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): size mismatch in cached_child_genomes_value_." << EidosTerminate();
				}
				*/
				
				return cached_child_genomes_value_;
			}
			else
			{
				if (!cached_parent_genomes_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class);
					cached_parent_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter = parent_genomes_.begin(); genome_iter != parent_genomes_.end(); genome_iter++)
						vec->PushObjectElement(&(*genome_iter));		// operator * can be overloaded by the iterator
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = (EidosValue_Object_vector *)cached_parent_genomes_value_.get();
					const std::vector<EidosObjectElement *> *vec_direct = vec->ObjectElementVector();
					int vec_size = (int)vec_direct->size();
					
					if (vec_size == (int)parent_genomes_.size())
					{
						for (int i = 0; i < vec_size; ++i)
							if ((*vec_direct)[i] != &(parent_genomes_[i]))
								EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): value mismatch in cached_parent_genomes_value_." << EidosTerminate();
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): size mismatch in cached_parent_genomes_value_." << EidosTerminate();
				}
				*/
				
				return cached_parent_genomes_value_;
			}
		}
		case gID_individuals:
		{
			if (child_generation_valid_)
			{
				slim_popsize_t subpop_size = child_subpop_size_;
				
				// Check for an outdated cache and detach from it
				// BCH 7 November 2016: This probably never occurs any more; we reset() in
				// GenerateChildrenToFit() now, pre-emptively.  See the comment there.
				if (cached_child_individuals_value_ && (cached_child_individuals_value_->Count() != subpop_size))
					cached_child_individuals_value_.reset();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_child_individuals_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class);
					cached_child_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->PushObjectElement(&child_individuals_[individual_index]);
				}
				
				return cached_child_individuals_value_;
			}
			else
			{
				slim_popsize_t subpop_size = parent_subpop_size_;
				
				// Check for an outdated cache and detach from it
				// BCH 7 November 2016: This probably never occurs any more; we reset() in
				// GenerateChildrenToFit() now, pre-emptively.  See the comment there.
				if (cached_parent_individuals_value_ && (cached_parent_individuals_value_->Count() != subpop_size))
					cached_parent_individuals_value_.reset();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_parent_individuals_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class);
					cached_parent_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->PushObjectElement(&parent_individuals_[individual_index]);
				}
				
				return cached_parent_individuals_value_;
			}
		}
		case gID_immigrantSubpopIDs:
		{
			EidosValue_Int_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair = migrant_fractions_.begin(); migrant_pair != migrant_fractions_.end(); ++migrant_pair)
				vec->PushInt(migrant_pair->first);
			
			return result_SP;
		}
		case gID_immigrantSubpopFractions:
		{
			EidosValue_Float_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair = migrant_fractions_.begin(); migrant_pair != migrant_fractions_.end(); ++migrant_pair)
				vec->PushFloat(migrant_pair->second);
			
			return result_SP;
		}
		case gID_selfingRate:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selfing_fraction_));
		case gID_cloningRate:
			if (sex_enabled_)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{female_clone_fraction_, male_clone_fraction_});
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(female_clone_fraction_));
		case gID_sexRatio:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(child_generation_valid_ ? child_sex_ratio_ : parent_sex_ratio_));
		case gID_spatialBounds:
		{
			SLiMSim &sim = population_.sim_;
			int dimensionality = sim.SpatialDimensionality();
			
			switch (dimensionality)
			{
				case 0: return gStaticEidosValue_Float_ZeroVec;
				case 1: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_x1_});
				case 2: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_y0_, bounds_x1_, bounds_y1_});
				case 3: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_y0_, bounds_z0_, bounds_x1_, bounds_y1_, bounds_z1_});
			}
		}
		case gID_individualCount:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(child_generation_valid_ ? child_subpop_size_ : parent_subpop_size_));
			
			// variables
		case gID_tag:					// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

int64_t Subpopulation::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_id:				return subpopulation_id_;
		case gID_firstMaleIndex:	return (child_generation_valid_ ? child_first_male_index_ : parent_first_male_index_);
		case gID_individualCount:	return (child_generation_valid_ ? child_subpop_size_ : parent_subpop_size_);
		case gID_tag:				return tag_value_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

double Subpopulation::GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_selfingRate:		return selfing_fraction_;
		case gID_sexRatio:			return (child_generation_valid_ ? child_sex_ratio_ : parent_sex_ratio_);
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Float(p_property_id);
	}
}


void Subpopulation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
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

EidosValue_SP Subpopulation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_setMigrationRates:		return ExecuteMethod_setMigrationRates(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_pointInBounds:			return ExecuteMethod_pointInBounds(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_pointReflected:		return ExecuteMethod_pointReflected(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_pointStopped:			return ExecuteMethod_pointStopped(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_pointPeriodic:			return ExecuteMethod_pointPeriodic(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_pointUniform:			return ExecuteMethod_pointUniform(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setCloningRate:		return ExecuteMethod_setCloningRate(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setSelfingRate:		return ExecuteMethod_setSelfingRate(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setSexRatio:			return ExecuteMethod_setSexRatio(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setSpatialBounds:		return ExecuteMethod_setSpatialBounds(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setSubpopulationSize:	return ExecuteMethod_setSubpopulationSize(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_cachedFitness:			return ExecuteMethod_cachedFitness(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_defineSpatialMap:		return ExecuteMethod_defineSpatialMap(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_spatialMapColor:		return ExecuteMethod_spatialMapColor(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_spatialMapValue:		return ExecuteMethod_spatialMapValue(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_outputMSSample:
		case gID_outputVCFSample:
		case gID_outputSample:			return ExecuteMethod_outputXSample(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:						return SLiMEidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	- (void)setMigrationRates(object sourceSubpops, numeric rates)
//
EidosValue_SP Subpopulation::ExecuteMethod_setMigrationRates(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg1_value = p_arguments[1].get();
	
	int source_subpops_count = arg0_value->Count();
	int rates_count = arg1_value->Count();
	std::vector<slim_objectid_t> subpops_seen;
	
	if (source_subpops_count != rates_count)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() requires sourceSubpops and rates to be equal in size." << EidosTerminate();
	
	for (int value_index = 0; value_index < source_subpops_count; ++value_index)
	{
		SLiMSim &sim = population_.sim_;
		EidosObjectElement *source_subpop = SLiM_ExtractSubpopulationFromEidosValue_io(arg0_value, value_index, sim, "setMigrationRates()");
		slim_objectid_t source_subpop_id = ((Subpopulation *)(source_subpop))->subpopulation_id_;
		
		if (source_subpop_id == subpopulation_id_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() does not allow migration to be self-referential (originating within the destination subpopulation)." << EidosTerminate();
		if (std::find(subpops_seen.begin(), subpops_seen.end(), source_subpop_id) != subpops_seen.end())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() two rates set for subpopulation p" << source_subpop_id << "." << EidosTerminate();
		
		double migrant_fraction = arg1_value->FloatAtIndex(value_index, nullptr);
		
		population_.SetMigration(*this, source_subpop_id, migrant_fraction);
		subpops_seen.emplace_back(source_subpop_id);
	}
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	– (logical$)pointInBounds(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointInBounds(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	int value_count = arg0_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): pointInBounds() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count != dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): pointInBounds() requires exactly as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	switch (dimensionality)
	{
		case 1:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			return ((x >= bounds_x0_) && (x <= bounds_x1_))
			? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
		}
		case 2:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			double y = arg0_value->FloatAtIndex(1, nullptr);
			return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_))
			? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
		}
		case 3:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			double y = arg0_value->FloatAtIndex(1, nullptr);
			double z = arg0_value->FloatAtIndex(2, nullptr);
			return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_) && (z >= bounds_z0_) && (z <= bounds_z1_))
			? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
		}
	}
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	– (float)pointReflected(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointReflected(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	int value_count = arg0_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): pointReflected() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count != dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): pointReflected() requires exactly as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	switch (dimensionality)
	{
		case 1:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			
			while (true)
			{
				if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
				else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
				else break;
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
		}
		case 2:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			double y = arg0_value->FloatAtIndex(1, nullptr);
			
			while (true)
			{
				if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
				else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
				else break;
			}
			
			while (true)
			{
				if (y < bounds_y0_) y = bounds_y0_ + (bounds_y0_ - y);
				else if (y > bounds_y1_) y = bounds_y1_ - (y - bounds_y1_);
				else break;
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y});
		}
		case 3:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			double y = arg0_value->FloatAtIndex(1, nullptr);
			double z = arg0_value->FloatAtIndex(2, nullptr);
			
			while (true)
			{
				if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
				else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
				else break;
			}
			
			while (true)
			{
				if (y < bounds_y0_) y = bounds_y0_ + (bounds_y0_ - y);
				else if (y > bounds_y1_) y = bounds_y1_ - (y - bounds_y1_);
				else break;
			}
			
			while (true)
			{
				if (z < bounds_z0_) z = bounds_z0_ + (bounds_z0_ - z);
				else if (z > bounds_z1_) z = bounds_z1_ - (z - bounds_z1_);
				else break;
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y, z});
		}
	}
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	– (float)pointStopped(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointStopped(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	int value_count = arg0_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): pointStopped() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count != dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): pointStopped() requires exactly as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	switch (dimensionality)
	{
		case 1:
		{
			double x = std::max(bounds_x0_, std::min(bounds_x1_, arg0_value->FloatAtIndex(0, nullptr)));
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
		}
		case 2:
		{
			double x = std::max(bounds_x0_, std::min(bounds_x1_, arg0_value->FloatAtIndex(0, nullptr)));
			double y = std::max(bounds_y0_, std::min(bounds_y1_, arg0_value->FloatAtIndex(1, nullptr)));
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y});
		}
		case 3:
		{
			double x = std::max(bounds_x0_, std::min(bounds_x1_, arg0_value->FloatAtIndex(0, nullptr)));
			double y = std::max(bounds_y0_, std::min(bounds_y1_, arg0_value->FloatAtIndex(1, nullptr)));
			double z = std::max(bounds_z0_, std::min(bounds_z1_, arg0_value->FloatAtIndex(2, nullptr)));
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y, z});
		}
	}
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	– (float)pointPeriodic(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointPeriodic(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	int value_count = arg0_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count != dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() requires exactly as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	bool periodic_x, periodic_y, periodic_z;
	
	sim.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
	
	if (!periodic_x && !periodic_y && !periodic_z)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() cannot be called when no periodic spatial dimension has been set up." << EidosTerminate();
	
	// Wrap coordinates; note that we assume here that bounds_x0_ == bounds_y0_ == bounds_z0_ == 0,
	// which is enforced when periodic boundary conditions are set, in setSpatialBounds().  Note also
	// that we don't use fmod(); maybe we should, rather than looping, but then we have to worry about
	// sign, and since new spatial points are probably usually close to being in bounds, these loops
	// probably won't execute more than once anyway...
	switch (dimensionality)
	{
		case 1:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			
			if (periodic_x)
			{
				while (x < 0.0)			x += bounds_x1_;
				while (x > bounds_x1_)	x -= bounds_x1_;
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
		}
		case 2:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			double y = arg0_value->FloatAtIndex(1, nullptr);
			
			if (periodic_x)
			{
				while (x < 0.0)			x += bounds_x1_;
				while (x > bounds_x1_)	x -= bounds_x1_;
			}
			if (periodic_y)
			{
				while (y < 0.0)			y += bounds_y1_;
				while (y > bounds_y1_)	y -= bounds_y1_;
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y});
		}
		case 3:
		{
			double x = arg0_value->FloatAtIndex(0, nullptr);
			double y = arg0_value->FloatAtIndex(1, nullptr);
			double z = arg0_value->FloatAtIndex(2, nullptr);
			
			if (periodic_x)
			{
				while (x < 0.0)			x += bounds_x1_;
				while (x > bounds_x1_)	x -= bounds_x1_;
			}
			if (periodic_y)
			{
				while (y < 0.0)			y += bounds_y1_;
				while (y > bounds_y1_)	y -= bounds_y1_;
			}
			if (periodic_z)
			{
				while (z < 0.0)			z += bounds_z1_;
				while (z > bounds_z1_)	z -= bounds_z1_;
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y, z});
		}
	}
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	– (float)pointUniform(void)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointUniform(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): pointUniform() cannot be called in non-spatial simulations." << EidosTerminate();
	
	switch (dimensionality)
	{
		case 1:
		{
			double x = gsl_rng_uniform(gEidos_rng) * (bounds_x1_ - bounds_x0_) + bounds_x0_;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
		}
		case 2:
		{
			double x = gsl_rng_uniform(gEidos_rng) * (bounds_x1_ - bounds_x0_) + bounds_x0_;
			double y = gsl_rng_uniform(gEidos_rng) * (bounds_y1_ - bounds_y0_) + bounds_y0_;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y});
		}
		case 3:
		{
			double x = gsl_rng_uniform(gEidos_rng) * (bounds_x1_ - bounds_x0_) + bounds_x0_;
			double y = gsl_rng_uniform(gEidos_rng) * (bounds_y1_ - bounds_y0_) + bounds_y0_;
			double z = gsl_rng_uniform(gEidos_rng) * (bounds_z1_ - bounds_z0_) + bounds_z0_;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y, z});
		}
	}
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	- (void)setCloningRate(numeric rate)
//
EidosValue_SP Subpopulation::ExecuteMethod_setCloningRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	int value_count = arg0_value->Count();
	
	if (sex_enabled_)
	{
		// SEX ONLY: either one or two values may be specified; if two, it is female at 0, male at 1
		if ((value_count < 1) || (value_count > 2))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires a rate vector containing either one or two values, in sexual simulations." << EidosTerminate();
		
		double female_cloning_fraction = arg0_value->FloatAtIndex(0, nullptr);
		double male_cloning_fraction = (value_count == 2) ? arg0_value->FloatAtIndex(1, nullptr) : female_cloning_fraction;
		
		if (female_cloning_fraction < 0.0 || female_cloning_fraction > 1.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << female_cloning_fraction << " supplied)." << EidosTerminate();
		if (male_cloning_fraction < 0.0 || male_cloning_fraction > 1.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << male_cloning_fraction << " supplied)." << EidosTerminate();
		
		female_clone_fraction_ = female_cloning_fraction;
		male_clone_fraction_ = male_cloning_fraction;
	}
	else
	{
		// ASEX ONLY: only one value may be specified
		if (value_count != 1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires a rate vector containing exactly one value, in asexual simulations.." << EidosTerminate();
		
		double cloning_fraction = arg0_value->FloatAtIndex(0, nullptr);
		
		if (cloning_fraction < 0.0 || cloning_fraction > 1.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << cloning_fraction << " supplied)." << EidosTerminate();
		
		female_clone_fraction_ = cloning_fraction;
		male_clone_fraction_ = cloning_fraction;
	}
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	- (void)setSelfingRate(numeric$ rate)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSelfingRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	double selfing_fraction = arg0_value->FloatAtIndex(0, nullptr);
	
	if ((selfing_fraction != 0.0) && sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): setSelfingRate() is limited to the hermaphroditic case, and cannot be called in sexual simulations." << EidosTerminate();
	
	if (selfing_fraction < 0.0 || selfing_fraction > 1.0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): setSelfingRate() requires a selfing fraction within [0,1] (" << selfing_fraction << " supplied)." << EidosTerminate();
	
	selfing_fraction_ = selfing_fraction;
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	- (void)setSexRatio(float$ sexRatio)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSexRatio(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	// SetSexRatio() can only be called when the child generation has not yet been generated.  It sets the sex ratio on the child generation,
	// and then that sex ratio takes effect when the children are generated from the parents in EvolveSubpopulation().
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() called when the child generation was valid." << EidosTerminate();
	
	// SEX ONLY
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() is limited to the sexual case, and cannot be called in asexual simulations." << EidosTerminate();
	
	double sex_ratio = arg0_value->FloatAtIndex(0, nullptr);
	
	if (sex_ratio < 0.0 || sex_ratio > 1.0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() requires a sex ratio within [0,1] (" << sex_ratio << " supplied)." << EidosTerminate();
	
	// After we change the subpop sex ratio, we need to generate new children genomes to fit the new requirements
	child_sex_ratio_ = sex_ratio;
	GenerateChildrenToFit(false);	// false means generate only new children, not new parents
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	– (void)setSpatialBounds(float position)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSpatialBounds(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	int value_count = arg0_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() cannot be called in non-spatial simulations." << EidosTerminate();
	
	if (value_count != dimensionality * 2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() requires twice as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	bool bad_bounds = false, bad_periodic_bounds = false;
	bool periodic_x, periodic_y, periodic_z;
	
	sim.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
	
	switch (dimensionality)
	{
		case 1:
			bounds_x0_ = arg0_value->FloatAtIndex(0, nullptr);	bounds_x1_ = arg0_value->FloatAtIndex(1, nullptr);
			
			if (bounds_x1_ <= bounds_x0_)
				bad_bounds = true;
			if (periodic_x && (bounds_x0_ != 0.0))
				bad_periodic_bounds = true;
			
			break;
		case 2:
			bounds_x0_ = arg0_value->FloatAtIndex(0, nullptr);	bounds_x1_ = arg0_value->FloatAtIndex(2, nullptr);
			bounds_y0_ = arg0_value->FloatAtIndex(1, nullptr);	bounds_y1_ = arg0_value->FloatAtIndex(3, nullptr);
			
			if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_))
				bad_bounds = true;
			if ((periodic_x && (bounds_x0_ != 0.0)) || (periodic_y && (bounds_y0_ != 0.0)))
				bad_periodic_bounds = true;
			
			break;
		case 3:
			bounds_x0_ = arg0_value->FloatAtIndex(0, nullptr);	bounds_x1_ = arg0_value->FloatAtIndex(3, nullptr);
			bounds_y0_ = arg0_value->FloatAtIndex(1, nullptr);	bounds_y1_ = arg0_value->FloatAtIndex(4, nullptr);
			bounds_z0_ = arg0_value->FloatAtIndex(2, nullptr);	bounds_z1_ = arg0_value->FloatAtIndex(5, nullptr);
			
			if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_) || (bounds_z1_ <= bounds_z0_))
				bad_bounds = true;
			if ((periodic_x && (bounds_x0_ != 0.0)) || (periodic_y && (bounds_y0_ != 0.0)) || (periodic_z && (bounds_z0_ != 0.0)))
				bad_periodic_bounds = true;
			
			break;
	}
	
	if (bad_bounds)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() requires min coordinates to be less than max coordinates." << EidosTerminate();
	
	// When a spatial dimension has been declared to be periodic, we require a min coordinate of 0.0 for that dimension
	// to keep the wrapping math simple and fast; it would not be hard to generalize the math but it would be slower
	if (bad_periodic_bounds)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() requires min coordinates to be 0.0 for dimensions that are periodic." << EidosTerminate();
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	- (void)setSubpopulationSize(integer$ size)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSubpopulationSize(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
	
	population_.SetSize(*this, subpop_size);
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	- (float)cachedFitness(Ni indices)
//
EidosValue_SP Subpopulation::ExecuteMethod_cachedFitness(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may only be called when the parental generation is active (before or during offspring generation)." << EidosTerminate();
	if (cached_fitness_size_ == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may not be called while fitness values are being calculated, or before the first time they are calculated." << EidosTerminate();
	
	bool do_all_indices = (arg0_value->Type() == EidosValueType::kValueNULL);
	slim_popsize_t index_count = (do_all_indices ? parent_subpop_size_ : SLiMCastToPopsizeTypeOrRaise(arg0_value->Count()));
	
	if (index_count == 1)
	{
		slim_popsize_t index = 0;
		
		if (!do_all_indices)
		{
			index = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
			
			if (index >= cached_fitness_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() index " << index << " out of range." << EidosTerminate();
		}
		
		double fitness = cached_parental_fitness_[index];
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fitness));
	}
	else
	{
		EidosValue_Float_vector *float_return = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(index_count);
		EidosValue_SP result_SP = EidosValue_SP(float_return);
		
		for (slim_popsize_t value_index = 0; value_index < index_count; value_index++)
		{
			slim_popsize_t index = value_index;
			
			if (!do_all_indices)
			{
				index = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(value_index, nullptr));
				
				if (index >= cached_fitness_size_)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() index " << index << " out of range." << EidosTerminate();
			}
			
			double fitness = cached_parental_fitness_[index];
			
			float_return->PushFloat(fitness);
		}
		
		return result_SP;
	}
}

//	*********************	– (void)defineSpatialMap(string$ name, string$ spatiality, int gridSize, float values, [logical$ interpolate = F], [Nf valueRange = NULL], [Ns colors = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_defineSpatialMap(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValue *arg2_value = p_arguments[2].get();
	EidosValue *arg3_value = p_arguments[3].get();
	EidosValue *arg4_value = p_arguments[4].get();
	EidosValue *arg5_value = p_arguments[5].get();
	EidosValue *arg6_value = p_arguments[6].get();
	
	std::string map_name = arg0_value->StringAtIndex(0, nullptr);
	std::string spatiality_string = arg1_value->StringAtIndex(0, nullptr);
	EidosValue *grid_size = arg2_value;
	EidosValue *values = arg3_value;
	bool interpolate = arg4_value->LogicalAtIndex(0, nullptr);
	EidosValue *value_range = arg5_value;
	EidosValue *colors = arg6_value;
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() map name must not be zero-length." << EidosTerminate();
	
	SLiMSim &sim = population_.sim_;
	int spatial_dimensionality = sim.SpatialDimensionality();
	int required_dimensionality;
	int map_spatiality;
	
	if (spatiality_string.compare(gEidosStr_x) == 0)		{	required_dimensionality = 1;	map_spatiality = 1;	}
	else if (spatiality_string.compare(gEidosStr_y) == 0)	{	required_dimensionality = 2;	map_spatiality = 1;	}
	else if (spatiality_string.compare(gEidosStr_z) == 0)	{	required_dimensionality = 3;	map_spatiality = 1;	}
	else if (spatiality_string.compare("xy") == 0)			{	required_dimensionality = 2;	map_spatiality = 2;	}
	else if (spatiality_string.compare("xz") == 0)			{	required_dimensionality = 3;	map_spatiality = 2;	}
	else if (spatiality_string.compare("yz") == 0)			{	required_dimensionality = 3;	map_spatiality = 2;	}
	else if (spatiality_string.compare("xyz") == 0)			{	required_dimensionality = 3;	map_spatiality = 3;	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() spatiality \"" << spatiality_string << "\" must be \"x\", \"y\", \"z\", \"xy\", \"xz\", \"yz\", or \"xyz\"." << EidosTerminate();
	
	if (required_dimensionality > spatial_dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() spatiality cannot utilize spatial dimensions beyond those set in initializeSLiMOptions()." << EidosTerminate();
	
	if (grid_size->Count() != map_spatiality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() gridSize must match the spatiality defined for the map." << EidosTerminate();
	
	int64_t map_size = 1;
	int64_t dimension_sizes[3];
	
	for (int dimension_index = 0; dimension_index < map_spatiality; ++dimension_index)
	{
		int64_t dimension_size = grid_size->IntAtIndex(dimension_index, nullptr);
		
		if (dimension_size < 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() all elements of gridSize must be of length >= 2." << EidosTerminate();
		
		dimension_sizes[dimension_index] = dimension_size;
		map_size *= dimension_size;
	}
	
	if (values->Count() != map_size)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): size of the values vector (" << values->Count() << ") does not match the product of the sizes in gridSize (" << map_size << ")." << EidosTerminate();
	
	bool range_is_null = (value_range->Type() == EidosValueType::kValueNULL);
	bool colors_is_null = (colors->Type() == EidosValueType::kValueNULL);
	double range_min = 0.0, range_max = 0.0;
	int color_count = 0;
	
	if (!range_is_null || !colors_is_null)
	{
		if (range_is_null || colors_is_null)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): valueRange and colors must either both be supplied, or neither supplied." << EidosTerminate();
		
		if (value_range->Count() != 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): valueRange must be exactly length 2 (giving the min and max value permitted)." << EidosTerminate();
		
		range_min = value_range->FloatAtIndex(0, nullptr);
		range_max = value_range->FloatAtIndex(1, nullptr);
		
		if (!std::isfinite(range_min) || !std::isfinite(range_max) || (range_min >= range_max))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): valueRange must be finite, and min < max is required." << EidosTerminate();
		
		color_count = colors->Count();
		
		if (color_count < 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): colors must be of length >= 2." << EidosTerminate();
	}
	
	// OK, everything seems to check out, so we can make our SpatialMap struct and populate it
	SpatialMap *spatial_map = new SpatialMap(spatiality_string, map_spatiality, dimension_sizes, interpolate, range_min, range_max, color_count);
	double *values_ptr = spatial_map->values_;
	const double *values_vec_ptr = values->FloatVector()->data();
	
	for (int64_t values_index = 0; values_index < map_size; ++values_index)
		*(values_ptr++) = *(values_vec_ptr++);
	
	if (color_count > 0)
	{
		float *red_ptr = spatial_map->red_components_;
		float *green_ptr = spatial_map->green_components_;
		float *blue_ptr = spatial_map->blue_components_;
		const std::string *colors_vec_ptr = colors->StringVector()->data();
		
		for (int colors_index = 0; colors_index < color_count; ++colors_index)
			Eidos_GetColorComponents(colors_vec_ptr[colors_index], red_ptr++, green_ptr++, blue_ptr++);
	}
	
	// Add the new SpatialMap to our map for future reference
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		// there is an existing entry under this name; remove it
		SpatialMap *old_map = map_iter->second;
		
		delete old_map;
		spatial_maps_.erase(map_iter);
	}
	
	spatial_maps_.insert(SpatialMapPair(map_name, spatial_map));
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	- (string)spatialMapColor(string$ name, float value)
//
EidosValue_SP Subpopulation::ExecuteMethod_spatialMapColor(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg1_value = p_arguments[1].get();
	
	std::string map_name = arg0_value->StringAtIndex(0, nullptr);
	EidosValue *values = arg1_value;
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapColor): spatialMapColor() map name must not be zero-length." << EidosTerminate();
	
	// Find the named SpatialMap
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		SpatialMap *map = map_iter->second;
		
		if (map->n_colors_ == 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapColor): spatialMapColor() no color map defined for spatial map." << EidosTerminate();
		
		int value_count = values->Count();
		EidosValue_String_vector *string_return = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(value_count);
		EidosValue_SP result_SP = EidosValue_SP(string_return);
		
		for (slim_popsize_t value_index = 0; value_index < value_count; value_index++)
		{
			double value = values->FloatAtIndex(value_index, nullptr);
			double rgb[3];
			char hex_chars[8];
			
			map->ColorForValue(value, rgb);
			Eidos_GetColorString(rgb[0], rgb[1], rgb[2], hex_chars);
			string_return->PushString(std::string(hex_chars));
		}
		
		return result_SP;
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapColor): spatialMapColor() could not find map with name " << map_name << "." << EidosTerminate();
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	– (float$)spatialMapValue(string$ name, float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_spatialMapValue(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg1_value = p_arguments[1].get();
	
	std::string map_name = arg0_value->StringAtIndex(0, nullptr);
	EidosValue *point = arg1_value;
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() map name must not be zero-length." << EidosTerminate();
	
	// Find the named SpatialMap
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		SpatialMap *map = map_iter->second;
		
		if (point->Count() != map->spatiality_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() length of point does not match spatiality of map " << map_name << "." << EidosTerminate();
		
		// We need to use the correct spatial bounds for each coordinate, which depends upon our exact spatiality
		// There is doubtless a way to make this code smarter, but brute force is sometimes best...
		double point_vec[3];
		
#define SLiMClampCoordinate(x) ((x < 0.0) ? 0.0 : ((x > 1.0) ? 1.0 : x))
		
		switch (map->spatiality_)
		{
			case 1:
			{
				if (map->spatiality_string_ == "x")
				{
					double x = (point->FloatAtIndex(0, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
					point_vec[0] = SLiMClampCoordinate(x);
				}
				else if (map->spatiality_string_ == "y")
				{
					double y = (point->FloatAtIndex(0, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
					point_vec[0] = SLiMClampCoordinate(y);
				}
				else if (map->spatiality_string_ == "z")
				{
					double z = (point->FloatAtIndex(0, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
					point_vec[0] = SLiMClampCoordinate(z);
				}
				break;
			}
			case 2:
			{
				if (map->spatiality_string_ == "xy")
				{
					double x = (point->FloatAtIndex(0, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
					point_vec[0] = SLiMClampCoordinate(x);
					
					double y = (point->FloatAtIndex(1, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
					point_vec[1] = SLiMClampCoordinate(y);
				}
				else if (map->spatiality_string_ == "yz")
				{
					double y = (point->FloatAtIndex(0, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
					point_vec[0] = SLiMClampCoordinate(y);
					
					double z = (point->FloatAtIndex(1, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
					point_vec[1] = SLiMClampCoordinate(z);
				}
				else if (map->spatiality_string_ == "xz")
				{
					double x = (point->FloatAtIndex(0, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
					point_vec[0] = SLiMClampCoordinate(x);
					
					double z = (point->FloatAtIndex(1, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
					point_vec[1] = SLiMClampCoordinate(z);
				}
				break;
			}
			case 3:
			{
				if (map->spatiality_string_ == "xyz")
				{
					double x = (point->FloatAtIndex(0, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
					point_vec[0] = SLiMClampCoordinate(x);
					
					double y = (point->FloatAtIndex(1, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
					point_vec[1] = SLiMClampCoordinate(y);
					
					double z = (point->FloatAtIndex(2, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
					point_vec[2] = SLiMClampCoordinate(z);
				}
				break;
			}
		}
		
#undef SLiMClampCoordinate
		
		double map_value = map->ValueAtPoint(point_vec);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(map_value));
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() could not find map with name " << map_name << "." << EidosTerminate();
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	– (void)outputMSSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F])
//	*********************	– (void)outputSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F])
//	*********************	– (void)outputVCFSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [logical$ outputMultiallelics = T], [Ns$ filePath = NULL], [logical$ append=F])
//
EidosValue_SP Subpopulation::ExecuteMethod_outputXSample(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValue *arg2_value = p_arguments[2].get();
	EidosValue *arg3_value = p_arguments[3].get();
	EidosValue *arg4_value = p_arguments[4].get();
	EidosValue *arg5_value = ((p_argument_count >= 6) ? p_arguments[5].get() : nullptr);	// conditional because we handle multiple methods with different args
	
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	SLiMSim &sim = population_.sim_;
	
	if ((sim.GenerationStage() == SLiMGenerationStage::kStage1ExecuteEarlyScripts) && (!sim.warned_early_output_))
	{
		output_stream << "#WARNING (Subpopulation::ExecuteMethod_outputXSample): " << Eidos_StringForGlobalStringID(p_method_id) << "() should probably not be called from an early() event; the output will reflect state at the beginning of the generation, not the end." << std::endl;
		sim.warned_early_output_ = true;
	}
	
	slim_popsize_t sample_size = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
	
	bool replace = arg1_value->LogicalAtIndex(0, nullptr);
	
	IndividualSex requested_sex;
	
	std::string sex_string = arg2_value->StringAtIndex(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): " << Eidos_StringForGlobalStringID(p_method_id) << "() requested sex \"" << sex_string << "\" unsupported." << EidosTerminate();
	
	if (!sim.SexEnabled() && requested_sex != IndividualSex::kUnspecified)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): " << Eidos_StringForGlobalStringID(p_method_id) << "() requested sex is not legal in a non-sexual simulation." << EidosTerminate();
	
	bool output_multiallelics = true;
	
	if (p_method_id == gID_outputVCFSample)
		output_multiallelics = arg3_value->LogicalAtIndex(0, nullptr);
	
	// Figure out the right output stream
	std::ofstream outfile;
	bool has_file = false;
	std::string outfile_path;
	EidosValue *file_arg = ((p_method_id == gID_outputVCFSample) ? arg4_value : arg3_value);
	EidosValue *append_arg = ((p_method_id == gID_outputVCFSample) ? arg5_value : arg4_value);
	
	if (file_arg->Type() != EidosValueType::kValueNULL)
	{
		outfile_path = Eidos_ResolvedPath(file_arg->StringAtIndex(0, nullptr));
		bool append = append_arg->LogicalAtIndex(0, nullptr);
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		has_file = true;
		
		if (!outfile.is_open())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): " << Eidos_StringForGlobalStringID(p_method_id) << "() could not open "<< outfile_path << "." << EidosTerminate();
	}
	
	std::ostream &out = *(has_file ? dynamic_cast<std::ostream *>(&outfile) : dynamic_cast<std::ostream *>(&output_stream));
	
	if (!has_file || (p_method_id == gID_outputSample))
	{
		// Output header line
		out << "#OUT: " << sim.Generation() << " S";
		
		if (p_method_id == gID_outputSample)
			out << "S";
		else if (p_method_id == gID_outputMSSample)
			out << "M";
		else if (p_method_id == gID_outputVCFSample)
			out << "V";
		
		out << " p" << subpopulation_id_ << " " << sample_size;
		
		if (sim.SexEnabled())
			out << " " << requested_sex;
		
		if (has_file)
			out << " " << outfile_path;
		
		out << std::endl;
	}
	
	// Call out to produce the actual sample
	if (p_method_id == gID_outputSample)
		population_.PrintSample_SLiM(out, *this, sample_size, replace, requested_sex);
	else if (p_method_id == gID_outputMSSample)
		population_.PrintSample_MS(out, *this, sample_size, replace, requested_sex, sim.TheChromosome());
	else if (p_method_id == gID_outputVCFSample)
		population_.PrintSample_VCF(out, *this, sample_size, replace, requested_sex, output_multiallelics);
	
	if (has_file)
		outfile.close(); 
	
	return gStaticEidosValueNULLInvisible;
}


//
//	Subpopulation_Class
//
#pragma mark -
#pragma mark Subpopulation_Class
#pragma mark -

class Subpopulation_Class : public SLiMEidosDictionary_Class
{
public:
	Subpopulation_Class(const Subpopulation_Class &p_original) = delete;	// no copy-construct
	Subpopulation_Class& operator=(const Subpopulation_Class&) = delete;	// no copying
	
	Subpopulation_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Subpopulation_Class = new Subpopulation_Class;


Subpopulation_Class::Subpopulation_Class(void)
{
}

const std::string &Subpopulation_Class::ElementType(void) const
{
	return gStr_Subpopulation;
}

const std::vector<const EidosPropertySignature *> *Subpopulation_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_id));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_firstMaleIndex));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomes));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_individuals));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_immigrantSubpopIDs));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_immigrantSubpopFractions));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_selfingRate));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_cloningRate));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_sexRatio));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_spatialBounds));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_individualCount));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Subpopulation_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *firstMaleIndexSig = nullptr;
	static EidosPropertySignature *genomesSig = nullptr;
	static EidosPropertySignature *individualsSig = nullptr;
	static EidosPropertySignature *immigrantSubpopIDsSig = nullptr;
	static EidosPropertySignature *immigrantSubpopFractionsSig = nullptr;
	static EidosPropertySignature *selfingRateSig = nullptr;
	static EidosPropertySignature *cloningRateSig = nullptr;
	static EidosPropertySignature *sexRatioSig = nullptr;
	static EidosPropertySignature *spatialBoundsSig = nullptr;
	static EidosPropertySignature *sizeSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =							(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,							gID_id,							true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		firstMaleIndexSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_firstMaleIndex,				gID_firstMaleIndex,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		genomesSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,						gID_genomes,					true,	kEidosValueMaskObject, gSLiM_Genome_Class));
		individualsSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_individuals,					gID_individuals,				true,	kEidosValueMaskObject, gSLiM_Individual_Class));
		immigrantSubpopIDsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopIDs,			gID_immigrantSubpopIDs,			true,	kEidosValueMaskInt));
		immigrantSubpopFractionsSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopFractions,	gID_immigrantSubpopFractions,	true,	kEidosValueMaskFloat));
		selfingRateSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_selfingRate,					gID_selfingRate,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		cloningRateSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_cloningRate,					gID_cloningRate,				true,	kEidosValueMaskFloat));
		sexRatioSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_sexRatio,					gID_sexRatio,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		spatialBoundsSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialBounds,				gID_spatialBounds,				true,	kEidosValueMaskFloat));
		sizeSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_individualCount,				gID_individualCount,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		tagSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,							gID_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:						return idSig;
		case gID_firstMaleIndex:			return firstMaleIndexSig;
		case gID_genomes:					return genomesSig;
		case gID_individuals:				return individualsSig;
		case gID_immigrantSubpopIDs:		return immigrantSubpopIDsSig;
		case gID_immigrantSubpopFractions:	return immigrantSubpopFractionsSig;
		case gID_selfingRate:				return selfingRateSig;
		case gID_cloningRate:				return cloningRateSig;
		case gID_sexRatio:					return sexRatioSig;
		case gID_spatialBounds:				return spatialBoundsSig;
		case gID_individualCount:			return sizeSig;
		case gID_tag:						return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Subpopulation_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*SLiMEidosDictionary_Class::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_setMigrationRates));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointInBounds));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointReflected));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointStopped));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointPeriodic));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointUniform));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setCloningRate));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSelfingRate));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSexRatio));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSpatialBounds));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSubpopulationSize));
		methods->emplace_back(SignatureForMethodOrRaise(gID_cachedFitness));
		methods->emplace_back(SignatureForMethodOrRaise(gID_defineSpatialMap));
		methods->emplace_back(SignatureForMethodOrRaise(gID_spatialMapColor));
		methods->emplace_back(SignatureForMethodOrRaise(gID_spatialMapValue));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputMSSample));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputVCFSample));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputSample));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Subpopulation_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *setMigrationRatesSig = nullptr;
	static EidosInstanceMethodSignature *pointInBoundsSig = nullptr;
	static EidosInstanceMethodSignature *pointReflectedSig = nullptr;
	static EidosInstanceMethodSignature *pointStoppedSig = nullptr;
	static EidosInstanceMethodSignature *pointPeriodicSig = nullptr;
	static EidosInstanceMethodSignature *pointUniformSig = nullptr;
	static EidosInstanceMethodSignature *setCloningRateSig = nullptr;
	static EidosInstanceMethodSignature *setSelfingRateSig = nullptr;
	static EidosInstanceMethodSignature *setSexRatioSig = nullptr;
	static EidosInstanceMethodSignature *setSpatialBoundsSig = nullptr;
	static EidosInstanceMethodSignature *setSubpopulationSizeSig = nullptr;
	static EidosInstanceMethodSignature *cachedFitnessSig = nullptr;
	static EidosInstanceMethodSignature *defineSpatialMapSig = nullptr;
	static EidosInstanceMethodSignature *spatialMapColorSig = nullptr;
	static EidosInstanceMethodSignature *spatialMapValueSig = nullptr;
	static EidosInstanceMethodSignature *outputMSSampleSig = nullptr;
	static EidosInstanceMethodSignature *outputVCFSampleSig = nullptr;
	static EidosInstanceMethodSignature *outputSampleSig = nullptr;
	
	if (!setMigrationRatesSig)
	{
		setMigrationRatesSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMigrationRates, kEidosValueMaskNULL))->AddIntObject("sourceSubpops", gSLiM_Subpopulation_Class)->AddNumeric("rates");
		pointInBoundsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointInBounds, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddFloat("point");
		pointReflectedSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointReflected, kEidosValueMaskFloat))->AddFloat("point");
		pointStoppedSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointStopped, kEidosValueMaskFloat))->AddFloat("point");
		pointPeriodicSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointPeriodic, kEidosValueMaskFloat))->AddFloat("point");
		pointUniformSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointUniform, kEidosValueMaskFloat));
		setCloningRateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setCloningRate, kEidosValueMaskNULL))->AddNumeric("rate");
		setSelfingRateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelfingRate, kEidosValueMaskNULL))->AddNumeric_S("rate");
		setSexRatioSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSexRatio, kEidosValueMaskNULL))->AddFloat_S("sexRatio");
		setSpatialBoundsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSpatialBounds, kEidosValueMaskNULL))->AddFloat("bounds");
		setSubpopulationSizeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSubpopulationSize, kEidosValueMaskNULL))->AddInt_S("size");
		cachedFitnessSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_cachedFitness, kEidosValueMaskFloat))->AddInt_N("indices");
		defineSpatialMapSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_defineSpatialMap, kEidosValueMaskNULL))->AddString_S("name")->AddString_S("spatiality")->AddInt("gridSize")->AddFloat("values")->AddLogical_OS("interpolate", gStaticEidosValue_LogicalF)->AddFloat_ON("valueRange", gStaticEidosValueNULL)->AddString_ON("colors", gStaticEidosValueNULL);
		spatialMapColorSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_spatialMapColor, kEidosValueMaskString))->AddString_S("name")->AddFloat("value");
		spatialMapValueSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_spatialMapValue, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddString_S("name")->AddFloat("point");
		outputMSSampleSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMSSample, kEidosValueMaskNULL))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF);
		outputVCFSampleSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputVCFSample, kEidosValueMaskNULL))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF);
		outputSampleSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputSample, kEidosValueMaskNULL))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF);
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_setMigrationRates:		return setMigrationRatesSig;
		case gID_pointInBounds:			return pointInBoundsSig;
		case gID_pointReflected:		return pointReflectedSig;
		case gID_pointStopped:			return pointStoppedSig;
		case gID_pointPeriodic:			return pointPeriodicSig;
		case gID_pointUniform:			return pointUniformSig;
		case gID_setCloningRate:		return setCloningRateSig;
		case gID_setSelfingRate:		return setSelfingRateSig;
		case gID_setSexRatio:			return setSexRatioSig;
		case gID_setSpatialBounds:		return setSpatialBoundsSig;
		case gID_setSubpopulationSize:	return setSubpopulationSizeSig;
		case gID_cachedFitness:			return cachedFitnessSig;
		case gID_defineSpatialMap:		return defineSpatialMapSig;
		case gID_spatialMapColor:		return spatialMapColorSig;
		case gID_spatialMapValue:		return spatialMapValueSig;
		case gID_outputMSSample:		return outputMSSampleSig;
		case gID_outputVCFSample:		return outputVCFSampleSig;
		case gID_outputSample:			return outputSampleSig;
			
			// all others, including gID_none
		default:
			return SLiMEidosDictionary_Class::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP Subpopulation_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}




































































