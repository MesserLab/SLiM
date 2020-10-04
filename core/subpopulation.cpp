//
//  subpopulation.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
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
#include "slim_globals.h"
#include "population.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"
#include "eidos_globals.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <assert.h>
#include <string>
#include <map>
#include <utility>
#include <cmath>

extern std::vector<EidosValue_Object *> gEidosValue_Object_Genome_Registry;		// this is in Eidos; see Subpopulation::ExecuteMethod_takeMigrants()
extern std::vector<EidosValue_Object *> gEidosValue_Object_Individual_Registry;	// this is in Eidos; see Subpopulation::ExecuteMethod_takeMigrants()


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
		default:
			EIDOS_TERMINATION << "ERROR (_SpatialMap::_SpatialMap): (internal error) unsupported spatiality." << EidosTerminate();
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

double _SpatialMap::ValueAtPoint_S1(double *p_point)
{
	// This looks up the value at point, which is in coordinates that have been normalized and clamped to [0,1]
	assert (spatiality_ == 1);
	
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
}

double _SpatialMap::ValueAtPoint_S2(double *p_point)
{
	// This looks up the value at point, which is in coordinates that have been normalized and clamped to [0,1]
	assert (spatiality_ == 2);
	
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
}

double _SpatialMap::ValueAtPoint_S3(double *p_point)
{
	// This looks up the value at point, which is in coordinates that have been normalized and clamped to [0,1]
	assert (spatiality_ == 3);
	
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
}

void _SpatialMap::ColorForValue(double p_value, double *p_rgb_ptr)
{
	if (n_colors_ == 0)
	{
		// this is the case when a color table was not defined; here, min could equal max
		// in this case, all values in the map should fall in the interval [min_value_, max_value_]
		double value_fraction = ((min_value_ < max_value_) ? ((p_value - min_value_) / (max_value_ - min_value_)) : 0.0);
		p_rgb_ptr[0] = value_fraction;
		p_rgb_ptr[1] = value_fraction;
		p_rgb_ptr[2] = value_fraction;
	}
	else
	{
		// this is the case when a color table was defined; here, min < max
		// in this case, values in the map may fall outside the interval [min_value_, max_value_]
		double value_fraction = (p_value - min_value_) / (max_value_ - min_value_);
		double color_index = value_fraction * (n_colors_ - 1);
		int color_index_1 = (int)floor(color_index);
		int color_index_2 = (int)ceil(color_index);
		
		if (color_index_1 < 0) color_index_1 = 0;
		if (color_index_1 >= n_colors_) color_index_1 = n_colors_ - 1;
		if (color_index_2 < 0) color_index_2 = 0;
		if (color_index_2 >= n_colors_) color_index_2 = n_colors_ - 1;
		
		double color_2_weight = color_index - color_index_1;
		double color_1_weight = 1.0F - color_2_weight;
		
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
}

void _SpatialMap::ColorForValue(double p_value, float *p_rgb_ptr)
{
	if (n_colors_ == 0)
	{
		// this is the case when a color table was not defined; here, min could equal max
		// in this case, all values in the map should fall in the interval [min_value_, max_value_]
		float value_fraction = (float)((min_value_ < max_value_) ? ((p_value - min_value_) / (max_value_ - min_value_)) : 0.0);
		p_rgb_ptr[0] = value_fraction;
		p_rgb_ptr[1] = value_fraction;
		p_rgb_ptr[2] = value_fraction;
	}
	else
	{
		// this is the case when a color table was defined; here, min < max
		// in this case, values in the map may fall outside the interval [min_value_, max_value_]
		double value_fraction = (p_value - min_value_) / (max_value_ - min_value_);
		double color_index = value_fraction * (n_colors_ - 1);
		int color_index_1 = (int)floor(color_index);
		int color_index_2 = (int)ceil(color_index);
		
		if (color_index_1 < 0) color_index_1 = 0;
		if (color_index_1 >= n_colors_) color_index_1 = n_colors_ - 1;
		if (color_index_2 < 0) color_index_2 = 0;
		if (color_index_2 >= n_colors_) color_index_2 = n_colors_ - 1;
		
		double color_2_weight = color_index - color_index_1;
		double color_1_weight = 1.0F - color_2_weight;
		
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
}


#pragma mark -
#pragma mark Subpopulation
#pragma mark -

void Subpopulation::MakeMemoryPools(size_t p_individual_capacity)
{
	// We allocate new genomes and individuals out of shared memory pools, in an effort to keep them close
	// together in memory so as to reap the benefits of locality in memory accesses.  This really does matter;
	// when I switched from using vector<Genome> to vector<Genome *>, some models got as much as 50% slower,
	// and adding these object pools gained back almost all of that loss.
	if (genome_pool_ || individual_pool_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::MakeMemoryPools): (internal error) memory pools already allocated in MakeMemoryPools()." << EidosTerminate();
	
	{
		size_t capacity = lround(pow(2.0, ceil(log2(p_individual_capacity * 2))));	// diploid
		
		if (capacity < 1024)
			capacity = 1024;
		
		//std::cout << "sizeof(Genome) == " << sizeof(Genome) << std::endl;
		
		genome_pool_ = new EidosObjectPool(sizeof(Genome), capacity);
	}
	
	{
		size_t capacity = lround(pow(2.0, ceil(log2(p_individual_capacity))));
		
		if (capacity < 1024)
			capacity = 1024;
		
		//std::cout << "sizeof(Individual) == " << sizeof(Individual) << std::endl;
		
		individual_pool_ = new EidosObjectPool(sizeof(Individual), capacity);
	}
}

Genome *Subpopulation::_NewSubpopGenome(int p_mutrun_count, slim_position_t p_mutrun_length, GenomeType p_genome_type, bool p_is_null)
{
	// This gets called if a null genome is requested but the null junkyard is empty, or if a non-null genome is requested
	// but the non-null junkyard is empty; so we know that the primary junkyard for the request cannot service the request.
	// If the other junkyard has a genome, we want to repurpose it; this prevents one junkyard from filling up with an
	// ever-growing number of genomes while requests to the other junkyard create new genomes (which can happen because
	// genomes can be transmogrified between null and non-null after creation).  We create a new genome only if both
	// junkyards are empty.
	if (p_is_null)
	{
		if (genome_junkyard_nonnull.size())
		{
			Genome *back = genome_junkyard_nonnull.back();
			genome_junkyard_nonnull.pop_back();
			
			// got a non-null genome (guaranteed cleared to nullptr by FreeSubpopGenome()), need to repurpose it to be a null genome
			back->ReinitializeGenomeNullptr(p_genome_type, 0, 0);
			
			return back;
		}
	}
	else
	{
		if (genome_junkyard_null.size())
		{
			Genome *back = genome_junkyard_null.back();
			genome_junkyard_null.pop_back();
			
			// got a null genome, need to repurpose it to be a non-null genome cleared to nullptr
			back->ReinitializeGenomeNullptr(p_genome_type, p_mutrun_count, p_mutrun_length);
			
			return back;
		}
	}
	
	return new (genome_pool_->AllocateChunk()) Genome(this, p_mutrun_count, p_mutrun_length, p_genome_type, p_is_null);
}

#ifdef SLIM_WF_ONLY
void Subpopulation::WipeIndividualsAndGenomes(std::vector<Individual *> &p_individuals, std::vector<Genome *> &p_genomes, slim_popsize_t p_individual_count, slim_popsize_t p_first_male, bool p_no_clear)
{
	SLiMSim &sim = population_.sim_;
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	if (p_first_male == -1)
	{
		// make hermaphrodites
		if (p_individual_count > 0)
		{
			if (p_no_clear)
			{
				for (int index = 0; index < p_individual_count; ++index)
				{
					p_genomes[index * 2]->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
					p_genomes[index * 2 + 1]->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
				}
			}
			else
			{
				MutationRun *shared_empty_run = MutationRun::NewMutationRun();
				
				for (int index = 0; index < p_individual_count; ++index)
				{
					p_genomes[index * 2]->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
					p_genomes[index * 2 + 1]->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
				}
			}
		}
	}
	else
	{
		// make females and males
		if (p_no_clear)
		{
			for (int index = 0; index < p_individual_count; ++index)
			{
				Genome *genome1 = p_genomes[index * 2];
				Genome *genome2 = p_genomes[index * 2 + 1];
				Individual *individual = p_individuals[index];
				bool is_female = (index < p_first_male);
				
				individual->sex_ = (is_female ? IndividualSex::kFemale : IndividualSex::kMale);
			
				switch (modeled_chromosome_type_)
				{
					case GenomeType::kAutosome:
					{
						genome1->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
						genome2->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
						break;
					}
					case GenomeType::kXChromosome:
					{
						genome1->ReinitializeGenomeNullptr(GenomeType::kXChromosome, mutrun_count, mutrun_length);
						
						if (is_female)	genome2->ReinitializeGenomeNullptr(GenomeType::kXChromosome, mutrun_count, mutrun_length);
						else			genome2->ReinitializeGenomeNullptr(GenomeType::kYChromosome, 0, 0);									// leave as a null genome
						
						break;
					}
					case GenomeType::kYChromosome:
					{
						genome1->ReinitializeGenomeNullptr(GenomeType::kXChromosome, 0, 0);													// leave as a null genome
						
						if (is_female)	genome2->ReinitializeGenomeNullptr(GenomeType::kXChromosome, 0, 0);									// leave as a null genome
						else			genome2->ReinitializeGenomeNullptr(GenomeType::kYChromosome, mutrun_count, mutrun_length);
						
						break;
					}
				}
			}
		}
		else
		{
			// since we're now guaranteed to have at least one male and one female, we're guaranteed to use the new empty run somewhere, so no leak
			MutationRun *shared_empty_run = MutationRun::NewMutationRun();
			
			for (int index = 0; index < p_individual_count; ++index)
			{
				Genome *genome1 = p_genomes[index * 2];
				Genome *genome2 = p_genomes[index * 2 + 1];
				Individual *individual = p_individuals[index];
				bool is_female = (index < p_first_male);
				
				individual->sex_ = (is_female ? IndividualSex::kFemale : IndividualSex::kMale);
				switch (modeled_chromosome_type_)
				{
					case GenomeType::kAutosome:
					{
						genome1->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
						genome2->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
						break;
					}
					case GenomeType::kXChromosome:
					{
						genome1->ReinitializeGenomeToMutrun(GenomeType::kXChromosome, mutrun_count, mutrun_length, shared_empty_run);
						
						if (is_female)	genome2->ReinitializeGenomeToMutrun(GenomeType::kXChromosome, mutrun_count, mutrun_length, shared_empty_run);
						else			genome2->ReinitializeGenomeToMutrun(GenomeType::kYChromosome, 0, 0, shared_empty_run);									// leave as a null genome
						
						break;
					}
					case GenomeType::kYChromosome:
					{
						genome1->ReinitializeGenomeToMutrun(GenomeType::kXChromosome, 0, 0, shared_empty_run);													// leave as a null genome
						
						if (is_female)	genome2->ReinitializeGenomeToMutrun(GenomeType::kXChromosome, 0, 0, shared_empty_run);									// leave as a null genome
						else			genome2->ReinitializeGenomeToMutrun(GenomeType::kYChromosome, mutrun_count, mutrun_length, shared_empty_run);
						
						break;
					}
				}
			}
		}
	}
}

// Reconfigure the child generation to match the set size, sex ratio, etc.  This may involve removing existing individuals,
// or adding new ones.  It may also involve transmogrifying existing individuals to a new sex, etc.  It can also transmogrify
// genomes between a null and non-null state, as a side effect of changing sex.  So this code is really gross and invasive.
void Subpopulation::GenerateChildrenToFitWF()
{
	SLiMSim &sim = population_.sim_;
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	cached_child_genomes_value_.reset();
	cached_child_individuals_value_.reset();
	
	// First, make the number of Individual objects match, and make the corresponding Genome changes
	int old_individual_count = (int)child_individuals_.size();
	int new_individual_count = child_subpop_size_;
	
	if (new_individual_count > old_individual_count)
	{
		// Make sure our memory pools for genomes and individuals are allocated
		if (!genome_pool_ && !individual_pool_)
			MakeMemoryPools(new_individual_count * 2);	// room for parents and children
		
		// We also have to make space for the pointers to the genomes and individuals
		child_genomes_.reserve(new_individual_count * 2);
		child_individuals_.reserve(new_individual_count);
		
		for (int new_index = old_individual_count; new_index < new_individual_count; ++new_index)
		{
			// allocate out of our object pools
			// BCH 23 August 2018: passing false to NewSubpopGenome() for p_is_null is sometimes inaccurate, but should
			// be harmless.  If the genomes are ultimately destined to be null genomes, their mutruns buffer will get
			// freed again below.  Now that the disposed genome junkyards can supply each other when empty, there should
			// be no bigger consequence than that performance hit.  It might be nice to figure out, here, what type of
			// genome we will eventually want at this position, and make the right kind up front; but that is a
			// substantial hassle, and this should only matter in unusual models (very large-magnitude population size
			// cycling, primarily – GenerateChildrenToFitWF() often generating many new children).
			Genome *genome1 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kAutosome, false);
			Genome *genome2 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kAutosome, false);
			Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, new_index, -1, genome1, genome2, IndividualSex::kHermaphrodite, -1, /* initial fitness for new subpops */ 1.0);
			
			child_genomes_.push_back(genome1);
			child_genomes_.push_back(genome2);
			child_individuals_.push_back(individual);
		}
	}
	else if (new_individual_count < old_individual_count)
	{
		for (int old_index = new_individual_count; old_index < old_individual_count; ++old_index)
		{
			Genome *genome1 = child_genomes_[old_index * 2];
			Genome *genome2 = child_genomes_[old_index * 2 + 1];
			Individual *individual = child_individuals_[old_index];
			
			// dispose of the genomes and individual
			FreeSubpopGenome(genome1);
			FreeSubpopGenome(genome2);
			
			individual->~Individual();
			individual_pool_->DisposeChunk(const_cast<Individual *>(individual));
		}
		
		child_genomes_.resize(new_individual_count * 2);
		child_individuals_.resize(new_individual_count);
	}
	
	// Next, fix the type of each genome, and clear them all, and fix individual sex if necessary
	if (sex_enabled_)
	{
		double &sex_ratio = child_sex_ratio_;
		slim_popsize_t &first_male_index = child_first_male_index_;
		
		slim_popsize_t total_males = static_cast<slim_popsize_t>(lround(sex_ratio * new_individual_count));	// round in favor of males, arbitrarily
		
		first_male_index = new_individual_count - total_males;
		
		if (first_male_index <= 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFitWF): sex ratio of " << sex_ratio << " produced no females." << EidosTerminate();
		else if (first_male_index >= child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFitWF): sex ratio of " << sex_ratio << " produced no males." << EidosTerminate();
		
		WipeIndividualsAndGenomes(child_individuals_, child_genomes_, new_individual_count, first_male_index, true);
	}
	else
	{
		WipeIndividualsAndGenomes(child_individuals_, child_genomes_, new_individual_count, -1, true);	// make hermaphrodites
	}
}
#endif	// SLIM_WF_ONLY

// Generate new individuals to fill out a freshly created subpopulation, including recording in the tree
// sequence unless this is the result of addSubpopSplit() (which does its own recording since parents are
// involved in that case).  This handles both the WF and nonWF cases, which are very similar.
void Subpopulation::GenerateParentsToFit(slim_age_t p_initial_age, double p_sex_ratio, bool p_allow_zero_size, bool p_require_both_sexes, bool p_record_in_treeseq)
{
	SLiMSim &sim = population_.sim_;
	bool recording_tree_sequence = p_record_in_treeseq && sim.RecordingTreeSequence();
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	cached_parent_genomes_value_.reset();
	cached_parent_individuals_value_.reset();
	
	if (parent_individuals_.size() || parent_genomes_.size())
		EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) individuals or genomes already present in GenerateParentsToFit()." << EidosTerminate();
	if ((parent_subpop_size_ == 0) && !p_allow_zero_size)
		EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) subpop size of 0 requested." << EidosTerminate();
	
	// Make sure our memory pools for genomes and individuals are allocated
	if (!genome_pool_ && !individual_pool_)
		MakeMemoryPools(parent_subpop_size_ * 2);	// room for parents and children
	
	// We also have to make space for the pointers to the genomes and individuals
	parent_genomes_.reserve(parent_subpop_size_ * 2);
	parent_individuals_.reserve(parent_subpop_size_);
	
	// Now create new individuals and genomes appropriate for the requested sex ratio and subpop size
	MutationRun *shared_empty_run = (parent_subpop_size_ ? MutationRun::NewMutationRun() : nullptr);
	
	if (sex_enabled_)
	{
		slim_popsize_t total_males = static_cast<slim_popsize_t>(lround(p_sex_ratio * parent_subpop_size_));	// round in favor of males, arbitrarily
		slim_popsize_t first_male_index = parent_subpop_size_ - total_males;
		
		parent_first_male_index_ = first_male_index;
		
		if (p_require_both_sexes)
		{
			if (first_male_index <= 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): sex ratio of " << p_sex_ratio << " produced no females." << EidosTerminate();
			else if (first_male_index >= parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): sex ratio of " << p_sex_ratio << " produced no males." << EidosTerminate();
		}
		
		// Make females and then males
		for (int new_index = 0; new_index < parent_subpop_size_; ++new_index)
		{
			bool is_female = (new_index < first_male_index);
			Genome *genome1, *genome2;
			
			switch (modeled_chromosome_type_)
			{
				case GenomeType::kAutosome:
				{
					genome1 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kAutosome, false);
					genome1->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
					
					genome2 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kAutosome, false);
					genome2->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
					break;
				}
				case GenomeType::kXChromosome:
				{
					genome1 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kXChromosome, false);
					genome1->ReinitializeGenomeToMutrun(GenomeType::kXChromosome, mutrun_count, mutrun_length, shared_empty_run);
					
					if (is_female)
					{
						genome2 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kXChromosome, false);
						genome2->ReinitializeGenomeToMutrun(GenomeType::kXChromosome, mutrun_count, mutrun_length, shared_empty_run);
					}
					else
					{
						genome2 = NewSubpopGenome(0, 0, GenomeType::kYChromosome, true);
					}
					break;
				}
				case GenomeType::kYChromosome:
				{
					genome1 = NewSubpopGenome(0, 0, GenomeType::kXChromosome, true);
					
					if (is_female)
					{
						genome2 = NewSubpopGenome(0, 0, GenomeType::kXChromosome, true);
					}
					else
					{
						genome2 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kYChromosome, false);
						genome2->ReinitializeGenomeToMutrun(GenomeType::kYChromosome, mutrun_count, mutrun_length, shared_empty_run);
					}
					break;
				}
			}
			
			IndividualSex individual_sex = (is_female ? IndividualSex::kFemale : IndividualSex::kMale);
			Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, new_index, gSLiM_next_pedigree_id++, genome1, genome2, individual_sex, p_initial_age, /* initial fitness for new subpops */ 1.0);
			
			// TREE SEQUENCE RECORDING
			if (recording_tree_sequence)
			{
				sim.SetCurrentNewIndividual(individual);
				sim.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
				sim.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
			}
			
			parent_genomes_.push_back(genome1);
			parent_genomes_.push_back(genome2);
			parent_individuals_.push_back(individual);
		}
	}
	else
	{
		// Make hermaphrodites
		for (int new_index = 0; new_index < parent_subpop_size_; ++new_index)
		{
			// allocate out of our object pools
			Genome *genome1 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kAutosome, false);
			Genome *genome2 = NewSubpopGenome(mutrun_count, mutrun_length, GenomeType::kAutosome, false);
			
			// start new parental genomes out with a shared empty mutrun; can't be nullptr like child genomes can
			genome1->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
			genome2->ReinitializeGenomeToMutrun(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_run);
			
			Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, new_index, gSLiM_next_pedigree_id++, genome1, genome2, IndividualSex::kHermaphrodite, p_initial_age, /* initial fitness for new subpops */ 1.0);
			
			// TREE SEQUENCE RECORDING
			if (recording_tree_sequence)
			{
				sim.SetCurrentNewIndividual(individual);
				sim.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
				sim.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
			}
			
			parent_genomes_.push_back(genome1);
			parent_genomes_.push_back(genome2);
			parent_individuals_.push_back(individual);
		}
	}
}

void Subpopulation::CheckIndividualIntegrity(void)
{
	EidosScript::ClearErrorPosition();
	
	if (population_.sim_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosNoBlockType)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) executing block type was not maintained correctly." << EidosTerminate();
	
#if defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY)
	SLiMModelType model_type = population_.sim_.ModelType();
#endif
	Chromosome &chromosome = population_.sim_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	//
	//	Check the parental generation; this is essentially the same in WF and nonWF models
	//
	
	if ((int)parent_individuals_.size() != parent_subpop_size_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between parent_subpop_size_ and parent_individuals_.size()." << EidosTerminate();
	if ((int)parent_genomes_.size() != parent_subpop_size_ * 2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between parent_subpop_size_ and parent_genomes_.size()." << EidosTerminate();
	
	for (int ind_index = 0; ind_index < parent_subpop_size_; ++ind_index)
	{
		Individual *individual = parent_individuals_[ind_index];
		Genome *genome1 = parent_genomes_[ind_index * 2];
		Genome *genome2 = parent_genomes_[ind_index * 2 + 1];
		bool invalid_age = false;
		
		if (!individual)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for individual." << EidosTerminate();
		if (!genome1 || !genome2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for genome." << EidosTerminate();
		
		if ((individual->genome1_ != genome1) || (individual->genome2_ != genome2))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between parent_genomes_ and individual->genomeX_." << EidosTerminate();
		
		if (individual->index_ != ind_index)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->index_ and ind_index." << EidosTerminate();
	
		if (&(individual->subpopulation_) != this)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->subpopulation_ and subpopulation." << EidosTerminate();
		
		if ((genome1->subpop_ != this) || (genome2->subpop_ != this))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between genome->subpop_ and subpopulation." << EidosTerminate();
		
		if (!genome1->IsNull() && ((genome1->mutrun_count_ != mutrun_count) || (genome1->mutrun_length_ != mutrun_length)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 1 of individual has the wrong mutrun count/length." << EidosTerminate();
		if (!genome2->IsNull() && ((genome2->mutrun_count_ != mutrun_count) || (genome2->mutrun_length_ != mutrun_length)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 2 of individual has the wrong mutrun count/length." << EidosTerminate();
		
		if (individual->pedigree_id_ == -1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) individual has an invalid pedigree ID." << EidosTerminate();
		if ((genome1->genome_id_ != individual->pedigree_id_ * 2) || (genome2->genome_id_ != individual->pedigree_id_ * 2 + 1))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome has an invalid genome ID." << EidosTerminate();
		
#if defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY)
		if (model_type == SLiMModelType::kModelTypeWF)
		{
			if (individual->age_ != -1)
				invalid_age = true;
		}
		else
		{
			if (individual->age_ < 0)
				invalid_age = true;
		}
#endif
		
		if (invalid_age)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) invalid value for individual->age_." << EidosTerminate();
		
		if (sex_enabled_)
		{
			bool is_female = (ind_index < parent_first_male_index_);
			GenomeType genome1_type, genome2_type;
			bool genome1_null = false, genome2_null = false;
			
			if ((is_female && (individual->sex_ != IndividualSex::kFemale)) ||
				(!is_female && (individual->sex_ != IndividualSex::kMale)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->sex_ and parent_first_male_index_." << EidosTerminate();
			
			switch (modeled_chromosome_type_)
			{
				case GenomeType::kAutosome:		genome1_type = GenomeType::kAutosome; genome2_type = GenomeType::kAutosome; break;
				case GenomeType::kXChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome2_null = !is_female; break;
				case GenomeType::kYChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome1_null = true; genome2_null = is_female; break;
				default: EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) unsupported chromosome type." << EidosTerminate();
			}
			
			if ((genome1->IsNull() != genome1_null) || (genome2->IsNull() != genome2_null))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual null genome status in sexual simulation." << EidosTerminate();
			if ((genome1->Type() != genome1_type) || (genome2->Type() != genome2_type))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual genome type in sexual simulation." << EidosTerminate();
		}
		else
		{
			if (individual->sex_ != IndividualSex::kHermaphrodite)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-hermaphrodite individual in non-sexual simulation." << EidosTerminate();
			
			if (genome1->IsNull() || genome2->IsNull())
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null genome in individual in non-sexual simulation." << EidosTerminate();
			
			if ((genome1->Type() != GenomeType::kAutosome) || (genome2->Type() != GenomeType::kAutosome))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-autosome genome in individual in non-sexual simulation." << EidosTerminate();
		}
		
#ifdef SLIM_WF_ONLY
		if (child_generation_valid_)
		{
			// When the child generation is valid, all parental genomes should have null mutrun pointers, so mutrun refcounts are correct
			for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
				if (genome1->mutruns_[mutrun_index].get() != nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a nonnull mutrun pointer." << EidosTerminate();
			
			for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
				if (genome2->mutruns_[mutrun_index].get() != nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a nonnull mutrun pointer." << EidosTerminate();
		}
		else
#endif	// SLIM_WF_ONLY
		{
			// When the parental generation is valid, all parental genomes should have non-null mutrun pointers
			for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
				if (genome1->mutruns_[mutrun_index].get() == nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a null mutrun pointer." << EidosTerminate();
			
			for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
				if (genome2->mutruns_[mutrun_index].get() == nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a null mutrun pointer." << EidosTerminate();
		}
	}
	
	
	//
	//	Check the child generation; this is only in WF models
	//
	
#if defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY)
	if (model_type == SLiMModelType::kModelTypeWF)
#endif
#ifdef SLIM_WF_ONLY
	{
		if ((int)child_individuals_.size() != child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between child_subpop_size_ and child_individuals_.size()." << EidosTerminate();
		if ((int)child_genomes_.size() != child_subpop_size_ * 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between child_subpop_size_ and child_genomes_.size()." << EidosTerminate();
		
		for (int ind_index = 0; ind_index < child_subpop_size_; ++ind_index)
		{
			Individual *individual = child_individuals_[ind_index];
			Genome *genome1 = child_genomes_[ind_index * 2];
			Genome *genome2 = child_genomes_[ind_index * 2 + 1];
			
			if (!individual)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for individual." << EidosTerminate();
			if (!genome1 || !genome2)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for genome." << EidosTerminate();
			
			if ((individual->genome1_ != genome1) || (individual->genome2_ != genome2))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between child_genomes_ and individual->genomeX_." << EidosTerminate();
			
			if (individual->index_ != ind_index)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->index_ and ind_index." << EidosTerminate();
			
			if (&(individual->subpopulation_) != this)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->subpopulation_ and subpopulation." << EidosTerminate();
			
			if ((genome1->subpop_ != this) || (genome2->subpop_ != this))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between genome->subpop_ and subpopulation." << EidosTerminate();
			
			if (!genome1->IsNull() && ((genome1->mutrun_count_ != mutrun_count) || (genome1->mutrun_length_ != mutrun_length)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 1 of individual has the wrong mutrun count/length." << EidosTerminate();
			if (!genome2->IsNull() && ((genome2->mutrun_count_ != mutrun_count) || (genome2->mutrun_length_ != mutrun_length)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 2 of individual has the wrong mutrun count/length." << EidosTerminate();
			
			if (child_generation_valid_)
			{
				if (individual->pedigree_id_ == -1)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) individual has an invalid pedigree ID." << EidosTerminate();
				if ((genome1->genome_id_ != individual->pedigree_id_ * 2) || (genome2->genome_id_ != individual->pedigree_id_ * 2 + 1))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome has an invalid genome ID." << EidosTerminate();
			}
			
			if (sex_enabled_)
			{
				bool is_female = (ind_index < child_first_male_index_);
				GenomeType genome1_type, genome2_type;
				bool genome1_null = false, genome2_null = false;
				
				if ((is_female && (individual->sex_ != IndividualSex::kFemale)) ||
					(!is_female && (individual->sex_ != IndividualSex::kMale)))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->sex_ and child_first_male_index_." << EidosTerminate();
				
				switch (modeled_chromosome_type_)
				{
					case GenomeType::kAutosome:		genome1_type = GenomeType::kAutosome; genome2_type = GenomeType::kAutosome; break;
					case GenomeType::kXChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome2_null = !is_female; break;
					case GenomeType::kYChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome1_null = true; genome2_null = is_female; break;
					default: EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) unsupported chromosome type." << EidosTerminate();
				}
				
				if ((genome1->IsNull() != genome1_null) || (genome2->IsNull() != genome2_null))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual null genome status in sexual simulation." << EidosTerminate();
				if ((genome1->Type() != genome1_type) || (genome2->Type() != genome2_type))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual genome type in sexual simulation." << EidosTerminate();
			}
			else
			{
				if (individual->sex_ != IndividualSex::kHermaphrodite)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-hermaphrodite individual in non-sexual simulation." << EidosTerminate();
				
				if (genome1->IsNull() || genome2->IsNull())
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null genome in individual in non-sexual simulation." << EidosTerminate();
				
				if ((genome1->Type() != GenomeType::kAutosome) || (genome2->Type() != GenomeType::kAutosome))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-autosome genome in individual in non-sexual simulation." << EidosTerminate();
			}
			
			if (child_generation_valid_)
			{
				// When the child generation is active, child genomes should have non-null mutrun pointers
				for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
					if (genome1->mutruns_[mutrun_index].get() == nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a null mutrun pointer." << EidosTerminate();
				
				for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
					if (genome2->mutruns_[mutrun_index].get() == nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a null mutrun pointer." << EidosTerminate();
			}
			else
			{
				// When the parental generation is active, child genomes should have null mutrun pointers, so mutrun refcounts are correct
				for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
					if (genome1->mutruns_[mutrun_index].get() != nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a nonnull mutrun pointer." << EidosTerminate();
				
				for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
					if (genome2->mutruns_[mutrun_index].get() != nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a nonnull mutrun pointer." << EidosTerminate();
			}
		}
	}
#endif	// SLIM_WF_ONLY
	
	
	//
	//	Check the genome junkyards; all genomes should contain nullptr mutruns
	//
	
	for (Genome *genome : genome_junkyard_nonnull)
	{
		if (genome->IsNull())
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null genome in the nonnull genome junkyard." << EidosTerminate();
		
		for (int mutrun_index = 0; mutrun_index < genome->mutrun_count_; ++mutrun_index)
			if (genome->mutruns_[mutrun_index].get() != nullptr)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) nonnull mutrun pointer in the nonnull genome junkyard." << EidosTerminate();
	}
	
	for (Genome *genome : genome_junkyard_null)
	{
		if (!genome->IsNull())
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) nonnull genome in the null genome junkyard." << EidosTerminate();
		
		for (int mutrun_index = 0; mutrun_index < genome->mutrun_count_; ++mutrun_index)
			if (genome->mutruns_[mutrun_index].get() != nullptr)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) nonnull mutrun pointer in the null genome junkyard." << EidosTerminate();
	}
}

Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq) :
	self_symbol_(Eidos_GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class))), 
	population_(p_population), subpopulation_id_(p_subpopulation_id), parent_subpop_size_(p_subpop_size)
#ifdef SLIM_WF_ONLY
	, child_subpop_size_(p_subpop_size)
#endif	// SLIM_WF_ONLY
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
	, gui_premigration_size_(p_subpop_size)
#endif
{
#if defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY)
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
	{
		GenerateParentsToFit(/* p_initial_age */ -1, /* p_sex_ratio */ 0.0, /* p_allow_zero_size */ false, /* p_require_both_sexes */ true, /* p_record_in_treeseq */ p_record_in_treeseq);
		GenerateChildrenToFitWF();
	}
	else
	{
		GenerateParentsToFit(/* p_initial_age */ 0, /* p_sex_ratio */ 0.0, /* p_allow_zero_size */ true, /* p_require_both_sexes */ false, /* p_record_in_treeseq */ p_record_in_treeseq);
	}
#elif defined(SLIM_WF_ONLY)
	GenerateParentsToFitWF(p_record_in_treeseq);
	GenerateChildrenToFitWF();
#elif defined(SLIM_NONWF_ONLY)
	if (p_source_subpop)
		EIDOS_TERMINATION << "ERROR (Subpopulation::Subpopulation): (internal error) Subpopulation can be constructed with a source subpop only in the WF case." << EidosTerminate();
	GenerateIndividualsToFitNonWF(0.0);
#endif
	
#ifdef SLIM_WF_ONLY
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
	{
		// Set up to draw random individuals, based initially on equal fitnesses
		cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
		cached_fitness_capacity_ = parent_subpop_size_;
		cached_fitness_size_ = parent_subpop_size_;
		
		double *fitness_buffer_ptr = cached_parental_fitness_;
		
		for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
			*(fitness_buffer_ptr++) = 1.0;
		
		lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
	}
#endif	// SLIM_WF_ONLY
}

// SEX ONLY
Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq,
							 double p_sex_ratio, GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff) :
	self_symbol_(Eidos_GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class))),
	population_(p_population), subpopulation_id_(p_subpopulation_id), parent_subpop_size_(p_subpop_size),
#ifdef SLIM_WF_ONLY
	parent_sex_ratio_(p_sex_ratio), child_subpop_size_(p_subpop_size), child_sex_ratio_(p_sex_ratio),
#endif	// SLIM_WF_ONLY
	sex_enabled_(true), modeled_chromosome_type_(p_modeled_chromosome_type), x_chromosome_dominance_coeff_(p_x_chromosome_dominance_coeff)
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
	, gui_premigration_size_(p_subpop_size)
#endif
{
#if defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY)
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
	{
		GenerateParentsToFit(/* p_initial_age */ -1, /* p_sex_ratio */ p_sex_ratio, /* p_allow_zero_size */ false, /* p_require_both_sexes */ true, /* p_record_in_treeseq */ p_record_in_treeseq);
		GenerateChildrenToFitWF();
	}
	else
	{
		GenerateParentsToFit(/* p_initial_age */ 0, /* p_sex_ratio */ p_sex_ratio, /* p_allow_zero_size */ true, /* p_require_both_sexes */ false, /* p_record_in_treeseq */ p_record_in_treeseq);
	}
#elif defined(SLIM_WF_ONLY)
	GenerateParentsToFitWF(p_record_in_treeseq);
	GenerateChildrenToFitWF();
#elif defined(SLIM_NONWF_ONLY)
	if (p_source_subpop)
		EIDOS_TERMINATION << "ERROR (Subpopulation::Subpopulation): (internal error) Subpopulation can be constructed with a source subpop only in the WF case." << EidosTerminate();
	GenerateIndividualsToFitNonWF(p_sex_ratio);
#endif
	
#ifdef SLIM_WF_ONLY
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
	{
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
#endif	// SLIM_WF_ONLY
	
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
	{
		// OK, so.  When reading a nonWF tree-seq file, we get passed in a sex ratio that is the sex ratio of the individuals in the file.
		// That's good, so the individuals that get created have the correct sex.  However, we want the sex ratio ivars in the subpop
		// to have their default value of 0.0, ultimately; those variables are not supposed to be updated/accurate in nonWF models.  This
		// fell through the cracks because, who cares if an undefined/unused variable has the wrong value?  But then we write out that
		// non-zero value to the .trees file, and that was confusing Peter's test code.  So now we explicitly set the ivars to 0.0 here,
		// now that we're done using the value.  This might also occur when a new subpop is created in a nonWF model; the initial sex ratio
		// value might have been sticking around permanently in the ivar, even though it would not be accurate any more.  BCH 9/7/2020
		parent_sex_ratio_ = 0.0;
		child_sex_ratio_ = 0.0;
	}
}


Subpopulation::~Subpopulation(void)
{
	//std::cout << "Subpopulation::~Subpopulation" << std::endl;
	
#ifdef SLIM_WF_ONLY
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
#endif	// SLIM_WF_ONLY
	
	{
		// dispose of genomes and individuals with our object pools
		for (Genome *genome : parent_genomes_)
		{
			genome->~Genome();
			genome_pool_->DisposeChunk(const_cast<Genome *>(genome));
		}
		
		for (Individual *individual : parent_individuals_)
		{
			individual->~Individual();
			individual_pool_->DisposeChunk(const_cast<Individual *>(individual));
		}
	}
	
#ifdef SLIM_WF_ONLY
	{
		// dispose of genomes and individuals with our object pools
		for (Genome *genome : child_genomes_)
		{
			genome->~Genome();
			genome_pool_->DisposeChunk(const_cast<Genome *>(genome));
		}
		
		for (Individual *individual : child_individuals_)
		{
			individual->~Individual();
			individual_pool_->DisposeChunk(const_cast<Individual *>(individual));
		}
	}
#endif	// SLIM_WF_ONLY
	
	for (Genome *genome : genome_junkyard_nonnull)
	{
		genome->~Genome();
		genome_pool_->DisposeChunk(const_cast<Genome *>(genome));
	}
	
	for (Genome *genome : genome_junkyard_null)
	{
		genome->~Genome();
		genome_pool_->DisposeChunk(const_cast<Genome *>(genome));
	}
	
	delete genome_pool_;
	delete individual_pool_;
	
	for (const auto &map_pair : spatial_maps_)
	{
		SpatialMap *map_ptr = map_pair.second;
		
		if (map_ptr)
			delete map_ptr;
	}
}

void Subpopulation::UpdateFitness(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, std::vector<SLiMEidosBlock*> &p_global_fitness_callbacks)
{
	const std::map<slim_objectid_t,MutationType*> &mut_types = population_.sim_.MutationTypes();
	
	// The FitnessOfParent...() methods called by this method rely upon cached fitness values
	// kept inside the Mutation objects.  Those caches may need to be validated before we can
	// calculate fitness values.  We check for that condition and repair it first.
	// BCH 26 Nov. 2017: Note that dominance_coeff_changed_ is really pointless in this design;
	// we could use just any_dominance_coeff_changed_ by itself, I think.  I have retained the
	// dominance_coeff_changed_ flag for now out of sheer inertia; maybe it will prove useful.
	if (population_.sim_.any_dominance_coeff_changed_)
	{
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
			population_.ValidateMutationFitnessCaches();	// note one subpop triggers it, but the recaching occurs for the whole sim
		
		population_.sim_.any_dominance_coeff_changed_ = false;
	}
	
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
        MutationType *found_muttype = population_.sim_.MutationTypeWithID(mutation_type_id);
		
		if (found_muttype)
		{
			if (mut_types.size() > 1)
			{
				// We have a single callback that applies to a known mutation type among more than one defined type; we can optimize that
				single_fitness_callback = true;
				single_callback_mut_type = found_muttype;
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
	
	// Looping through all of the mutation types and setting flags can be very expensive, so as a first pass we check
	// whether it is even conceivable that we will be able to have skip_chromosomal_fitness == true.  If the simulation
	// is not pure neutral and we have no fitness callback that could change that, it is a no-go without checking the
	// mutation types at all.
	if (!population_.sim_.pure_neutral_)
	{
		skip_chromosomal_fitness = false;	// we're not pure neutral, so we have to prove that it is possible
		
		for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
		{
			if (fitness_callback->active_)
			{
				const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
				
				if (compound_statement_node->cached_return_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }"
					EidosValue *result = compound_statement_node->cached_return_value_.get();
					
					if ((result->Type() == EidosValueType::kValueFloat) || (result->Count() == 1))
					{
						if (result->FloatAtIndex(0, nullptr) == 1.0)
						{
							// we have a fitness callback that is neutral-making, so it could conceivably work;
							// change our minds but keep checking
							skip_chromosomal_fitness = true;
							continue;
						}
					}
				}
				
				// if we reach this point, we have an active callback that is not neutral-making, so we fail and we're done
				skip_chromosomal_fitness = false;
				break;
			}
		}
	}
	
	// At this point it appears conceivable that we could skip, but we don't yet know.  We need to do a more thorough
	// check, actually tracking precisely which mutation types are neutral and non-neutral, and which are made neutral
	// by fitness() callbacks.  Note this block is the only place where is_pure_neutral_now_ is valid or used!!!
	if (skip_chromosomal_fitness)
	{
		// first set a flag on all mut types indicating whether they are pure neutral according to their DFE
		for (auto &mut_type_iter : mut_types)
			mut_type_iter.second->is_pure_neutral_now_ = mut_type_iter.second->all_pure_neutral_DFE_;
		
		// then go through the fitness callback list and set the pure neutral flag for mut types neutralized by an active callback
		for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
		{
			if (fitness_callback->active_)
			{
				const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
				
				if (compound_statement_node->cached_return_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }"
					EidosValue *result = compound_statement_node->cached_return_value_.get();
					
					if ((result->Type() == EidosValueType::kValueFloat) && (result->Count() == 1))
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
                                MutationType *found_muttype = population_.sim_.MutationTypeWithID(mutation_type_id);
                                
								if (found_muttype)
									found_muttype->is_pure_neutral_now_ = true;
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
			// upon is_pure_neutral_now_; that rogue callback could make other callbacks active/inactive, etc.  So in
			// principle we should now go through and clear the is_pure_neutral_now_ flags to avoid any confusion.  But
			// we are the only ones to use is_pure_neutral_now_, and we're done using it, so we can skip that work.
			
			//for (auto &mut_type_iter : mut_types)
			//	mut_type_iter.second->is_pure_neutral_now_ = false;
		}
	}
	
	// Figure out global callbacks; these are callbacks with NULL supplied for the mut-type id, which means that they are called
	// exactly once per individual, for every individual regardless of genetics, to provide an entry point for alternate fitness definitions
	int global_fitness_callback_count = (int)p_global_fitness_callbacks.size();
	bool global_fitness_callbacks_exist = (global_fitness_callback_count > 0);
	
	// We optimize the pure neutral case, as long as no fitness callbacks are defined; fitness values are then simply 1.0, for everybody.
	// BCH 12 Jan 2018: now fitness_scaling_ modifies even pure_neutral_ models, but the framework here remains valid
	bool pure_neutral = (!fitness_callbacks_exist && !global_fitness_callbacks_exist && population_.sim_.pure_neutral_);
	double subpop_fitness_scaling = fitness_scaling_;
	
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
	// Reset our override of individual cached fitness values; we make this decision afresh with each UpdateFitness() call.  See
	// the header for further comments on this mechanism.
	individual_cached_fitness_OVERRIDE_ = false;
#endif
	
	// calculate fitnesses in parent population and cache the values
	if (sex_enabled_)
	{
		// SEX ONLY
		double totalMaleFitness = 0.0, totalFemaleFitness = 0.0;
		
		// Set up to draw random females
		if (pure_neutral)
		{
			if (Individual::s_any_individual_fitness_scaling_set_)
			{
				for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
				{
					double fitness = subpop_fitness_scaling * parent_individuals_[female_index]->fitness_scaling_;
					
					parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
					totalFemaleFitness += fitness;
				}
			}
			else
			{
				double fitness = subpop_fitness_scaling;	// no individual fitness_scaling_
				
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
				// Here we override setting up every cached_fitness_UNSAFE_ value, and set up a subpop-level cache instead.
				// This is why cached_fitness_UNSAFE_ is marked "UNSAFE".  See the header for details on this.
				if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
				{
					individual_cached_fitness_OVERRIDE_ = true;
					individual_cached_fitness_OVERRIDE_value_ = fitness;
				}
				else
#endif
				{
					for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
						parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
				}
				
				totalFemaleFitness = fitness * parent_first_male_index_;
			}
		}
		else if (skip_chromosomal_fitness)
		{
			for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
			{
				double fitness = subpop_fitness_scaling * parent_individuals_[female_index]->fitness_scaling_;
				
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, female_index);
				
				parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
				totalFemaleFitness += fitness;
			}
		}
		else
		{
			// general case for females
			for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
			{
				double fitness = subpop_fitness_scaling * parent_individuals_[female_index]->fitness_scaling_;
				
				if (fitness > 0.0)
				{
					if (!fitness_callbacks_exist)
						fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(female_index);
					else if (single_fitness_callback)
						fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(female_index, p_fitness_callbacks, single_callback_mut_type);
					else
						fitness *= FitnessOfParentWithGenomeIndices_Callbacks(female_index, p_fitness_callbacks);
					
					// multiply in the effects of any global fitness callbacks (muttype==NULL)
					if (global_fitness_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, female_index);
				}
				
				parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
				totalFemaleFitness += fitness;
			}
		}
		
		totalFitness += totalFemaleFitness;
		if ((population_.sim_.ModelType() == SLiMModelType::kModelTypeWF) && (totalFemaleFitness <= 0.0))
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of females is <= 0.0." << EidosTerminate(nullptr);
		
		// Set up to draw random males
		if (pure_neutral)
		{
			if (Individual::s_any_individual_fitness_scaling_set_)
			{
				for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
				{
					double fitness = subpop_fitness_scaling * parent_individuals_[male_index]->fitness_scaling_;
					
					parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
					totalMaleFitness += fitness;
				}
			}
			else
			{
				double fitness = subpop_fitness_scaling;	// no individual fitness_scaling_
				
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
				// Here we override setting up every cached_fitness_UNSAFE_ value, and set up a subpop-level cache instead.
				// This is why cached_fitness_UNSAFE_ is marked "UNSAFE".  See the header for details on this.
				if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
				{
					individual_cached_fitness_OVERRIDE_ = true;
					individual_cached_fitness_OVERRIDE_value_ = fitness;
				}
				else
#endif
				{
					for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
						parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
				}
				
				if (parent_subpop_size_ > parent_first_male_index_)
					totalMaleFitness = fitness * (parent_subpop_size_ - parent_first_male_index_);
			}
		}
		else if (skip_chromosomal_fitness)
		{
			for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
			{
				double fitness = subpop_fitness_scaling * parent_individuals_[male_index]->fitness_scaling_;
				
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, male_index);
				
				parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
				totalMaleFitness += fitness;
			}
		}
		else
		{
			// general case for males
			for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
			{
				double fitness = subpop_fitness_scaling * parent_individuals_[male_index]->fitness_scaling_;
				
				if (fitness > 0.0)
				{
					if (!fitness_callbacks_exist)
						fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(male_index);
					else if (single_fitness_callback)
						fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(male_index, p_fitness_callbacks, single_callback_mut_type);
					else
						fitness *= FitnessOfParentWithGenomeIndices_Callbacks(male_index, p_fitness_callbacks);
					
					// multiply in the effects of any global fitness callbacks (muttype==NULL)
					if (global_fitness_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, male_index);
				}
				
				parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
				totalMaleFitness += fitness;
			}
		}
		
		totalFitness += totalMaleFitness;
		
		if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
		{
			if (totalMaleFitness <= 0.0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of males is <= 0.0." << EidosTerminate(nullptr);
			if (!std::isfinite(totalFitness))
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of subpopulation is not finite; numerical error will prevent accurate simulation." << EidosTerminate(nullptr);
		}
	}
	else
	{
		if (pure_neutral)
		{
			if (Individual::s_any_individual_fitness_scaling_set_)
			{
				for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
				{
					double fitness = subpop_fitness_scaling * parent_individuals_[individual_index]->fitness_scaling_;
					
					parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
					totalFitness += fitness;
				}
			}
			else
			{
				double fitness = subpop_fitness_scaling;	// no individual fitness_scaling_
				
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
				// Here we override setting up every cached_fitness_UNSAFE_ value, and set up a subpop-level cache instead.
				// This is why cached_fitness_UNSAFE_ is marked "UNSAFE".  See the header for details on this.
				if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
				{
					individual_cached_fitness_OVERRIDE_ = true;
					individual_cached_fitness_OVERRIDE_value_ = fitness;
				}
				else
#endif
				{
					for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
						parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
				}
				
				totalFitness = fitness * parent_subpop_size_;
			}
		}
		else if (skip_chromosomal_fitness)
		{
			for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
			{
				double fitness = subpop_fitness_scaling * parent_individuals_[individual_index]->fitness_scaling_;
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, individual_index);
				
				parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
				totalFitness += fitness;
			}
		}
		else
		{
			// general case for hermaphrodites
			for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
			{
				double fitness = subpop_fitness_scaling * parent_individuals_[individual_index]->fitness_scaling_;
				
				if (fitness > 0)
				{
					if (!fitness_callbacks_exist)
						fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(individual_index);
					else if (single_fitness_callback)
						fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(individual_index, p_fitness_callbacks, single_callback_mut_type);
					else
						fitness *= FitnessOfParentWithGenomeIndices_Callbacks(individual_index, p_fitness_callbacks);
					
					// multiply in the effects of any global fitness callbacks (muttype==NULL)
					if (global_fitness_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, individual_index);
				}
				
				parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
				totalFitness += fitness;
			}
		}
		
		if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
		{
			if (totalFitness <= 0.0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of all individuals is <= 0.0." << EidosTerminate(nullptr);
			if (!std::isfinite(totalFitness))
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of subpopulation is not finite; numerical error will prevent accurate simulation." << EidosTerminate(nullptr);
		}
	}
	
#ifdef SLIM_WF_ONLY
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeWF)
		UpdateWFFitnessBuffers(pure_neutral && !Individual::s_any_individual_fitness_scaling_set_);
#endif	// SLIM_WF_ONLY
}

#ifdef SLIM_WF_ONLY
void Subpopulation::UpdateWFFitnessBuffers(bool p_pure_neutral)
{
	// This is called only by UpdateFitness(), after the fitness of all individuals has been updated, and only in WF models.
	// It updates the cached_parental_fitness_ and cached_male_fitness_ buffers, and then generates new lookup tables for mate choice.
	
	// Reallocate the fitness buffers to be large enough
	if (cached_fitness_capacity_ < parent_subpop_size_)
	{
		cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
		if (sex_enabled_)
			cached_male_fitness_ = (double *)realloc(cached_male_fitness_, sizeof(double) * parent_subpop_size_);
		cached_fitness_capacity_ = parent_subpop_size_;
	}
	
	// Set up the fitness buffers with the new information
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
	if (individual_cached_fitness_OVERRIDE_)
	{
		// This is the optimized case, where all individuals have the same fitness and it is cached at the subpop level
		double universal_cached_fitness = individual_cached_fitness_OVERRIDE_value_;
		
		if (sex_enabled_)
		{
			for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
			{
				cached_parental_fitness_[female_index] = universal_cached_fitness;
				cached_male_fitness_[female_index] = 0;
			}
			
			for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
			{
				cached_parental_fitness_[male_index] = universal_cached_fitness;
				cached_male_fitness_[male_index] = universal_cached_fitness;
			}
		}
		else
		{
			for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
				cached_parental_fitness_[i] = universal_cached_fitness;
		}
	}
	else
#endif
	{
		// This is the normal case, where cached_fitness_UNSAFE_ has the cached fitness values for each individual
		if (sex_enabled_)
		{
			for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
			{
				double fitness = parent_individuals_[female_index]->cached_fitness_UNSAFE_;
				
				cached_parental_fitness_[female_index] = fitness;
				cached_male_fitness_[female_index] = 0;
			}
			
			for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
			{
				double fitness = parent_individuals_[male_index]->cached_fitness_UNSAFE_;
				
				cached_parental_fitness_[male_index] = fitness;
				cached_male_fitness_[male_index] = fitness;
			}
		}
		else
		{
			for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
				cached_parental_fitness_[i] = parent_individuals_[i]->cached_fitness_UNSAFE_;
		}
	}
	
	cached_fitness_size_ = parent_subpop_size_;
	
	// Remake our mate-choice lookup tables
	if (sex_enabled_)
	{
		// throw out the old tables
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
		
		// in pure neutral models we don't set up the discrete preproc
		if (!p_pure_neutral)
		{
			lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, cached_parental_fitness_);
			lookup_male_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_ - parent_first_male_index_, cached_parental_fitness_ + parent_first_male_index_);
		}
	}
	else
	{
		// throw out the old tables
		if (lookup_parent_)
		{
			gsl_ran_discrete_free(lookup_parent_);
			lookup_parent_ = nullptr;
		}
		
		// in pure neutral models we don't set up the discrete preproc
		if (!p_pure_neutral)
		{
			lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
		}
	}
}
#endif	// SLIM_WF_ONLY

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
				
				if (compound_statement_node->cached_return_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
					EidosValue_SP result_SP = compound_statement_node->cached_return_value_;
					EidosValue *result = result_SP.get();
					
					if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
						EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << EidosTerminate(fitness_callback->identifier_token_);
					
					p_computed_fitness = result->FloatAtIndex(0, nullptr);
					
					// the cached value is owned by the tree, so we do not dispose of it
					// there is also no script output to handle
				}
				else if (fitness_callback->has_cached_optimization_)
				{
					// We can special-case particular simple callbacks for speed.  This is similar to the cached_return_value_
					// mechanism above, but it is done in SLiM, not in Eidos, and is specific to callbacks, not general.
					// The has_cached_optimization_ flag is the umbrella flag for all such optimizations; we then figure
					// out below which cached optimization is in effect for this callback.  See SLiMSim::OptimizeScriptBlock()
					// for comments on the specific cases optimized here.
					if (fitness_callback->has_cached_opt_reciprocal)
					{
						double A = fitness_callback->cached_opt_A_;
						
						p_computed_fitness = (A / p_computed_fitness);	// p_computed_fitness is relFitness
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessCallbacks): (internal error) cached optimization flag mismatch" << EidosTerminate(fitness_callback->identifier_token_);
					}
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
	Individual *individual = parent_individuals_[p_individual_index];
	Genome *genome1 = parent_genomes_[p_individual_index * 2];
	Genome *genome2 = parent_genomes_[p_individual_index * 2 + 1];
	SLiMSim &sim = population_.sim_;
	
	for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
	{
		if (fitness_callback->active_)
		{
			// The callback is active, so we need to execute it
			// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
			const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
			
			if (compound_statement_node->cached_return_value_)
			{
				// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
				EidosValue_SP result_SP = compound_statement_node->cached_return_value_;
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyGlobalFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << EidosTerminate(fitness_callback->identifier_token_);
				
				computed_fitness *= result->FloatAtIndex(0, nullptr);
				
				// the cached value is owned by the tree, so we do not dispose of it
				// there is also no script output to handle
			}
			else if (fitness_callback->has_cached_optimization_)
			{
				// We can special-case particular simple callbacks for speed.  This is similar to the cached_return_value_
				// mechanism above, but it is done in SLiM, not in Eidos, and is specific to callbacks, not general.
				// The has_cached_optimization_ flag is the umbrella flag for all such optimizations; we then figure
				// out below which cached optimization is in effect for this callback.  See SLiMSim::OptimizeScriptBlock()
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
	Genome *genome1 = parent_genomes_[p_individual_index * 2];
	Genome *genome2 = parent_genomes_[p_individual_index * 2 + 1];
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
					
					if (selection_coeff != 0.0F)
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
	Individual *individual = parent_individuals_[p_individual_index];
	Genome *genome1 = parent_genomes_[p_individual_index * 2];
	Genome *genome2 = parent_genomes_[p_individual_index * 2 + 1];
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
	Individual *individual = parent_individuals_[p_individual_index];
	Genome *genome1 = parent_genomes_[p_individual_index * 2];
	Genome *genome2 = parent_genomes_[p_individual_index * 2 + 1];
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
						if (selection_coeff != 0.0F)
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

#ifdef SLIM_WF_ONLY
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
	
	// Clear out any dictionary values and color values stored in what are now the child individuals; since this is per-individual it
	// takes a significant amount of time, so we try to minimize the overhead by doing it only when these facilities have been used
	// BCH 6 April 2019: likewise, now, with resetting tags in individuals and genomes to the "unset" value
	if (Individual::s_any_individual_dictionary_set_)
	{
		for (Individual *child : child_individuals_)
			child->RemoveAllKeys();
	}
	
	if (Individual::s_any_individual_color_set_)
	{
		for (Individual *child : child_individuals_)
			child->ClearColor();
	}
	
	if (Individual::s_any_individual_or_genome_tag_set_)
	{
		for (Individual *child : child_individuals_)
		{
			child->tag_value_ = SLIM_TAG_UNSET_VALUE;
			child->tagF_value_ = SLIM_TAGF_UNSET_VALUE;
			child->genome1_->tag_value_ = SLIM_TAG_UNSET_VALUE;
			child->genome2_->tag_value_ = SLIM_TAG_UNSET_VALUE;
		}
	}
	
	// The parents now have the values that used to belong to the children.
	parent_subpop_size_ = child_subpop_size_;
	parent_sex_ratio_ = child_sex_ratio_;
	parent_first_male_index_ = child_first_male_index_;
	
	// mark the child generation as invalid, until it is generated
	child_generation_valid_ = false;
	
	// The parental genomes, which have now been swapped into the child genome vactor, no longer fit the bill.  We need to throw them out and generate new genome vectors.
	if (will_need_new_children)
		GenerateChildrenToFitWF();
}
#endif	// SLIM_WF_ONLY

#ifdef SLIM_NONWF_ONLY
void Subpopulation::ApplyReproductionCallbacks(std::vector<SLiMEidosBlock*> &p_reproduction_callbacks, slim_popsize_t p_individual_index)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	Individual *individual = parent_individuals_[p_individual_index];
	
	for (SLiMEidosBlock *reproduction_callback : p_reproduction_callbacks)
	{
		if (reproduction_callback->active_)
		{
			IndividualSex sex_specificity = reproduction_callback->sex_specificity_;
			
			if ((sex_specificity == IndividualSex::kUnspecified) || (sex_specificity == individual->sex_))
			{
				Genome *genome1 = parent_genomes_[p_individual_index * 2];
				Genome *genome2 = parent_genomes_[p_individual_index * 2 + 1];
				SLiMSim &sim = population_.sim_;
				
				// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &sim.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = sim.FunctionMap();
					EidosInterpreter interpreter(reproduction_callback->compound_statement_node_, client_symbols, function_map, &sim);
					
					if (reproduction_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(reproduction_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (reproduction_callback->contains_individual_)
						callback_symbols.InitializeConstantSymbolEntry(gID_individual, individual->CachedEidosValue());
					if (reproduction_callback->contains_genome1_)
						callback_symbols.InitializeConstantSymbolEntry(gID_genome1, genome1->CachedEidosValue());
					if (reproduction_callback->contains_genome2_)
						callback_symbols.InitializeConstantSymbolEntry(gID_genome2, genome2->CachedEidosValue());
					if (reproduction_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
					
					try
					{
						// Interpret the script; the result from the interpretation must be NULL, for now (to allow future extension)
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(reproduction_callback->script_);
						EidosValue *result = result_SP.get();
						
						if (result->Type() != EidosValueType::kValueVOID)
						{
							if (result->Type() == EidosValueType::kValueNULL)
								EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyReproductionCallbacks): reproduction() callbacks must not return a value (i.e., must return void).  (NULL has been returned here instead; NULL was the required return value in the SLiM 3 prerelease, but the policy has been changed.)" << EidosTerminate(reproduction_callback->identifier_token_);
							
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyReproductionCallbacks): reproduction() callbacks must not return a value (i.e., must return void)." << EidosTerminate(reproduction_callback->identifier_token_);
						}
						
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
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(population_.sim_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosReproductionCallback)]);
#endif
}
#endif  // SLIM_NONWF_ONLY

#ifdef SLIM_NONWF_ONLY
void Subpopulation::ReproduceSubpopulation(void)
{
	for (int individual_index = 0; individual_index < parent_subpop_size_; ++individual_index)
		ApplyReproductionCallbacks(registered_reproduction_callbacks_, individual_index);
}
#endif  // SLIM_NONWF_ONLY

#ifdef SLIM_NONWF_ONLY
void Subpopulation::MergeReproductionOffspring(void)
{
	int new_count = (int)nonWF_offspring_individuals_.size();
	
	if (sex_enabled_)
	{
		// resize to create new slots for the new individuals
		parent_genomes_.resize(parent_genomes_.size() + new_count * 2);
		parent_individuals_.resize(parent_individuals_.size() + new_count);
		
		// in sexual models, females must be put before males and parent_first_male_index_ must be adjusted
		Genome **parent_genome_ptrs = parent_genomes_.data();
		Individual **parent_individual_ptrs = parent_individuals_.data();
		int old_male_count = parent_subpop_size_ - parent_first_male_index_;
		int new_female_count = 0;
		
		// count the new females
		for (int new_index = 0; new_index < new_count; ++new_index)
			if (nonWF_offspring_individuals_[new_index]->sex_ == IndividualSex::kFemale)
				new_female_count++;
		
		// move old males up that many slots to make room; need to fix the index_ ivars of the moved males
		memmove(parent_individual_ptrs + parent_first_male_index_ + new_female_count, parent_individual_ptrs + parent_first_male_index_, old_male_count * sizeof(Individual *));
		memmove(parent_genome_ptrs + (parent_first_male_index_ + new_female_count) * 2, parent_genome_ptrs + parent_first_male_index_ * 2, old_male_count * 2 * sizeof(Genome *));
		
		for (int moved_index = 0; moved_index < old_male_count; moved_index++)
		{
			int new_index = parent_first_male_index_ + new_female_count + moved_index;
			
			parent_individual_ptrs[new_index]->index_ = new_index;
		}
		
		// put all the new stuff into place; if the code above did its job, there are slots waiting for each item
		int new_female_position = parent_first_male_index_;
		int new_male_position = parent_subpop_size_ + new_female_count;
		
		for (int new_index = 0; new_index < new_count; ++new_index)
		{
			Genome *genome1 = nonWF_offspring_genomes_[new_index * 2];
			Genome *genome2 = nonWF_offspring_genomes_[new_index * 2 + 1];
			Individual *individual = nonWF_offspring_individuals_[new_index];
			slim_popsize_t insert_index;
			
			if (individual->sex_ == IndividualSex::kFemale)
				insert_index = new_female_position++;
			else
				insert_index = new_male_position++;
			
			individual->index_ = insert_index;
			
			parent_genome_ptrs[insert_index * 2] = genome1;
			parent_genome_ptrs[insert_index * 2 + 1] = genome2;
			parent_individual_ptrs[insert_index] = individual;
		}
		
		parent_first_male_index_ += new_female_count;
	}
	else
	{
		// reserve space for the new offspring to be merged in
		parent_genomes_.reserve(parent_genomes_.size() + new_count * 2);
		parent_individuals_.reserve(parent_individuals_.size() + new_count);
		
		// in hermaphroditic models there is no ordering, so just add new stuff at the end
		for (int new_index = 0; new_index < new_count; ++new_index)
		{
			Genome *genome1 = nonWF_offspring_genomes_[new_index * 2];
			Genome *genome2 = nonWF_offspring_genomes_[new_index * 2 + 1];
			Individual *individual = nonWF_offspring_individuals_[new_index];
			
			individual->index_ = parent_subpop_size_ + new_index;
			
			parent_genomes_.push_back(genome1);
			parent_genomes_.push_back(genome2);
			parent_individuals_.push_back(individual);
		}
	}
	
	// final cleanup
	parent_subpop_size_ += new_count;
	
	cached_parent_genomes_value_.reset();
	cached_parent_individuals_value_.reset();
	
	nonWF_offspring_genomes_.clear();
	nonWF_offspring_individuals_.clear();
}
#endif  // SLIM_NONWF_ONLY

#ifdef SLIM_NONWF_ONLY
void Subpopulation::ViabilitySelection(void)
{
	// Loop through our individuals and do draws based on fitness to determine who dies; dead individuals get compacted out
	Genome **genome_data = parent_genomes_.data();
	Individual **individual_data = parent_individuals_.data();
	int survived_genome_index = 0;
	int survived_individual_index = 0;
	int females_deceased = 0;
	bool individuals_died = false;
	
	for (int individual_index = 0; individual_index < parent_subpop_size_; ++individual_index)
	{
		Individual *individual = individual_data[individual_index];
		double fitness = individual->cached_fitness_UNSAFE_;	// never overridden in nonWF models, so this is safe with no check
		bool survived;
		
		if (fitness <= 0.0)			survived = false;
		else if (fitness >= 1.0)	survived = true;
		else						survived = (Eidos_rng_uniform(EIDOS_GSL_RNG) < fitness);
		
		if (survived)
		{
			// individuals that survive get copied down to the next available slot
			if (survived_individual_index != individual_index)
			{
				genome_data[survived_genome_index] = genome_data[individual_index * 2];
				genome_data[survived_genome_index + 1] = genome_data[individual_index * 2 + 1];
				individual_data[survived_individual_index] = individual;
				
				// fix the individual's index_
				individual_data[survived_individual_index]->index_ = survived_individual_index;
			}
			
			survived_genome_index += 2;
			survived_individual_index++;
		}
		else
		{
			// individuals that do not survive get deallocated, and will be overwritten
			Genome *genome1 = genome_data[individual_index * 2];
			Genome *genome2 = genome_data[individual_index * 2 + 1];
			
			if (sex_enabled_)
				if (individual->sex_ == IndividualSex::kFemale)
					females_deceased++;
			
			FreeSubpopGenome(genome1);
			FreeSubpopGenome(genome2);
			
			individual->~Individual();
			individual_pool_->DisposeChunk(const_cast<Individual *>(individual));
			
			individuals_died = true;
		}
	}
	
	// Then fix our bookkeeping for the first male index, subpop size, caches, etc.
	if (individuals_died)
	{
		parent_subpop_size_ = survived_individual_index;
		
		if (sex_enabled_)
			parent_first_male_index_ -= females_deceased;
		
		parent_genomes_.resize(parent_subpop_size_ * 2);
		parent_individuals_.resize(parent_subpop_size_);
		
		cached_parent_genomes_value_.reset();
		cached_parent_individuals_value_.reset();
	}
}
#endif  // SLIM_NONWF_ONLY

#ifdef SLIM_NONWF_ONLY
void Subpopulation::IncrementIndividualAges(void)
{
	// Loop through our individuals and increment all their ages by one
	for (Individual *individual : parent_individuals_)
		individual->age_++;
}
#endif  // SLIM_NONWF_ONLY

size_t Subpopulation::MemoryUsageForParentTables(void)
{
	size_t usage = 0;
	
	if (lookup_parent_)
		usage += lookup_parent_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_female_parent_)
		usage += lookup_female_parent_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_male_parent_)
		usage += lookup_male_parent_->K * (sizeof(size_t) + sizeof(double));
	
	return usage;
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(CurrentFirstMaleIndex()));
		case gID_genomes:
		{
#ifdef SLIM_WF_ONLY
			if (child_generation_valid_)
			{
				if (!cached_child_genomes_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->reserve(child_genomes_.size());
					cached_child_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter : child_genomes_)
						vec->push_object_element_NORR(genome_iter);
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = cached_child_genomes_value_->ObjectElementVector_Mutable();
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
#endif	// SLIM_WF_ONLY
			{
				if (!cached_parent_genomes_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->reserve(parent_genomes_.size());
					cached_parent_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter : parent_genomes_)
						vec->push_object_element_NORR(genome_iter);
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = cached_parent_genomes_value_->ObjectElementVector_Mutable();
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
#ifdef SLIM_WF_ONLY
			if (child_generation_valid_)
			{
				slim_popsize_t subpop_size = child_subpop_size_;
				
				// Check for an outdated cache; this should never happen, so we flag it as an error
				if (cached_child_individuals_value_ && (cached_child_individuals_value_->Count() != subpop_size))
					EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): (internal error) cached_child_individuals_value_ out of date." << EidosTerminate();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_child_individuals_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(subpop_size);
					cached_child_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->push_object_element_NORR(child_individuals_[individual_index]);
				}
				
				return cached_child_individuals_value_;
			}
			else
#endif	// SLIM_WF_ONLY
			{
				slim_popsize_t subpop_size = parent_subpop_size_;
				
				// Check for an outdated cache; this should never happen, so we flag it as an error
				if (cached_parent_individuals_value_ && (cached_parent_individuals_value_->Count() != subpop_size))
					EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): (internal error) cached_parent_individuals_value_ out of date." << EidosTerminate();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_parent_individuals_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(subpop_size);
					cached_parent_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->push_object_element_NORR(parent_individuals_[individual_index]);
				}
				
				return cached_parent_individuals_value_;
			}
		}
#ifdef SLIM_WF_ONLY
		case gID_immigrantSubpopIDs:
		{
			if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property immigrantSubpopIDs is not available in nonWF models." << EidosTerminate();
			
			EidosValue_Int_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair : migrant_fractions_)
				vec->push_int(migrant_pair.first);
			
			return result_SP;
		}
		case gID_immigrantSubpopFractions:
		{
			if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property immigrantSubpopFractions is not available in nonWF models." << EidosTerminate();
			
			EidosValue_Float_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair : migrant_fractions_)
				vec->push_float(migrant_pair.second);
			
			return result_SP;
		}
		case gID_selfingRate:
		{
			if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property selfingRate is not available in nonWF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selfing_fraction_));
		}
		case gID_cloningRate:
		{
			if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property cloningRate is not available in nonWF models." << EidosTerminate();
			
			if (sex_enabled_)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{female_clone_fraction_, male_clone_fraction_});
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(female_clone_fraction_));
		}
		case gID_sexRatio:
		{
			if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property sexRatio is not available in nonWF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(child_generation_valid_ ? child_sex_ratio_ : parent_sex_ratio_));
		}
#endif	// SLIM_WF_ONLY
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
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_individualCount:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(CurrentSubpopSize()));
			
			// variables
		case gID_tag:					// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property tag accessed on subpopulation before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
		case gID_fitnessScaling:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fitness_scaling_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

EidosValue *Subpopulation::GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpopulation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_firstMaleIndex(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->CurrentFirstMaleIndex(), value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_individualCount(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->CurrentSubpopSize(), value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property tag accessed on subpopulation before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->fitness_scaling_, value_index);
	}
	
	return float_result;
}

void Subpopulation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			fitness_scaling_ = p_value.FloatAtIndex(0, nullptr);
			
			if ((fitness_scaling_ < 0.0) || std::isnan(fitness_scaling_))
				EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

void Subpopulation::SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

void Subpopulation::SetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		if ((source_value < 0.0) || std::isnan(source_value))
			EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->fitness_scaling_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			double source_value = source_data[value_index];
			
			if ((source_value < 0.0) || std::isnan(source_value))
				EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			((Subpopulation *)(p_values[value_index]))->fitness_scaling_ = source_value;
		}
	}
}

EidosValue_SP Subpopulation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
#ifdef SLIM_WF_ONLY
		case gID_setMigrationRates:		return ExecuteMethod_setMigrationRates(p_method_id, p_arguments, p_interpreter);
		case gID_setCloningRate:		return ExecuteMethod_setCloningRate(p_method_id, p_arguments, p_interpreter);
		case gID_setSelfingRate:		return ExecuteMethod_setSelfingRate(p_method_id, p_arguments, p_interpreter);
		case gID_setSexRatio:			return ExecuteMethod_setSexRatio(p_method_id, p_arguments, p_interpreter);
		case gID_setSubpopulationSize:	return ExecuteMethod_setSubpopulationSize(p_method_id, p_arguments, p_interpreter);
#endif	// SLIM_WF_ONLY
			
#ifdef SLIM_NONWF_ONLY
		case gID_addCloned:				return ExecuteMethod_addCloned(p_method_id, p_arguments, p_interpreter);
		case gID_addCrossed:			return ExecuteMethod_addCrossed(p_method_id, p_arguments, p_interpreter);
		case gID_addEmpty:				return ExecuteMethod_addEmpty(p_method_id, p_arguments, p_interpreter);
		case gID_addRecombinant:		return ExecuteMethod_addRecombinant(p_method_id, p_arguments, p_interpreter);
		case gID_addSelfed:				return ExecuteMethod_addSelfed(p_method_id, p_arguments, p_interpreter);
		case gID_removeSubpopulation:	return ExecuteMethod_removeSubpopulation(p_method_id, p_arguments, p_interpreter);
		case gID_takeMigrants:			return ExecuteMethod_takeMigrants(p_method_id, p_arguments, p_interpreter);
#endif  // SLIM_NONWF_ONLY

		case gID_pointInBounds:			return ExecuteMethod_pointInBounds(p_method_id, p_arguments, p_interpreter);
		case gID_pointReflected:		return ExecuteMethod_pointReflected(p_method_id, p_arguments, p_interpreter);
		case gID_pointStopped:			return ExecuteMethod_pointStopped(p_method_id, p_arguments, p_interpreter);
		case gID_pointPeriodic:			return ExecuteMethod_pointPeriodic(p_method_id, p_arguments, p_interpreter);
		case gID_pointUniform:			return ExecuteMethod_pointUniform(p_method_id, p_arguments, p_interpreter);
		case gID_setSpatialBounds:		return ExecuteMethod_setSpatialBounds(p_method_id, p_arguments, p_interpreter);
		case gID_cachedFitness:			return ExecuteMethod_cachedFitness(p_method_id, p_arguments, p_interpreter);
		case gID_sampleIndividuals:		return ExecuteMethod_sampleIndividuals(p_method_id, p_arguments, p_interpreter);
		case gID_subsetIndividuals:		return ExecuteMethod_subsetIndividuals(p_method_id, p_arguments, p_interpreter);
		case gID_defineSpatialMap:		return ExecuteMethod_defineSpatialMap(p_method_id, p_arguments, p_interpreter);
		case gID_spatialMapColor:		return ExecuteMethod_spatialMapColor(p_method_id, p_arguments, p_interpreter);
		case gID_spatialMapValue:		return ExecuteMethod_spatialMapValue(p_method_id, p_arguments, p_interpreter);
		case gID_outputMSSample:
		case gID_outputVCFSample:
		case gID_outputSample:			return ExecuteMethod_outputXSample(p_method_id, p_arguments, p_interpreter);
		case gID_configureDisplay:		return ExecuteMethod_configureDisplay(p_method_id, p_arguments, p_interpreter);
			
		default:						return EidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

#ifdef SLIM_NONWF_ONLY

IndividualSex Subpopulation::_GenomeConfigurationForSex(EidosValue *p_sex_value, GenomeType &p_genome1_type, GenomeType &p_genome2_type, bool &p_genome1_null, bool &p_genome2_null)
{
	EidosValueType sex_value_type = p_sex_value->Type();
	IndividualSex sex;
	
	if (sex_enabled_)
	{
		if (sex_value_type == EidosValueType::kValueNULL)
		{
			// in sexual simulations, NULL (the default) means pick a sex with equal probability
			sex = (Eidos_RandomBool() ? IndividualSex::kMale : IndividualSex::kFemale);
		}
		else if (sex_value_type == EidosValueType::kValueString)
		{
			// if a string is provided, it must be either "M" or "F"
			std::string sex_string = p_sex_value->StringAtIndex(0, nullptr);
			
			if (sex_string == "M")
				sex = IndividualSex::kMale;
			else if (sex_string == "F")
				sex = IndividualSex::kFemale;
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenomeConfigurationForSex): unrecognized value '" << sex_string << "' for parameter sex." << EidosTerminate();
		}
		else // if (sex_value_type == EidosValueType::kValueFloat)
		{
			double sex_prob = p_sex_value->FloatAtIndex(0, nullptr);
			
			if ((sex_prob >= 0.0) && (sex_prob <= 1.0))
				sex = ((Eidos_rng_uniform(EIDOS_GSL_RNG) < sex_prob) ? IndividualSex::kMale : IndividualSex::kFemale);
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenomeConfigurationForSex): probability " << sex_prob << " out of range [0.0, 1.0] for parameter sex." << EidosTerminate();
		}
		
		switch (modeled_chromosome_type_)
		{
			case GenomeType::kAutosome:
				p_genome1_type = GenomeType::kAutosome;
				p_genome2_type = GenomeType::kAutosome;
				p_genome1_null = false;
				p_genome2_null = false;
				break;
			case GenomeType::kXChromosome:
				p_genome1_type = GenomeType::kXChromosome;
				p_genome2_type = ((sex == IndividualSex::kMale) ? GenomeType::kYChromosome : GenomeType::kXChromosome);
				p_genome1_null = false;
				p_genome2_null = (sex == IndividualSex::kMale);
				break;
			case GenomeType::kYChromosome:
				p_genome1_type = GenomeType::kXChromosome;
				p_genome2_type = ((sex == IndividualSex::kMale) ? GenomeType::kYChromosome : GenomeType::kXChromosome);
				p_genome1_null = true;
				p_genome2_null = (sex == IndividualSex::kFemale);
				break;
		}
	}
	else
	{
		if (sex_value_type != EidosValueType::kValueNULL)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenomeConfigurationForSex): sex must be NULL in non-sexual models." << EidosTerminate();
		
		sex = IndividualSex::kHermaphrodite;
		p_genome1_type = GenomeType::kAutosome;
		p_genome2_type = GenomeType::kAutosome;
		p_genome1_null = false;
		p_genome2_null = false;
	}
	
	return sex;
}

//	*********************	– (No<Individual>$)addCloned(object<Individual>$ parent)
//
EidosValue_SP Subpopulation::ExecuteMethod_addCloned(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMSim &sim = population_.sim_;
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): method -addCloned() is not available in WF models." << EidosTerminate();
	if (sim.GenerationStage() != SLiMGenerationStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): method -addCloned() may only be called from a reproduction() callback." << EidosTerminate();
	if (sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): method -addCloned() may not be called from a nested callback." << EidosTerminate();
	
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	// Get and check the first parent (the mother)
	EidosValue *parent_value = p_arguments[0].get();
	Individual *parent = (Individual *)parent_value->ObjectElementAtIndex(0, nullptr);
	IndividualSex parent_sex = parent->sex_;
	Subpopulation &parent_subpop = parent->subpopulation_;
	
	// Check for some other illegal setups
	if (parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// Determine the sex of the offspring, and the consequent expected genome types
	GenomeType genome1_type = parent->genome1_->Type(), genome2_type = parent->genome2_->Type();
	bool genome1_null = parent->genome1_->IsNull(), genome2_null = parent->genome2_->IsNull();
	IndividualSex child_sex = parent_sex;
	
	// Make the new individual as a candidate
	Genome *genome1 = NewSubpopGenome(mutrun_count, mutrun_length, genome1_type, genome1_null);
	Genome *genome2 = NewSubpopGenome(mutrun_count, mutrun_length, genome2_type, genome2_null);
	Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, /* index */ -1, /* pedigree ID */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN);
	Genome &parent_genome_1 = *parent_subpop.parent_genomes_[2 * parent->index_];
	Genome &parent_genome_2 = *parent_subpop.parent_genomes_[2 * parent->index_ + 1];
	std::vector<SLiMEidosBlock*> *parent_mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
	
	individual->TrackPedigreeWithParents(*parent, *parent);
	
	// TREE SEQUENCE RECORDING
	if (sim.RecordingTreeSequence())
	{
		sim.SetCurrentNewIndividual(individual);
		sim.RecordNewGenome(nullptr, genome1, &parent_genome_1, nullptr);
		sim.RecordNewGenome(nullptr, genome2, &parent_genome_2, nullptr);
	}
	
	if (!parent_mutation_callbacks->size()) parent_mutation_callbacks = nullptr;
	
	population_.DoClonalMutation(&parent_subpop, *genome1, parent_genome_1, child_sex, parent_mutation_callbacks);
	population_.DoClonalMutation(&parent_subpop, *genome2, parent_genome_2, child_sex, parent_mutation_callbacks);
	
	// Run the candidate past modifyChild() callbacks; the parent subpop's registered callbacks are used
	bool proposed_child_accepted = true;
	
	std::vector<SLiMEidosBlock*> &modify_child_callbacks_ = parent_subpop.registered_modify_child_callbacks_;
	
	if (modify_child_callbacks_.size())
		proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, genome1, genome2, child_sex, parent, &parent_genome_1, &parent_genome_1, parent, &parent_genome_2, &parent_genome_2, /* p_is_selfing */ false, /* p_is_cloning */ true, /* p_target_subpop */ this, /* p_source_subpop */ &parent_subpop, modify_child_callbacks_);
	
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
	if (proposed_child_accepted)
	{
		if ((child_sex == IndividualSex::kHermaphrodite) || (child_sex == IndividualSex::kMale))
			gui_offspring_cloned_M_++;
		if ((child_sex == IndividualSex::kHermaphrodite) || (child_sex == IndividualSex::kFemale))
			gui_offspring_cloned_F_++;
		
		// this offspring came from a parent in parent_subpop but ended up here, so it is, in effect, a migrant;
		// we tally things, SLiMgui display purposes, as if it were generated in parent_subpop and then moved
		parent_subpop.gui_premigration_size_++;
		if (&parent_subpop != this)
			gui_migrants_[parent_subpop.subpopulation_id_]++;
	}
#endif
	
	return _ResultAfterModifyChildCallbacks(proposed_child_accepted, individual, genome1, genome2);
}

//	*********************	– (No<Individual>$)addCrossed(object<Individual>$ parent1, object<Individual>$ parent2, [Nfs$ sex = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_addCrossed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMSim &sim = population_.sim_;
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): method -addCrossed() is not available in WF models." << EidosTerminate();
	if (sim.GenerationStage() != SLiMGenerationStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): method -addCrossed() may only be called from a reproduction() callback." << EidosTerminate();
	if (sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): method -addCrossed() may not be called from a nested callback." << EidosTerminate();
	
	bool prevent_incidental_selfing = sim.PreventIncidentalSelfing();
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	// Get and check the first parent (the mother)
	EidosValue *parent1_value = p_arguments[0].get();
	Individual *parent1 = (Individual *)parent1_value->ObjectElementAtIndex(0, nullptr);
	IndividualSex parent1_sex = parent1->sex_;
	Subpopulation &parent1_subpop = parent1->subpopulation_;
	
	if ((parent1_sex != IndividualSex::kFemale) && (parent1_sex != IndividualSex::kHermaphrodite))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent1 must be female in sexual models (or hermaphroditic in non-sexual models)." << EidosTerminate();
	
	// Get and check the second parent (the father)
	EidosValue *parent2_value = p_arguments[1].get();
	Individual *parent2 = (Individual *)parent2_value->ObjectElementAtIndex(0, nullptr);
	IndividualSex parent2_sex = parent2->sex_;
	Subpopulation &parent2_subpop = parent2->subpopulation_;
	
	if ((parent2_sex != IndividualSex::kMale) && (parent2_sex != IndividualSex::kHermaphrodite))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent2 must be male in sexual models (or hermaphroditic in non-sexual models)." << EidosTerminate();
	
	// Check for some other illegal setups
	if ((parent1->index_ == -1) || (parent2->index_ == -1))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	
	if (prevent_incidental_selfing && (parent1 == parent2))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent1 and parent2 must be different individuals, since preventIncidentalSelfing has been set to T (use addSelfed to generate a non-incidentally selfed offspring)." << EidosTerminate();
	
	// Determine the sex of the offspring based on the sex parameter, and the consequent expected genome types
	EidosValue *sex_value = p_arguments[2].get();
	GenomeType genome1_type, genome2_type;
	bool genome1_null, genome2_null;
	IndividualSex child_sex = _GenomeConfigurationForSex(sex_value, genome1_type, genome2_type, genome1_null, genome2_null);
	
	// Make the new individual as a candidate
	Genome *genome1 = NewSubpopGenome(mutrun_count, mutrun_length, genome1_type, genome1_null);
	Genome *genome2 = NewSubpopGenome(mutrun_count, mutrun_length, genome2_type, genome2_null);
	Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, /* index */ -1, /* pedigree ID */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN);
	std::vector<SLiMEidosBlock*> *parent1_recombination_callbacks = &parent1_subpop.registered_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> *parent2_recombination_callbacks = &parent2_subpop.registered_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> *parent1_mutation_callbacks = &parent1_subpop.registered_mutation_callbacks_;
	std::vector<SLiMEidosBlock*> *parent2_mutation_callbacks = &parent2_subpop.registered_mutation_callbacks_;
	
	individual->TrackPedigreeWithParents(*parent1, *parent2);
	
	// TREE SEQUENCE RECORDING
	if (sim.RecordingTreeSequence())
		sim.SetCurrentNewIndividual(individual);
	
	if (!parent1_recombination_callbacks->size()) parent1_recombination_callbacks = nullptr;
	if (!parent2_recombination_callbacks->size()) parent2_recombination_callbacks = nullptr;
	if (!parent1_mutation_callbacks->size()) parent1_mutation_callbacks = nullptr;
	if (!parent2_mutation_callbacks->size()) parent2_mutation_callbacks = nullptr;
	
	population_.DoCrossoverMutation(&parent1_subpop, *genome1, parent1->index_, child_sex, parent1_sex, parent1_recombination_callbacks, parent1_mutation_callbacks);
	population_.DoCrossoverMutation(&parent2_subpop, *genome2, parent2->index_, child_sex, parent2_sex, parent2_recombination_callbacks, parent2_mutation_callbacks);
	
	// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
	std::vector<SLiMEidosBlock*> &modify_child_callbacks_ = parent1_subpop.registered_modify_child_callbacks_;
	
	if (modify_child_callbacks_.size())
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, genome1, genome2, child_sex, parent1, parent1->genome1_, parent1->genome2_, parent2, parent2->genome1_, parent2->genome2_, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, modify_child_callbacks_);
		
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
		if (proposed_child_accepted)
		{
			gui_offspring_crossed_++;
			
			// this offspring came from parents in parent1_subpop and parent2_subpop but ended up here, so it is, in effect, a migrant;
			// we tally things, SLiMgui display purposes, as if it were generated in those other subpops and then moved
			parent1_subpop.gui_premigration_size_ += 0.5;
			parent2_subpop.gui_premigration_size_ += 0.5;
			if (&parent1_subpop != this)
				gui_migrants_[parent1_subpop.subpopulation_id_] += 0.5;
			if (&parent2_subpop != this)
				gui_migrants_[parent2_subpop.subpopulation_id_] += 0.5;
		}
#endif
		
		return _ResultAfterModifyChildCallbacks(proposed_child_accepted, individual, genome1, genome2);
	}
	else
	{
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
		gui_offspring_crossed_++;
		
		// this offspring came from parents in parent1_subpop and parent2_subpop but ended up here, so it is, in effect, a migrant;
		// we tally things, SLiMgui display purposes, as if it were generated in those other subpops and then moved
		parent1_subpop.gui_premigration_size_ += 0.5;
		parent2_subpop.gui_premigration_size_ += 0.5;
		if (&parent1_subpop != this)
			gui_migrants_[parent1_subpop.subpopulation_id_] += 0.5;
		if (&parent2_subpop != this)
			gui_migrants_[parent2_subpop.subpopulation_id_] += 0.5;
#endif
		
		return _ResultAfterModifyChildCallbacks(true, individual, genome1, genome2);
	}
}

//	*********************	– (No<Individual>$)addEmpty([Nfs$ sex = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_addEmpty(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMSim &sim = population_.sim_;
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): method -addEmpty() is not available in WF models." << EidosTerminate();
	if (sim.GenerationStage() != SLiMGenerationStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): method -addEmpty() may only be called from a reproduction() callback." << EidosTerminate();
	if (sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): method -addEmpty() may not be called from a nested callback." << EidosTerminate();
	
	EidosValue *sex_value = p_arguments[0].get();
	GenomeType genome1_type, genome2_type;
	bool genome1_null, genome2_null;
	IndividualSex child_sex = _GenomeConfigurationForSex(sex_value, genome1_type, genome2_type, genome1_null, genome2_null);
	
	// Make the new individual as a candidate
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	Genome *genome1 = NewSubpopGenome(mutrun_count, mutrun_length, genome1_type, genome1_null);
	Genome *genome2 = NewSubpopGenome(mutrun_count, mutrun_length, genome2_type, genome2_null);
	Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, /* index */ -1, /* pedigree ID */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN);
	
	individual->TrackPedigreeWithoutParents();
	
	// TREE SEQUENCE RECORDING
	if (sim.RecordingTreeSequence())
	{
		sim.SetCurrentNewIndividual(individual);
		sim.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
		sim.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
	}
	
	// set up empty mutation runs, since we're not calling DoCrossoverMutation() or DoClonalMutation()
#if DEBUG
	genome1->check_cleared_to_nullptr();
	genome2->check_cleared_to_nullptr();
#endif
	
	genome1->clear_to_empty();
	genome2->clear_to_empty();
	
	// Run the candidate past modifyChild() callbacks; the target subpop's registered callbacks are used
	bool proposed_child_accepted = true;
	
	if (registered_modify_child_callbacks_.size())
		proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, genome1, genome2, child_sex, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, registered_modify_child_callbacks_);
	
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
	if (proposed_child_accepted) gui_offspring_empty_++;
	
	gui_premigration_size_++;
#endif
	
	return _ResultAfterModifyChildCallbacks(proposed_child_accepted, individual, genome1, genome2);
}

//	*********************	– (No<Individual>$)addRecombinant(No<Genome>$ strand1, No<Genome>$ strand2, Ni breaks1,
//															  No<Genome>$ strand3, No<Genome>$ strand4, Ni breaks2, [Nfs$ sex = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_addRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMSim &sim = population_.sim_;
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): method -addRecombinant() is not available in WF models." << EidosTerminate();
	if (sim.GenerationStage() != SLiMGenerationStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): method -addRecombinant() may only be called from a reproduction() callback." << EidosTerminate();
	if (sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): method -addRecombinant() may not be called from a nested callback." << EidosTerminate();
	
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	// Note that empty Genome vectors are not legal values for the strandX parameters; the strands must either be NULL or singleton.
	// If strand1 and strand2 are both NULL, breaks1 must be NULL/empty, and the offspring genome will be empty and will not receive mutations.
	// If strand1 is non-NULL and strand2 is NULL, breaks1 must be NULL/empty, and the offspring genome will be cloned with mutations.
	// If strand1 and strand2 are both non-NULL, breaks1 must be non-NULL but need not be sorted, and recombination with mutations will occur.
	// If strand1 is NULL and strand2 is non-NULL, that is presently an error, but may be given a meaning later.
	// The same is true, mutatis mutandis, for strand3, strand4, and breaks2.  The sex parameter is interpreted as in addCrossed().
	EidosValue *strand1_value = p_arguments[0].get();
	EidosValue *strand2_value = p_arguments[1].get();
	EidosValue *breaks1_value = p_arguments[2].get();
	EidosValue *strand3_value = p_arguments[3].get();
	EidosValue *strand4_value = p_arguments[4].get();
	EidosValue *breaks2_value = p_arguments[5].get();
	EidosValue *sex_value = p_arguments[6].get();
	
	// Get the genomes for the supplied strands, or nullptr for NULL
	Genome *strand1 = ((strand1_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand1_value->ObjectElementAtIndex(0, nullptr));
	Genome *strand2 = ((strand2_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand2_value->ObjectElementAtIndex(0, nullptr));
	Genome *strand3 = ((strand3_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand3_value->ObjectElementAtIndex(0, nullptr));
	Genome *strand4 = ((strand4_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand4_value->ObjectElementAtIndex(0, nullptr));
	
	// The parental strands must be visible in the subpopulation, and we need to be able to find them to check their sex
	Individual *strand1_parent = (strand1 ? strand1->individual_ : nullptr);
	Individual *strand2_parent = (strand2 ? strand2->individual_ : nullptr);
	Individual *strand3_parent = (strand3 ? strand3->individual_ : nullptr);
	Individual *strand4_parent = (strand4 ? strand4->individual_ : nullptr);
	
	if ((strand1 && (strand1_parent->index_ == -1)) || (strand2 && (strand2_parent->index_ == -1)) ||
		(strand3 && (strand3_parent->index_ == -1)) || (strand4 && (strand4_parent->index_ == -1)))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): a parental strand is not visible in the subpopulation (i.e., belongs to a new juvenile)." << EidosTerminate();
	
	// If both strands are non-NULL for a pair, they must be the same type of genome
	if (strand1 && strand2 && (strand1->Type() != strand2->Type()))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 are not the same type of genome, and thus cannot recombine." << EidosTerminate();
	if (strand3 && strand4 && (strand3->Type() != strand4->Type()))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 are not the same type of genome, and thus cannot recombine." << EidosTerminate();
	
	// If the first strand of a pair is NULL, the second must also be NULL
	if (!strand1 && strand2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): if strand1 is NULL, strand2 must also be NULL." << EidosTerminate();
	if (!strand3 && strand4)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): if strand3 is NULL, strand4 must also be NULL." << EidosTerminate();
	
	// If both pairs have a non-NULL genome, they must both be autosomal or both be non-autosomal (this should never be hit)
	if (strand1 && strand3 &&
		(((strand1->Type() == GenomeType::kAutosome) && (strand3->Type() != GenomeType::kAutosome)) || 
		 ((strand3->Type() == GenomeType::kAutosome) && (strand1->Type() != GenomeType::kAutosome))))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): autosomal genomes cannot be mixed with non-autosomal genomes." << EidosTerminate();
	
	// The first pair cannot be Y chromosomes; those must be supplied in the second pair (if at all)
	if (strand1 && (strand1->Type() == GenomeType::kYChromosome))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the Y chromosome must be supplied as the second pair of strands in sexual models." << EidosTerminate();
	
	// Figure out what sex we're generating, and what that implies about the offspring genomes
	if ((sex_value->Type() == EidosValueType::kValueNULL) && strand3)
	{
		// NULL can mean "infer the child sex from the strands given"; do that here
		// if strand3 is supplied and is a sex chromosome, it determines the sex of the offspring (strand4 must be NULL or matching type)
		static EidosValue_SP static_sex_string_F;
		static EidosValue_SP static_sex_string_M;
		
		if (!static_sex_string_F) static_sex_string_F = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("F"));
		if (!static_sex_string_M) static_sex_string_M = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("M"));
		
		if (strand3->Type() == GenomeType::kXChromosome)
			sex_value = static_sex_string_F.get();
		else if (strand3->Type() == GenomeType::kYChromosome)
			sex_value = static_sex_string_M.get();
	}
	
	GenomeType genome1_type, genome2_type;
	bool genome1_null, genome2_null;
	IndividualSex child_sex = _GenomeConfigurationForSex(sex_value, genome1_type, genome2_type, genome1_null, genome2_null);
	
	// Check that the chosen sex makes sense with respect to the strands given
	if (strand1 && genome1_type != strand1->Type())
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the type of strand1 does not match the expectation from the sex of the generated offspring." << EidosTerminate();
	if (strand3 && genome2_type != strand3->Type())
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the type of strand3 does not match the expectation from the sex of the generated offspring." << EidosTerminate();
	
	// Check that the breakpoint vectors make sense; breakpoints may not be supplied for a NULL pair or a half-NULL pair, but must be supplied for a non-NULL pair
	if ((breaks1_value->Count() != 0) && !strand2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): non-empty breaks1 supplied with a NULL strand2; recombination between strand1 and strand2 is not possible, so breaks1 must be NULL or empty." << EidosTerminate();
	if ((breaks2_value->Count() != 0) && !strand4)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): non-empty breaks2 supplied with a NULL strand4; recombination between strand3 and strand4 is not possible, so breaks2 must be NULL or empty." << EidosTerminate();
	if ((breaks1_value->Type() == EidosValueType::kValueNULL) && strand1 && strand2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 are both supplied, so breaks1 may not be NULL (but may be empty)." << EidosTerminate();
	if ((breaks2_value->Type() == EidosValueType::kValueNULL) && strand3 && strand4)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 are both supplied, so breaks2 may not be NULL (but may be empty)." << EidosTerminate();
	
	// Sort and unique and bounds-check the breakpoints
	std::vector<slim_position_t> breakvec1, breakvec2;
	
	if (breaks1_value->Count())
	{
		for (int break_index = 0; break_index < breaks1_value->Count(); ++break_index)
			breakvec1.push_back(SLiMCastToPositionTypeOrRaise(breaks1_value->IntAtIndex(break_index, nullptr)));
		
		std::sort(breakvec1.begin(), breakvec1.end());
		breakvec1.erase(unique(breakvec1.begin(), breakvec1.end()), breakvec1.end());
		
		if (breakvec1.back() > chromosome.last_position_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): breaks1 contained a value (" << breakvec1.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
		
		// handle a breakpoint at position 0, which swaps the initial strand; DoRecombinantMutation() does not like this
		if (breakvec1.front() == 0)
		{
			breakvec1.erase(breakvec1.begin());
			std::swap(strand1, strand2);
			std::swap(strand1_parent, strand2_parent);
			std::swap(strand1_value, strand2_value);
		}
	}
	
	if (breaks2_value->Count())
	{
		for (int break_index = 0; break_index < breaks2_value->Count(); ++break_index)
			breakvec2.push_back(SLiMCastToPositionTypeOrRaise(breaks2_value->IntAtIndex(break_index, nullptr)));
		
		std::sort(breakvec2.begin(), breakvec2.end());
		breakvec2.erase(unique(breakvec2.begin(), breakvec2.end()), breakvec2.end());
		
		if (breakvec2.back() > chromosome.last_position_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): breaks2 contained a value (" << breakvec2.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
		
		// handle a breakpoint at position 0, which swaps the initial strand; DoRecombinantMutation() does not like this
		if (breakvec2.front() == 0)
		{
			breakvec2.erase(breakvec2.begin());
			std::swap(strand3, strand4);
			std::swap(strand3_parent, strand4_parent);
			std::swap(strand3_value, strand4_value);
		}
	}
	
	// Make the new individual as a candidate
	Genome *genome1 = NewSubpopGenome(mutrun_count, mutrun_length, genome1_type, genome1_null);
	Genome *genome2 = NewSubpopGenome(mutrun_count, mutrun_length, genome2_type, genome2_null);
	Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, /* index */ -1, /* pedigree ID */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN);
	std::vector<SLiMEidosBlock*> *mutation_callbacks = &registered_mutation_callbacks_;
	
	individual->TrackPedigreeWithoutParents();
	
	// TREE SEQUENCE RECORDING
	if (sim.RecordingTreeSequence())
		sim.SetCurrentNewIndividual(individual);
	
	if (!mutation_callbacks) mutation_callbacks = nullptr;
	
	// Construct the first child genome, depending upon whether recombination is requested, etc.
	if (strand1)
	{
		if (strand2 && breakvec1.size())
		{
			// determine the sex of the "parent"; we need this to choose the mutation rate map for new mutations
			// if we can't figure out a consistent sex, and we need one because we have separate mutation rate maps, it is an error
			// this seems unlikely to bite anybody, so it is not worth adding another parameter to allow it to be resolved
			IndividualSex parent_sex = IndividualSex::kHermaphrodite;
			
			if (sex_enabled_ && !chromosome.UsingSingleMutationMap())
			{
				if (strand1_parent && strand2_parent)
				{
					if (strand1_parent->sex_ == strand2_parent->sex_)
						parent_sex = strand1_parent->sex_;
				}
				else if (strand1_parent)
					parent_sex = strand1_parent->sex_;
				else if (strand2_parent)
					parent_sex = strand2_parent->sex_;
				
				if (parent_sex == IndividualSex::kHermaphrodite)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
			}
			
			// both strands are non-NULL and we have a breakpoint, so we do recombination between them
			if (sim.RecordingTreeSequence())
				sim.RecordNewGenome(&breakvec1, genome1, strand1, strand2);
			
			population_.DoRecombinantMutation(/* p_mutorigin_subpop */ this, *genome1, strand1, strand2, parent_sex, breakvec1, mutation_callbacks);
		}
		else
		{
			// one strand is non-NULL but the other is NULL, so we clone the non-NULL strand
			if (sim.RecordingTreeSequence())
				sim.RecordNewGenome(nullptr, genome1, strand1, nullptr);
			
			population_.DoClonalMutation(/* p_mutorigin_subpop */ this, *genome1, *strand1, child_sex, mutation_callbacks);
		}
	}
	else
	{
		// both strands are NULL, so we make a new empty genome, as addEmpty() does
		if (sim.RecordingTreeSequence())
			sim.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
		
		genome1->clear_to_empty();
	}
	
	// Construct the second child genome, depending upon whether recombination is requested, etc.
	if (strand3)
	{
		if (strand4 && breakvec2.size())
		{
			// determine the sex of the "parent"; we need this to choose the mutation rate map for new mutations
			// if we can't figure out a consistent sex, and we need one because we have separate mutation rate maps, it is an error
			// this seems unlikely to bite anybody, so it is not worth adding another parameter to allow it to be resolved
			IndividualSex parent_sex = IndividualSex::kHermaphrodite;
			
			if (sex_enabled_ && !chromosome.UsingSingleMutationMap())
			{
				if (strand3_parent && strand4_parent)
				{
					if (strand3_parent->sex_ == strand4_parent->sex_)
						parent_sex = strand3_parent->sex_;
				}
				else if (strand3_parent)
					parent_sex = strand3_parent->sex_;
				else if (strand4_parent)
					parent_sex = strand4_parent->sex_;
				
				if (parent_sex == IndividualSex::kHermaphrodite)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
			}
			
			// both strands are non-NULL and we have a breakpoint, so we do recombination between them
			if (sim.RecordingTreeSequence())
				sim.RecordNewGenome(&breakvec2, genome2, strand3, strand4);
			
			population_.DoRecombinantMutation(/* p_mutorigin_subpop */ this, *genome2, strand3, strand4, parent_sex, breakvec2, mutation_callbacks);
		}
		else
		{
			// one strand is non-NULL but the other is NULL, so we clone the non-NULL strand
			if (sim.RecordingTreeSequence())
				sim.RecordNewGenome(nullptr, genome2, strand3, nullptr);
			
			population_.DoClonalMutation(/* p_mutorigin_subpop */ this, *genome2, *strand3, child_sex, mutation_callbacks);
		}
	}
	else
	{
		// both strands are NULL, so we make a new empty genome, as addEmpty() does
		if (sim.RecordingTreeSequence())
			sim.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
		
		genome2->clear_to_empty();
	}
	
	// Run the candidate past modifyChild() callbacks; the target subpop's registered callbacks are used
	if (registered_modify_child_callbacks_.size())
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, genome1, genome2, child_sex, /* p_parent1 */ nullptr, strand1, strand2, /* p_parent2 */ nullptr, strand3, strand4, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, registered_modify_child_callbacks_);
		
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
		if (proposed_child_accepted)
		{
			gui_offspring_crossed_++;
			
			// this offspring came from parents in various subpops but ended up here, so it is, in effect, a migrant;
			// we tally things, SLiMgui display purposes, as if it were generated in the parental subpops and then moved
			// this is pretty gross, but runs only in SLiMgui, so whatever :->
			Subpopulation *strand1_subpop = (strand1_parent ? &strand1_parent->subpopulation_ : nullptr);
			Subpopulation *strand2_subpop = (strand2_parent ? &strand2_parent->subpopulation_ : nullptr);
			Subpopulation *strand3_subpop = (strand3_parent ? &strand3_parent->subpopulation_ : nullptr);
			Subpopulation *strand4_subpop = (strand4_parent ? &strand4_parent->subpopulation_ : nullptr);
			bool both_offspring_strands_inherited = (strand1_subpop && strand3_subpop);
			double strand1_weight = 0.0, strand2_weight = 0.0, strand3_weight = 0.0, strand4_weight = 0.0;
			
			if (strand1_subpop && strand2_subpop)
			{
				strand1_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
				strand2_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
			}
			else if (strand1_subpop)
			{
				strand1_weight = (both_offspring_strands_inherited ? 0.5 : 1.0);
			}
			
			if (strand3_subpop && strand4_subpop)
			{
				strand3_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
				strand4_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
			}
			else if (strand3_subpop)
			{
				strand3_weight = (both_offspring_strands_inherited ? 0.5 : 1.0);
			}
			
			if (strand1_weight > 0)
			{
				strand1_subpop->gui_premigration_size_ += strand1_weight;
				if (strand1_subpop != this)
					gui_migrants_[strand1_subpop->subpopulation_id_]++;
			}
			if (strand2_weight > 0)
			{
				strand2_subpop->gui_premigration_size_ += strand2_weight;
				if (strand2_subpop != this)
					gui_migrants_[strand2_subpop->subpopulation_id_]++;
			}
			if (strand3_weight > 0)
			{
				strand3_subpop->gui_premigration_size_ += strand3_weight;
				if (strand3_subpop != this)
					gui_migrants_[strand3_subpop->subpopulation_id_]++;
			}
			if (strand4_weight > 0)
			{
				strand4_subpop->gui_premigration_size_ += strand4_weight;
				if (strand4_subpop != this)
					gui_migrants_[strand4_subpop->subpopulation_id_]++;
			}
		}
#endif
		
		return _ResultAfterModifyChildCallbacks(proposed_child_accepted, individual, genome1, genome2);
	}
	else
	{
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
		gui_offspring_crossed_++;
		
		// this offspring came from parents in various subpops but ended up here, so it is, in effect, a migrant;
		// we tally things, SLiMgui display purposes, as if it were generated in the parental subpops and then moved
		// this is pretty gross, but runs only in SLiMgui, so whatever :->
		Subpopulation *strand1_subpop = (strand1_parent ? &strand1_parent->subpopulation_ : nullptr);
		Subpopulation *strand2_subpop = (strand2_parent ? &strand2_parent->subpopulation_ : nullptr);
		Subpopulation *strand3_subpop = (strand3_parent ? &strand3_parent->subpopulation_ : nullptr);
		Subpopulation *strand4_subpop = (strand4_parent ? &strand4_parent->subpopulation_ : nullptr);
		bool both_offspring_strands_inherited = (strand1_subpop && strand3_subpop);
		double strand1_weight = 0.0, strand2_weight = 0.0, strand3_weight = 0.0, strand4_weight = 0.0;
		
		if (strand1_subpop && strand2_subpop)
		{
			strand1_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
			strand2_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
		}
		else if (strand1_subpop)
		{
			strand1_weight = (both_offspring_strands_inherited ? 0.5 : 1.0);
		}
		
		if (strand3_subpop && strand4_subpop)
		{
			strand3_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
			strand4_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
		}
		else if (strand3_subpop)
		{
			strand3_weight = (both_offspring_strands_inherited ? 0.5 : 1.0);
		}
		
		if (strand1_weight > 0)
		{
			strand1_subpop->gui_premigration_size_ += strand1_weight;
			if (strand1_subpop != this)
				gui_migrants_[strand1_subpop->subpopulation_id_]++;
		}
		if (strand2_weight > 0)
		{
			strand2_subpop->gui_premigration_size_ += strand2_weight;
			if (strand2_subpop != this)
				gui_migrants_[strand2_subpop->subpopulation_id_]++;
		}
		if (strand3_weight > 0)
		{
			strand3_subpop->gui_premigration_size_ += strand3_weight;
			if (strand3_subpop != this)
				gui_migrants_[strand3_subpop->subpopulation_id_]++;
		}
		if (strand4_weight > 0)
		{
			strand4_subpop->gui_premigration_size_ += strand4_weight;
			if (strand4_subpop != this)
				gui_migrants_[strand4_subpop->subpopulation_id_]++;
		}
#endif
		
		return _ResultAfterModifyChildCallbacks(true, individual, genome1, genome2);
	}
}

//	*********************	– (No<Individual>$)addSelfed(object<Individual>$ parent)
//
EidosValue_SP Subpopulation::ExecuteMethod_addSelfed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMSim &sim = population_.sim_;
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): method -addSelfed() is not available in WF models." << EidosTerminate();
	if (sim.GenerationStage() != SLiMGenerationStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): method -addSelfed() may only be called from a reproduction() callback." << EidosTerminate();
	if (sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): method -addSelfed() may not be called from a nested callback." << EidosTerminate();
	
	Chromosome &chromosome = sim.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	// Get and check the first parent (the mother)
	EidosValue *parent_value = p_arguments[0].get();
	Individual *parent = (Individual *)parent_value->ObjectElementAtIndex(0, nullptr);
	IndividualSex parent_sex = parent->sex_;
	Subpopulation &parent_subpop = parent->subpopulation_;
	
	if (parent_sex != IndividualSex::kHermaphrodite)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): parent must be hermaphroditic in addSelfed()." << EidosTerminate();
	
	// Check for some other illegal setups
	if (parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// Determine the sex of the offspring, and the consequent expected genome types; for selfing this is predetermined
	GenomeType genome1_type = GenomeType::kAutosome, genome2_type = GenomeType::kAutosome;
	bool genome1_null = false, genome2_null = false;
	IndividualSex child_sex = IndividualSex::kHermaphrodite;
	
	// Make the new individual as a candidate
	Genome *genome1 = NewSubpopGenome(mutrun_count, mutrun_length, genome1_type, genome1_null);
	Genome *genome2 = NewSubpopGenome(mutrun_count, mutrun_length, genome2_type, genome2_null);
	Individual *individual = new (individual_pool_->AllocateChunk()) Individual(*this, /* index */ -1, /* pedigree ID */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN);
	std::vector<SLiMEidosBlock*> *parent_recombination_callbacks = &parent_subpop.registered_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> *parent_mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
	
	individual->TrackPedigreeWithParents(*parent, *parent);
	
	// TREE SEQUENCE RECORDING
	if (sim.RecordingTreeSequence())
		sim.SetCurrentNewIndividual(individual);
	
	if (!parent_recombination_callbacks->size()) parent_recombination_callbacks = nullptr;
	if (!parent_mutation_callbacks->size()) parent_mutation_callbacks = nullptr;
	
	population_.DoCrossoverMutation(&parent_subpop, *genome1, parent->index_, child_sex, parent_sex, parent_recombination_callbacks, parent_mutation_callbacks);
	population_.DoCrossoverMutation(&parent_subpop, *genome2, parent->index_, child_sex, parent_sex, parent_recombination_callbacks, parent_mutation_callbacks);
	
	// Run the candidate past modifyChild() callbacks; the parent subpop's registered callbacks are used
	std::vector<SLiMEidosBlock*> &modify_child_callbacks_ = parent_subpop.registered_modify_child_callbacks_;
	
	if (modify_child_callbacks_.size())
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, genome1, genome2, child_sex, parent, parent->genome1_, parent->genome2_, parent, parent->genome1_, parent->genome2_, /* p_is_selfing */ true, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ &parent_subpop, modify_child_callbacks_);
		
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
		if (proposed_child_accepted)
		{
			gui_offspring_selfed_++;
			
			// this offspring came from a parent in parent_subpop but ended up here, so it is, in effect, a migrant;
			// we tally things, SLiMgui display purposes, as if it were generated in parent_subpop and then moved
			parent_subpop.gui_premigration_size_++;
			if (&parent_subpop != this)
				gui_migrants_[parent_subpop.subpopulation_id_]++;
		}
#endif
		
		return _ResultAfterModifyChildCallbacks(proposed_child_accepted, individual, genome1, genome2);
	}
	else
	{
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
		gui_offspring_selfed_++;
		
		// this offspring came from a parent in parent_subpop but ended up here, so it is, in effect, a migrant;
		// we tally things, SLiMgui display purposes, as if it were generated in parent_subpop and then moved
		parent_subpop.gui_premigration_size_++;
		if (&parent_subpop != this)
			gui_migrants_[parent_subpop.subpopulation_id_]++;
#endif
		
		return _ResultAfterModifyChildCallbacks(true, individual, genome1, genome2);
	}
}

//	*********************	- (void)takeMigrants(object<Individual> migrants)
//
EidosValue_SP Subpopulation::ExecuteMethod_takeMigrants(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMSim &sim = population_.sim_;
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): method -takeMigrants() is not available in WF models." << EidosTerminate();
	if ((sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): method -takeMigrants() must be called directly from an early() or late() event." << EidosTerminate();
	
	EidosValue_Object *migrants_value = (EidosValue_Object *)p_arguments[0].get();
	int migrant_count = migrants_value->Count();
	
	if (migrant_count == 0)
		return gStaticEidosValueVOID;
	
	// So, we want to loop through migrants, add them all to the focal subpop (changing their subpop ivar and their index), and remove them from their
	// old subpop.  This is tricky because Genome and Individual are allocated out of subpop-specific pools; we can't just use the same objects in a
	// new subpop, because the objects wouldn't know how to free themselves, their owning subpop might cease to exist out from under them, etc.  So
	// instead, we have to play a shell game: make a new individual and genomes in the focal subpop, use swap() to move the internal information from
	// old to new, dispose of the old objects, add the new objects to the focal subpop.  Unfortunately, it's even a bit more complex than that, because
	// this is all done mid-script, so the user can have references to the original individual/genomes.  We want the switcheroo to be invisible to the
	// user; so we will actually go into the Eidos symbol tables and patch references to the old individual/genomes into references to the new ones.
	// We do that patching relatively quickly by storing the new (patch) pointer inside each object that needs patching; that adds 8 bytes to the size
	// of Individual and Genome just to support this method, which sucks, but without that the patching algorithm would be O(N*M) in the number of
	// migrants and the number of Genome/Individual elements in EidosValue variables, which would be really painful for large models; now it is O(N+M).
	
	// I think this might possibly be the most disgusting code I have ever written.  Sorry.  This is the price we pay for allocating Genomes and
	// Individuals out of subpop-specific pools; if it weren't for that, this method would be trivial and beautiful.  That optimization is worth it,
	// though – memory locality is king.
	std::vector<Genome *> old_genome_ptrs, new_genome_ptrs;
	std::vector<Individual *> old_individual_ptrs, new_individual_ptrs;
	
	// First, clear our genome and individual caches in all subpopulations; we don't want to have to do the work of patching them below, and any
	// subpops involved in the migration will be invalidated anyway so this probably isn't even that much overkill in most models.
	for (auto subpop_pair : population_.subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		
		subpop->cached_parent_genomes_value_.reset();
		subpop->cached_parent_individuals_value_.reset();
	}
	
	// Then loop over the migrants and move them one by one
	for (int migrant_index = 0; migrant_index < migrant_count; ++migrant_index)
	{
		Individual *migrant = (Individual *)migrants_value->ObjectElementAtIndex(migrant_index, nullptr);
		Subpopulation *source_subpop = &migrant->subpopulation_;
		
		if (source_subpop != this)
		{
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
			// before doing anything else, tally this incoming migrant for SLiMgui
			++gui_migrants_[source_subpop->subpopulation_id_];
#endif
			
			slim_popsize_t source_subpop_size = source_subpop->parent_subpop_size_;
			slim_popsize_t source_subpop_index = migrant->index_;
			Genome *genome1 = migrant->genome1_;
			Genome *genome2 = migrant->genome2_;
			
			if (source_subpop_index < 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): method -takeMigrants() may not move an individual that is not visible in a subpopulation.  This error may also occur if you try to migrate the same individual more than once in a single takeMigrants() call (i.e., if the migrants vector is not uniqued)." << EidosTerminate();
			
			// remove the originals from source_subpop's vectors
			if (migrant->sex_ == IndividualSex::kFemale)
			{
				// females have to be backfilled by the last female, and then that hole is backfilled by a male, and the first male index changes
				slim_popsize_t source_first_male = source_subpop->parent_first_male_index_;
				
				if (source_subpop_index < source_first_male - 1)
				{
					Individual *backfill = source_subpop->parent_individuals_[source_first_male - 1];
					
					source_subpop->parent_individuals_[source_subpop_index] = backfill;
					backfill->index_ = source_subpop_index;
					
					source_subpop->parent_genomes_[source_subpop_index * 2] = source_subpop->parent_genomes_[(source_first_male - 1) * 2];
					source_subpop->parent_genomes_[source_subpop_index * 2 + 1] = source_subpop->parent_genomes_[(source_first_male - 1) * 2 + 1];
				}
				
				if (source_first_male - 1 < source_subpop_size - 1)
				{
					Individual *backfill = source_subpop->parent_individuals_[source_subpop_size - 1];
					
					source_subpop->parent_individuals_[source_first_male - 1] = backfill;
					backfill->index_ = source_first_male - 1;
					
					source_subpop->parent_genomes_[(source_first_male - 1) * 2] = source_subpop->parent_genomes_[(source_subpop_size - 1) * 2];
					source_subpop->parent_genomes_[(source_first_male - 1) * 2 + 1] = source_subpop->parent_genomes_[(source_subpop_size - 1) * 2 + 1];
				}
				
				source_subpop->parent_subpop_size_ = --source_subpop_size;
				source_subpop->parent_individuals_.resize(source_subpop_size);
				source_subpop->parent_genomes_.resize(source_subpop_size * 2);
				
				source_subpop->parent_first_male_index_ = --source_first_male;
			}
			else
			{
				// males and hermaphrodites can be removed with a simple backfill from the end of the vector
				if (source_subpop_index < source_subpop_size - 1)
				{
					Individual *backfill = source_subpop->parent_individuals_[source_subpop_size - 1];
					
					source_subpop->parent_individuals_[source_subpop_index] = backfill;
					backfill->index_ = source_subpop_index;
					
					source_subpop->parent_genomes_[source_subpop_index * 2] = source_subpop->parent_genomes_[(source_subpop_size - 1) * 2];
					source_subpop->parent_genomes_[source_subpop_index * 2 + 1] = source_subpop->parent_genomes_[(source_subpop_size - 1) * 2 + 1];
				}
				
				source_subpop->parent_subpop_size_ = --source_subpop_size;
				source_subpop->parent_individuals_.resize(source_subpop_size);
				source_subpop->parent_genomes_.resize(source_subpop_size * 2);
			}
			
			// mark the migrant as invisible in its original subpopulation; this prevents us from trying to migrate the same individual twice
			migrant->index_ = -1;
			
			// make the transmogrified individual and genomes, from our own pools
			// passing p_is_null==true to NewSubpopGenome() is correct here; we transfer mutruns from the old genomes, so
			// we get a new null genome from the pool, and then after swapping a null genome will be returned to the pool
			Genome *genome1_transmogrified = NewSubpopGenome(0, 0, GenomeType::kAutosome, true);
			Genome *genome2_transmogrified = NewSubpopGenome(0, 0, GenomeType::kAutosome, true);
			Individual *migrant_transmogrified = new (individual_pool_->AllocateChunk()) Individual(*this, -1, -1, genome1_transmogrified, genome2_transmogrified, IndividualSex::kHermaphrodite, -1, NAN);
			
			// swap over the original individual and genome internals; skip self_value_ and subpop_ and genome1_ and genome2_
			// actually, for most values we can just copy since we don't care what state is left in the original object
			genome1_transmogrified->genome_type_ = genome1->genome_type_;
			std::swap(genome1->mutrun_count_, genome1_transmogrified->mutrun_count_);
			std::swap(genome1->mutrun_length_, genome1_transmogrified->mutrun_length_);
			if (genome1->mutruns_ == genome1->run_buffer_)
			{
				// if the original genome points into its internal buffer, swap internal buffer contents and point into our internal buffer
				std::swap(genome1->run_buffer_, genome1_transmogrified->run_buffer_);
				genome1_transmogrified->mutruns_ = genome1_transmogrified->run_buffer_;
			}
			else
			{
				// if the original genome points to an external buffer, ignore the internal buffer and just swap external buffers
				std::swap(genome1->mutruns_, genome1_transmogrified->mutruns_);
			}
			genome1->mutruns_ = nullptr;
			genome1_transmogrified->tag_value_ = genome1->tag_value_;
			genome1_transmogrified->genome_id_ = genome1->genome_id_;
			genome1_transmogrified->tsk_node_id_ = genome1->tsk_node_id_;
			
			genome2_transmogrified->genome_type_ = genome2->genome_type_;
			std::swap(genome2->mutrun_count_, genome2_transmogrified->mutrun_count_);
			std::swap(genome2->mutrun_length_, genome2_transmogrified->mutrun_length_);
			if (genome2->mutruns_ == genome2->run_buffer_)
			{
				// if the original genome points into its internal buffer, swap internal buffer contents and point into our internal buffer
				std::swap(genome2->run_buffer_, genome2_transmogrified->run_buffer_);
				genome2_transmogrified->mutruns_ = genome2_transmogrified->run_buffer_;
			}
			else
			{
				// if the original genome points to an external buffer, ignore the internal buffer and just swap external buffers
				std::swap(genome2->mutruns_, genome2_transmogrified->mutruns_);
			}
			genome2->mutruns_ = nullptr;
			genome2_transmogrified->tag_value_ = genome2->tag_value_;
			genome2_transmogrified->genome_id_ = genome2->genome_id_;
			genome2_transmogrified->tsk_node_id_ = genome2->tsk_node_id_;
			
			migrant_transmogrified->color_ = migrant->color_;
			migrant_transmogrified->color_red_ = migrant->color_red_;
			migrant_transmogrified->color_green_ = migrant->color_green_;
			migrant_transmogrified->color_blue_ = migrant->color_blue_;
			migrant_transmogrified->pedigree_id_ = migrant->pedigree_id_;
			migrant_transmogrified->pedigree_p1_ = migrant->pedigree_p1_;
			migrant_transmogrified->pedigree_p2_ = migrant->pedigree_p2_;
			migrant_transmogrified->pedigree_g1_ = migrant->pedigree_g1_;
			migrant_transmogrified->pedigree_g2_ = migrant->pedigree_g2_;
			migrant_transmogrified->pedigree_g3_ = migrant->pedigree_g3_;
			migrant_transmogrified->pedigree_g4_ = migrant->pedigree_g4_;
			migrant_transmogrified->tag_value_ = migrant->tag_value_;
			migrant_transmogrified->tagF_value_ = migrant->tagF_value_;
			migrant_transmogrified->fitness_scaling_ = migrant->fitness_scaling_;
			migrant_transmogrified->cached_fitness_UNSAFE_ = migrant->cached_fitness_UNSAFE_;	// never overridden in noWF models, so this is safe
			migrant_transmogrified->sex_ = migrant->sex_;
#ifdef SLIM_NONWF_ONLY
			migrant_transmogrified->age_ = migrant->age_;
#endif  // SLIM_NONWF_ONLY
			migrant_transmogrified->spatial_x_ = migrant->spatial_x_;
			migrant_transmogrified->spatial_y_ = migrant->spatial_y_;
			migrant_transmogrified->spatial_z_ = migrant->spatial_z_;
			
			// insert the transmogrified instances into ourselves
			if ((migrant_transmogrified->sex_ == IndividualSex::kFemale) && (parent_first_male_index_ < parent_subpop_size_))
			{
				// room has to be made for females by shifting the first male and changing the first male index
				Individual *backfill = parent_individuals_[parent_first_male_index_];
				
				parent_individuals_.push_back(backfill);
				parent_genomes_.push_back(parent_genomes_[parent_first_male_index_ * 2]);
				parent_genomes_.push_back(parent_genomes_[parent_first_male_index_ * 2 + 1]);
				backfill->index_ = parent_subpop_size_;
				
				parent_individuals_[parent_first_male_index_] = migrant_transmogrified;
				parent_genomes_[parent_first_male_index_ * 2] = genome1_transmogrified;
				parent_genomes_[parent_first_male_index_ * 2 + 1] = genome2_transmogrified;
				migrant_transmogrified->index_ = parent_first_male_index_;
				
				parent_subpop_size_++;
				parent_first_male_index_++;
			}
			else
			{
				// males and hermaphrodites can just be added to the end; so can females, if no males are present
				parent_individuals_.push_back(migrant_transmogrified);
				parent_genomes_.push_back(genome1_transmogrified);
				parent_genomes_.push_back(genome2_transmogrified);
				migrant_transmogrified->index_ = parent_subpop_size_;
				
				parent_subpop_size_++;
				if (migrant_transmogrified->sex_ == IndividualSex::kFemale)
					parent_first_male_index_++;
			}
			
			// set the migrant flag of the migrated individual; note this is not set if the individual was already in the destination subpop
			migrant_transmogrified->migrant_ = true;
			
			// track all the old and new pointers
			old_genome_ptrs.push_back(genome1);
			old_genome_ptrs.push_back(genome2);
			old_individual_ptrs.push_back(migrant);
			
			new_genome_ptrs.push_back(genome1_transmogrified);
			new_genome_ptrs.push_back(genome2_transmogrified);
			new_individual_ptrs.push_back(migrant_transmogrified);
		}
	}
	
	// Get rid of the EidosValue for the migrants, so we don't waste time below fixing it
	EidosValue_SP *migrants_arg_non_const = const_cast<EidosValue_SP *>(p_arguments.data() + 0);
	
	(*migrants_arg_non_const).reset();		// very very naughty; strictly, undefined behavior, but should be fine
	migrants_value = nullptr;
	
	migrant_count = (int)old_individual_ptrs.size();		// the number that actually migrated
	
	// Patch all references to the transmogrified genomes and individuals.  We do this by (1) zeroing out the patch_pointer_
	// ivar in all genomes and individuals that are accessible through EidosValues, (2) setting patch_pointer_ in all of
	// the genomes and individuals that need to be patched, and then (3) patching the pointers in all EidosValues.
	if (migrant_count)
	{
		// Zero out patch_pointer_ in all Genome objects referenced by EidosValues
		for (EidosValue_Object *genome_value : gEidosValue_Object_Genome_Registry)
		{
			if (genome_value->IsSingleton())
			{
				EidosValue_Object_singleton *genome_singleton = (EidosValue_Object_singleton *)genome_value;
				Genome *genome = (Genome *)genome_singleton->ObjectElementValue();
				
				genome->patch_pointer_ = nullptr;
			}
			else
			{
				EidosValue_Object_vector *genome_vector = (EidosValue_Object_vector *)genome_value;
				
				EidosObjectElement * const *vec_data = genome_vector->data();
				size_t vec_size = genome_vector->size();
				
				for (size_t vec_index = 0; vec_index < vec_size; ++vec_index)
				{
					Genome *genome = ((Genome *)vec_data[vec_index]);
					
					genome->patch_pointer_ = nullptr;
				}
			}
		}
		
		// Zero out patch_pointer_ in all Individual objects referenced by EidosValues
		for (EidosValue_Object *individual_value : gEidosValue_Object_Individual_Registry)
		{
			if (individual_value->IsSingleton())
			{
				EidosValue_Object_singleton *individual_singleton = (EidosValue_Object_singleton *)individual_value;
				Individual *individual = (Individual *)individual_singleton->ObjectElementValue();
				
				individual->patch_pointer_ = nullptr;
			}
			else
			{
				EidosValue_Object_vector *individual_vector = (EidosValue_Object_vector *)individual_value;
				
				EidosObjectElement * const *vec_data = individual_vector->data();
				size_t vec_size = individual_vector->size();
				
				for (size_t vec_index = 0; vec_index < vec_size; ++vec_index)
				{
					Individual *individual = ((Individual *)vec_data[vec_index]);
					
					individual->patch_pointer_ = nullptr;
				}
			}
		}
		
		// If there are any EidosValues of Genome class, set up patch_pointer_ in all genomes that need patching
		if (gEidosValue_Object_Genome_Registry.size())
		{
			for (int migrant_index = 0; migrant_index < migrant_count * 2; ++migrant_index)
			{
				Genome *old_genome = old_genome_ptrs[migrant_index];
				Genome *new_genome = new_genome_ptrs[migrant_index];
				
				old_genome->patch_pointer_ = new_genome;
			}
		}
		
		// If there are any EidosValues of Individual class, set up patch_pointer_ in all individuals that need patching
		if (gEidosValue_Object_Individual_Registry.size())
		{
			for (int migrant_index = 0; migrant_index < migrant_count; ++migrant_index)
			{
				Individual *old_individual = old_individual_ptrs[migrant_index];
				Individual *new_individual = new_individual_ptrs[migrant_index];
				
				old_individual->patch_pointer_ = new_individual;
			}
		}
		
		// Use patch_pointer_ to patch all Genome objects referenced by EidosValues
		for (EidosValue_Object *genome_value : gEidosValue_Object_Genome_Registry)
		{
			if (genome_value->IsSingleton())
			{
				EidosValue_Object_singleton *genome_singleton = (EidosValue_Object_singleton *)genome_value;
				Genome *genome = (Genome *)genome_singleton->ObjectElementValue();
				
				if (genome->patch_pointer_)
					genome_singleton->ObjectElementValue_Mutable() = genome->patch_pointer_;
			}
			else
			{
				EidosValue_Object_vector *genome_vector = (EidosValue_Object_vector *)genome_value;
				
				EidosObjectElement **vec_data = genome_vector->data();
				size_t vec_size = genome_vector->size();
				
				for (size_t vec_index = 0; vec_index < vec_size; ++vec_index)
				{
					Genome *genome = ((Genome *)vec_data[vec_index]);
					
					if (genome->patch_pointer_)
						vec_data[vec_index] = genome->patch_pointer_;
				}
			}
		}
		
		// Use patch_pointer_ to patch all Individual objects referenced by EidosValues
		for (EidosValue_Object *individual_value : gEidosValue_Object_Individual_Registry)
		{
			if (individual_value->IsSingleton())
			{
				EidosValue_Object_singleton *individual_singleton = (EidosValue_Object_singleton *)individual_value;
				Individual *individual = (Individual *)individual_singleton->ObjectElementValue();
				
				if (individual->patch_pointer_)
					individual_singleton->ObjectElementValue_Mutable() = individual->patch_pointer_;
			}
			else
			{
				EidosValue_Object_vector *individual_vector = (EidosValue_Object_vector *)individual_value;
				
				EidosObjectElement **vec_data = individual_vector->data();
				size_t vec_size = individual_vector->size();
				
				for (size_t vec_index = 0; vec_index < vec_size; ++vec_index)
				{
					Individual *individual = ((Individual *)vec_data[vec_index]);
					
					if (individual->patch_pointer_)
						vec_data[vec_index] = individual->patch_pointer_;
				}
			}
		}
		
		// Invalidate interactions; we just do this for all subpops, for now, rather than trying to
		// selectively invalidate only the subpops involved in the migrations that occurred
		auto &interactionTypes = sim.InteractionTypes();
		
		for (auto int_type : interactionTypes)
			int_type.second->Invalidate();
	}
	
	// Finally, dispose of the old individuals and genomes, in their respective subpops; there should be no references to
	// them any more, since we have removed them from their subpopulation and have patched out all EidosValue references.
	for (Genome *genome : old_genome_ptrs)
		(genome->subpop_)->FreeSubpopGenome(genome);
	
	for (Individual *individual : old_individual_ptrs)
	{
		EidosObjectPool *pool = (individual->subpopulation_).individual_pool_;
		
		individual->~Individual();
		pool->DisposeChunk(const_cast<Individual *>(individual));
	}
	
	return gStaticEidosValueVOID;
}

#endif  // SLIM_NONWF_ONLY

#ifdef SLIM_WF_ONLY
//	*********************	- (void)setMigrationRates(object sourceSubpops, numeric rates)
//
EidosValue_SP Subpopulation::ExecuteMethod_setMigrationRates(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): method -setMigrationRates() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *sourceSubpops_value = p_arguments[0].get();
	EidosValue *rates_value = p_arguments[1].get();
	
	int source_subpops_count = sourceSubpops_value->Count();
	int rates_count = rates_value->Count();
	std::vector<slim_objectid_t> subpops_seen;
	
	if (source_subpops_count != rates_count)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() requires sourceSubpops and rates to be equal in size." << EidosTerminate();
	
	for (int value_index = 0; value_index < source_subpops_count; ++value_index)
	{
		SLiMSim &sim = population_.sim_;
		EidosObjectElement *source_subpop = SLiM_ExtractSubpopulationFromEidosValue_io(sourceSubpops_value, value_index, sim, "setMigrationRates()");
		slim_objectid_t source_subpop_id = ((Subpopulation *)(source_subpop))->subpopulation_id_;
		
		if (source_subpop_id == subpopulation_id_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() does not allow migration to be self-referential (originating within the destination subpopulation)." << EidosTerminate();
		if (std::find(subpops_seen.begin(), subpops_seen.end(), source_subpop_id) != subpops_seen.end())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() two rates set for subpopulation p" << source_subpop_id << "." << EidosTerminate();
		
		double migrant_fraction = rates_value->FloatAtIndex(value_index, nullptr);
		
		population_.SetMigration(*this, source_subpop_id, migrant_fraction);
		subpops_seen.emplace_back(source_subpop_id);
	}
	
	return gStaticEidosValueVOID;
}
#endif	// SLIM_WF_ONLY

//	*********************	– (logical)pointInBounds(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointInBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	int dimensionality = sim.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): pointInBounds() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count == 0)
		return gStaticEidosValue_Logical_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): pointInBounds() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// singleton case, get it out of the way
		double x = point_value->FloatAtIndex(0, nullptr);
		return ((x >= bounds_x0_) && (x <= bounds_x1_)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	}
	
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	
	if (point_count == 1)
	{
		// single-point case, do it separately to return a singleton logical value, and handle the multi-point case more quickly
		switch (dimensionality)
		{
			case 1:
			{
				double x = point_buf[0];
				return ((x >= bounds_x0_) && (x <= bounds_x1_))
					? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			case 2:
			{
				double x = point_buf[0];
				double y = point_buf[1];
				return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_))
					? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			case 3:
			{
				double x = point_buf[0];
				double y = point_buf[1];
				double z = point_buf[2];
				return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_) && (z >= bounds_z0_) && (z <= bounds_z1_))
					? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			default:
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): (internal error) unrecognized dimensionality." << EidosTerminate();
		}
	}
	
	// multiple-point case, new in SLiM 3
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(point_count);
	
	switch (dimensionality)
	{
		case 1:
			for (int point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				eidos_logical_t in_bounds = ((x >= bounds_x0_) && (x <= bounds_x1_));
				
				logical_result->set_logical_no_check(in_bounds, point_index);
			}
			break;
		case 2:
			for (int point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				double y = *(point_buf++);
				eidos_logical_t in_bounds = ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_));
				
				logical_result->set_logical_no_check(in_bounds, point_index);
			}
			break;
		case 3:
			for (int point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				double y = *(point_buf++);
				double z = *(point_buf++);
				eidos_logical_t in_bounds = ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_) && (z >= bounds_z0_) && (z <= bounds_z1_));
				
				logical_result->set_logical_no_check(in_bounds, point_index);
			}
			break;
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(logical_result);
}			

//	*********************	– (float)pointReflected(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointReflected(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	int dimensionality = sim.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): pointReflected() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): pointReflected() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// Handle the singleton separately, so we can handle the non-singleton case more quickly
		double x = point_value->FloatAtIndex(0, nullptr);
		
		while (true)
		{
			if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
			else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
			else break;
		}
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
	}
	
	// non-singleton general case
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(value_count);
	int value_index = 0;
	
	switch (dimensionality)
	{
		case 1:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				while (true)
				{
					if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
					else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
					else break;
				}
				float_result->set_float_no_check(x, value_index++);
			}
			break;
		case 2:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				while (true)
				{
					if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
					else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
					else break;
				}
				float_result->set_float_no_check(x, value_index++);
				
				double y = *(point_buf++);
				while (true)
				{
					if (y < bounds_y0_) y = bounds_y0_ + (bounds_y0_ - y);
					else if (y > bounds_y1_) y = bounds_y1_ - (y - bounds_y1_);
					else break;
				}
				float_result->set_float_no_check(y, value_index++);
			}
			break;
		case 3:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				while (true)
				{
					if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
					else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
					else break;
				}
				float_result->set_float_no_check(x, value_index++);
				
				double y = *(point_buf++);
				while (true)
				{
					if (y < bounds_y0_) y = bounds_y0_ + (bounds_y0_ - y);
					else if (y > bounds_y1_) y = bounds_y1_ - (y - bounds_y1_);
					else break;
				}
				float_result->set_float_no_check(y, value_index++);
				
				double z = *(point_buf++);
				while (true)
				{
					if (z < bounds_z0_) z = bounds_z0_ + (bounds_z0_ - z);
					else if (z > bounds_z1_) z = bounds_z1_ - (z - bounds_z1_);
					else break;
				}
				float_result->set_float_no_check(z, value_index++);
			}
			break;
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(float_result);
}			

//	*********************	– (float)pointStopped(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointStopped(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	int dimensionality = sim.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): pointStopped() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): pointStopped() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// Handle the singleton separately, so we can handle the non-singleton case more quickly
		double x = point_value->FloatAtIndex(0, nullptr);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::max(bounds_x0_, std::min(bounds_x1_, x))));
	}
	
	// non-singleton general case
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(value_count);
	int value_index = 0;
	
	switch (dimensionality)
	{
		case 1:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				float_result->set_float_no_check(std::max(bounds_x0_, std::min(bounds_x1_, x)), value_index++);
			}
			break;
		case 2:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				float_result->set_float_no_check(std::max(bounds_x0_, std::min(bounds_x1_, x)), value_index++);
				
				double y = *(point_buf++);
				float_result->set_float_no_check(std::max(bounds_y0_, std::min(bounds_y1_, y)), value_index++);
			}
			break;
		case 3:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				float_result->set_float_no_check(std::max(bounds_x0_, std::min(bounds_x1_, x)), value_index++);
				
				double y = *(point_buf++);
				float_result->set_float_no_check(std::max(bounds_y0_, std::min(bounds_y1_, y)), value_index++);
				
				double z = *(point_buf++);
				float_result->set_float_no_check(std::max(bounds_z0_, std::min(bounds_z1_, z)), value_index++);
			}
			break;
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(float_result);
}			

//	*********************	– (float)pointPeriodic(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointPeriodic(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	int dimensionality = sim.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() cannot be called in non-spatial simulations." << EidosTerminate();
	
	bool periodic_x, periodic_y, periodic_z;
	
	sim.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
	
	if (!periodic_x && !periodic_y && !periodic_z)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() cannot be called when no periodic spatial dimension has been set up." << EidosTerminate();
	
	if (value_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// Handle the singleton separately, so we can handle the non-singleton case more quickly
		double x = point_value->FloatAtIndex(0, nullptr);
		
		if (periodic_x)
		{
			while (x < 0.0)			x += bounds_x1_;
			while (x > bounds_x1_)	x -= bounds_x1_;
		}
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
	}
	
	// non-singleton general case
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(value_count);
	int value_index = 0;
	
	// Wrap coordinates; note that we assume here that bounds_x0_ == bounds_y0_ == bounds_z0_ == 0,
	// which is enforced when periodic boundary conditions are set, in setSpatialBounds().  Note also
	// that we don't use fmod(); maybe we should, rather than looping, but then we have to worry about
	// sign, and since new spatial points are probably usually close to being in bounds, these loops
	// probably won't execute more than once anyway...
	switch (dimensionality)
	{
		case 1:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				if (periodic_x)
				{
					while (x < 0.0)			x += bounds_x1_;
					while (x > bounds_x1_)	x -= bounds_x1_;
				}
				float_result->set_float_no_check(x, value_index++);
			}
			break;
		case 2:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				if (periodic_x)
				{
					while (x < 0.0)			x += bounds_x1_;
					while (x > bounds_x1_)	x -= bounds_x1_;
				}
				float_result->set_float_no_check(x, value_index++);
				
				double y = *(point_buf++);
				if (periodic_y)
				{
					while (y < 0.0)			y += bounds_y1_;
					while (y > bounds_y1_)	y -= bounds_y1_;
				}
				float_result->set_float_no_check(y, value_index++);
			}
			break;
		case 3:
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = *(point_buf++);
				if (periodic_x)
				{
					while (x < 0.0)			x += bounds_x1_;
					while (x > bounds_x1_)	x -= bounds_x1_;
				}
				float_result->set_float_no_check(x, value_index++);
				
				double y = *(point_buf++);
				if (periodic_y)
				{
					while (y < 0.0)			y += bounds_y1_;
					while (y > bounds_y1_)	y -= bounds_y1_;
				}
				float_result->set_float_no_check(y, value_index++);
				
				double z = *(point_buf++);
				if (periodic_z)
				{
					while (z < 0.0)			z += bounds_z1_;
					while (z > bounds_z1_)	z -= bounds_z1_;
				}
				float_result->set_float_no_check(z, value_index++);
			}
			break;
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(float_result);
}			

//	*********************	– (float)pointUniform([integer$ n = 1])
//
EidosValue_SP Subpopulation::ExecuteMethod_pointUniform(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): pointUniform() cannot be called in non-spatial simulations." << EidosTerminate();
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t point_count = n_value->IntAtIndex(0, nullptr);
	
	if (point_count < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): pointUniform() requires n >= 0." << EidosTerminate();
	if (point_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int64_t length_out = point_count * dimensionality;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(length_out);
	EidosValue_SP result_SP = EidosValue_SP(float_result);
	int value_index = 0;
	
	switch (dimensionality)
	{
		case 1:
		{
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * (bounds_x1_ - bounds_x0_) + bounds_x0_, value_index++);
			}
			break;
		}
		case 2:
		{
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * (bounds_x1_ - bounds_x0_) + bounds_x0_, value_index++);
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * (bounds_y1_ - bounds_y0_) + bounds_y0_, value_index++);
			}
			break;
		}
		case 3:
		{
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * (bounds_x1_ - bounds_x0_) + bounds_x0_, value_index++);
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * (bounds_y1_ - bounds_y0_) + bounds_y0_, value_index++);
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * (bounds_z1_ - bounds_z0_) + bounds_z0_, value_index++);
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return result_SP;
}			

#ifdef SLIM_WF_ONLY
//	*********************	- (void)setCloningRate(numeric rate)
//
EidosValue_SP Subpopulation::ExecuteMethod_setCloningRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): method -setCloningRate() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *rate_value = p_arguments[0].get();
	
	int value_count = rate_value->Count();
	
	if (sex_enabled_)
	{
		// SEX ONLY: either one or two values may be specified; if two, it is female at 0, male at 1
		if ((value_count < 1) || (value_count > 2))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires a rate vector containing either one or two values, in sexual simulations." << EidosTerminate();
		
		double female_cloning_fraction = rate_value->FloatAtIndex(0, nullptr);
		double male_cloning_fraction = (value_count == 2) ? rate_value->FloatAtIndex(1, nullptr) : female_cloning_fraction;
		
		if ((female_cloning_fraction < 0.0) || (female_cloning_fraction > 1.0) || std::isnan(female_cloning_fraction))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << EidosStringForFloat(female_cloning_fraction) << " supplied)." << EidosTerminate();
		if ((male_cloning_fraction < 0.0) || (male_cloning_fraction > 1.0) || std::isnan(male_cloning_fraction))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << EidosStringForFloat(male_cloning_fraction) << " supplied)." << EidosTerminate();
		
		female_clone_fraction_ = female_cloning_fraction;
		male_clone_fraction_ = male_cloning_fraction;
	}
	else
	{
		// ASEX ONLY: only one value may be specified
		if (value_count != 1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires a rate vector containing exactly one value, in asexual simulations.." << EidosTerminate();
		
		double cloning_fraction = rate_value->FloatAtIndex(0, nullptr);
		
		if ((cloning_fraction < 0.0) || (cloning_fraction > 1.0) || std::isnan(cloning_fraction))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << EidosStringForFloat(cloning_fraction) << " supplied)." << EidosTerminate();
		
		female_clone_fraction_ = cloning_fraction;
		male_clone_fraction_ = cloning_fraction;
	}
	
	return gStaticEidosValueVOID;
}			
#endif	// SLIM_WF_ONLY

#ifdef SLIM_WF_ONLY
//	*********************	- (void)setSelfingRate(numeric$ rate)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSelfingRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): method -setSelfingRate() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *rate_value = p_arguments[0].get();
	
	double selfing_fraction = rate_value->FloatAtIndex(0, nullptr);
	
	if ((selfing_fraction != 0.0) && sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): setSelfingRate() is limited to the hermaphroditic case, and cannot be called in sexual simulations." << EidosTerminate();
	
	if ((selfing_fraction < 0.0) || (selfing_fraction > 1.0) || std::isnan(selfing_fraction))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): setSelfingRate() requires a selfing fraction within [0,1] (" << EidosStringForFloat(selfing_fraction) << " supplied)." << EidosTerminate();
	
	selfing_fraction_ = selfing_fraction;
	
	return gStaticEidosValueVOID;
}			
#endif	// SLIM_WF_ONLY

#ifdef SLIM_WF_ONLY
//	*********************	- (void)setSexRatio(float$ sexRatio)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSexRatio(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): method -setSexRatio() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *sexRatio_value = p_arguments[0].get();
	
	// SetSexRatio() can only be called when the child generation has not yet been generated.  It sets the sex ratio on the child generation,
	// and then that sex ratio takes effect when the children are generated from the parents in EvolveSubpopulation().
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() called when the child generation was valid." << EidosTerminate();
	
	// SEX ONLY
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() is limited to the sexual case, and cannot be called in asexual simulations." << EidosTerminate();
	
	double sex_ratio = sexRatio_value->FloatAtIndex(0, nullptr);
	
	if ((sex_ratio < 0.0) || (sex_ratio > 1.0) || std::isnan(sex_ratio))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() requires a sex ratio within [0,1] (" << EidosStringForFloat(sex_ratio) << " supplied)." << EidosTerminate();
	
	// After we change the subpop sex ratio, we need to generate new children genomes to fit the new requirements
	child_sex_ratio_ = sex_ratio;
	GenerateChildrenToFitWF();
	
	return gStaticEidosValueVOID;
}
#endif	// SLIM_WF_ONLY

//	*********************	– (void)setSpatialBounds(numeric position)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSpatialBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *position_value = p_arguments[0].get();
	
	SLiMSim &sim = population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	int value_count = position_value->Count();
	
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
			bounds_x0_ = position_value->FloatAtIndex(0, nullptr);	bounds_x1_ = position_value->FloatAtIndex(1, nullptr);
			
			if (bounds_x1_ <= bounds_x0_)
				bad_bounds = true;
			if (periodic_x && (bounds_x0_ != 0.0))
				bad_periodic_bounds = true;
			
			break;
		case 2:
			bounds_x0_ = position_value->FloatAtIndex(0, nullptr);	bounds_x1_ = position_value->FloatAtIndex(2, nullptr);
			bounds_y0_ = position_value->FloatAtIndex(1, nullptr);	bounds_y1_ = position_value->FloatAtIndex(3, nullptr);
			
			if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_))
				bad_bounds = true;
			if ((periodic_x && (bounds_x0_ != 0.0)) || (periodic_y && (bounds_y0_ != 0.0)))
				bad_periodic_bounds = true;
			
			break;
		case 3:
			bounds_x0_ = position_value->FloatAtIndex(0, nullptr);	bounds_x1_ = position_value->FloatAtIndex(3, nullptr);
			bounds_y0_ = position_value->FloatAtIndex(1, nullptr);	bounds_y1_ = position_value->FloatAtIndex(4, nullptr);
			bounds_z0_ = position_value->FloatAtIndex(2, nullptr);	bounds_z1_ = position_value->FloatAtIndex(5, nullptr);
			
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
	
	return gStaticEidosValueVOID;
}			

#ifdef SLIM_WF_ONLY
//	*********************	- (void)setSubpopulationSize(integer$ size)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSubpopulationSize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (population_.sim_.ModelType() == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSubpopulationSize): method -setSubpopulationSize() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *size_value = p_arguments[0].get();
	
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(size_value->IntAtIndex(0, nullptr));
	
	population_.SetSize(*this, subpop_size);
	
	return gStaticEidosValueVOID;
}
#endif	// SLIM_WF_ONLY

#ifdef SLIM_NONWF_ONLY
//	*********************	- (void)removeSubpopulation()
//
EidosValue_SP Subpopulation::ExecuteMethod_removeSubpopulation(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMSim &sim = population_.sim_;
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSubpopulation): method -removeSubpopulation() is not available in WF models." << EidosTerminate();
	if ((sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (sim.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSubpopulation): method -removeSubpopulation() must be called directly from an early() or late() event." << EidosTerminate();
	
	population_.RemoveSubpopulation(*this);
	
	return gStaticEidosValueVOID;
}
#endif  // SLIM_NONWF_ONLY

//	*********************	- (float)cachedFitness(Ni indices)
//
EidosValue_SP Subpopulation::ExecuteMethod_cachedFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *indices_value = p_arguments[0].get();
	
#ifdef SLIM_WF_ONLY
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may only be called when the parental generation is active (before or during offspring generation)." << EidosTerminate();
#endif	// SLIM_WF_ONLY
	
	SLiMSim &sim = population_.sim_;
	
	if (sim.ModelType() == SLiMModelType::kModelTypeWF)
	{
		if (sim.GenerationStage() == SLiMGenerationStage::kWFStage6CalculateFitness)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may not be called while fitness values are being calculated." << EidosTerminate();
		if (sim.GenerationStage() == SLiMGenerationStage::kWFStage5ExecuteLateScripts)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may not be called during late() events in WF models, since the new generation does not yet have fitness values (which are calculated immediately after late() events have executed)." << EidosTerminate();
	}
	else
	{
		if (sim.GenerationStage() == SLiMGenerationStage::kNonWFStage3CalculateFitness)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may not be called while fitness values are being calculated." << EidosTerminate();
		// in nonWF models uncalculated fitness values for new individuals are guaranteed to be NaN, so there is no need for a check here
	}
	
	bool do_all_indices = (indices_value->Type() == EidosValueType::kValueNULL);
	slim_popsize_t index_count = (do_all_indices ? parent_subpop_size_ : SLiMCastToPopsizeTypeOrRaise(indices_value->Count()));
	
	if (index_count == 1)
	{
		slim_popsize_t index = 0;
		
		if (!do_all_indices)
		{
			index = SLiMCastToPopsizeTypeOrRaise(indices_value->IntAtIndex(0, nullptr));
			
			if (index >= parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() index " << index << " out of range." << EidosTerminate();
		}
		
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
		double fitness = (individual_cached_fitness_OVERRIDE_ ? individual_cached_fitness_OVERRIDE_value_ : parent_individuals_[index]->cached_fitness_UNSAFE_);
#else
		double fitness = parent_individuals_[index]->cached_fitness_UNSAFE_;
#endif
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fitness));
	}
	else
	{
		EidosValue_Float_vector *float_return = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(index_count);
		EidosValue_SP result_SP = EidosValue_SP(float_return);
		
		for (slim_popsize_t value_index = 0; value_index < index_count; value_index++)
		{
			slim_popsize_t index = value_index;
			
			if (!do_all_indices)
			{
				index = SLiMCastToPopsizeTypeOrRaise(indices_value->IntAtIndex(value_index, nullptr));
				
				if (index >= parent_subpop_size_)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() index " << index << " out of range." << EidosTerminate();
			}
			
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
			double fitness = (individual_cached_fitness_OVERRIDE_ ? individual_cached_fitness_OVERRIDE_value_ : parent_individuals_[index]->cached_fitness_UNSAFE_);
#else
			double fitness = parent_individuals_[index]->cached_fitness_UNSAFE_;
#endif
			
			float_return->set_float_no_check(fitness, value_index);
		}
		
		return result_SP;
	}
}

//  *********************	– (No<Individual>)sampleIndividuals(integer$ size, [logical$ replace = F], [No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_sampleIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This method is patterned closely upon Eidos_ExecuteFunction_sample(), but with no weights vector, and with various ways to narrow down the candidate pool
	EidosValue_SP result_SP(nullptr);
	
	int64_t sample_size = p_arguments[0]->IntAtIndex(0, nullptr);
	bool replace = p_arguments[1]->LogicalAtIndex(0, nullptr);
	int x_count = parent_subpop_size_;
	
	if (sample_size < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): sampleIndividuals() requires a sample size >= 0 (" << sample_size << " supplied)." << EidosTerminate(nullptr);
	if ((sample_size == 0) || (x_count == 0))
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// a specific individual may be excluded
	EidosValue *exclude_value = p_arguments[2].get();
	Individual *excluded_individual = nullptr;
	slim_popsize_t excluded_index = -1;
	
	if (exclude_value->Type() != EidosValueType::kValueNULL)
		excluded_individual = (Individual *)exclude_value->ObjectElementAtIndex(0, nullptr);
	
	if (excluded_individual)
	{
		if (&excluded_individual->subpopulation_ != this)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): the excluded individual must belong to the subpopulation being sampled." << EidosTerminate(nullptr);
		if (excluded_individual->index_ == -1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): the excluded individual must be a valid, visible individual (not a newly generated child)." << EidosTerminate(nullptr);
		
		excluded_index = excluded_individual->index_;
	}
	
	// a sex may be specified
	EidosValue *sex_value = p_arguments[3].get();
	IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() != EidosValueType::kValueNULL)
	{
		std::string sex_string = sex_value->StringAtIndex(0, nullptr);
		
		if (sex_string == "M")			sex = IndividualSex::kMale;
		else if (sex_string == "F")		sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): unrecognized value for sex in sampleIndividuals(); sex must be 'F', 'M', or NULL." << EidosTerminate(nullptr);
		
		if (!sex_enabled_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): sex must be NULL in non-sexual models." << EidosTerminate(nullptr);
	}
	
	// a tag value may be specified
	EidosValue *tag_value = p_arguments[4].get();
	bool tag_specified = (tag_value->Type() != EidosValueType::kValueNULL);
	slim_usertag_t tag = (tag_specified ? tag_value->IntAtIndex(0, nullptr) : 0);
	
	// an age min or max may be specified in nonWF models
	EidosValue *ageMin_value = p_arguments[5].get();
	EidosValue *ageMax_value = p_arguments[6].get();
	bool ageMin_specified = (ageMin_value->Type() != EidosValueType::kValueNULL);
	bool ageMax_specified = (ageMax_value->Type() != EidosValueType::kValueNULL);
#ifdef SLIM_NONWF_ONLY
	int64_t ageMin = (ageMin_specified ? ageMin_value->IntAtIndex(0, nullptr) : -1);
	int64_t ageMax = (ageMax_specified ? ageMax_value->IntAtIndex(0, nullptr) : INT64_MAX);
	
	if ((ageMin_specified || ageMax_specified) && (population_.sim_.ModelType() != SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): ageMin and ageMax may only be specified in nonWF models." << EidosTerminate(nullptr);
#endif  // SLIM_NONWF_ONLY
	
	// a migrant value may be specified
	EidosValue *migrant_value = p_arguments[7].get();
	bool migrant_specified = (migrant_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t migrant = (migrant_specified ? migrant_value->LogicalAtIndex(0, nullptr) : false);
	
	// determine the range the sample will be drawn from; this does not take into account tag or ageMin/ageMax
	int first_candidate_index, last_candidate_index, candidate_count;
	
	if (sex == IndividualSex::kUnspecified)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = parent_subpop_size_;
	}
	else if (sex == IndividualSex::kFemale)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_first_male_index_ - 1;
		candidate_count = parent_first_male_index_;
	}
	else // if (sex == IndividualSex::kMale)
	{
		first_candidate_index = parent_first_male_index_;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = (last_candidate_index - first_candidate_index) + 1;
	}
	
	if ((excluded_index >= first_candidate_index) && (excluded_index <= last_candidate_index))
		candidate_count--;
	else
		excluded_index = -1;
	
	if (!tag_specified && !ageMin_specified && !ageMax_specified && !migrant_specified)
	{
		// we're in the simple case of no specifed tag/ageMin/ageMax/migrant, so maybe we can handle it quickly
		if (candidate_count == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		else if (!replace && (candidate_count < sample_size))
			sample_size = candidate_count;
		
		if (sample_size == 1)
		{
			// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
			int sample_index = (int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, candidate_count) + first_candidate_index;
			
			if ((excluded_index != -1) && (sample_index >= excluded_index))
				sample_index++;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(parent_individuals_[sample_index], gSLiM_Individual_Class));
		}
		else if (replace)
		{
			// with replacement, we can just do a series of independent draws
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get())->resize_no_initialize(sample_size);
			
			for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
			{
				int sample_index = (int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, candidate_count) + first_candidate_index;
				
				if ((excluded_index != -1) && (sample_index >= excluded_index))
					sample_index++;
				
				result->set_object_element_no_check_NORR(parent_individuals_[sample_index], samples_generated);
			}
			
			return result_SP;
		}
		else if (sample_size == 2)
		{
			// a sample size of two without replacement is expected to be common (interacting pairs) so optimize for it
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get())->resize_no_initialize(sample_size);
			
			int sample_index1 = (int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, candidate_count) + first_candidate_index;
			
			if ((excluded_index != -1) && (sample_index1 >= excluded_index))
				sample_index1++;
			
			result->set_object_element_no_check_NORR(parent_individuals_[sample_index1], 0);
			
			int sample_index2;
			
			do
			{
				sample_index2 = (int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, candidate_count) + first_candidate_index;
				
				if ((excluded_index != -1) && (sample_index2 >= excluded_index))
					sample_index2++;
			}
			while (sample_index2 == sample_index1);
			
			result->set_object_element_no_check_NORR(parent_individuals_[sample_index2], 1);
			
			return result_SP;
		}
	}
	
	// BCH 12/17/2019: Adding an optimization here.  It is common to call sampleIndividuals() on a large subpopulation
	// to draw a single mate in a reproduction() callback.  It takes a long time to build the index vector below; let's
	// try optimizing that specific case by trying a few random individuals to see if we get lucky.  If we don't,
	// we'll drop through to the base case code, without having lost much time.  At a guess, I'm requiring a candidate
	// count of at least 30 since with a smaller size than that building the index vector won't hurt much anyway.
	// I'm limiting this to 20 tries, so we don't spend too much time on it; the ideal limit will of course depend on
	// the number of candidates versus the number of *valid* candidates, and there's no way to know.
	if ((sample_size == 1) && (candidate_count >= 30))
	{
		for (int try_count = 0; try_count < 20; ++try_count)
		{
			int sample_index = (int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, candidate_count) + first_candidate_index;
			
			if ((excluded_index != -1) && (sample_index >= excluded_index))
				sample_index++;
			
			Individual *candidate = parent_individuals_[sample_index];
			
			if (tag_specified && (candidate->tag_value_ != tag))
				continue;
			if (migrant_specified && (candidate->migrant_ != migrant))
				continue;
#ifdef SLIM_NONWF_ONLY
			if (ageMin_specified && (candidate->age_ < ageMin))
				continue;
			if (ageMax_specified && (candidate->age_ > ageMax))
				continue;
#endif  // SLIM_NONWF_ONLY
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(parent_individuals_[sample_index], gSLiM_Individual_Class));
		}
	}
	
	// base case
	{
		// get indices of individuals; we sample from this vector and then look up the corresponding individual
		std::vector<int> index_vector;
		
		if (!tag_specified && !ageMin_specified && !ageMax_specified && !migrant_specified)
		{
			if (excluded_index == -1)
			{
				for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
					index_vector.emplace_back(value_index);
			}
			else
			{
				for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
				{
					if (value_index != excluded_index)
						index_vector.emplace_back(value_index);
				}
			}
		}
		else
		{
			for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
			{
				Individual *candidate = parent_individuals_[value_index];
				
				if (tag_specified && (candidate->tag_value_ != tag))
					continue;
				if (migrant_specified && (candidate->migrant_ != migrant))
					continue;
#ifdef SLIM_NONWF_ONLY
				if (ageMin_specified && (candidate->age_ < ageMin))
					continue;
				if (ageMax_specified && (candidate->age_ > ageMax))
					continue;
#endif  // SLIM_NONWF_ONLY
				if (value_index == excluded_index)
					continue;
				
				index_vector.emplace_back(value_index);
			}
		}
		
		candidate_count = (int)index_vector.size();
		
		if (candidate_count == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		else if (!replace && (candidate_count < sample_size))
			sample_size = candidate_count;
		
		// do the sampling
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get())->resize_no_initialize(sample_size);
		int64_t contender_count = candidate_count;
		
		for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
		{
#if DEBUG
			// this error should never occur, since we checked the count above
			if (contender_count <= 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): (internal error) sampleIndividuals() ran out of eligible individuals from which to sample." << EidosTerminate(nullptr);		// CODE COVERAGE: This is dead code
#endif
			
			int rose_index = (int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, (uint32_t)contender_count);
			
			result->set_object_element_no_check_NORR(parent_individuals_[index_vector[rose_index]], samples_generated);
			
			if (!replace)
			{
				index_vector[rose_index] = index_vector.back();
				index_vector.resize(--contender_count);
			}
		}
	}
	
	return result_SP;
}

//  *********************	– (object<Individual>)subsetIndividuals([No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_subsetIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This method is patterned closely upon ExecuteMethod_sampleIndividuals(), but without the sampling aspect
	EidosValue_SP result_SP(nullptr);
	
	int x_count = parent_subpop_size_;
	
	if (x_count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// a specific individual may be excluded
	EidosValue *exclude_value = p_arguments[0].get();
	Individual *excluded_individual = nullptr;
	slim_popsize_t excluded_index = -1;
	
	if (exclude_value->Type() != EidosValueType::kValueNULL)
		excluded_individual = (Individual *)exclude_value->ObjectElementAtIndex(0, nullptr);
	
	if (excluded_individual)
	{
		if (&excluded_individual->subpopulation_ != this)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): the excluded individual must belong to the subpopulation being subset." << EidosTerminate(nullptr);
		if (excluded_individual->index_ == -1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): the excluded individual must be a valid, visible individual (not a newly generated child)." << EidosTerminate(nullptr);
		
		excluded_index = excluded_individual->index_;
	}
	
	// a sex may be specified
	EidosValue *sex_value = p_arguments[1].get();
	IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() != EidosValueType::kValueNULL)
	{
		std::string sex_string = sex_value->StringAtIndex(0, nullptr);
		
		if (sex_string == "M")			sex = IndividualSex::kMale;
		else if (sex_string == "F")		sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): unrecognized value for sex in subsetIndividuals(); sex must be 'F', 'M', or NULL." << EidosTerminate(nullptr);
		
		if (!sex_enabled_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): sex must be NULL in non-sexual models." << EidosTerminate(nullptr);
	}
	
	// a tag value may be specified
	EidosValue *tag_value = p_arguments[2].get();
	bool tag_specified = (tag_value->Type() != EidosValueType::kValueNULL);
	slim_usertag_t tag = (tag_specified ? tag_value->IntAtIndex(0, nullptr) : 0);
	
	// an age min or max may be specified in nonWF models
	EidosValue *ageMin_value = p_arguments[3].get();
	EidosValue *ageMax_value = p_arguments[4].get();
	bool ageMin_specified = (ageMin_value->Type() != EidosValueType::kValueNULL);
	bool ageMax_specified = (ageMax_value->Type() != EidosValueType::kValueNULL);
#ifdef SLIM_NONWF_ONLY
	int64_t ageMin = (ageMin_specified ? ageMin_value->IntAtIndex(0, nullptr) : -1);
	int64_t ageMax = (ageMax_specified ? ageMax_value->IntAtIndex(0, nullptr) : INT64_MAX);
	
	if ((ageMin_specified || ageMax_specified) && (population_.sim_.ModelType() != SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): ageMin and ageMax may only be specified in nonWF models." << EidosTerminate(nullptr);
#endif  // SLIM_NONWF_ONLY
	
	// a migrant value may be specified
	EidosValue *migrant_value = p_arguments[5].get();
	bool migrant_specified = (migrant_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t migrant = (migrant_specified ? migrant_value->LogicalAtIndex(0, nullptr) : false);
	
	// determine the range the sample will be drawn from; this does not take into account tag or ageMin/ageMax
	int first_candidate_index, last_candidate_index, candidate_count;
	
	if (sex == IndividualSex::kUnspecified)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = parent_subpop_size_;
	}
	else if (sex == IndividualSex::kFemale)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_first_male_index_ - 1;
		candidate_count = parent_first_male_index_;
	}
	else // if (sex == IndividualSex::kMale)
	{
		first_candidate_index = parent_first_male_index_;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = (last_candidate_index - first_candidate_index) + 1;
	}
	
	if ((excluded_index >= first_candidate_index) && (excluded_index <= last_candidate_index))
		candidate_count--;
	else
		excluded_index = -1;
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get())->reserve(candidate_count);
	
	if (!tag_specified && !ageMin_specified && !ageMax_specified && !migrant_specified)
	{
		// usually there will be no specifed tag/ageMin/ageMax, so handle it more quickly
		if (excluded_index == -1)
		{
			for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
				result->push_object_element_no_check_NORR(parent_individuals_[value_index]);
		}
		else
		{
			for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
			{
				if (value_index == excluded_index)
					continue;
				
				result->push_object_element_no_check_NORR(parent_individuals_[value_index]);
			}
		}
	}
	else
	{
		// this is the full case, a bit slower
		for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
		{
			Individual *candidate = parent_individuals_[value_index];
			
			if (tag_specified && (candidate->tag_value_ != tag))
				continue;
			if (migrant_specified && (candidate->migrant_ != migrant))
				continue;
#ifdef SLIM_NONWF_ONLY
			if (ageMin_specified && (candidate->age_ < ageMin))
				continue;
			if (ageMax_specified && (candidate->age_ > ageMax))
				continue;
#endif  // SLIM_NONWF_ONLY
			if (value_index == excluded_index)
				continue;
			
			result->push_object_element_no_check_NORR(parent_individuals_[value_index]);
		}
	}
	
	return result_SP;
}

//	*********************	– (void)defineSpatialMap(string$ name, string$ spatiality, Ni gridSize, numeric values, [logical$ interpolate = F], [Nif valueRange = NULL], [Ns colors = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_defineSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *name_value = p_arguments[0].get();
	EidosValue *spatiality_value = p_arguments[1].get();
	EidosValue *gridSize_value = p_arguments[2].get();
	EidosValue *values = p_arguments[3].get();
	EidosValue *interpolate_value = p_arguments[4].get();
	EidosValue *value_range = p_arguments[5].get();
	EidosValue *colors = p_arguments[6].get();
	
	std::string map_name = name_value->StringAtIndex(0, nullptr);
	std::string spatiality_string = spatiality_value->StringAtIndex(0, nullptr);
	bool interpolate = interpolate_value->LogicalAtIndex(0, nullptr);
	
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
	
	int values_dimcount = values->DimensionCount();
	const int64_t *values_dim = values->Dimensions();
	
	if ((values_dimcount != 1) && (values_dimcount != map_spatiality))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() the dimensionality of the supplied matrix/array does not match the spatiality defined for the map." << EidosTerminate();
	
	int64_t map_size = 1;
	int64_t dimension_sizes[3];
	
	if (gridSize_value->Type() != EidosValueType::kValueNULL)
	{
		// A gridSize argument was supplied, so it must match the spatiality of the map and the size of the data in values
		if (gridSize_value->Count() != map_spatiality)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() gridSize must match the spatiality defined for the map." << EidosTerminate();
		
		for (int dimension_index = 0; dimension_index < map_spatiality; ++dimension_index)
		{
			int64_t dimension_size = gridSize_value->IntAtIndex(dimension_index, nullptr);
			
			if (dimension_size < 2)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() all elements of gridSize must be of length >= 2." << EidosTerminate();
			if ((values_dimcount != 1) && (dimension_size != values_dim[dimension_index]))
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() supplied gridSize does not match dimensions of matrix/array value; either supply a matching gridSize or supply NULL for gridSize." << EidosTerminate();
			
			dimension_sizes[dimension_index] = dimension_size;
			map_size *= dimension_size;
		}
		
		if (values->Count() != map_size)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() size of the values vector (" << values->Count() << ") does not match the product of the sizes in gridSize (" << map_size << ")." << EidosTerminate();
	}
	else
	{
		// No gridSize was supplied, so values must be a matrix/array that matches the spatiality of the map
		for (int dimension_index = 0; dimension_index < map_spatiality; ++dimension_index)
		{
			int64_t dimension_size = (values_dimcount == 1) ? values->Count() : values_dim[dimension_index];	// treat a vector as a 1D matrix
			
			if (dimension_size < 2)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() all dimensions of value must be of size >= 2." << EidosTerminate();
			
			dimension_sizes[dimension_index] = dimension_size;
			map_size *= dimension_size;
		}
	}
	
	const double *values_float_vec_ptr = (values->Type() == EidosValueType::kValueFloat) ? values->FloatVector()->data() : nullptr;
	const int64_t *values_integer_vec_ptr = (values->Type() == EidosValueType::kValueInt) ? values->IntVector()->data() : nullptr;
	bool range_is_null = (value_range->Type() == EidosValueType::kValueNULL);
	bool colors_is_null = (colors->Type() == EidosValueType::kValueNULL);
	double range_min = 0.0, range_max = 0.0;
	int color_count = 0;
	
	if (!range_is_null || !colors_is_null)
	{
		if (range_is_null || colors_is_null)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() valueRange and colors must either both be supplied, or neither supplied." << EidosTerminate();
		
		if (value_range->Count() != 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() valueRange must be exactly length 2 (giving the min and max value permitted)." << EidosTerminate();
		
		// valueRange and colors were provided, so use them for coloring
		range_min = value_range->FloatAtIndex(0, nullptr);
		range_max = value_range->FloatAtIndex(1, nullptr);
		
		if (!std::isfinite(range_min) || !std::isfinite(range_max) || (range_min >= range_max))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() valueRange must be finite, and min < max is required." << EidosTerminate();
		
		color_count = colors->Count();
		
		if (color_count < 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() colors must be of length >= 2." << EidosTerminate();
	}
	else
	{
		// so that we can provide a default color map, we try to find the value range here
		range_min = range_max = (values_float_vec_ptr ? values_float_vec_ptr[0] : values_integer_vec_ptr[0]);
		
		for (int64_t values_index = 1; values_index < map_size; ++values_index)
		{
			double value = (values_float_vec_ptr ? values_float_vec_ptr[values_index] : values_integer_vec_ptr[values_index]);
			
			range_min = std::min(range_min, value);
			range_max = std::max(range_max, value);
		}
		
		if (!std::isfinite(range_min) || !std::isfinite(range_max))
		{
			range_min = 0.0;
			range_max = 0.0;
		}
	}
	
	// OK, everything seems to check out, so we can make our SpatialMap struct and populate it
	SpatialMap *spatial_map = new SpatialMap(spatiality_string, map_spatiality, dimension_sizes, interpolate, range_min, range_max, color_count);
	double *values_ptr = spatial_map->values_;
	
	if (values_float_vec_ptr)
		for (int64_t values_index = 0; values_index < map_size; ++values_index)
			*(values_ptr++) = *(values_float_vec_ptr++);
	else
		for (int64_t values_index = 0; values_index < map_size; ++values_index)
			*(values_ptr++) = *(values_integer_vec_ptr++);
	
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
	
	return gStaticEidosValueVOID;
}

//	*********************	- (string)spatialMapColor(string$ name, numeric value)
//
EidosValue_SP Subpopulation::ExecuteMethod_spatialMapColor(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *name_value = p_arguments[0].get();
	EidosValue *value_value = p_arguments[1].get();
	
	std::string map_name = name_value->StringAtIndex(0, nullptr);
	EidosValue *values = value_value;
	
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
}

//	*********************	– (float)spatialMapValue(string$ name, float point)
//
#define SLiMClampCoordinate(x) ((x < 0.0) ? 0.0 : ((x > 1.0) ? 1.0 : x))

EidosValue_SP Subpopulation::ExecuteMethod_spatialMapValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *name_value = p_arguments[0].get();
	EidosValue *point_value = p_arguments[1].get();
	
	std::string map_name = name_value->StringAtIndex(0, nullptr);
	EidosValue *point = point_value;
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() map name must not be zero-length." << EidosTerminate();
	
	// Find the named SpatialMap
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		SpatialMap *map = map_iter->second;
		EidosValue_Float_vector *float_result;
		int x_count;
		
		if (point->Count() == map->spatiality_)
		{
			x_count = 1;
			float_result = nullptr;
		}
		else if (point->Count() % map->spatiality_ == 0)
		{
			x_count = point->Count() / map->spatiality_;
			float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		}
		else
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() length of point must match spatiality of map " << map_name << ", or be a multiple thereof." << EidosTerminate();
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			// We need to use the correct spatial bounds for each coordinate, which depends upon our exact spatiality
			// There is doubtless a way to make this code smarter, but brute force is sometimes best...
			double map_value;
			
			switch (map->spatiality_)
			{
				case 1:
				{
					double point_vec[1];
					int value_offset = value_index;
					
					if (map->spatiality_string_ == "x")
					{
						double x = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
						point_vec[0] = SLiMClampCoordinate(x);
					}
					else if (map->spatiality_string_ == "y")
					{
						double y = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
						point_vec[0] = SLiMClampCoordinate(y);
					}
					else if (map->spatiality_string_ == "z")
					{
						double z = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
						point_vec[0] = SLiMClampCoordinate(z);
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): (internal error) unrecognized spatiality." << EidosTerminate();
					
					map_value = map->ValueAtPoint_S1(point_vec);
					break;
				}
				case 2:
				{
					double point_vec[2];
					int value_offset = value_index * 2;
					
					if (map->spatiality_string_ == "xy")
					{
						double x = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
						point_vec[0] = SLiMClampCoordinate(x);
						
						double y = (point->FloatAtIndex(1 + value_offset, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
						point_vec[1] = SLiMClampCoordinate(y);
					}
					else if (map->spatiality_string_ == "yz")
					{
						double y = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
						point_vec[0] = SLiMClampCoordinate(y);
						
						double z = (point->FloatAtIndex(1 + value_offset, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
						point_vec[1] = SLiMClampCoordinate(z);
					}
					else if (map->spatiality_string_ == "xz")
					{
						double x = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
						point_vec[0] = SLiMClampCoordinate(x);
						
						double z = (point->FloatAtIndex(1 + value_offset, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
						point_vec[1] = SLiMClampCoordinate(z);
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): (internal error) unrecognized spatiality." << EidosTerminate();
					
					map_value = map->ValueAtPoint_S2(point_vec);
					break;
				}
				case 3:
				{
					double point_vec[3];
					int value_offset = value_index * 3;
					
					if (map->spatiality_string_ == "xyz")
					{
						double x = (point->FloatAtIndex(0 + value_offset, nullptr) - bounds_x0_) / (bounds_x1_ - bounds_x0_);
						point_vec[0] = SLiMClampCoordinate(x);
						
						double y = (point->FloatAtIndex(1 + value_offset, nullptr) - bounds_y0_) / (bounds_y1_ - bounds_y0_);
						point_vec[1] = SLiMClampCoordinate(y);
						
						double z = (point->FloatAtIndex(2 + value_offset, nullptr) - bounds_z0_) / (bounds_z1_ - bounds_z0_);
						point_vec[2] = SLiMClampCoordinate(z);
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): (internal error) unrecognized spatiality." << EidosTerminate();
					
					map_value = map->ValueAtPoint_S3(point_vec);
					break;
				}
				default:
				{
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): (internal error) unrecognized spatiality." << EidosTerminate();
				}
			}
			
			if (float_result)
				float_result->set_float_no_check(map_value, value_index);
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(map_value));
		}
		
		return EidosValue_SP(float_result);
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() could not find map with name " << map_name << "." << EidosTerminate();
}

#undef SLiMClampCoordinate

//	*********************	– (void)outputMSSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F], [logical$ filterMonomorphic = F])
//	*********************	– (void)outputSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F])
//	*********************	– (void)outputVCFSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [logical$ outputMultiallelics = T], [Ns$ filePath = NULL], [logical$ append=F], [logical$ simplifyNucleotides = F], [logical$ outputNonnucleotides = T])
//
EidosValue_SP Subpopulation::ExecuteMethod_outputXSample(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *sampleSize_value = p_arguments[0].get();
	EidosValue *replace_value = p_arguments[1].get();
	EidosValue *requestedSex_value = p_arguments[2].get();
	EidosValue *outputMultiallelics_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[3].get() : nullptr);
	EidosValue *filePath_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[4].get() : p_arguments[3].get());
	EidosValue *append_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[5].get() : p_arguments[4].get());
	EidosValue *filterMonomorphic_arg = ((p_method_id == gID_outputMSSample) ? p_arguments[5].get() : nullptr);
	EidosValue *simplifyNucleotides_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[6].get() : nullptr);
	EidosValue *outputNonnucleotides_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[7].get() : nullptr);
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	SLiMSim &sim = population_.sim_;
	
	if (!sim.warned_early_output_)
	{
		if (sim.GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				output_stream << "#WARNING (Subpopulation::ExecuteMethod_outputXSample): " << Eidos_StringForGlobalStringID(p_method_id) << "() should probably not be called from an early() event in a WF model; the output will reflect state at the beginning of the generation, not the end." << std::endl;
				sim.warned_early_output_ = true;
			}
		}
	}
	
	slim_popsize_t sample_size = SLiMCastToPopsizeTypeOrRaise(sampleSize_value->IntAtIndex(0, nullptr));
	
	bool replace = replace_value->LogicalAtIndex(0, nullptr);
	
	IndividualSex requested_sex;
	
	std::string sex_string = requestedSex_value->StringAtIndex(0, nullptr);
	
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
		output_multiallelics = outputMultiallelics_arg->LogicalAtIndex(0, nullptr);
	
	bool simplify_nucs = false;
	
	if (p_method_id == gID_outputVCFSample)
		simplify_nucs = simplifyNucleotides_arg->LogicalAtIndex(0, nullptr);
	
	bool output_nonnucs = true;
	
	if (p_method_id == gID_outputVCFSample)
		output_nonnucs = outputNonnucleotides_arg->LogicalAtIndex(0, nullptr);
	
	bool filter_monomorphic = false;
	
	if (p_method_id == gID_outputMSSample)
		filter_monomorphic = filterMonomorphic_arg->LogicalAtIndex(0, nullptr);
	
	// Figure out the right output stream
	std::ofstream outfile;
	bool has_file = false;
	std::string outfile_path;
	
	if (filePath_arg->Type() != EidosValueType::kValueNULL)
	{
		outfile_path = Eidos_ResolvedPath(filePath_arg->StringAtIndex(0, nullptr));
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
		population_.PrintSample_MS(out, *this, sample_size, replace, requested_sex, sim.TheChromosome(), filter_monomorphic);
	else if (p_method_id == gID_outputVCFSample)
		population_.PrintSample_VCF(out, *this, sample_size, replace, requested_sex, output_multiallelics, simplify_nucs, output_nonnucs);
	
	if (has_file)
		outfile.close(); 
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)configureDisplay([Nf center = NULL], [Nf$ scale = NULL], [Ns$ color = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_configureDisplay(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	EidosValue *center_value = p_arguments[0].get();
	EidosValue *scale_value = p_arguments[1].get();
	EidosValue *color_value = p_arguments[2].get();
	
	// This method doesn't actually do anything unless we're running under SLiMgui
	
	if (center_value->Type() == EidosValueType::kValueNULL)
	{
#ifdef SLIMGUI
		gui_center_from_user_ = false;
#endif
	}
	else
	{
		int center_count = center_value->Count();
		
		if (center_count != 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_configureDisplay): configureDisplay() requires that center be of exactly size 2 (x and y)." << EidosTerminate();
		
		double x = center_value->FloatAtIndex(0, nullptr);
		double y = center_value->FloatAtIndex(1, nullptr);
		
		if ((x < 0.0) || (x > 1.0) || (y < 0.0) || (y > 1.0))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_configureDisplay): configureDisplay() requires that the specified center be within [0,1] for both x and y." << EidosTerminate();
		
#ifdef SLIMGUI
		gui_center_x_ = x;
		gui_center_y_ = y;
		gui_center_from_user_ = true;
#endif
	}
	
	if (scale_value->Type() == EidosValueType::kValueNULL)
	{
#ifdef SLIMGUI
		gui_radius_scaling_from_user_ = false;
#endif
	}
	else
	{
		double scale = scale_value->FloatAtIndex(0, nullptr);
		
		if ((scale <= 0.0) || (scale > 5.0))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_configureDisplay): configureDisplay() requires that the specified scale be within (0,5]." << EidosTerminate();
		
#ifdef SLIMGUI
		gui_radius_scaling_ = scale;
		gui_radius_scaling_from_user_ = true;
#endif
	}
	
	if (color_value->Type() == EidosValueType::kValueNULL)
	{
#ifdef SLIMGUI
		gui_color_from_user_ = false;
#endif
	}
	else
	{
		std::string &&color = color_value->StringAtIndex(0, nullptr);
		
		if (color.empty())
		{
#ifdef SLIMGUI
			gui_color_from_user_ = false;
#endif
		}
		else
		{
			float color_red, color_green, color_blue;
			
			Eidos_GetColorComponents(color, &color_red, &color_green, &color_blue);
			
#ifdef SLIMGUI
			gui_color_red_ = color_red;
			gui_color_green_ = color_green;
			gui_color_blue_ = color_blue;
			gui_color_from_user_ = true;
#endif
		}
	}
	
	return gStaticEidosValueVOID;
}


//
//	Subpopulation_Class
//
#pragma mark -
#pragma mark Subpopulation_Class
#pragma mark -

class Subpopulation_Class : public EidosDictionary_Class
{
public:
	Subpopulation_Class(const Subpopulation_Class &p_original) = delete;	// no copy-construct
	Subpopulation_Class& operator=(const Subpopulation_Class&) = delete;	// no copying
	inline Subpopulation_Class(void) { }
	
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};

EidosObjectClass *gSLiM_Subpopulation_Class = new Subpopulation_Class;


const std::string &Subpopulation_Class::ElementType(void) const
{
	return gStr_Subpopulation;
}

const std::vector<EidosPropertySignature_CSP> *Subpopulation_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*EidosDictionary_Class::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,							true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_firstMaleIndex,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_firstMaleIndex));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,					true,	kEidosValueMaskObject, gSLiM_Genome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individuals,				true,	kEidosValueMaskObject, gSLiM_Individual_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopIDs,			true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopFractions,	true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_selfingRate,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_cloningRate,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sexRatio,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialBounds,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individualCount,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_individualCount));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Subpopulation::SetProperty_Accelerated_tag));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fitnessScaling,				false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_fitnessScaling)->DeclareAcceleratedSet(Subpopulation::SetProperty_Accelerated_fitnessScaling));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Subpopulation_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*EidosDictionary_Class::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMigrationRates, kEidosValueMaskVOID))->AddIntObject("sourceSubpops", gSLiM_Subpopulation_Class)->AddNumeric("rates"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointInBounds, kEidosValueMaskLogical))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointReflected, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointStopped, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointPeriodic, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointUniform, kEidosValueMaskFloat))->AddInt_OS(gEidosStr_n, gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setCloningRate, kEidosValueMaskVOID))->AddNumeric("rate"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelfingRate, kEidosValueMaskVOID))->AddNumeric_S("rate"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSexRatio, kEidosValueMaskVOID))->AddFloat_S("sexRatio"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSpatialBounds, kEidosValueMaskVOID))->AddNumeric("bounds"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSubpopulationSize, kEidosValueMaskVOID))->AddInt_S("size"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addCloned, kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Individual_Class))->AddObject_S("parent", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addCrossed, kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Individual_Class))->AddObject_S("parent1", gSLiM_Individual_Class)->AddObject_S("parent2", gSLiM_Individual_Class)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addEmpty, kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Individual_Class))->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addRecombinant, kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Individual_Class))->AddObject_SN("strand1", gSLiM_Genome_Class)->AddObject_SN("strand2", gSLiM_Genome_Class)->AddInt_N("breaks1")->AddObject_SN("strand3", gSLiM_Genome_Class)->AddObject_SN("strand4", gSLiM_Genome_Class)->AddInt_N("breaks2")->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSelfed, kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Individual_Class))->AddObject_S("parent", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_takeMigrants, kEidosValueMaskVOID))->AddObject("migrants", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_removeSubpopulation, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_cachedFitness, kEidosValueMaskFloat))->AddInt_N("indices"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sampleIndividuals, kEidosValueMaskObject, gSLiM_Individual_Class))->AddInt_S("size")->AddLogical_OS("replace", gStaticEidosValue_LogicalF)->AddObject_OSN("exclude", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddString_OSN("sex", gStaticEidosValueNULL)->AddInt_OSN("tag", gStaticEidosValueNULL)->AddInt_OSN("minAge", gStaticEidosValueNULL)->AddInt_OSN("maxAge", gStaticEidosValueNULL)->AddLogical_OSN("migrant", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_subsetIndividuals, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_OSN("exclude", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddString_OSN("sex", gStaticEidosValueNULL)->AddInt_OSN("tag", gStaticEidosValueNULL)->AddInt_OSN("minAge", gStaticEidosValueNULL)->AddInt_OSN("maxAge", gStaticEidosValueNULL)->AddLogical_OSN("migrant", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_defineSpatialMap, kEidosValueMaskVOID))->AddString_S("name")->AddString_S("spatiality")->AddInt_N("gridSize")->AddNumeric("values")->AddLogical_OS("interpolate", gStaticEidosValue_LogicalF)->AddNumeric_ON("valueRange", gStaticEidosValueNULL)->AddString_ON("colors", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_spatialMapColor, kEidosValueMaskString))->AddString_S("name")->AddNumeric("value"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_spatialMapValue, kEidosValueMaskFloat))->AddString_S("name")->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMSSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("filterMonomorphic", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputVCFSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("simplifyNucleotides", gStaticEidosValue_LogicalF)->AddLogical_OS("outputNonnucleotides", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_configureDisplay, kEidosValueMaskVOID))->AddFloat_ON("center", gStaticEidosValueNULL)->AddFloat_OSN("scale", gStaticEidosValueNULL)->AddString_OSN("color", gStaticEidosValueNULL));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}




































































