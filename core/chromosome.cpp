//
//  chromosome.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Philipp Messer.  All rights reserved.
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


#include "chromosome.h"
#include "eidos_rng.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "community.h"
#include "species.h"
#include "individual.h"
#include "subpopulation.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <cmath>
#include <utility>
#include <tuple>


// This struct is used to represent a constant-mutation-rate subrange of a genomic element; it is used internally by Chromosome.
// It is private to Chromosome, and is not exposed in Eidos in any way; it just assists with the drawing of new mutations.
struct GESubrange
{
public:
	GenomicElement *genomic_element_ptr_;			// pointer to the genomic element that contains this subrange
	slim_position_t start_position_;				// the start position of the subrange
	slim_position_t end_position_;					// the end position of the subrange
	
	GESubrange(GenomicElement *p_genomic_element_ptr, slim_position_t p_start_position, slim_position_t p_end_position);
};

inline __attribute__((always_inline)) GESubrange::GESubrange(GenomicElement *p_genomic_element_ptr, slim_position_t p_start_position, slim_position_t p_end_position) :
	genomic_element_ptr_(p_genomic_element_ptr), start_position_(p_start_position), end_position_(p_end_position)
{
}


#pragma mark -
#pragma mark Chromosome
#pragma mark -

Chromosome::Chromosome(Species &p_species, ChromosomeType p_type, int64_t p_id, std::string p_symbol, slim_chromosome_index_t p_index, int p_preferred_mutcount) :
	id_(p_id),
	symbol_(p_symbol),
	name_(),
	index_(p_index),
	type_(p_type),
	
	exp_neg_overall_mutation_rate_H_(0.0), exp_neg_overall_mutation_rate_M_(0.0), exp_neg_overall_mutation_rate_F_(0.0),
	exp_neg_overall_recombination_rate_H_(0.0), exp_neg_overall_recombination_rate_M_(0.0), exp_neg_overall_recombination_rate_F_(0.0), 
	
#ifndef USE_GSL_POISSON
	probability_both_0_H_(0.0), probability_both_0_OR_mut_0_break_non0_H_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_(0.0),
	probability_both_0_M_(0.0), probability_both_0_OR_mut_0_break_non0_M_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_(0.0),
	probability_both_0_F_(0.0), probability_both_0_OR_mut_0_break_non0_F_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_(0.0), 
#endif

	haplosome_pool_(p_species.population_.species_haplosome_pool_),
	preferred_mutrun_count_(p_preferred_mutcount),
	x_experiments_enabled_(false),
	community_(p_species.community_),
	species_(p_species),
	last_position_(0),
	extent_immutable_(false),
	overall_mutation_rate_H_(0.0), overall_mutation_rate_M_(0.0), overall_mutation_rate_F_(0.0),
	overall_mutation_rate_H_userlevel_(0.0), overall_mutation_rate_M_userlevel_(0.0), overall_mutation_rate_F_userlevel_(0.0),
	overall_recombination_rate_H_(0.0), overall_recombination_rate_M_(0.0), overall_recombination_rate_F_(0.0),
	overall_recombination_rate_H_userlevel_(0.0), overall_recombination_rate_M_userlevel_(0.0), overall_recombination_rate_F_userlevel_(0.0),
	using_DSB_model_(false), non_crossover_fraction_(0.0), gene_conversion_avg_length_(0.0), gene_conversion_inv_half_length_(0.0), simple_conversion_fraction_(0.0), mismatch_repair_bias_(0.0),
	last_position_mutrun_(0)
{
	// If the user has said "no mutation run experiments" in initializeSLiMOptions(), then a count of zero
	// supplied here is interpreted as a count of 1, and experiments will thus not be conducted.
	if (!species_.UserWantsMutrunExperiments() && (preferred_mutrun_count_ == 0))
		preferred_mutrun_count_ = 1;
	
	// Set up the default color for fixed mutations in SLiMgui
	color_sub_ = "#3333FF";
	if (!color_sub_.empty())
		Eidos_GetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
	
	// depending on the type of chromosome, cache some properties for quick reference
	switch (type_)
	{
		case ChromosomeType::kA_DiploidAutosome:				// type "A"
			intrinsic_ploidy_ = 2;
			always_uses_null_haplosomes_ = false;
			is_sex_chromosome_ = false;
			defaults_to_zero_recombination_ = false;
			type_string_ = gStr_A;
			break;
		case ChromosomeType::kH_HaploidAutosome:				// type "H"
			intrinsic_ploidy_ = 1;
			always_uses_null_haplosomes_ = false;
			is_sex_chromosome_ = false;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_H;
			break;
		case ChromosomeType::kX_XSexChromosome:					// type "X"
			intrinsic_ploidy_ = 2;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = true;
			defaults_to_zero_recombination_ = false;
			type_string_ = gStr_X;
			break;
		case ChromosomeType::kY_YSexChromosome:					// type "Y"
			intrinsic_ploidy_ = 1;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = true;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_Y;
			break;
		case ChromosomeType::kZ_ZSexChromosome:					// type "Z"
			intrinsic_ploidy_ = 2;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = true;
			defaults_to_zero_recombination_ = false;
			type_string_ = gStr_Z;
			break;
		case ChromosomeType::kW_WSexChromosome:					// type "W"
			intrinsic_ploidy_ = 1;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = true;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_W;
			break;
		case ChromosomeType::kHF_HaploidFemaleInherited:		// type "HF"
			intrinsic_ploidy_ = 1;
			always_uses_null_haplosomes_ = false;
			is_sex_chromosome_ = false;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_HF;
			break;
		case ChromosomeType::kFL_HaploidFemaleLine:				// type "FL"
			intrinsic_ploidy_ = 1;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = false;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_FL;
			break;
		case ChromosomeType::kHM_HaploidMaleInherited:			// type "HM"
			intrinsic_ploidy_ = 1;
			always_uses_null_haplosomes_ = false;
			is_sex_chromosome_ = false;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_HM;
			break;
		case ChromosomeType::kML_HaploidMaleLine:				// type "ML"
			intrinsic_ploidy_ = 1;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = false;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_ML;
			break;
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// type "H-"
			intrinsic_ploidy_ = 2;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = false;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr_H_;		// "H-"
			break;
		case ChromosomeType::kNullY_YSexChromosomeWithNull:		// type "-Y"
			intrinsic_ploidy_ = 2;
			always_uses_null_haplosomes_ = true;
			is_sex_chromosome_ = true;
			defaults_to_zero_recombination_ = true;
			type_string_ = gStr__Y;		// "-Y"
			break;
	}
}

Chromosome::~Chromosome(void)
{
	//EIDOS_ERRSTREAM << "Chromosome::~Chromosome" << std::endl;
	
	if (lookup_mutation_H_)
		gsl_ran_discrete_free(lookup_mutation_H_);
	
	if (lookup_mutation_M_)
		gsl_ran_discrete_free(lookup_mutation_M_);
	
	if (lookup_mutation_F_)
		gsl_ran_discrete_free(lookup_mutation_F_);
	
	if (lookup_recombination_H_)
		gsl_ran_discrete_free(lookup_recombination_H_);
	
	if (lookup_recombination_M_)
		gsl_ran_discrete_free(lookup_recombination_M_);
	
	if (lookup_recombination_F_)
		gsl_ran_discrete_free(lookup_recombination_F_);
	
	// Dispose of any nucleotide sequence
	delete ancestral_seq_buffer_;
	ancestral_seq_buffer_ = nullptr;
	
	// Dispose of all genomic elements, which we own
	for (GenomicElement *element : genomic_elements_)
		delete element;
	
	// dispose of haplosomes within our junkyards
	for (Haplosome *haplosome : haplosomes_junkyard_nonnull)
	{
		haplosome->~Haplosome();
		haplosome_pool_.DisposeChunk(const_cast<Haplosome *>(haplosome));
	}
	haplosomes_junkyard_nonnull.clear();
	
	for (Haplosome *haplosome : haplosomes_junkyard_null)
	{
		haplosome->~Haplosome();
		haplosome_pool_.DisposeChunk(const_cast<Haplosome *>(haplosome));
	}
	haplosomes_junkyard_null.clear();
	
	// Dispose of all mutation run contexts
#ifndef _OPENMP
	delete mutation_run_context_SINGLE_.allocation_pool_;
#else
	for (size_t threadnum = 0; threadnum < mutation_run_context_PERTHREAD.size(); ++threadnum)
	{
		omp_destroy_lock(&mutation_run_context_PERTHREAD[threadnum]->allocation_pool_lock_);
		delete mutation_run_context_PERTHREAD[threadnum]->allocation_pool_;
		delete mutation_run_context_PERTHREAD[threadnum];
	}
	mutation_run_context_PERTHREAD.clear();
#endif
	
	// Dispose of mutation run experiment data
	if (x_experiments_enabled_)
	{
		if (x_current_runtimes_)
			free(x_current_runtimes_);
		x_current_runtimes_ = nullptr;
		
		if (x_previous_runtimes_)
			free(x_previous_runtimes_);
		x_previous_runtimes_ = nullptr;
	}
}

void Chromosome::CreateNucleotideMutationRateMap(void)
{
	// In Species::CacheNucleotideMatrices() we find the maximum sequence-based mutation rate requested.  Absent a
	// hotspot map, this is the overall rate at which we need to generate mutations everywhere along the chromosome,
	// because any particular spot could have the nucleotide sequence that leads to that maximum rate; we don't want
	// to have to calculate the mutation rate map every time the sequence changes, so instead we use rejection
	// sampling.  With a hotspot map, the mutation rate map is the product of the hotspot map and the maximum
	// sequence-based rate.  Note that we could get more tricky here – even without a hotspot map we could vary
	// the mutation rate map based upon the genomic elements in the chromosome, since different genomic elements
	// may have different maximum sequence-based mutation rates.  We do not do that right now, to keep the model
	// simple.
	
	// Note that in nucleotide-based models we completely hide the existence of the mutation rate map from the user;
	// all the user sees are the mutationMatrix parameters to initializeGenomicElementType() and the hotspot map
	// defined by initializeHotspotMap().  We still use the standard mutation rate map machinery under the hood,
	// though.  So this method is, in a sense, an internal call to initializeMutationRate() that sets up the right
	// rate map to achieve what the user has requested through other APIs.
	
	double max_nucleotide_mut_rate = species_.MaxNucleotideMutationRate();
	
	std::vector<slim_position_t> &hotspot_end_positions_H = hotspot_end_positions_H_;
	std::vector<slim_position_t> &hotspot_end_positions_M = hotspot_end_positions_M_;
	std::vector<slim_position_t> &hotspot_end_positions_F = hotspot_end_positions_F_;
	std::vector<double> &hotspot_multipliers_H = hotspot_multipliers_H_;
	std::vector<double> &hotspot_multipliers_M = hotspot_multipliers_M_;
	std::vector<double> &hotspot_multipliers_F = hotspot_multipliers_F_;
	
	std::vector<slim_position_t> &mut_positions_H = mutation_end_positions_H_;
	std::vector<slim_position_t> &mut_positions_M = mutation_end_positions_M_;
	std::vector<slim_position_t> &mut_positions_F = mutation_end_positions_F_;
	std::vector<double> &mut_rates_H = mutation_rates_H_;
	std::vector<double> &mut_rates_M = mutation_rates_M_;
	std::vector<double> &mut_rates_F = mutation_rates_F_;
	
	// clear the mutation map; there may be old cruft in there, if we're called by setHotspotMap() for example
	mut_positions_H.resize(0);
	mut_positions_M.resize(0);
	mut_positions_F.resize(0);
	mut_rates_H.resize(0);
	mut_rates_M.resize(0);
	mut_rates_F.resize(0);
	
	if ((hotspot_multipliers_M.size() > 0) && (hotspot_multipliers_F.size() > 0))
	{
		// two sex-specific hotspot maps
		for (double multiplier_M : hotspot_multipliers_M)
		{
			double rate = max_nucleotide_mut_rate * multiplier_M;
			
			if (rate > 1.0)
				EIDOS_TERMINATION << "ERROR (Chromosome::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
			
			mut_rates_M.emplace_back(rate);
		}
		for (double multiplier_F : hotspot_multipliers_F)
		{
			double rate = max_nucleotide_mut_rate * multiplier_F;
			
			if (rate > 1.0)
				EIDOS_TERMINATION << "ERROR (Chromosome::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
			
			mut_rates_F.emplace_back(rate);
		}
		
		mut_positions_M = hotspot_end_positions_M;
		mut_positions_F = hotspot_end_positions_F;
	}
	else if (hotspot_multipliers_H.size() > 0)
	{
		// one hotspot map
		for (double multiplier_H : hotspot_multipliers_H)
		{
			double rate = max_nucleotide_mut_rate * multiplier_H;
			
			if (rate > 1.0)
				EIDOS_TERMINATION << "ERROR (Chromosome::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
			
			mut_rates_H.emplace_back(rate);
		}
		
		mut_positions_H = hotspot_end_positions_H;
	}
	else
	{
		// No hotspot map specified at all; use a rate of 1.0 across the chromosome with an inferred length
		if (max_nucleotide_mut_rate > 1.0)
			EIDOS_TERMINATION << "ERROR (Chromosome::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
		
		mut_rates_H.emplace_back(max_nucleotide_mut_rate);
		//mut_positions_H.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	
	community_.chromosome_changed_ = true;
}

// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
void Chromosome::InitializeDraws(void)
{
	// If we are in the special case of having no genetics, do some simple initialization and return.
	// This leaves us in an unusual state, with nullptr for recombination/mutation rate maps and a
	// last_position_ of -1; this state is not normally achievable, and might cause special problems.
	if (!species_.HasGenetics())
	{
		single_recombination_map_ = true;
		single_mutation_map_ = true;
		
		last_position_ = -1;
		extent_immutable_ = false;
		
		if (hotspot_multipliers_H_.size() == 0)
			hotspot_multipliers_H_.emplace_back(1.0);
		if (hotspot_end_positions_H_.size() == 0)
			hotspot_end_positions_H_.emplace_back(-1);
		
		if (mutation_rates_H_.size() == 0)
			mutation_rates_H_.emplace_back(1.0);
		if (mutation_end_positions_H_.size() == 0)
			mutation_end_positions_H_.emplace_back(-1);
		
		lookup_mutation_H_ = nullptr;
		overall_mutation_rate_M_userlevel_ = overall_mutation_rate_F_userlevel_ = overall_mutation_rate_H_userlevel_ = 0.0;
		overall_mutation_rate_M_ = overall_mutation_rate_F_ = overall_mutation_rate_H_ = 0.0;
		exp_neg_overall_mutation_rate_M_ = exp_neg_overall_mutation_rate_F_ = exp_neg_overall_mutation_rate_H_ = 1.0;
		
		if (recombination_rates_H_.size() == 0)
			recombination_rates_H_.emplace_back(1.0);
		if (recombination_end_positions_H_.size() == 0)
			recombination_end_positions_H_.emplace_back(-1);
		
		lookup_recombination_H_ = nullptr;
		any_recombination_rates_05_ = false;
		overall_recombination_rate_M_userlevel_ = overall_recombination_rate_F_userlevel_ = overall_recombination_rate_H_userlevel_ = 0.0;
		overall_recombination_rate_M_ = overall_recombination_rate_F_ = overall_recombination_rate_H_ = 0.0;
		exp_neg_overall_recombination_rate_M_ = exp_neg_overall_recombination_rate_F_ = exp_neg_overall_recombination_rate_H_ = 1.0;
		
#ifndef USE_GSL_POISSON
		_InitializeJointProbabilities(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_,
									  overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_,
									  probability_both_0_H_, probability_both_0_OR_mut_0_break_non0_H_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_);
		
#endif
		
		return;
	}
	
	if (genomic_elements_.size() == 0)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): empty chromosome." << EidosTerminate();
	
	// BCH 8/28/2024: we now sort the genomic elements here; we need them to be sorted later on anyway
	// I avoided doing this before because it changed the behavior, but it is hard to imagine anybody caring
	// Since elements must be non-overlapping, we only need to sort by start position
	std::sort(genomic_elements_.begin(), genomic_elements_.end(),
			  [](GenomicElement *e1, GenomicElement *e2) { return e1->start_position_ < e2->start_position_; });
	
	// determine which case we are working with: separate recombination maps for the sexes, or one map
	auto rec_rates_H_size = recombination_rates_H_.size();
	auto rec_rates_M_size = recombination_rates_M_.size();
	auto rec_rates_F_size = recombination_rates_F_.size();
	
	if ((rec_rates_H_size > 0) && (rec_rates_M_size == 0) && (rec_rates_F_size == 0))
		single_recombination_map_ = true;
	else if ((rec_rates_H_size == 0) && (rec_rates_M_size > 0) && (rec_rates_F_size > 0))
		single_recombination_map_ = false;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): (internal error) unclear whether we have separate recombination maps or not, or recombination rate not specified." << EidosTerminate();
	
	// determine which case we are working with: separate mutation maps for the sexes, or one map
	auto mut_rates_H_size = mutation_rates_H_.size();
	auto mut_rates_M_size = mutation_rates_M_.size();
	auto mut_rates_F_size = mutation_rates_F_.size();
	
	if ((mut_rates_H_size > 0) && (mut_rates_M_size == 0) && (mut_rates_F_size == 0))
		single_mutation_map_ = true;
	else if ((mut_rates_H_size == 0) && (mut_rates_M_size > 0) && (mut_rates_F_size > 0))
		single_mutation_map_ = false;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): (internal error) unclear whether we have separate mutation maps or not, or mutation rate not specified." << EidosTerminate();
	
	// Calculate the last chromosome base position from our genomic elements, mutation and recombination maps
	// the end of the last genomic element may be before the end of the chromosome; the end of mutation and
	// recombination maps all need to agree, though, if they have been supplied.  Checks that the maps do
	// not end before the end of the chromosome will be done in _InitializeOne...Map().
	// BCH 9/20/2024: A chromosome declared explicitly with initializeChromosome() has an immutable length
	if (!extent_immutable_)
	{
		last_position_ = 0;
		
		for (GenomicElement *genomic_element : genomic_elements_)
		{ 
			if (genomic_element->end_position_ > last_position_)
				last_position_ = genomic_element->end_position_;
		}
		
		if (single_mutation_map_)
		{
			if (mutation_end_positions_H_.size())
				last_position_ = std::max(last_position_, *(std::max_element(mutation_end_positions_H_.begin(), mutation_end_positions_H_.end())));
		}
		else
		{
			if (mutation_end_positions_M_.size())
				last_position_ = std::max(last_position_, *(std::max_element(mutation_end_positions_M_.begin(), mutation_end_positions_M_.end())));
			if (mutation_end_positions_F_.size())
				last_position_ = std::max(last_position_, *(std::max_element(mutation_end_positions_F_.begin(), mutation_end_positions_F_.end())));
		}
		
		if (single_recombination_map_)
		{
			if (recombination_end_positions_H_.size())
				last_position_ = std::max(last_position_, *(std::max_element(recombination_end_positions_H_.begin(), recombination_end_positions_H_.end())));
		}
		else
		{
			if (recombination_end_positions_M_.size())
				last_position_ = std::max(last_position_, *(std::max_element(recombination_end_positions_M_.begin(), recombination_end_positions_M_.end())));
			if (recombination_end_positions_F_.size())
				last_position_ = std::max(last_position_, *(std::max_element(recombination_end_positions_F_.begin(), recombination_end_positions_F_.end())));
		}
		
		extent_immutable_ = true;
	}
	
	// Patch the hotspot end vector if it is empty; see setHotspotMap() and initializeHotspotMap().
	// Basically, the length of the chromosome might not have been known yet when the user set the map.
	// This is done for the mutation rate maps in _InitializeOneMutationMap(); we do it here for the hotspot map.
	// Here we also fill in a rate of 1.0 if no rate was supplied
	if ((hotspot_multipliers_H_.size() == 0) && (hotspot_multipliers_M_.size() == 0) && (hotspot_multipliers_F_.size() == 0))
		hotspot_multipliers_H_.emplace_back(1.0);
	if ((hotspot_end_positions_H_.size() == 0) && (hotspot_multipliers_H_.size() == 1))
		hotspot_end_positions_H_.emplace_back(last_position_);
	if ((hotspot_end_positions_M_.size() == 0) && (hotspot_multipliers_M_.size() == 1))
		hotspot_end_positions_M_.emplace_back(last_position_);
	if ((hotspot_end_positions_F_.size() == 0) && (hotspot_multipliers_F_.size() == 1))
		hotspot_end_positions_F_.emplace_back(last_position_);
	
	// Now remake our mutation map info, which we delegate to _InitializeOneMutationMap()
	if (single_mutation_map_)
	{
		_InitializeOneMutationMap(lookup_mutation_H_, mutation_end_positions_H_, mutation_rates_H_, overall_mutation_rate_H_userlevel_, overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_, mutation_subranges_H_);
		
		// Copy the H rates into the M and F ivars, so that they can be used by DrawMutationAndBreakpointCounts() if needed
		overall_mutation_rate_M_userlevel_ = overall_mutation_rate_F_userlevel_ = overall_mutation_rate_H_userlevel_;
		overall_mutation_rate_M_ = overall_mutation_rate_F_ = overall_mutation_rate_H_;
		exp_neg_overall_mutation_rate_M_ = exp_neg_overall_mutation_rate_F_ = exp_neg_overall_mutation_rate_H_;
	}
	else
	{
		_InitializeOneMutationMap(lookup_mutation_M_, mutation_end_positions_M_, mutation_rates_M_, overall_mutation_rate_M_userlevel_, overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_, mutation_subranges_M_);
		_InitializeOneMutationMap(lookup_mutation_F_, mutation_end_positions_F_, mutation_rates_F_, overall_mutation_rate_F_userlevel_, overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_, mutation_subranges_F_);
	}
	
	// Now remake our recombination map info, which we delegate to _InitializeOneRecombinationMap()
	any_recombination_rates_05_ = false;
	
	if (single_recombination_map_)
	{
		_InitializeOneRecombinationMap(lookup_recombination_H_, recombination_end_positions_H_, recombination_rates_H_, overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_, overall_recombination_rate_H_userlevel_);
		
		// Copy the H rates into the M and F ivars, so that they can be used by DrawMutationAndBreakpointCounts() if needed
		overall_recombination_rate_M_ = overall_recombination_rate_F_ = overall_recombination_rate_H_;
		exp_neg_overall_recombination_rate_M_ = exp_neg_overall_recombination_rate_F_ = exp_neg_overall_recombination_rate_H_;
		overall_recombination_rate_M_userlevel_ = overall_recombination_rate_F_userlevel_ = overall_recombination_rate_H_userlevel_;
	}
	else
	{
		_InitializeOneRecombinationMap(lookup_recombination_M_, recombination_end_positions_M_, recombination_rates_M_, overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_, overall_recombination_rate_M_userlevel_);
		_InitializeOneRecombinationMap(lookup_recombination_F_, recombination_end_positions_F_, recombination_rates_F_, overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_, overall_recombination_rate_F_userlevel_);
	}
	
#ifndef USE_GSL_POISSON
	// Calculate joint mutation/recombination probabilities for the H/M/F cases
	if (single_mutation_map_ && single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_,
									  overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_,
									  probability_both_0_H_, probability_both_0_OR_mut_0_break_non0_H_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_);
	}
	else if (single_mutation_map_ && !single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_,
									  overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_,
									  probability_both_0_M_, probability_both_0_OR_mut_0_break_non0_M_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_);
		
		_InitializeJointProbabilities(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_,
									  overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_,
									  probability_both_0_F_, probability_both_0_OR_mut_0_break_non0_F_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_);
	}
	else if (!single_mutation_map_ && single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_,
									  overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_,
									  probability_both_0_M_, probability_both_0_OR_mut_0_break_non0_M_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_);
		
		_InitializeJointProbabilities(overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_,
									  overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_,
									  probability_both_0_F_, probability_both_0_OR_mut_0_break_non0_F_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_);
	}
	else if (!single_mutation_map_ && !single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_,
									  overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_,
									  probability_both_0_M_, probability_both_0_OR_mut_0_break_non0_M_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_);
		
		_InitializeJointProbabilities(overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_,
									  overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_,
									  probability_both_0_F_, probability_both_0_OR_mut_0_break_non0_F_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_);
	}
#endif
}

#ifndef USE_GSL_POISSON
void Chromosome::_InitializeJointProbabilities(double p_overall_mutation_rate, double p_exp_neg_overall_mutation_rate,
											   double p_overall_recombination_rate, double p_exp_neg_overall_recombination_rate,
											   double &p_both_0, double &p_both_0_OR_mut_0_break_non0, double &p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0)
{
	// precalculate probabilities for Poisson draws of mutation count and breakpoint count
	double prob_mutation_0 = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_mutation_rate);			// exp(-mu); can be 0 due to underflow
	double prob_breakpoint_0 = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_recombination_rate);	// exp(-mu); can be 0 due to underflow
	
	// this is a validity check on the previous calculation of the exp_neg rates
	if ((p_exp_neg_overall_mutation_rate != prob_mutation_0) || (p_exp_neg_overall_recombination_rate != prob_breakpoint_0))
		EIDOS_TERMINATION << "ERROR (Chromosome::_InitializeJointProbabilities): zero-probability does not match previous calculation." << EidosTerminate();
	
	double prob_mutation_not_0 = 1.0 - prob_mutation_0;
	double prob_breakpoint_not_0 = 1.0 - prob_breakpoint_0;
	double prob_both_0 = prob_mutation_0 * prob_breakpoint_0;
	double prob_mutation_0_breakpoint_not_0 = prob_mutation_0 * prob_breakpoint_not_0;
	double prob_mutation_not_0_breakpoint_0 = prob_mutation_not_0 * prob_breakpoint_0;
	
	//	std::cout << "element_mutation_rate_ == " << element_mutation_rate_ << std::endl;
	//	std::cout << "prob_mutation_0 == " << prob_mutation_0 << std::endl;
	//	std::cout << "prob_breakpoint_0 == " << prob_breakpoint_0 << std::endl;
	//	std::cout << "prob_mutation_not_0 == " << prob_mutation_not_0 << std::endl;
	//	std::cout << "prob_breakpoint_not_0 == " << prob_breakpoint_not_0 << std::endl;
	//	std::cout << "prob_both_0 == " << prob_both_0 << std::endl;
	//	std::cout << "prob_mutation_0_breakpoint_not_0 == " << prob_mutation_0_breakpoint_not_0 << std::endl;
	//	std::cout << "prob_mutation_not_0_breakpoint_0 == " << prob_mutation_not_0_breakpoint_0 << std::endl;
	
	p_both_0 = prob_both_0;
	p_both_0_OR_mut_0_break_non0 = prob_both_0 + prob_mutation_0_breakpoint_not_0;
	p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0 = prob_both_0 + (prob_mutation_0_breakpoint_not_0 + prob_mutation_not_0_breakpoint_0);
}
#endif

void Chromosome::ChooseMutationRunLayout(void)
{
	// We now have a final last position, so we can calculate our mutation run layout
	
	if (species_.HasGenetics())
	{
#ifdef _OPENMP
		// When running multi-threaded, we prefer the base number of mutruns to equal the number of threads
		// This allows us to subdivide responsibility along the haplosome equally among threads
		mutrun_count_base_ = gEidosMaxThreads;
		mutrun_count_multiplier_ = 1;
		
		// However, the shortest any mutrun can be is one base position, so with a short chromosome, the
		// number of mutruns gets clipped to the number of base positions (and not all threads will be used)
		if (mutrun_count_base_ > (last_position_ + 1))
			mutrun_count_base_ = (int32_t)(last_position_ + 1);
#else
		// When running single-threaded, the base number of mutruns is always 1
		mutrun_count_base_ = 1;
		mutrun_count_multiplier_ = 1;
#endif
		
		if (preferred_mutrun_count_ != 0)
		{
			// The user has given us a mutation run count, so use that count and divide the chromosome evenly
			if (preferred_mutrun_count_ < 1)
				EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): there must be at least one mutation run per haplosome." << EidosTerminate();
			
			// If the preferred number of mutation runs is actually larger than the number of discrete positions,
			// it gets clipped.  No warning is emitted; this is pretty obvious, and the verbose output line suffices
			if (preferred_mutrun_count_ > (last_position_ + 1))
				preferred_mutrun_count_ = (int32_t)(last_position_ + 1);
			
			// Similarly, we clip silently at SLIM_MUTRUN_MAXIMUM_COUNT; larger values are not presently allowed,
			// although the code is general and does not actually have a hard limit on the number of mutruns
			if (preferred_mutrun_count_ > SLIM_MUTRUN_MAXIMUM_COUNT)
				preferred_mutrun_count_ = SLIM_MUTRUN_MAXIMUM_COUNT;
			
#ifdef _OPENMP
			// When running multithreaded, we have some additional restrictions to try to keep the number of mutation runs
			// aligned with the number of threads; but we also want to allow the user to use fewer mutruns/threads
			if (((preferred_mutrun_count_ % gEidosMaxThreads) == 0) ||	// if it is an exact multiple of the number of threads
				(preferred_mutrun_count_ < gEidosMaxThreads))				// or, less than the number of threads
				;													// then it is fine
			else
				EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): when multithreaded, if the number of mutation runs is specified it must be a multiple of the number of threads, or it must be less than the number of threads (clipped mutationRuns count is " << preferred_mutrun_count_ << ", thread count is " << gEidosMaxThreads << ")." << EidosTerminate();
#endif
			
			if (preferred_mutrun_count_ == gEidosMaxThreads)		// NOLINTNEXTLINE(*-branch-clone) : intentional branch clones
			{
				// We have preferred_mutrun_count_ mutrun sections, each containing 1 mutation run; this is really the same as the next case
				mutrun_count_base_ = preferred_mutrun_count_;
				mutrun_count_multiplier_ = 1;
			}
			else if ((preferred_mutrun_count_ % gEidosMaxThreads) == 0)
			{
				// We have gEidosMaxThreads mutrun sections, each containing (preferred_mutrun_count_ / gEidosMaxThreads) mutation runs
				mutrun_count_base_ = gEidosMaxThreads;
				mutrun_count_multiplier_ = preferred_mutrun_count_ / gEidosMaxThreads;
			}
			else
			{
				// The number of threads does not equal gEidosMaxThreads, so we have preferred_mutrun_count_ mutruns sections, each containing 1 mutrun
				mutrun_count_base_ = preferred_mutrun_count_;
				mutrun_count_multiplier_ = 1;
			}
			
			mutrun_count_ = mutrun_count_base_ * mutrun_count_multiplier_;
			mutrun_length_ = (slim_position_t)ceil((last_position_ + 1) / (double)mutrun_count_);
			
			if (SLiM_verbosity_level >= 2)
				SLIM_OUTSTREAM << std::endl << "// Override mutation run count = " << mutrun_count_ << ", run length = " << mutrun_length_ << std::endl;
		}
		else
		{
			// The user has not supplied a count, so we will conduct experiments to find the best count;
			// for simplicity we will just always start with a single run, since that is often best anyway,
			// unless we're running multithreaded; then we start with one run per thread, generally
			mutrun_count_ = mutrun_count_base_ * mutrun_count_multiplier_;
			mutrun_length_ = (slim_position_t)ceil((last_position_ + 1) / (double)mutrun_count_);
			
			// When we are running experiments, the mutation run length needs to be a power of two so that it can be divided evenly,
			// potentially a fairly large number of times.  We impose a maximum mutrun count of SLIM_MUTRUN_MAXIMUM_COUNT, so
			// actually it needs to just be an exact multiple of SLIM_MUTRUN_MAXIMUM_COUNT, not an exact power of two.
			mutrun_length_ = (slim_position_t)round(ceil(mutrun_length_ / (double)SLIM_MUTRUN_MAXIMUM_COUNT) * SLIM_MUTRUN_MAXIMUM_COUNT);
			
			if (SLiM_verbosity_level >= 2)
				SLIM_OUTSTREAM << std::endl << "// Initial mutation run count = " << mutrun_count_ << ", run length = " << mutrun_length_ << std::endl;
		}
	}
	else
	{
		// No-genetics species use null haplosomes, and have no mutruns
		mutrun_count_base_ = 0;
		mutrun_count_multiplier_ = 1;
		mutrun_count_ = 0;
		mutrun_length_ = 0;
	}
	
	last_position_mutrun_ = mutrun_count_ * mutrun_length_ - 1;
	
	// Consistency check
	if (((mutrun_length_ < 1) && species_.HasGenetics()) || (mutrun_count_ * mutrun_length_ <= last_position_) || (last_position_mutrun_ < last_position_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): (internal error) math error in mutation run calculations." << EidosTerminate();
}

// initialize one recombination map, used internally by InitializeDraws() to avoid code duplication
void Chromosome::_InitializeOneRecombinationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate, double &p_overall_rate_userlevel)
{
	// Patch the recombination interval end vector if it is empty; see setRecombinationRate() and initializeRecombinationRate().
	// Basically, the length of the chromosome might not have been known yet when the user set the rate.
	if (p_end_positions.size() == 0)
	{
		// patching can only be done when a single uniform rate is specified
		if (p_rates.size() != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints not specified." << EidosTerminate();
		
		p_end_positions.emplace_back(last_position_);
	}
	
	if (p_end_positions[p_rates.size() - 1] < last_position_)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints do not cover the full chromosome." << EidosTerminate();
	
	// BCH 11 April 2018: Fixing this code to use Peter Ralph's reparameterization that corrects for the way we use these rates
	// downstream.  So the rates we are passed in p_rates are from the user, and they represent "ancestry breakpoint" rates: the
	// desired probability of a crossover from one base to the next.  Downstream, when we actually generate breakpoints, we will
	// draw the number of breakpoints from a Poisson distribution with the overall rate of p_overall_rate that we sum up here,
	// then we will draw the location of each breakpoint from the p_lookup table we build here, and then we will sort and unique
	// those breakpoints so that we only ever have one breakpoint, at most, between any two bases.  That uniquing is the key step
	// that drives this reparameterization.  We need to choose a reparameterized lambda value for the Poisson draw such that the
	// probability P[Poisson(lambda) >= 1] == rate.  Simply using lambda=rate is a good approximation when rate is small, but when
	// rate is larger than 0.01 it starts to break down, and when rate is 0.5 it is quite inaccurate; P[Poisson(0.5) >= 1] ~= 0.39.
	// So we reparameterize according to the formula:
	//
	//		lambda = -log(1 - p)
	//
	// where p is the probability requested by the user and lambda is the expected mean used for the Poisson draw to ensure that
	// P[Poisson(lambda) >= 1] == p.  Note that this formula blows up for p >= 1, and gets numerically unstable as it approaches
	// that limit; the lambda values chosen will become very large.  Since values above 0.5 are unbiological anyway, we do not
	// allow them; all recombination rates must be <= 0.5 in SLiM, as of this change.  0.5 represents complete independence;
	// we do not allow "anti-linkage" at present since it seems to be unbiological.  If somebody argues with us on that, we could
	// extend the limit up to, say, 0.9 without things blowing up badly, but 1.0 is out of the question.
	//
	// Here we also calculate the overall recombination rate in reparameterized units, which is what we actually use to draw
	// breakpoints, *and* the overall recombination rate in non-reparameterized units (as in SLiM 2.x and before), which is what
	// we report to the user if they ask for it (representing the expected number of crossovers along the chromosome).  The
	// reparameterized overall recombination rate is the expected number of breakpoints we will generate *before* uniquing,
	// which has no meaning for the user.
	std::vector<double> reparameterized_rates;
	
	reparameterized_rates.reserve(p_rates.size());
	
	for (double user_rate : p_rates)
	{
		// this bounds check should have been done already externally to us, but no harm in making sure...
		if ((user_rate < 0.0) || (user_rate > 0.5))
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): all recombination rates in SLiM must be in [0.0, 0.5]." << EidosTerminate();
		
#if 1
		// This is the new recombination-rate paradigm that compensates for uniquing of breakpoints, etc.
		
		// keep track of whether any recombination rates in the model are 0.5; if so, gene conversion needs to exclude those positions
		if (user_rate == 0.5)
			any_recombination_rates_05_ = true;
		
		reparameterized_rates.emplace_back(-log(1.0 - user_rate));
		
		/*
		 A good model to test that the recombination rate observed is actually the crossover rate requested:
		
		initialize() {
		   defineConstant("L", 10);		// 10 breakpoint positions, so 11 bases
		   defineConstant("N", 1000);		// 1000 diploids
		
		   initializeMutationRate(0);
		   initializeMutationType("m1", 0.5, "f", 0.0);
		   initializeGenomicElementType("g1", m1, 1.0);
		   initializeGenomicElement(g1, 0, L);
		   initializeRecombinationRate(0.5);
		}
		1 early() {
		   sim.addSubpop("p1", N);
		   sim.tag = 0;
		}
		recombination() {
		   sim.tag = sim.tag + size(unique(breakpoints));
		   return F;
		}
		10000 late() {
		   print(sim.tag / (10000 * N * 2 * L));
		}

		 */
#else
		// This is the old recombination-rate paradigm, which should be used only for testing output compatibility in tests
#warning old recombination-rate paradigm enabled!
		
		reparameterized_rates.emplace_back(user_rate);
#endif
	}
	
	// Calculate the overall recombination rate and the lookup table for breakpoints
	std::vector<double> B(reparameterized_rates.size());
	std::vector<double> B_userlevel(reparameterized_rates.size());
	
	B_userlevel[0] = p_rates[0] * static_cast<double>(p_end_positions[0]);
	B[0] = reparameterized_rates[0] * static_cast<double>(p_end_positions[0]);	// No +1 here, because the position to the left of the first base is not a valid recombination position.
																	// So a 1-base model (position 0 to 0) has an end of 0, and thus an overall rate of 0.  This means that
																	// gsl_ran_discrete_preproc() is given an interval with rate 0, but that seems OK.  BCH 4 April 2016
	for (unsigned int i = 1; i < reparameterized_rates.size(); i++)
	{
		double length = static_cast<double>(p_end_positions[i] - p_end_positions[i - 1]);
		
		B_userlevel[i] = p_rates[i] * length;
		B[i] = reparameterized_rates[i] * length;
	}
	
	p_overall_rate_userlevel = Eidos_ExactSum(B_userlevel.data(), p_rates.size());
	p_overall_rate = Eidos_ExactSum(B.data(), reparameterized_rates.size());
	
	// All the recombination machinery below uses the reparameterized rates
#ifndef USE_GSL_POISSON
	p_exp_neg_overall_rate = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_rate);				// exp(-mu); can be 0 due to underflow
#endif
	
	if (p_lookup)
		gsl_ran_discrete_free(p_lookup);
	
	p_lookup = gsl_ran_discrete_preproc(reparameterized_rates.size(), B.data());
}

// initialize one mutation map, used internally by InitializeDraws() to avoid code duplication
void Chromosome::_InitializeOneMutationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_requested_overall_rate, double &p_overall_rate, double &p_exp_neg_overall_rate, std::vector<GESubrange> &p_subranges)
{
	// Patch the mutation interval end vector if it is empty; see setMutationRate() and initializeMutationRate().
	// Basically, the length of the chromosome might not have been known yet when the user set the rate.
	if (p_end_positions.size() == 0)
	{
		// patching can only be done when a single uniform rate is specified
		if (p_rates.size() != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): mutation rate endpoints not specified." << EidosTerminate();
		
		p_end_positions.emplace_back(last_position_);
	}
	
	if (p_end_positions[p_rates.size() - 1] < last_position_)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): mutation rate endpoints do not cover the full chromosome." << EidosTerminate();
	
	// Calculate the overall mutation rate and the lookup table for mutation events.  This is more complicated than
	// for recombination maps, because the mutation rate map needs to be intersected with the genomic element map
	// such that all areas outside of any genomic element have a rate of 0.  In the end we need (i) a vector of
	// constant-rate subregions that do not span genomic element boundaries, (ii) a lookup table to go from the
	// index of a constant-rate subregion to the index of the containing genomic element, and (iii) a lookup table
	// to go from the index of a constant-rate subregion to the first base position and length of the subregion.
	// In effect, each of these subregions is sort of the new equivalent of a genomic element; we intersect the
	// mutation rate map with the genomic element map to create a list of new genomic elements of constant mut rate.
	// The class we use to represent these constant-rate subregions is GESubrange, declared in chromosome.h.
	p_subranges.clear();
	
	// We need to work with a *sorted* genomic elements vector here.  BCH 8/28/2024: That is now guaranteed.
	
	// This code deals in two different currencies: *requested* mutation rates and *adjusted* mutation rates.
	// This stems from the fact that, beginning in SLiM 3.5, we unique mutation positions as we're generating
	// gametes.  This uniquing means that some drawn positions may be filtered out, resulting in a realized
	// mutation rate that is lower than the requested rate (see section 22.2.3 in the manual for discussion).
	// We want to compensate for this effect, so that the realized rate is equal to the requested rate.  To do
	// that, we calculate adjusted rates that, after uniquing, will result in the requested rate.  However, we
	// want to hide this implementation detail from the user; the rate they see in Eidos from Chromosome should
	// be the same as the rate they requested originally, not the adjusted rate.  So we do calculations with
	// both, so that we can satisfy that requirement.  The adjustment is done piecewise, so that the adjusted
	// rate for each GESubrange is correct.  If we have a nucleotide-based matation matrix that leads to some
	// rejection sampling, that will be based on the ratios of the original rates, but it will be done *after*
	// positions have been drawn and uniqued, so that should also be correct, I think.  BCH 5 Sep. 2020
	std::vector<double> A;	// a vector of requested subrange weights
	std::vector<double> B;	// a vector of adjusted subrange weights
	unsigned int mutrange_index = 0;
	slim_position_t end_of_previous_mutrange = -1;
	
	for (GenomicElement *ge_ptr : genomic_elements_)
	{
		GenomicElement &ge = *ge_ptr;
		
		for ( ; mutrange_index < p_rates.size(); mutrange_index++)
		{
			slim_position_t end_of_mutrange = p_end_positions[mutrange_index];
			
			if (end_of_mutrange < ge.start_position_)
			{
				// This mutrange falls entirely before the current genomic element, so it is discarded by falling through
			}
			else if (end_of_previous_mutrange + 1 > ge.end_position_)
			{
				// This mutrange falls entirely after the current genomic element, so we need to move to the next genomic element
				break;
			}
			else
			{
				// This mutrange at least partially intersects the current genomic element, so we need to make a new GESubrange
				slim_position_t subrange_start = std::max(end_of_previous_mutrange + 1, ge.start_position_);
				slim_position_t subrange_end = std::min(end_of_mutrange, ge.end_position_);
				slim_position_t subrange_length = subrange_end - subrange_start + 1;
				double requested_rate = p_rates[mutrange_index];
				
				if (requested_rate >= 1.0)
					EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): requested mutation rate is >= 1.0." << EidosTerminate();
				
				double adjusted_rate = -log1p(-requested_rate);
				double requested_subrange_weight = requested_rate * subrange_length;
				double adjusted_subrange_weight = adjusted_rate * subrange_length;
				
				A.emplace_back(requested_subrange_weight);
				B.emplace_back(adjusted_subrange_weight);
				p_subranges.emplace_back(&ge, subrange_start, subrange_end);
				
				// Now we need to decide whether to advance the genomic element or not; advancing mutrange_index here is not needed
				if (end_of_mutrange >= ge.end_position_)
					break;
			}
			
			end_of_previous_mutrange = end_of_mutrange;
		}
	}
	
	p_requested_overall_rate = Eidos_ExactSum(A.data(), A.size());
	p_overall_rate = Eidos_ExactSum(B.data(), B.size());
	
#ifndef USE_GSL_POISSON
	p_exp_neg_overall_rate = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_rate);				// exp(-mu); can be 0 due to underflow
#endif
	
	if (p_lookup)
		gsl_ran_discrete_free(p_lookup);
	
	p_lookup = gsl_ran_discrete_preproc(B.size(), B.data());
}

// prints an error message and exits
void Chromosome::MutationMapConfigError(void) const
{
	EIDOS_TERMINATION << "ERROR (Chromosome::MutationMapConfigError): (internal error) an error occurred in the configuration of mutation maps." << EidosTerminate();
}
void Chromosome::RecombinationMapConfigError(void) const
{
	EIDOS_TERMINATION << "ERROR (Chromosome::RecombinationMapConfigError): (internal error) an error occurred in the configuration of recombination maps." << EidosTerminate();
}

int Chromosome::DrawSortedUniquedMutationPositions(int p_count, IndividualSex p_sex, std::vector<std::pair<slim_position_t, GenomicElement *>> &p_positions)
{
	// BCH 1 September 2020: This method generates a vector of positions, sorted and uniqued for the caller.  This avoid various issues
	// with two mutations occurring at the same position in the same gamete.  For example, nucleotide states in the tree-seq tables could
	// end up in a state that would normally be illegal, such as an A->G mutation followed by a G->G mutation, because SLiM's code wouldn't
	// understand that the second mutation occurred on the background of the first; it would think that both mutations were A->G, erroneously.
	// This also messed up uniquing of mutations with a mutation() callback in SLiM 3.5; you might want all mutations representing a given
	// nucleotide at a given position to share the same mutation object (IBS instead of IBD), but if two mutations occurred at the same
	// position in the same gamete, the first might be accepted, and then second – which should unique down to the first – would also be
	// accepted.  The right fix seems to be to unique the drawn mutation positions before creating mutations, avoiding all these issues;
	// multiple mutations at the same position in the same gamete seems unbiological anyway.
	gsl_ran_discrete_t *lookup;
	const std::vector<GESubrange> *subranges;
	
	if (single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_mutation_H_;
		subranges = &mutation_subranges_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			lookup = lookup_mutation_M_;
			subranges = &mutation_subranges_M_;
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			lookup = lookup_mutation_F_;
			subranges = &mutation_subranges_F_;
		}
		else
		{
			MutationMapConfigError();
		}
	}
	
	// draw all the positions, and keep track of the genomic element type for each
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	Eidos_MT_State *mt = EIDOS_MT_RNG(omp_get_thread_num());
	
	for (int i = 0; i < p_count; ++i)
	{
		int mut_subrange_index = static_cast<int>(gsl_ran_discrete(rng, lookup));
		const GESubrange &subrange = (*subranges)[mut_subrange_index];
		GenomicElement *source_element = subrange.genomic_element_ptr_;
		
		// Draw the position along the chromosome for the mutation, within the genomic element
		slim_position_t position = subrange.start_position_ + static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64(mt, subrange.end_position_ - subrange.start_position_ + 1));
		// old 32-bit position not MT64 code:
		//slim_position_t position = subrange.start_position_ + static_cast<slim_position_t>(Eidos_rng_uniform_int(rng, (uint32_t)(subrange.end_position_ - subrange.start_position_ + 1)));
		
		p_positions.emplace_back(position, source_element);
	}
	
	// sort and unique by position; 1 and 2 mutations are particularly common, so try to speed those up
	if (p_count > 1)
	{
		if (p_count == 2)
		{
			if (p_positions[0].first > p_positions[1].first)
				std::swap(p_positions[0], p_positions[1]);
			else if (p_positions[0].first == p_positions[1].first)
				p_positions.resize(1);
		}
		else
		{
			std::sort(p_positions.begin(), p_positions.end(), [](const std::pair<slim_position_t, GenomicElement *> &p1, const std::pair<slim_position_t, GenomicElement *> &p2) { return p1.first < p2.first; });
			auto unique_iter = std::unique(p_positions.begin(), p_positions.end(), [](const std::pair<slim_position_t, GenomicElement *> &p1, const std::pair<slim_position_t, GenomicElement *> &p2) { return p1.first == p2.first; });
			p_positions.resize(std::distance(p_positions.begin(), unique_iter));
		}
	}
	
	return (int)p_positions.size();
}

// draw a new mutation, based on the genomic element types present and their mutational proclivities
MutationIndex Chromosome::DrawNewMutation(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_tick_t p_tick) const
{
	const GenomicElement &source_element = *(p_position.second);
	const GenomicElementType &genomic_element_type = *(source_element.genomic_element_type_ptr_);
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
	
	// NOTE THAT THE STACKING POLICY IS NOT ENFORCED HERE, SINCE WE DO NOT KNOW WHAT HAPLOSOME WE WILL BE INSERTED INTO!  THIS IS THE CALLER'S RESPONSIBILITY!
	MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
	
	// A nucleotide value of -1 is always used here; in nucleotide-based models this gets patched later, but that is sequence-dependent and background-dependent
	Mutation *mutation = gSLiM_Mutation_Block + new_mut_index;
	
	new (mutation) Mutation(mutation_type_ptr, index_, p_position.first, selection_coeff, p_subpop_index, p_tick, -1);
	
	// addition to the main registry and the muttype registries will happen if the new mutation clears the stacking policy
	
	return new_mut_index;
}

// apply mutation() to a generated mutation; we might return nullptr (proposed mutation rejected), the original proposed mutation (it was accepted), or a replacement Mutation *
Mutation *Chromosome::ApplyMutationCallbacks(Mutation *p_mut, Haplosome *p_haplosome, GenomicElement *p_genomic_element, int8_t p_original_nucleotide, std::vector<SLiMEidosBlock*> &p_mutation_callbacks) const
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplyMutationCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	slim_objectid_t mutation_type_id = p_mut->mutation_type_ptr_->mutation_type_id_;
	
	// note the focal child during the callback, so we can prevent illegal operations during the callback
	SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
	community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosMutationCallback;
	
	bool mutation_accepted = true, mutation_replaced = false;
	
	for (SLiMEidosBlock *mutation_callback : p_mutation_callbacks)
	{
		if (mutation_callback->block_active_)
		{
			slim_objectid_t callback_mutation_type_id = mutation_callback->mutation_type_id_;
			
			if ((callback_mutation_type_id == -1) || (callback_mutation_type_id == mutation_type_id))
			{
#ifndef DEBUG_POINTS_ENABLED
#error "DEBUG_POINTS_ENABLED is not defined; include eidos_globals.h"
#endif
#if DEBUG_POINTS_ENABLED
				// SLiMgui debugging point
				EidosDebugPointIndent indenter;
				
				{
					EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
					EidosToken *decl_token = mutation_callback->root_node_->token_;
					
					if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
						(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
					{
						SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG mutation(";
						if ((mutation_callback->mutation_type_id_ != -1) && (mutation_callback->subpopulation_id_ != -1))
							SLIM_ERRSTREAM << "m" << mutation_callback->mutation_type_id_ << ", p" << mutation_callback->subpopulation_id_;
						else if (mutation_callback->mutation_type_id_ != -1)
							SLIM_ERRSTREAM << "m" << mutation_callback->mutation_type_id_;
						else if (mutation_callback->subpopulation_id_ != -1)
							SLIM_ERRSTREAM << "NULL, p" << mutation_callback->subpopulation_id_;
						SLIM_ERRSTREAM << ")";
						
						if (mutation_callback->block_id_ != -1)
							SLIM_ERRSTREAM << " s" << mutation_callback->block_id_;
						
						SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
						indenter.indent();
					}
				}
#endif
				
				// The callback is active and matches the mutation type id of the mutation, so we need to execute it
				// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
				EidosValue_Object local_mut(p_mut, gSLiM_Mutation_Class);
				EidosValue_Int local_originalNuc(p_original_nucleotide);
				
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = community_.FunctionMap();
					EidosInterpreter interpreter(mutation_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
					
					if (mutation_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(mutation_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (mutation_callback->contains_mut_)
					{
						local_mut.StackAllocated();			// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_mut, EidosValue_SP(&local_mut));
					}
					if (mutation_callback->contains_parent_)
						callback_symbols.InitializeConstantSymbolEntry(gID_parent, p_haplosome->OwningIndividual()->CachedEidosValue());
					if (mutation_callback->contains_haplosome_)
						callback_symbols.InitializeConstantSymbolEntry(gID_haplosome, p_haplosome->CachedEidosValue());
					if (mutation_callback->contains_element_)
						callback_symbols.InitializeConstantSymbolEntry(gID_element, p_genomic_element->CachedEidosValue());
					if (mutation_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, p_haplosome->OwningIndividual()->subpopulation_->SymbolTableEntry().second);
					if (mutation_callback->contains_originalNuc_)
					{
						local_originalNuc.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_originalNuc, EidosValue_SP(&local_originalNuc));
					}
					
					try
					{
						// Interpret the script; the result from the interpretation must be a singleton logical, T if the mutation is accepted, F if it is rejected, or a Mutation object
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(mutation_callback->script_);
						EidosValue *result = result_SP.get();
						EidosValueType resultType = result->Type();
						int resultCount = result->Count();
						
						if ((resultType == EidosValueType::kValueLogical) && (resultCount == 1))
						{
#if DEBUG
							// this checks the value type at runtime
							mutation_accepted = result->LogicalData()[0];
#else
							// unsafe cast for speed
							mutation_accepted = ((EidosValue_Logical *)result)->data()[0];
#endif
						}
						else if ((resultType == EidosValueType::kValueObject) && (((EidosValue_Object *)result)->Class() == gSLiM_Mutation_Class) && (resultCount == 1))
						{
#if DEBUG
							// this checks the value type at runtime
							Mutation *replacementMutation = (Mutation *)result->ObjectData()[0];
#else
							// unsafe cast for speed
							Mutation *replacementMutation = (Mutation *)((EidosValue_Object *)result)->data()[0];
#endif
							
							if (replacementMutation == p_mut)
							{
								// returning the proposed mutation is the same as accepting it
								mutation_accepted = true;
							}
							else
							{
								// First, check that the mutation fits the requirements: position, mutation type, nucleotide; we are already set up for the mutation type and can't change in
								// mid-stream (would we switch to running mutation() callbacks for the new type??), and we have already chosen the nucleotide for consistency with the background
								// BCH 3 September 2020: A thread on slim-discuss (https://groups.google.com/forum/#!topic/slim-discuss/ALnZrzIroKY) has convinced me that it would be useful
								// to be able to return a replacement mutation of a different mutation type, so that you can generate mutations of a single type up front and multifurcate that
								// generation process into multiple mutation types on the other side (perhaps with different dominance coefficients, for example).  So I have relaxed the
								// mutation type requirement here.  If the mutation type is changed in the replacement, we switch to running callbacks for the new mutation type, but we do not
								// go back to the beginning; we're running callbacks in the order they were declared, and just switching which type we're running, mid-stream.
								// BCH 4 September 2020: Peter has persuaded me to relax the nucleotide restriction as well.  If the user changes the nucleotide, it may produce surprising
								// results (a mutation that doesn't change the nucleotide, like G->G, or a deviation from requested mutation-matrix probabilities), but that's the user's
								// responsibility to deal with; it can be a useful thing to do, Peter thinks.
								if (replacementMutation->position_ != p_mut->position_)
									EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback must match the position of the proposed mutation." << EidosTerminate(mutation_callback->identifier_token_);
								if ((replacementMutation->state_ == MutationState::kRemovedWithSubstitution) || (replacementMutation->state_ == MutationState::kFixedAndSubstituted))
									EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback cannot be fixed/substituted." << EidosTerminate(mutation_callback->identifier_token_);
								if (replacementMutation->mutation_type_ptr_ != p_mut->mutation_type_ptr_)
								{
									//EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback must match the mutation type of the proposed mutation." << EidosTerminate(mutation_callback->identifier_token_);
									mutation_type_id = p_mut->mutation_type_ptr_->mutation_type_id_;
								}
								//if (replacementMutation->nucleotide_ != p_mut->nucleotide_)
								//	EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback must match the nucleotide of the proposed mutation." << EidosTerminate(mutation_callback->identifier_token_);
								
								// It fits the bill, so replace the original proposed mutation with this new mutation; note we don't fix local_mut, it's about to go out of scope anyway
								p_mut = replacementMutation;
								mutation_replaced = true;
								mutation_accepted = true;
							}
						}
						else
							EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): mutation() callbacks must provide a return value that is either a singleton logical or a singleton Mutation object." << EidosTerminate(mutation_callback->identifier_token_);
					}
					catch (...)
					{
						throw;
					}
				}
				
				if (!mutation_accepted)
					break;
			}
		}
	}
	
	// If a replacement mutation has been accepted at this point, we now check that it is not already present in the background haplosome; if it is present, the mutation is a no-op (implemented as a rejection)
	if (mutation_replaced && mutation_accepted)
	{
		if (p_haplosome->contains_mutation(p_mut))
			mutation_accepted = false;
	}
	
	community_.executing_block_type_ = old_executing_block_type;
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMutationCallback)]);
#endif
	
	return (mutation_accepted ? p_mut : nullptr);
}

// draw a new mutation with reference to the genomic background upon which it is occurring, for nucleotide-based models and/or mutation() callbacks
MutationIndex Chromosome::DrawNewMutationExtended(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_tick_t p_tick, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, slim_position_t *p_breakpoints, int p_breakpoints_count, std::vector<SLiMEidosBlock*> *p_mutation_callbacks) const
{
	slim_position_t position = p_position.first;
	GenomicElement &source_element = *(p_position.second);
	const GenomicElementType &genomic_element_type = *(source_element.genomic_element_type_ptr_);
	
	// Determine which parental haplosome the mutation will be atop (so we can get the genetic context for it)
	bool on_first_haplosome = true;
	
	if (p_breakpoints)
	{
		for (int break_index = 0; break_index < p_breakpoints_count; ++break_index)
		{
			if (p_breakpoints[break_index] > position)
				break;
			
			on_first_haplosome = !on_first_haplosome;
		}
	}
	
	Haplosome *background_haplosome = (on_first_haplosome ? parent_haplosome_1 : parent_haplosome_2);
	
	// Determine whether the mutation will be created at all, and if it is, what nucleotide to use
	int8_t original_nucleotide = -1, nucleotide = -1;
	
	if (genomic_element_type.mutation_matrix_)
	{
		EidosValue_Float *mm = genomic_element_type.mutation_matrix_.get();
		int mm_count = mm->Count();
		
		if (mm_count == 16)
		{
			// The mutation matrix only cares about the single-nucleotide context; figure it out
			HaplosomeWalker walker(background_haplosome);
			
			walker.MoveToPosition(position);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= position is guaranteed by MoveToPosition()
				if (pos == position)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						original_nucleotide = mut_nuc;
					
					walker.NextMutation();
				}
				else
				{
					break;
				}
			}
			
			// No mutation is present at the position, so the background comes from the ancestral sequence
			if (original_nucleotide == -1)
				original_nucleotide = (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position);
			
			// OK, now we know the background nucleotide; determine the mutation rates to derived nucleotides
			double *nuc_thresholds = genomic_element_type.mm_thresholds + (size_t)original_nucleotide * 4;
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			double draw = Eidos_rng_uniform(rng);
			
			if (draw < nuc_thresholds[0])		nucleotide = 0;
			else if (draw < nuc_thresholds[1])	nucleotide = 1;
			else if (draw < nuc_thresholds[2])	nucleotide = 2;
			else if (draw < nuc_thresholds[3])	nucleotide = 3;
			else {
				// The mutation is an excess mutation, the result of an overall mutation rate on this genetic background
				// that is less than the maximum overall mutation rate for any genetic background; it should be discarded,
				// as if this mutation event never occurred at all.  We signal this by returning -1.
				return -1;
			}
		}
		else if (mm_count == 256)
		{
			// The mutation matrix cares about the trinucleotide context; figure it out
			int8_t background_nuc1 = -1, background_nuc3 = -1;
			HaplosomeWalker walker(background_haplosome);
			
			walker.MoveToPosition(position - 1);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= position - 1 is guaranteed by MoveToPosition()
				if (pos == position - 1)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						background_nuc1 = mut_nuc;
					
					walker.NextMutation();
				}
				else if (pos == position)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						original_nucleotide = mut_nuc;
					
					walker.NextMutation();
				}
				else if (pos == position + 1)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						background_nuc3 = mut_nuc;
					
					walker.NextMutation();
				}
				else
				{
					break;
				}
			}
			
			// No mutation is present at the position, so the background comes from the ancestral sequence
			// If a base in the trinucleotide is off the end of the chromosome, we assume it is an A; that is arbitrary,
			// but should avoid skewed rates associated with the dynamics of C/G in certain sequences (like CpG)
			if (background_nuc1 == -1)
				background_nuc1 = ((position == 0) ? 0 : (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position - 1));
			if (original_nucleotide == -1)
				original_nucleotide = (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position);
			if (background_nuc3 == -1)
				background_nuc3 = ((position == last_position_) ? 0 : (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position + 1));
			
			// OK, now we know the background nucleotide; determine the mutation rates to derived nucleotides
			int trinuc = ((int)background_nuc1) * 16 + ((int)original_nucleotide) * 4 + (int)background_nuc3;
			double *nuc_thresholds = genomic_element_type.mm_thresholds + (size_t)trinuc * 4;
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			double draw = Eidos_rng_uniform(rng);
			
			if (draw < nuc_thresholds[0])		nucleotide = 0;
			else if (draw < nuc_thresholds[1])	nucleotide = 1;
			else if (draw < nuc_thresholds[2])	nucleotide = 2;
			else if (draw < nuc_thresholds[3])	nucleotide = 3;
			else {
				// The mutation is an excess mutation, the result of an overall mutation rate on this genetic background
				// that is less than the maximum overall mutation rate for any genetic background; it should be discarded,
				// as if this mutation event never occurred at all.  We signal this by returning -1.
				return -1;
			}
		}
		else
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawNewMutationExtended): (internal error) unexpected mutation matrix size." << EidosTerminate();
	}
	
	// Draw mutation type and selection coefficient, and create the new mutation
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
	
	// NOTE THAT THE STACKING POLICY IS NOT ENFORCED HERE!  THIS IS THE CALLER'S RESPONSIBILITY!
	MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
	Mutation *mutation = gSLiM_Mutation_Block + new_mut_index;
	
	new (mutation) Mutation(mutation_type_ptr, index_, position, selection_coeff, p_subpop_index, p_tick, nucleotide);
	
	// Call mutation() callbacks if there are any
	if (p_mutation_callbacks)
	{
		Mutation *post_callback_mut = ApplyMutationCallbacks(gSLiM_Mutation_Block + new_mut_index, background_haplosome, &source_element, original_nucleotide, *p_mutation_callbacks);
		
		// If the callback didn't return the proposed mutation, it will not be used; dispose of it
		if (post_callback_mut != mutation)
		{
			//std::cout << "proposed mutation not used, disposing..." << std::endl;
			mutation->Release();
		}
		
		// If it returned nullptr, the mutation event was rejected
		if (post_callback_mut == nullptr)
		{
			//std::cout << "mutation rejected..." << std::endl;
			return -1;
		}
		
		// Otherwise, we will request the addition of whatever mutation it returned (which might be the proposed mutation).
		// Note that if an existing mutation was returned, ApplyMutationCallbacks() guarantees that it is not already present in the background haplosome.
		MutationIndex post_callback_mut_index = post_callback_mut->BlockIndex();
		
		if (new_mut_index != post_callback_mut_index)
		{
			//std::cout << "replacing mutation!" << std::endl;
			new_mut_index = post_callback_mut_index;
		}
	}
	
	// addition to the main registry and the muttype registries will happen if the new mutation clears the stacking policy
	
	return new_mut_index;
}

// draw a set of uniqued breakpoints according to the "crossover breakpoint" model and run them through recombination() callbacks, returning the final usable set
void Chromosome::_DrawCrossoverBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers) const
{
	// BEWARE! Chromosome::DrawDSBBreakpoints() below must be altered in parallel with this method!
#if DEBUG
	if (using_DSB_model_)
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawCrossoverBreakpoints): (internal error) this method should not be called when the DSB recombination model is being used." << EidosTerminate();
#endif
	
	gsl_ran_discrete_t *lookup;
	const std::vector<slim_position_t> *end_positions;
	
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_recombination_H_;
		end_positions = &recombination_end_positions_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_parent_sex == IndividualSex::kMale)
		{
			lookup = lookup_recombination_M_;
			end_positions = &recombination_end_positions_M_;
		}
		else if (p_parent_sex == IndividualSex::kFemale)
		{
			lookup = lookup_recombination_F_;
			end_positions = &recombination_end_positions_F_;
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
	
	// draw recombination breakpoints
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	Eidos_MT_State *mt = EIDOS_MT_RNG(omp_get_thread_num());
	
	for (int i = 0; i < p_num_breakpoints; i++)
	{
		slim_position_t breakpoint = 0;
		int recombination_interval = static_cast<int>(gsl_ran_discrete(rng, lookup));
		
		// choose a breakpoint anywhere in the chosen recombination interval with equal probability
		
		// BCH 4 April 2016: Added +1 to positions in the first interval.  We do not want to generate a recombination breakpoint
		// to the left of the 0th base, and the code in InitializeDraws() above explicitly omits that position from its calculation
		// of the overall recombination rate.  Using recombination_end_positions_[recombination_interval] here for the first
		// interval means that we use one less breakpoint position than usual; conceptually, the previous breakpoint ended at -1,
		// so it ought to be recombination_end_positions_[recombination_interval]+1, but we do not add one there, in order to
		// use one fewer positions.  We then shift all the positions to the right one, with the +1 that is added here, thereby
		// making the position that was omitted be the position to the left of the 0th base.
		//
		// I also added +1 in the formula for regions after the 0th.  In general, we want a recombination interval to own all the
		// positions to the left of its enclosed bases, up to and including the position to the left of the final base given as the
		// end position of the interval.  The next interval's first owned recombination position is therefore to the left of the
		// base that is one position to the right of the end of the preceding interval.  So we have to add one to the position
		// given by recombination_end_positions_[recombination_interval - 1], at minimum.  Since Eidos_rng_uniform_int() returns
		// a zero-based random number, that means we need a +1 here as well.
		//
		// The key fact here is that a recombination breakpoint position of 1 means "break to the left of the base at position 1" –
		// the breakpoint falls between bases, to the left of the base at the specified number.  This is a consequence of the logic
		// in the crossover-mutation code, which copies mutations as long as their position is *less than* the position of the next
		// breakpoint.  When their position is *equal*, the breakpoint gets serviced by switching strands.  That logic causes the
		// breakpoints to fall to the left of their designated base.
		//
		// Note that Eidos_rng_uniform_int() crashes (well, aborts fatally) if passed 0 for n.  We need to guarantee that that doesn't
		// happen, and we don't want to waste time checking for that condition here.  For a 1-base model, we are guaranteed that
		// the overall recombination rate will be zero, by the logic in InitializeDraws(), and so we should not be called in the
		// first place.  For longer chromosomes that start with a 1-base recombination interval, the rate calculated by
		// InitializeDraws() for the first interval should be 0, so gsl_ran_discrete() should never return the first interval to
		// us here.  For all other recombination intervals, the math of pos[x]-pos[x-1] should always result in a value >0,
		// since we guarantee that recombination end positions are in strictly ascending order.  So we should never crash.  :->
		
		if (recombination_interval == 0)
			breakpoint = static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64(mt, (*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64(mt, (*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		p_crossovers.emplace_back(breakpoint);
	}
	
	// sort and unique
	if (p_num_breakpoints > 2)
	{
		std::sort(p_crossovers.begin(), p_crossovers.end());
		p_crossovers.erase(std::unique(p_crossovers.begin(), p_crossovers.end()), p_crossovers.end());
	}
	else if (p_num_breakpoints == 2)
	{
		// do our own dumb inline sort/unique if we have just two elements, to avoid the calls above
		// I didn't actually test this to confirm that it's faster, but models that generate many
		// breakpoints will generally hit the case above anyway, and models that generate few will
		// suffer only the additional (num_breakpoints == 2) test before falling through...
		slim_position_t bp1 = p_crossovers[0];
		slim_position_t bp2 = p_crossovers[1];
		
		if (bp1 > bp2)
			std::swap(p_crossovers[0], p_crossovers[1]);
		else if (bp1 == bp2)
			p_crossovers.resize(1);
	}
}

// draw a set of uniqued breakpoints according to the "double-stranded break" model and run them through recombination() callbacks, returning the final usable set
// the information returned here also includes a list of heteroduplex regions where mismatches between the two parental strands will need to be resolved
void Chromosome::_DrawDSBBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers, std::vector<slim_position_t> &p_heteroduplex) const
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Chromosome::DrawDSBBreakpoints(): usage of statics, probably many other issues");
	
	// BEWARE! Chromosome::DrawCrossoverBreakpoints() above must be altered in parallel with this method!
#if DEBUG
	if (!using_DSB_model_)
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawDSBBreakpoints): (internal error) this method should not be called when the crossover breakpoints recombination model is being used." << EidosTerminate();
#endif
	
	gsl_ran_discrete_t *lookup;
	const std::vector<slim_position_t> *end_positions;
	const std::vector<double> *rates;
	
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_recombination_H_;
		end_positions = &recombination_end_positions_H_;
		rates = &recombination_rates_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_parent_sex == IndividualSex::kMale)
		{
			lookup = lookup_recombination_M_;
			end_positions = &recombination_end_positions_M_;
			rates = &recombination_rates_M_;
		}
		else if (p_parent_sex == IndividualSex::kFemale)
		{
			lookup = lookup_recombination_F_;
			end_positions = &recombination_end_positions_F_;
			rates = &recombination_rates_F_;
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
	
	// Draw extents and crossover/noncrossover and simple/complex decisions for p_num_breakpoints DSBs; we may not end up using all
	// of them, if the uniquing step reduces the set of DSBs, but we don't want to redraw these things if we have to loop back due
	// to a collision, because such redrawing would be liable to produce bias towards shorter extents.  (Redrawing the crossover/
	// noncrossover and simple/complex decisions would probably be harmless, but it is simpler to just make all decisions up front.)
	int try_count = 0;
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	static std::vector<std::tuple<slim_position_t, slim_position_t, bool, bool>> dsb_infos;	// using a static prevents reallocation
	
	// If the redrawLengthsOnFailure parameter to initializeGeneConversion() is T, we jump back here on layout failure
	// Note that we also redraw noncrossover and simple, on this code path; that shouldn't matter since they are independent of layout
generateDSBsRedrawingLengths:
	
	dsb_infos.resize(0);
	
	if (gene_conversion_avg_length_ < 2.0)
	{
		for (int i = 0; i < p_num_breakpoints; i++)
		{
			// If the gene conversion tract mean length is < 2.0, gsl_ran_geometric() will blow up, and we should treat the tract length as zero
			bool noncrossover = (Eidos_rng_uniform(rng) <= non_crossover_fraction_);				// tuple position 2
			bool simple = (Eidos_rng_uniform(rng) <= simple_conversion_fraction_);					// tuple position 3
			
			dsb_infos.emplace_back(0, 0, noncrossover, simple);
		}
	}
	else
	{
		for (int i = 0; i < p_num_breakpoints; i++)
		{
			slim_position_t extent1 = gsl_ran_geometric(rng, gene_conversion_inv_half_length_);		// tuple position 0
			slim_position_t extent2 = gsl_ran_geometric(rng, gene_conversion_inv_half_length_);		// tuple position 1
			bool noncrossover = (Eidos_rng_uniform(rng) <= non_crossover_fraction_);				// tuple position 2
			bool simple = (Eidos_rng_uniform(rng) <= simple_conversion_fraction_);					// tuple position 3
			
			dsb_infos.emplace_back(extent1, extent2, noncrossover, simple);
		}
	}
	
	// If the redrawLengthsOnFailure parameter to initializeGeneConversion() is F, we jump back here on layout failure
generateDSBsWithoutRedrawingLengths:
	
	if (++try_count > 100)
	{
		// Note this block handles failure for both redraw_lengths_on_failure_ cases
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawDSBBreakpoints): non-overlapping recombination regions could not be achieved in 100 tries; terminating.  The recombination rate and/or mean gene conversion tract length may be too high.";
		if (!redraw_lengths_on_failure_)
			EIDOS_TERMINATION << "  You might wish to pass redrawLengthsOnFailure=T to initializeGeneConversion() to make gene conversion tract layout more robust.";
		EIDOS_TERMINATION << EidosTerminate();
	}
	
	// First draw DSB points; dsb_points contains positions and a flag for whether the breakpoint is at a rate=0.5 position
	Eidos_MT_State *mt = EIDOS_MT_RNG(omp_get_thread_num());
	static std::vector<std::pair<slim_position_t, bool>> dsb_points;	// using a static prevents reallocation
	dsb_points.resize(0);
	
	for (int i = 0; i < p_num_breakpoints; i++)
	{
		slim_position_t breakpoint = 0;
		int recombination_interval = static_cast<int>(gsl_ran_discrete(rng, lookup));
		
		if (recombination_interval == 0)
			breakpoint = static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64(mt, (*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64(mt, (*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		if ((*rates)[recombination_interval] == 0.5)
			dsb_points.emplace_back(breakpoint, true);
		else
			dsb_points.emplace_back(breakpoint, false);
	}
	
	// Sort and unique the resulting DSB vector
	if (p_num_breakpoints > 1)
	{
		std::sort(dsb_points.begin(), dsb_points.end());		// sorts by the first element of the pair
		dsb_points.erase(std::unique(dsb_points.begin(), dsb_points.end()), dsb_points.end());
	}
	
	// Assemble lists of crossover breakpoints and heteroduplex regions, starting from a clean slate
	int final_num_breakpoints = (int)dsb_points.size();
	slim_position_t last_position_used = -1;
	
	p_crossovers.resize(0);
	p_heteroduplex.resize(0);
	
	for (int i = 0; i < final_num_breakpoints; i++)
	{
		std::pair<slim_position_t, bool> &dsb_pair = dsb_points[i];
		std::tuple<slim_position_t, slim_position_t, bool, bool> &dsb_info = dsb_infos[i];
		slim_position_t dsb_point = dsb_pair.first;
		
		if (dsb_pair.second)
		{
			// This DSB is at a rate=0.5 point, so we do not generate a gene conversion tract; it just translates directly to a crossover breakpoint
			// Note that we do NOT check non_crossover_fraction_ here; it does not apply to rate=0.5 positions, since they cannot undergo gene conversion
			if (dsb_point <= last_position_used)
			{
				if (redraw_lengths_on_failure_)
					goto generateDSBsRedrawingLengths;
				else
					goto generateDSBsWithoutRedrawingLengths;
			}
			
			p_crossovers.emplace_back(dsb_point);
			last_position_used = dsb_point;
		}
		else
		{
			// This DSB is not at a rate=0.5 point, so we generate a gene conversion tract around it
			slim_position_t tract_start = dsb_point - std::get<0>(dsb_info);
			slim_position_t tract_end = SLiMClampToPositionType(dsb_point + std::get<1>(dsb_info));
			
			// We do not want to allow GC tracts to extend all the way to the chromosome beginning or end
			// This is partly because biologically it seems weird, and partly because a breakpoint at position 0 breaks tree-seq recording
			if ((tract_start <= 0) || (tract_end > last_position_) || (tract_start <= last_position_used))
			{
				if (redraw_lengths_on_failure_)
					goto generateDSBsRedrawingLengths;
				else
					goto generateDSBsWithoutRedrawingLengths;
			}
			
			if (tract_start == tract_end)
			{
				// gene conversion tract of zero length, so no tract after all, but we do use non_crossover here
				if (!std::get<2>(dsb_info))
					p_crossovers.emplace_back(tract_start);
				last_position_used = tract_start;
			}
			else
			{
				// gene conversion tract of non-zero length, so generate the tract
				p_crossovers.emplace_back(tract_start);
				if (std::get<2>(dsb_info))
					p_crossovers.emplace_back(tract_end);
				last_position_used = tract_end;
				
				// decide if it is a simple or a complex tract
				if (!std::get<3>(dsb_info))
				{
					// complex gene conversion tract; we need to save it in the list of heteroduplex regions
					p_heteroduplex.emplace_back(tract_start);
					p_heteroduplex.emplace_back(tract_end - 1);	// heteroduplex positions are base positions, so the last position is to the left of the GC tract end
				}
			}
		}
	}
}

// This high-level function is the funnel for drawing breakpoints.  It delegates down to DrawDSBBreakpoints()
// or DrawCrossoverBreakpoints(), handles recombination() callbacks, and returns a sorted, uniqued vector.
// You can supply it with a number of breakpoints to draw, or pass -1 to have it draw the number for you.
// If the caller can handle complex gene conversion tracts, they should pass a vector for those to be placed
// in.  If not, pass nullptr, and this method will raise if complex gene conversion tracts are in use.  For
// addRecombinant() and addMultiRecombinant(), this method allows the parent to be different from the
// haplosomes that are supplied; the parent individual is used to look up the haplosomes if they are passed
// as nullptr.  The haplosomes are used only if recombination() callbacks are in effect.  The parent
// is also used to look up the sex, for sex-specific recombination rates.
void Chromosome::DrawBreakpoints(Individual *p_parent, Haplosome *p_haplosome1, Haplosome *p_haplosome2, int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers, std::vector<slim_position_t> *p_heteroduplex, const char *p_caller_name)
{
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): in " << p_caller_name << ", recombination breakpoints cannot be drawn for a species with no genetics." << EidosTerminate();
	
	if (!p_heteroduplex && using_DSB_model_ && (simple_conversion_fraction_ != 1.0))
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): in " << p_caller_name << ", complex gene conversion tracts cannot be active since there is no provision for handling heteroduplex regions." << EidosTerminate();
	
	// look up parent information; note that if parent is nullptr, we do not run recombination() callbacks!
	// the parent is a required pseudo-parameter for the recombination() callback
	IndividualSex parent_sex = IndividualSex::kUnspecified;
	std::vector<SLiMEidosBlock*> recombination_callbacks;
	Subpopulation *parent_subpop = nullptr;
	
	if (p_parent)
	{
		parent_sex = p_parent->sex_;
		parent_subpop = p_parent->subpopulation_;
		recombination_callbacks = species_.CallbackBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, parent_subpop->subpopulation_id_, id_);
		
		// SPECIES CONSISTENCY CHECK
		if (&p_parent->subpopulation_->species_ != &species_)
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): in " << p_caller_name << ", the parent, if supplied, must belong to the same species as the target chromosome." << EidosTerminate();
	}
	else	// !p_parent
	{
		// In a sexual model with sex-specific recombination maps, we need to know the parent we're
		// generating breakpoints for; in other situations it is optional, but recombination()
		// breakpoints will not be called if parent is NULL.
		if (!single_recombination_map_)
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): in " << p_caller_name << ", a parent must be supplied since sex-specific recombination maps are in use (to determine which map to use, from the sex of the parent)." << EidosTerminate();
	}
	
	// look up haplosome information, used only for the recombination() callback
	if ((!p_haplosome1 && p_haplosome2) || (!p_haplosome2 && p_haplosome1))
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): (internal error) in " << p_caller_name << ", haplosomes must either be supplied or not supplied." << EidosTerminate();
	
	if (p_haplosome1)
	{
		// SPECIES CONSISTENCY CHECK
		if ((&p_haplosome1->OwningIndividual()->subpopulation_->species_ != &species_) ||
			(&p_haplosome2->OwningIndividual()->subpopulation_->species_ != &species_))
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): in " << p_caller_name << ", parental haplosomes must belong to the same species as the target chromosome." << EidosTerminate();
		
		if ((p_haplosome1->chromosome_index_ != index_) || (p_haplosome2->chromosome_index_ != index_))
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): in " << p_caller_name << ", parental haplosomes must belong to the target chromosome." << EidosTerminate();
		
		// Note that if haplosomes were passed in but p_parent is nullptr, we do NOT attempt to infer a parent
		// subpopulation in order to get recombination() callbacks from it.  In the general case that would
		// not work, since the two haplosomes might belong to different subpopulations; and if we can't do it
		// in general, we shouldn't try to do it at all.  If you want recombination() callbacks, pass p_parent.
	}
	else if (p_parent)
	{
		// Get the indices of the haplosomes associated with this chromosome.  Note that the first/last indices
		// might be the same, if this is a haploid chromosome.  That is OK here.  The user is allowed to set a
		// recombination rate on a haploid chromosome and generate breakpoints for it; what they do with that
		// information is up to them.  (They might use them in an addRecombinant() or addMultiRecombinant() call,
		// for example.)  In that case, of a haploid chromosome, the same single parent haplosome will be passed
		// twice to recombination() callbacks; that seems better than not defining one of the pseudo-parameters.
		int first_haplosome_index = species_.FirstHaplosomeIndices()[index_];
		int last_haplosome_index = species_.LastHaplosomeIndices()[index_];
		
		// Note that for calling recombination() callbacks below, we treat the parent's first haplosome as the
		// initial copy strand.  If a distinction needs to be made, pass the haplosomes in to this method.
		p_haplosome1 = p_parent->haplosomes_[first_haplosome_index];
		p_haplosome2 = p_parent->haplosomes_[last_haplosome_index];
	}
	
	// Draw the number of breakpoints, if it was not supplied
	if (p_num_breakpoints == -1)
		p_num_breakpoints = DrawBreakpointCount(parent_sex);
	
	if ((p_num_breakpoints < 0) || (p_num_breakpoints > 1000000L))
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): in " << p_caller_name << ", the number of recombination breakpoints must be in [0, 1000000]." << EidosTerminate();
	
#if DEBUG
	if (p_crossovers.size())
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): (internal error) in " << p_caller_name << ", p_crossovers was not supplied empty." << EidosTerminate();
#endif
	
	// draw the breakpoints based on the recombination rate map, and sort and unique the result
	if (p_num_breakpoints)
	{
		if (using_DSB_model_)
		{
			if (p_heteroduplex)
			{
				// p_heteroduplex is not nullptr, so the caller intends to use it for something
				_DrawDSBBreakpoints(parent_sex, p_num_breakpoints, p_crossovers, *p_heteroduplex);
			}
			else
			{
				// p_heteroduplex is nullptr, so we need to pass in our own vector; it is not actually used
				// in this case anyway, since simple_conversion_fraction_ must be 1.0 (as checked above)
				std::vector<slim_position_t> heteroduplex;
				
				_DrawDSBBreakpoints(parent_sex, p_num_breakpoints, p_crossovers, heteroduplex);
			}
		}
		else
		{
			_DrawCrossoverBreakpoints(parent_sex, p_num_breakpoints, p_crossovers);
		}
		
		// p_crossovers is sorted and uniqued at this point
		
		if (recombination_callbacks.size())
		{
			// a non-zero number of breakpoints, with recombination callbacks
			bool breaks_changed = species_.population_.ApplyRecombinationCallbacks(p_parent, p_haplosome1, p_haplosome2, p_crossovers, recombination_callbacks);
			
			// we only sort/unique if the breakpoints have changed, since they were sorted/uniqued before
			if (breaks_changed && (p_crossovers.size() > 1))
			{
				std::sort(p_crossovers.begin(), p_crossovers.end());
				p_crossovers.erase(unique(p_crossovers.begin(), p_crossovers.end()), p_crossovers.end());
			}
		}
	}
	else if (recombination_callbacks.size())
	{
		// zero breakpoints from the SLiM core, but we have recombination() callbacks
		species_.population_.ApplyRecombinationCallbacks(p_parent, p_haplosome1, p_haplosome2, p_crossovers, recombination_callbacks);
		
		if (p_crossovers.size() > 1)
		{
			std::sort(p_crossovers.begin(), p_crossovers.end());
			p_crossovers.erase(unique(p_crossovers.begin(), p_crossovers.end()), p_crossovers.end());
		}
	}
	else
	{
		// no breakpoints, no gene conversion, no recombination() callbacks
	}
	
	// values in p_crossovers and p_heteroduplex are returned to the caller
	// p_crossovers is guaranteed to be sorted and uniqued, which we check here
	// we also check that no position is less than zero or beyond the chromosome end
#if DEBUG
	slim_position_t previous_value = -1;
	slim_position_t last_chrom_position = last_position_;
	
	for (slim_position_t value : p_crossovers)
	{
		if (value <= previous_value)
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): (internal error) breakpoints vector is not sorted/uniqued." << EidosTerminate();
		if (value > last_chrom_position)
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawBreakpoints): (internal error) breakpoints vector goes beyond the chromosome end." << EidosTerminate();
		
		previous_value = value;
	}
#endif
}

size_t Chromosome::MemoryUsageForMutationMaps(void)
{
	size_t usage = 0;
	
	usage = (mutation_rates_H_.size() + mutation_rates_M_.size() + mutation_rates_F_.size()) * sizeof(double);
	usage += (mutation_end_positions_H_.size() + mutation_end_positions_M_.size() + mutation_end_positions_F_.size()) * sizeof(slim_position_t);
	usage += (mutation_subranges_H_.size() + mutation_subranges_M_.size() + mutation_subranges_F_.size()) * sizeof(GESubrange);
	usage += (hotspot_multipliers_H_.size() + hotspot_multipliers_M_.size() + hotspot_multipliers_F_.size()) * sizeof(double);
	usage += (hotspot_end_positions_H_.size() + hotspot_end_positions_M_.size() + hotspot_end_positions_F_.size()) * sizeof(slim_position_t);
	
	if (lookup_mutation_H_)
		usage += lookup_mutation_H_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_mutation_M_)
		usage += lookup_mutation_M_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_mutation_F_)
		usage += lookup_mutation_F_->K * (sizeof(size_t) + sizeof(double));
	
	return usage;
}

size_t Chromosome::MemoryUsageForRecombinationMaps(void)
{
	size_t usage = 0;
	
	usage = (recombination_rates_H_.size() + recombination_rates_M_.size() + recombination_rates_F_.size()) * sizeof(double);
	usage += (recombination_end_positions_H_.size() + recombination_end_positions_M_.size() + recombination_end_positions_F_.size()) * sizeof(slim_position_t);
	
	if (lookup_recombination_H_)
		usage += lookup_recombination_H_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_recombination_M_)
		usage += lookup_recombination_M_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_recombination_F_)
		usage += lookup_recombination_F_->K * (sizeof(size_t) + sizeof(double));
	
	return usage;
}

size_t Chromosome::MemoryUsageForAncestralSequence(void)
{
	size_t usage = 0;
	
	if (ancestral_seq_buffer_)
	{
		std::size_t length = ancestral_seq_buffer_->size();
		
		usage += ((length + 31) / 32) * sizeof(uint64_t);
	}
	
	return usage;
}

void Chromosome::SetUpMutationRunContexts(void)
{
	// Make an EidosObjectPool to allocate mutation runs from; this is for memory locality, so make it nice and big
#ifndef _OPENMP
	mutation_run_context_SINGLE_.allocation_pool_ = new EidosObjectPool("EidosObjectPool(MutationRun)", sizeof(MutationRun), 65536);
#else
	//std::cout << "***** Initializing " << gEidosMaxThreads << " independent MutationRunContexts" << std::endl;
	
	// Make per-thread MutationRunContexts; the number of threads that we set up for here is NOT gEidosMaxThreads,
	// but rather, the "base" number of mutation runs per haplosome chosen by Chromosome.  The chromosome is divided
	// into that many chunks along its length (or a multiple thereof), and there is one thread per "base" chunk.
	mutation_run_context_COUNT_ = mutrun_count_base_;
	mutation_run_context_PERTHREAD.resize(mutation_run_context_COUNT_);
	
	if (mutation_run_context_COUNT_ > 0)
	{
		// Check that each RNG was initialized by a different thread, as intended below;
		// this is not required, but it improves memory locality throughout the run
		bool threadObserved[mutation_run_context_COUNT_];
		
#pragma omp parallel default(none) shared(mutation_run_context_PERTHREAD, threadObserved) num_threads(mutation_run_context_COUNT_)
		{
			// Each thread allocates and initializes its own MutationRunContext, for "first touch" optimization
			int threadnum = omp_get_thread_num();
			
			mutation_run_context_PERTHREAD[threadnum] = new MutationRunContext();
			mutation_run_context_PERTHREAD[threadnum]->allocation_pool_ = new EidosObjectPool("EidosObjectPool(MutationRun)", sizeof(MutationRun), 65536);
			omp_init_lock(&mutation_run_context_PERTHREAD[threadnum]->allocation_pool_lock_);
			threadObserved[threadnum] = true;
		}	// end omp parallel
		
		for (int threadnum = 0; threadnum < mutation_run_context_COUNT_; ++threadnum)
			if (!threadObserved[threadnum])
				std::cerr << "WARNING: parallel MutationRunContexts were not correctly initialized on their corresponding threads; this may cause slower simulation." << std::endl;
	}
#endif	// end _OPENMP
}

// These get called if a null haplosome is requested but the null junkyard is empty, or if a non-null haplosome is requested
// but the non-null junkyard is empty; so we know that the primary junkyard for the request cannot service the request.
// If the other junkyard has a haplosome, we want to repurpose it; this prevents one junkyard from filling up with an
// ever-growing number of haplosomes while requests to the other junkyard create new haplosomes (which can happen because
// haplosomes can be transmogrified between null and non-null after creation).  We create a new haplosome only if both
// junkyards are empty.

Haplosome *Chromosome::_NewHaplosome_NULL(Individual *p_individual)
{
	// this does not set chromosome_subposition_; use NewHaplosome_NULL()
	if (haplosomes_junkyard_nonnull.size())
	{
		Haplosome *back = haplosomes_junkyard_nonnull.back();
		haplosomes_junkyard_nonnull.pop_back();
		
		// got a non-null haplosome, need to repurpose it to be a null haplosome
		back->ReinitializeHaplosomeToNull(p_individual);
		
		return back;
	}
	
	return new (haplosome_pool_.AllocateChunk()) Haplosome(Haplosome::NullHaplosome{}, p_individual, this);
}

Haplosome *Chromosome::_NewHaplosome_NONNULL(Individual *p_individual)
{
	// this does not set chromosome_subposition_; use NewHaplosome_NONNULL()
	if (haplosomes_junkyard_null.size())
	{
		Haplosome *back = haplosomes_junkyard_null.back();
		haplosomes_junkyard_null.pop_back();
		
		// got a null haplosome, need to repurpose it to be a non-null haplosome cleared to nullptr
		back->ReinitializeHaplosomeToNonNull(p_individual, this);
		
		return back;
	}
	
	return new (haplosome_pool_.AllocateChunk()) Haplosome(Haplosome::NonNullHaplosome{}, p_individual, this);
}



//
// Mutation run experiments
//
#pragma mark -
#pragma mark Mutation run experiments
#pragma mark -

void Chromosome::InitiateMutationRunExperiments(void)
{
	if (preferred_mutrun_count_ != 0)
	{
		// If the user supplied a count, go with that and don't run experiments
		x_experiments_enabled_ = false;
		
		if (SLiM_verbosity_level >= 2)
		{
			SLIM_OUTSTREAM << std::endl;
			SLIM_OUTSTREAM << "// Mutation run experiments disabled since a mutation run count was supplied" << std::endl;
		}
		
		return;
	}
	if (mutrun_length_ <= SLIM_MUTRUN_MAXIMUM_COUNT)
	{
		// If the chromosome length is too short, go with that and don't run experiments;
		// we want to guarantee that with SLIM_MUTRUN_MAXIMUM_COUNT runs each mutrun is at
		// least one mutation in length, so the code doesn't break down
		x_experiments_enabled_ = false;
		
		if (SLiM_verbosity_level >= 2)
		{
			SLIM_OUTSTREAM << std::endl;
			SLIM_OUTSTREAM << "// Mutation run experiments disabled since the chromosome is very short" << std::endl;
		}
		
		return;
	}
	
	x_experiments_enabled_ = true;
	species_.DoingMutrunExperimentsForChromosome();
	
	x_experiment_count_ = 0;
	
	x_current_mutcount_ = mutrun_count_;
	x_current_runtimes_ = (double *)malloc(SLIM_MUTRUN_EXPERIMENT_LENGTH * sizeof(double));
	x_current_buflen_ = 0;
	
	x_previous_mutcount_ = 0;			// marks that no previous experiment has been done
	x_previous_runtimes_ = (double *)malloc(SLIM_MUTRUN_EXPERIMENT_LENGTH * sizeof(double));
	x_previous_buflen_ = 0;
	
	if (!x_current_runtimes_ || !x_previous_runtimes_)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitiateMutationRunExperiments): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	x_continuing_trend_ = false;
	
	x_stasis_limit_ = 5;				// once we reach stasis, we will conduct 5 stasis experiments before exploring again
	x_stasis_alpha_ = 0.01;				// initially, we use an alpha of 0.01 to break out of stasis due to a change in mean
	x_stasis_counter_ = 0;
	x_prev1_stasis_mutcount_ = 0;		// we have never reached stasis before, so we have no memory of it
	x_prev2_stasis_mutcount_ = 0;		// we have never reached stasis before, so we have no memory of it
	
	if (SLiM_verbosity_level >= 2)
	{
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "// Mutation run experiments started" << std::endl;
	}
}

void Chromosome::ZeroMutationRunExperimentClock(void)
{
	if (x_experiments_enabled_)
	{
		if (x_within_measurement_period_)
			std::cerr << "WARNING: ZeroMutationRunExperimentClock() called when the measurement period is already begun!" << std::endl;
		
		if (x_total_gen_clocks_ != 0)
		{
			// Clocks should only get logged in the interval within which they are used; if there are leftover counts
			// at this point, somebody is logging counts that are not getting used in the total.  Warn once.
			static bool beenHere = false;
			
			if (!beenHere)
			{
				THREAD_SAFETY_IN_ANY_PARALLEL("Chromosome::PrepareForCycle(): usage of statics");
				
				std::cerr << "WARNING: mutation run experiment clocks were logged outside of the measurement interval!" << std::endl;
				beenHere = true;
			}
			
			x_total_gen_clocks_ = 0;
		}
		
		x_within_measurement_period_ = true;
		
#if MUTRUN_EXPERIMENT_TIMING_OUTPUT
		std::cout << "tick " << community_.Tick() << ", chromosome " << id_ << ": starting timing" << std::endl;
#endif
	}
}

void Chromosome::FinishMutationRunExperimentTiming(void)
{
	if (x_experiments_enabled_)
	{
		if (!x_within_measurement_period_)
			std::cerr << "WARNING: FinishMutationRunExperimentTiming() called when the measurement period has not begun!" << std::endl;
		
#if MUTRUN_EXPERIMENT_TIMING_OUTPUT
		std::cout << "tick " << community_.Tick() << ", chromosome " << id_ << ": ending timing with total count == " << x_total_gen_clocks_ << " (" << Eidos_ElapsedProfileTime(x_total_gen_clocks_) << " seconds)" << std::endl;
#endif
		
		// We only run mutrun experiments in ticks when our species in active; when inactive, any
		// clocks accumulated will simply be discarded unused, since they are not representative
		if (species_.Active())
			MaintainMutationRunExperiments(Eidos_ElapsedProfileTime(x_total_gen_clocks_));
		
		x_total_gen_clocks_ = 0;
		x_within_measurement_period_ = false;
	}
}

void Chromosome::TransitionToNewExperimentAgainstCurrentExperiment(int32_t p_new_mutrun_count)
{
	// Save off the old experiment
	x_previous_mutcount_ = x_current_mutcount_;
	std::swap(x_current_runtimes_, x_previous_runtimes_);
	x_previous_buflen_ = x_current_buflen_;
	
	// Set up the next experiment
	x_current_mutcount_ = p_new_mutrun_count;
	x_current_buflen_ = 0;
}

void Chromosome::TransitionToNewExperimentAgainstPreviousExperiment(int32_t p_new_mutrun_count)
{
	// Set up the next experiment
	x_current_mutcount_ = p_new_mutrun_count;
	x_current_buflen_ = 0;
}

void Chromosome::EnterStasisForMutationRunExperiments(void)
{
	if ((x_current_mutcount_ == x_prev1_stasis_mutcount_) || (x_current_mutcount_ == x_prev2_stasis_mutcount_))
	{
		// One of our recent trips to stasis was at the same count, so we broke stasis incorrectly; get stricter.
		// The purpose for keeping two previous counts is to detect when we are ping-ponging between two values
		// that produce virtually identical performance; we want to detect that and just settle on one of them.
		x_stasis_alpha_ *= 0.5;
		x_stasis_limit_ *= 2;
		
#if MUTRUN_EXPERIMENT_OUTPUT
		if (SLiM_verbosity_level >= 2)
			SLIM_OUTSTREAM << "// Remembered previous stasis at " << x_current_mutcount_ << ", strengthening stasis criteria" << std::endl;
#endif
	}
	else
	{
		// Our previous trips to stasis were at a different number of mutation runs, so reset our stasis parameters
		x_stasis_limit_ = 5;
		x_stasis_alpha_ = 0.01;
		
#if MUTRUN_EXPERIMENT_OUTPUT
		if (SLiM_verbosity_level >= 2)
			SLIM_OUTSTREAM << "// No memory of previous stasis at " << x_current_mutcount_ << ", resetting stasis criteria" << std::endl;
#endif
	}
	
	x_stasis_counter_ = 1;
	x_continuing_trend_ = false;
	
	// Preserve a memory of the last two *different* mutcounts we entered stasis on.  Only forget the old value
	// in x_prev2_stasis_mutcount_ if x_prev1_stasis_mutcount_ is about to get a new and different value.
	// This makes the anti-ping-pong mechanism described above effective even if we ping-pong irregularly.
	if (x_prev1_stasis_mutcount_ != x_current_mutcount_)
		x_prev2_stasis_mutcount_ = x_prev1_stasis_mutcount_;
	x_prev1_stasis_mutcount_ = x_current_mutcount_;
	
#if MUTRUN_EXPERIMENT_OUTPUT
	if (SLiM_verbosity_level >= 2)
		SLIM_OUTSTREAM << "// ****** ENTERING STASIS AT " << x_current_mutcount_ << " : x_stasis_limit_ = " << x_stasis_limit_ << ", x_stasis_alpha_ = " << x_stasis_alpha_ << std::endl;
#endif
}

void Chromosome::MaintainMutationRunExperiments(double p_last_gen_runtime)
{
	// Log the last cycle time into our buffer
	if (x_current_buflen_ >= SLIM_MUTRUN_EXPERIMENT_LENGTH)
		EIDOS_TERMINATION << "ERROR (Chromosome::MaintainMutationRunExperiments): Buffer overrun, failure to reset after completion of an experiment." << EidosTerminate();
	
	x_current_runtimes_[x_current_buflen_] = p_last_gen_runtime;
	
	// Remember the history of the mutation run count
	x_mutcount_history_.emplace_back(x_current_mutcount_);
	
	// If the current experiment is not over, continue running it
	++x_current_buflen_;
	
	double current_mean = 0.0, previous_mean = 0.0, p = 0.0;
	
	if ((x_current_buflen_ == 10) && (x_current_mutcount_ != x_previous_mutcount_) && (x_previous_mutcount_ != 0))
	{
		// We want to be able to cut an experiment short if it is clearly a disaster.  So if we're not in stasis, and
		// we've run for 10 cycles, and the experiment mean is already different from the baseline at alpha 0.01,
		// and the experiment mean is worse than the baseline mean (if it is better, we want to continue collecting),
		// let's short-circuit the rest of the experiment and bail – like early termination of a medical trial.
		p = Eidos_TTest_TwoSampleWelch(x_current_runtimes_, x_current_buflen_, x_previous_runtimes_, x_previous_buflen_, &current_mean, &previous_mean);
		x_experiment_count_++;
		
		if ((p < 0.01) && (current_mean > previous_mean))
		{
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbosity_level >= 2)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << cycle_ << " : Early t-test yielded HIGHLY SIGNIFICANT p of " << p << " with negative results; terminating early." << std::endl;
			}
#endif
			
			goto early_ttest_passed;
		}
#if MUTRUN_EXPERIMENT_OUTPUT
		else if (SLiM_verbosity_level >= 2)
		{
			if (p >= 0.01)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << cycle_ << " : Early t-test yielded not highly significant p of " << p << "; continuing." << std::endl;
			}
			else if (current_mean > previous_mean)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << cycle_ << " : Early t-test yielded highly significant p of " << p << " with positive results; continuing data collection." << std::endl;
			}
		}
#endif
	}
	
	if (x_current_buflen_ < SLIM_MUTRUN_EXPERIMENT_LENGTH)
		return;
	
	if (x_previous_mutcount_ == 0)
	{
		// FINISHED OUR FIRST EXPERIMENT; move on to the next experiment, which is always double the number of mutruns
#if MUTRUN_EXPERIMENT_OUTPUT
		if (SLiM_verbosity_level >= 2)
		{
			SLIM_OUTSTREAM << std::endl;
			SLIM_OUTSTREAM << "// ** " << cycle_ << " : First mutation run experiment completed with mutrun count " << x_current_mutcount_ << "; will now try " << (x_current_mutcount_ * 2) << std::endl;
		}
#endif
		
		TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ * 2);
	}
	else
	{
		// If we've just finished the second stasis experiment, run another stasis experiment before trying to draw any
		// conclusions.  We often enter stasis with one cycle's worth of data that was actually collected quite a
		// while ago, because we did exploration in both directions first.  This can lead to breaking out of stasis
		// immediately after entering, because we're comparing apples and oranges.  So we avoid doing that here.
		if ((x_stasis_counter_ <= 1) && (x_current_mutcount_ == x_previous_mutcount_))
		{
			TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_);
			++x_stasis_counter_;
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbosity_level >= 2)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << cycle_ << " : Mutation run experiment completed (second stasis cycle, no tests conducted)" << std::endl;
			}
#endif
			
			return;
		}
		
		// Otherwise, get a result from a t-test and decide what to do
		p = Eidos_TTest_TwoSampleWelch(x_current_runtimes_, x_current_buflen_, x_previous_runtimes_, x_previous_buflen_, &current_mean, &previous_mean);
		x_experiment_count_++;
		
	early_ttest_passed:
		
#if MUTRUN_EXPERIMENT_OUTPUT
		if (SLiM_verbosity_level >= 2)
		{
			SLIM_OUTSTREAM << std::endl;
			SLIM_OUTSTREAM << "// " << cycle_ << " : Mutation run experiment completed:" << std::endl;
			SLIM_OUTSTREAM << "//    mean == " << current_mean << " for " << x_current_mutcount_ << " mutruns (" << x_current_buflen_ << " data points)" << std::endl;
			SLIM_OUTSTREAM << "//    mean == " << previous_mean << " for " << x_previous_mutcount_ << " mutruns (" << x_previous_buflen_ << " data points)" << std::endl;
		}
#endif
		
		if (x_current_mutcount_ == x_previous_mutcount_)	// are we in stasis?
		{
			//
			// FINISHED A STASIS EXPERIMENT; unless we have changed at alpha = 0.01 we stay put
			//
			bool means_different_stasis = (p < x_stasis_alpha_);
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbosity_level >= 2)
				SLIM_OUTSTREAM << "//    p == " << p << " : " << (means_different_stasis ? "SIGNIFICANT DIFFERENCE" : "no significant difference") << " at stasis alpha " << x_stasis_alpha_ << std::endl;
#endif
			
			if (means_different_stasis)
			{
				// OK, it looks like something has changed about our scenario, so we should come out of stasis and re-test.
				// We don't have any information about the new state of affairs, so we have no directional preference.
				// Let's try a larger number of mutation runs first, since haplosomes tend to fill up, unless we're at the max.
				if (x_current_mutcount_ * 2 > SLIM_MUTRUN_MAXIMUM_COUNT)
					TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ / 2);
				else
					TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ * 2);
				
#if MUTRUN_EXPERIMENT_OUTPUT
				if (SLiM_verbosity_level >= 2)
					SLIM_OUTSTREAM << "// ** " << cycle_ << " : Stasis mean changed, EXITING STASIS and trying new mutcount of " << x_current_mutcount_ << std::endl;
#endif
			}
			else
			{
				// We seem to be in a constant scenario.  Increment our stasis counter and see if we have reached our stasis limit
				if (++x_stasis_counter_ >= x_stasis_limit_)
				{
					// We reached the stasis limit, so we will try an experiment even though we don't seem to have changed;
					// as before, we try more mutation runs first, since increasing genetic complexity is typical
					if (x_current_mutcount_ * 2 > SLIM_MUTRUN_MAXIMUM_COUNT)
						TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ / 2);
					else
						TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ * 2);
					
#if MUTRUN_EXPERIMENT_OUTPUT
					if (SLiM_verbosity_level >= 2)
						SLIM_OUTSTREAM << "// ** " << cycle_ << " : Stasis limit reached, EXITING STASIS and trying new mutcount of " << x_current_mutcount_ << std::endl;
#endif
				}
				else
				{
					// We have not yet reached the stasis limit, so run another stasis experiment.
					// In this case we don't do a transition; we want to continue comparing against the original experiment
					// data so that if stasis slowly drift away from us, we eventually detect that as a change in stasis.
					x_current_buflen_ = 0;
					
#if MUTRUN_EXPERIMENT_OUTPUT
					if (SLiM_verbosity_level >= 2)
						SLIM_OUTSTREAM << "//    " << cycle_ << " : Stasis limit not reached (" << x_stasis_counter_ << " of " << x_stasis_limit_ << "), running another stasis experiment at " << x_current_mutcount_ << std::endl;
#endif
				}
			}
		}
		else
		{
			//
			// FINISHED A NON-STASIS EXPERIMENT; trying a move toward more/fewer mutruns
			//
			double alpha = 0.05;
			bool means_different_05 = (p < alpha);
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbosity_level >= 2)
				SLIM_OUTSTREAM << "//    p == " << p << " : " << (means_different_05 ? "SIGNIFICANT DIFFERENCE" : "no significant difference") << " at alpha " << alpha << std::endl;
#endif
			
			int32_t trend_next = (x_current_mutcount_ < x_previous_mutcount_) ? (x_current_mutcount_ / 2) : (x_current_mutcount_ * 2);
			int32_t trend_limit = (x_current_mutcount_ < x_previous_mutcount_) ? mutrun_count_base_ : SLIM_MUTRUN_MAXIMUM_COUNT;	// for single-threaded, mutrun_count_base_ == 1
			
			if ((current_mean < previous_mean) || (!means_different_05 && (x_current_mutcount_ < x_previous_mutcount_)))
			{
				// We enter this case under two different conditions.  The first is that the new mean is better
				// than the old mean; whether that is significant or not, we want to continue in the same direction
				// with a new experiment, which is what we do here.  The other case is if the new mean is worse
				// than the old mean, but the difference is non-significant *and* we're trending toward fewer
				// mutation runs.  We treat that the same way: continue with a new experiment in the same direction.
				// But if the new mean is worse that the old mean and we're trending toward more mutation runs,
				// we do NOT follow this case, because an inconclusive but negative increasing trend pushes up our
				// peak memory usage and can be quite inefficient, and usually we just jump back down anyway.
				// BCH 8/14/2023: The if() below is intended to diagnose if trend_next will go beyond trend_limit,
				// and is thus not a legal move.  Just testing (x_current_mutcount_ == trend_limit) used to suffice,
				// because the base count was always a power of 2.  Now that is no longer true, and so we can, e.g.,
				// be at 768 and thinking about doubling to 1536.  We test for going beyond SLIM_MUTRUN_MAXIMUM_COUNT
				// explicitly now, to address that case.  Going too low is still effectively prevented, since we
				// will always reach the base count exactly before going below it.
				if ((x_current_mutcount_ == trend_limit) || (trend_next > SLIM_MUTRUN_MAXIMUM_COUNT))
				{
					if (current_mean < previous_mean)
					{
						// Can't go beyond the trend limit (1 or SLIM_MUTRUN_MAXIMUM_COUNT), so we're done; ****** ENTER STASIS
						// We keep the current experiment as the first stasis experiment.
						TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_);
						
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbosity_level >= 2)
							SLIM_OUTSTREAM << "// ****** " << cycle_ << " : Experiment " << (means_different_05 ? "successful" : "inconclusive but positive") << " at " << x_previous_mutcount_ << ", nowhere left to go; entering stasis at " << x_current_mutcount_ << "." << std::endl;
#endif
						
						EnterStasisForMutationRunExperiments();
					}
					else
					{
						// The means are not significantly different, and the current experiment is worse than the
						// previous one, and we can't go beyond the trend limit, so we're done; ****** ENTER STASIS
						// We keep the previous experiment as the first stasis experiment.
						TransitionToNewExperimentAgainstPreviousExperiment(x_previous_mutcount_);
						
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbosity_level >= 2)
							SLIM_OUTSTREAM << "// ****** " << cycle_ << " : Experiment " << (means_different_05 ? "failed" : "inconclusive but negative") << " at " << x_previous_mutcount_ << ", nowhere left to go; entering stasis at " << x_current_mutcount_ << "." << std::endl;
#endif
						
						EnterStasisForMutationRunExperiments();
					}
				}
				else
				{
					if (current_mean < previous_mean)
					{
						// Even if the difference is not significant, we appear to be moving in a beneficial direction,
						// so we will run the next experiment against the current experiment's results
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbosity_level >= 2)
							SLIM_OUTSTREAM << "// ** " << cycle_ << " : Experiment " << (means_different_05 ? "successful" : "inconclusive but positive") << " at " << x_current_mutcount_ << " (against " << x_previous_mutcount_ << "), continuing trend with " << trend_next << " (against " << x_current_mutcount_ << ")" << std::endl;
#endif
						
						TransitionToNewExperimentAgainstCurrentExperiment(trend_next);
						x_continuing_trend_ = true;
					}
					else
					{
						// The difference is not significant, but we might be moving in a bad direction, and a series
						// of such moves, each non-significant against the previous experiment, can lead us way down
						// the garden path.  To make sure that doesn't happen, we run successive inconclusive experiments
						// against whichever preceding experiment had the lowest mean.
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbosity_level >= 2)
							SLIM_OUTSTREAM << "// ** " << cycle_ << " : Experiment inconclusive but negative at " << x_current_mutcount_ << " (against " << x_previous_mutcount_ << "), checking " << trend_next << " (against " << x_previous_mutcount_ << ")" << std::endl;
#endif
						
						TransitionToNewExperimentAgainstPreviousExperiment(trend_next);
					}
				}
			}
			else
			{
				// The old mean was better, and either the difference is significant or the trend is toward more mutation
				// runs, so we want to give up on this trend and go back
				if (x_continuing_trend_)
				{
					// We already tried a step on the opposite side of the old position, so the old position appears ideal; ****** ENTER STASIS.
					// We throw away the current, failed experiment and keep the last experiment at the previous position as the first stasis experiment.
					TransitionToNewExperimentAgainstPreviousExperiment(x_previous_mutcount_);
					
#if MUTRUN_EXPERIMENT_OUTPUT
					if (SLiM_verbosity_level >= 2)
						SLIM_OUTSTREAM << "// ****** " << cycle_ << " : Experiment failed, already tried opposite side, so " << x_current_mutcount_ << " appears optimal; entering stasis at " << x_current_mutcount_ << "." << std::endl;
#endif
					
					EnterStasisForMutationRunExperiments();
				}
				else
				{
					// We have not tried a step on the opposite side of the old position; let's return to our old position,
					// which we know is better than the position we just ran an experiment at, and then advance onward to
					// run an experiment at the next position in that reversed trend direction.
					int32_t new_mutcount = ((x_current_mutcount_ > x_previous_mutcount_) ? (x_previous_mutcount_ / 2) : (x_previous_mutcount_ * 2));
					
					if ((x_previous_mutcount_ == mutrun_count_base_) || (x_previous_mutcount_ == SLIM_MUTRUN_MAXIMUM_COUNT) ||
						(new_mutcount < mutrun_count_base_) || (new_mutcount > SLIM_MUTRUN_MAXIMUM_COUNT))
					{
						// can't jump over the previous mutcount, so we enter stasis at it
						TransitionToNewExperimentAgainstPreviousExperiment(x_previous_mutcount_);
						
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbosity_level >= 2)
							SLIM_OUTSTREAM << "// ****** " << cycle_ << " : Experiment failed, opposite side blocked so " << x_current_mutcount_ << " appears optimal; entering stasis at " << x_current_mutcount_ << "." << std::endl;
#endif
						
						EnterStasisForMutationRunExperiments();
					}
					else
					{
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbosity_level >= 2)
							SLIM_OUTSTREAM << "// ** " << cycle_ << " : Experiment failed at " << x_current_mutcount_ << ", opposite side untried, reversing trend back to " << new_mutcount << " (against " << x_previous_mutcount_ << ")" << std::endl;
#endif
						
						TransitionToNewExperimentAgainstPreviousExperiment(new_mutcount);
						x_continuing_trend_ = true;
					}
				}
			}
		}
	}
	
	// Promulgate the new mutation run count
	if (x_current_mutcount_ != mutrun_count_)
	{
		// Fix all haplosomes.  We could do this by brute force, by making completely new mutation runs for every
		// existing haplosome and then calling Population::UniqueMutationRuns(), but that would be inefficient,
		// and would also cause a huge memory usage spike.  Instead, we want to preserve existing redundancy.
		
		while (x_current_mutcount_ > mutrun_count_)
		{
#if MUTRUN_EXPERIMENT_OUTPUT
			std::clock_t start_clock = std::clock();
#endif
			
			if (x_current_mutcount_ > SLIM_MUTRUN_MAXIMUM_COUNT)
				EIDOS_TERMINATION << "ERROR (Chromosome::MaintainMutationRunExperiments): (internal error) splitting mutation runs to beyond SLIM_MUTRUN_MAXIMUM_COUNT (x_current_mutcount_ == " << x_current_mutcount_ << ")." << EidosTerminate();
			
			// We are splitting existing runs in two, so make a map from old mutrun index to new pair of
			// mutrun indices; every time we encounter the same old index we will substitute the same pair.
			species_.population_.SplitMutationRunsForChromosome(mutrun_count_ * 2, this);
			
			// Fix the chromosome values
			mutrun_count_multiplier_ *= 2;
			mutrun_count_ *= 2;
			mutrun_length_ /= 2;
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbosity_level >= 2)
				SLIM_OUTSTREAM << "// ++ Splitting to achieve new mutation run count of " << mutrun_count_ << " took " << ((std::clock() - start_clock) / (double)CLOCKS_PER_SEC) << " seconds" << std::endl;
#endif
			
#if DEBUG
		community_.AllSpecies_CheckIntegrity();
#endif
		}
		
		while (x_current_mutcount_ < mutrun_count_)
		{
#if MUTRUN_EXPERIMENT_OUTPUT
			std::clock_t start_clock = std::clock();
#endif
			
			if (mutrun_count_multiplier_ % 2 != 0)
				EIDOS_TERMINATION << "ERROR (Chromosome::MaintainMutationRunExperiments): (internal error) joining mutation runs to beyond mutrun_count_base_ (mutrun_count_base_ == " << mutrun_count_base_ << ", x_current_mutcount_ == " << x_current_mutcount_ << ")." << EidosTerminate();
			
			// We are joining existing runs together, so make a map from old mutrun index pairs to a new
			// index; every time we encounter the same pair of indices we will substitute the same index.
			species_.population_.JoinMutationRunsForChromosome(mutrun_count_ / 2, this);
			
			// Fix the chromosome values
			mutrun_count_multiplier_ /= 2;
			mutrun_count_ /= 2;
			mutrun_length_ *= 2;
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbosity_level >= 2)
				SLIM_OUTSTREAM << "// ++ Joining to achieve new mutation run count of " << mutrun_count_ << " took " << ((std::clock() - start_clock) / (double)CLOCKS_PER_SEC) << " seconds" << std::endl;
#endif
		}
		
		if (mutrun_count_ != x_current_mutcount_)
			EIDOS_TERMINATION << "ERROR (Chromosome::MaintainMutationRunExperiments): Failed to transition to new mutation run count" << x_current_mutcount_ << "." << EidosTerminate();
		
#if DEBUG
		community_.AllSpecies_CheckIntegrity();
#endif
	}
}

void Chromosome::PrintMutationRunExperimentSummary(void)
{
#if MUTRUN_EXPERIMENT_OUTPUT
	// Print a full mutation run count history if MUTRUN_EXPERIMENT_OUTPUT is enabled
	if (x_experiments_enabled_)
	{
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "// Chromosome " << id_ << " mutrun count history:" << std::endl;
		SLIM_OUTSTREAM << "// mutrun_history <- c(";
		
		bool first_count = true;
		
		for (int32_t count : x_mutcount_history_)
		{
			if (first_count)
				first_count = false;
			else
				SLIM_OUTSTREAM << ", ";
			
			SLIM_OUTSTREAM << count;
		}
		
		SLIM_OUTSTREAM << ")" << std::endl << std::endl;
	}
#endif
	
	// If verbose output is enabled and we've been running mutation run experiments,
	// figure out the modal mutation run count and print that, for the user's benefit.
	if (x_experiments_enabled_)
	{
		int modal_index, modal_tally;
		int power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
		
		for (int i = 0; i < 20; ++i)		// NOLINT(*-loop-convert) : parallel to the loop below
			power_tallies[i] = 0;
		
		for (int32_t count : x_mutcount_history_)
		{
			int32_t power = (int32_t)round(log2(count));
			
			power_tallies[power]++;
		}
		
		modal_index = -1;
		modal_tally = -1;
		
		for (int i = 0; i < 20; ++i)
			if (power_tallies[i] > modal_tally)
			{
				modal_tally = power_tallies[i];
				modal_index = i;
			}
		
		int modal_count = (int)round(pow(2.0, modal_index));
		double modal_fraction = power_tallies[modal_index] / (double)(x_mutcount_history_.size());
		
		SLIM_OUTSTREAM << "// Chromosome " << id_ << ": " << modal_count << " (" << (modal_fraction * 100) << "% of cycles, " << x_experiment_count_ << " experiments)" << std::endl;
	}
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

// These are private helper methods that error if we are still in initialize() callbacks
// Most properties and all methods on Chromosome should be protected by these calls to prevent bugs
void Chromosome::CheckPartialInitializationForProperty(EidosGlobalStringID p_property_id)
{
	if (community_.Tick() == 0)
		EIDOS_TERMINATION << "ERROR (Chromosome::CheckPartialInitializationForProperty): Chromosome property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " cannot be accessed until initialize() callbacks are complete; the chromosome object is not yet fully initialized." << EidosTerminate();
}
void Chromosome::CheckPartialInitializationForMethod(EidosGlobalStringID p_method_id)
{
	if (community_.Tick() == 0)
		EIDOS_TERMINATION << "ERROR (Chromosome::CheckPartialInitializationForProperty): Chromosome method " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() cannot be called until initialize() callbacks are complete; the chromosome object is not yet fully initialized." << EidosTerminate();
}

const EidosClass *Chromosome::Class(void) const
{
	return gSLiM_Chromosome_Class;
}

void Chromosome::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<" << symbol_ << ">";
}

EidosValue_SP Chromosome::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_genomicElements:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			EidosValue_Object *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_GenomicElement_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (GenomicElement *genomic_element : genomic_elements_)
				vec->push_object_element_NORR(genomic_element);
			
			return result_SP;
		}
		case gID_id:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(id_));
		}
		case gID_isSexChromosome:
		{
			return is_sex_chromosome_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
		}
		case gID_intrinsicPloidy:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(intrinsic_ploidy_));
		}
		case gID_lastPosition:
		{
			if (!extent_immutable_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property lastPosition is not yet defined, since the length of the target chromosome has not yet been determined; you could provide a specified length to initializeChromosome(), or avoid requesting the chromosome's lastPosition until the chromosome's initialization has been finalized (after the execution of initialize() callbacks)." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(last_position_));
		}
		case gEidosID_length:
		{
			if (!extent_immutable_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property lastPosition is not yet defined, since the length of the target chromosome has not yet been determined; you could provide a specified length to initializeChromosome(), or avoid requesting the chromosome's length until the chromosome's initialization has been finalized (after the execution of initialize() callbacks)." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(last_position_ + 1));
		}
		case gID_species:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(&species_, gSLiM_Species_Class));
		}
		case gID_symbol:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(symbol_));
		}
		case gEidosID_type:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(type_string_));
		}
			
		case gID_hotspotEndPositions:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositions is only defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositions is not defined since sex-specific hotspot maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(hotspot_end_positions_H_));
		}
		case gID_hotspotEndPositionsM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsM is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsM is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(hotspot_end_positions_M_));
		}
		case gID_hotspotEndPositionsF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsF is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsF is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(hotspot_end_positions_F_));
		}
			
		case gID_hotspotMultipliers:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliers is only defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliers is not defined since sex-specific hotspot maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(hotspot_multipliers_H_));
		}
		case gID_hotspotMultipliersM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersM is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersM is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(hotspot_multipliers_M_));
		}
		case gID_hotspotMultipliersF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersF is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersF is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(hotspot_multipliers_F_));
		}
			
		case gID_mutationEndPositions:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositions is not defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositions is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_end_positions_H_));
		}
		case gID_mutationEndPositionsM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsM is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_end_positions_M_));
		}
		case gID_mutationEndPositionsF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsF is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_end_positions_F_));
		}
			
		case gID_mutationRates:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRates is not defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRates is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mutation_rates_H_));
		}
		case gID_mutationRatesM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesM is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mutation_rates_M_));
		}
		case gID_mutationRatesF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesF is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mutation_rates_F_));
		}
			
		case gID_overallMutationRate:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRate is not defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRate is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(overall_mutation_rate_H_userlevel_));
		}
		case gID_overallMutationRateM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateM is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(overall_mutation_rate_M_userlevel_));
		}
		case gID_overallMutationRateF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (species_.IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateF is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(overall_mutation_rate_F_userlevel_));
		}
			
		case gID_overallRecombinationRate:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRate is not defined since sex-specific recombination rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(overall_recombination_rate_H_userlevel_));
		}
		case gID_overallRecombinationRateM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRateM is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(overall_recombination_rate_M_userlevel_));
		}
		case gID_overallRecombinationRateF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRateF is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(overall_recombination_rate_F_userlevel_));
		}
			
		case gID_recombinationEndPositions:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationEndPositions is not defined since sex-specific recombination rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(recombination_end_positions_H_));
		}
		case gID_recombinationEndPositionsM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationEndPositionsM is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(recombination_end_positions_M_));
		}
		case gID_recombinationEndPositionsF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationEndPositionsF is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(recombination_end_positions_F_));
		}
			
		case gID_recombinationRates:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationRates is not defined since sex-specific recombination rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(recombination_rates_H_));
		}
		case gID_recombinationRatesM:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationRatesM is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(recombination_rates_M_));
		}
		case gID_recombinationRatesF:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationRatesF is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(recombination_rates_F_));
		}
			
			// variables
		case gID_colorSubstitution:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(color_sub_));
		}
		case gID_geneConversionEnabled:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			return using_DSB_model_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
		}
		case gID_geneConversionGCBias:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionGCBias is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mismatch_repair_bias_));
		}
		case gID_geneConversionNonCrossoverFraction:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionNonCrossoverFraction is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(non_crossover_fraction_));
		}
		case gID_geneConversionMeanLength:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionMeanLength is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(gene_conversion_avg_length_));
		}
		case gID_geneConversionSimpleConversionFraction:
		{
			CheckPartialInitializationForProperty(p_property_id);
			
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionSimpleConversionFraction is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(simple_conversion_fraction_));
		}
		case gID_name:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(name_));
		}
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property tag accessed on chromosome before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void Chromosome::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_colorSubstitution:
		{
			color_sub_ = p_value.StringAtIndex_NOCAST(0, nullptr);
			if (!color_sub_.empty())
				Eidos_GetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
			return;
		}
		case gID_name:
		{
			name_ = p_value.StringAtIndex_NOCAST(0, nullptr);
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return super::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP Chromosome::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_ancestralNucleotides:			return ExecuteMethod_ancestralNucleotides(p_method_id, p_arguments, p_interpreter);
		case gID_genomicElementForPosition:		return ExecuteMethod_genomicElementForPosition(p_method_id, p_arguments, p_interpreter);
		case gID_hasGenomicElementForPosition:	return ExecuteMethod_hasGenomicElementForPosition(p_method_id, p_arguments, p_interpreter);
		case gID_setAncestralNucleotides:		return ExecuteMethod_setAncestralNucleotides(p_method_id, p_arguments, p_interpreter);
		case gID_setGeneConversion:				return ExecuteMethod_setGeneConversion(p_method_id, p_arguments, p_interpreter);
		case gID_setHotspotMap:					return ExecuteMethod_setHotspotMap(p_method_id, p_arguments, p_interpreter);
		case gID_setMutationRate:				return ExecuteMethod_setMutationRate(p_method_id, p_arguments, p_interpreter);
		case gID_setRecombinationRate:			return ExecuteMethod_setRecombinationRate(p_method_id, p_arguments, p_interpreter);
		case gID_drawBreakpoints:				return ExecuteMethod_drawBreakpoints(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (is)ancestralNucleotides([Ni$ start = NULL], [Ni$ end = NULL], [s$ format = "string"])
//
EidosValue_SP Chromosome::ExecuteMethod_ancestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	// The ancestral sequence is actually kept by Species, so go get it
	if (!species_.IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): ancestralNucleotides() may only be called in nucleotide-based models." << EidosTerminate();
	
	NucleotideArray *sequence = AncestralSequence();
	EidosValue *start_value = p_arguments[0].get();
	EidosValue *end_value = p_arguments[1].get();
	
	int64_t start = (start_value->Type() == EidosValueType::kValueNULL) ? 0 : start_value->IntAtIndex_NOCAST(0, nullptr);
	int64_t end = (end_value->Type() == EidosValueType::kValueNULL) ? last_position_ : end_value->IntAtIndex_NOCAST(0, nullptr);
	
	if ((start < 0) || (end < 0) || (start > last_position_) || (end > last_position_) || (start > end))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): start and end must be within the chromosome's extent, and start must be <= end." << EidosTerminate();
	if (((std::size_t)start >= sequence->size()) || ((std::size_t)end >= sequence->size()))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): (internal error) start and end must be within the ancestral sequence's length." << EidosTerminate();
	
	int64_t length = end - start + 1;
	
	if (length > INT_MAX)
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): the returned vector would exceed the maximum vector length in Eidos." << EidosTerminate();
	
	EidosValue_String *format_value = (EidosValue_String *)p_arguments[2].get();
	const std::string &format = format_value->StringRefAtIndex_NOCAST(0, nullptr);
	
	if (format == "codon")
		return sequence->NucleotidesAsCodonVector(start, end, /* p_force_vector */ false);
	if (format == "string")
		return sequence->NucleotidesAsStringSingleton(start, end);
	if (format == "integer")
		return sequence->NucleotidesAsIntegerVector(start, end);
	if (format == "char")
		return sequence->NucleotidesAsStringVector(start, end);
	
	EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): parameter format must be either 'string', 'char', 'integer', or 'codon'." << EidosTerminate();
}

//	*********************	– (integer)drawBreakpoints([No<Individual>$ parent = NULL], [Ni$ n = NULL])
//
EidosValue_SP Chromosome::ExecuteMethod_drawBreakpoints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	EidosValue *parent_value = p_arguments[0].get();
	EidosValue *n_value = p_arguments[1].get();
	
	Individual *parent = nullptr;
	
	if (parent_value->Type() != EidosValueType::kValueNULL)
		parent = (Individual *)parent_value->ObjectElementAtIndex_NOCAST(0, nullptr);
	
	int num_breakpoints = -1;	// means "draw them for us"
	
	if (n_value->Type() != EidosValueType::kValueNULL)
	{
		int64_t n = n_value->IntAtIndex_NOCAST(0, nullptr);
		
		if ((n < 0) || (n > 1000000L))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_drawBreakpoints): drawBreakpoints() requires 0 <= n <= 1000000." << EidosTerminate();
		
		num_breakpoints = (int)n;
	}
	
	std::vector<slim_position_t> all_breakpoints;
	
	// Note that for calling recombination() callbacks below, we always treat the parent's first haplosome as
	// the initial copy strand.  This is documented; it is perhaps a weakness of the API here, but if we
	// randomly chose an initial copy strand it would not be used downstream, so.
	// FIXME an idea: a new parameter, [l$ randomizeStrands = F], could be added that, if true, would
	// put a breakpoint at 0 half of the time, regardless of recombination rate, so the initial copy
	// strand is randomized for anyone using the generated breakpoints.  This solves the problem, a
	// little bit clunkily.  The main client of this method is users of addRecombinant(), though, and
	// it now has its own randomizeStrands flag, so maybe this change is unnecessary?
	
	DrawBreakpoints(parent, /* p_haplosome1 */ nullptr, /* p_haplosome2 */ nullptr, num_breakpoints,
					all_breakpoints, /* p_heteroduplex */ nullptr, "drawBreakpoints()");
	
	if (all_breakpoints.size() == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	else
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(all_breakpoints));
}

GenomicElement *Chromosome::ElementForPosition(slim_position_t pos)
{
	auto element_iter = std::lower_bound(genomic_elements_.begin(), genomic_elements_.end(), pos,
		[](const GenomicElement *e, slim_position_t position) { return e->end_position_ < position; });
	
	if (element_iter == genomic_elements_.end())
		return nullptr;
	
	GenomicElement *element = *element_iter;
	
	if (element->start_position_ > pos)
		return nullptr;
	
	return element;
}

//	*********************	(object<GenomicElement>)genomicElementForPosition(integer positions)
//
EidosValue_SP Chromosome::ExecuteMethod_genomicElementForPosition(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	EidosValue *positions_value = p_arguments[0].get();
	int positions_count = positions_value->Count();
	EidosValue_Object_SP obj_result_SP = EidosValue_Object_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_GenomicElement_Class));
	EidosValue_Object *obj_result = obj_result_SP->reserve(positions_count);
	const int64_t *positions_data = positions_value->IntData();
	
	for (int pos_index = 0; pos_index < positions_count; ++pos_index)
	{
		int64_t pos = positions_data[pos_index];
		GenomicElement *element = ElementForPosition(pos);
		
		if (element)
			obj_result->push_object_element_no_check_NORR(element);
	}
	
	return obj_result_SP;
}

//	*********************	(logical)hasGenomicElementForPosition(integer positions)
//
EidosValue_SP Chromosome::ExecuteMethod_hasGenomicElementForPosition(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	EidosValue *positions_value = p_arguments[0].get();
	int positions_count = positions_value->Count();
	EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
	EidosValue_Logical *logical_result = logical_result_SP->reserve(positions_count);
	const int64_t *positions_data = positions_value->IntData();
	
	for (int pos_index = 0; pos_index < positions_count; ++pos_index)
	{
		int64_t pos = positions_data[pos_index];
		logical_result->push_logical_no_check(ElementForPosition(pos) ? true : false);
	}
	
	return logical_result_SP;
}

//	*********************	(integer$)setAncestralNucleotides(is sequence)
//
EidosValue_SP Chromosome::ExecuteMethod_setAncestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	EidosValue *sequence_value = p_arguments[0].get();
	
	if (!species_.IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): initializeAncestralNucleotides() may be only be called in nucleotide-based models." << EidosTerminate();
	
	EidosValueType sequence_value_type = sequence_value->Type();
	int sequence_value_count = sequence_value->Count();
	
	if (sequence_value_count == 0)
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): initializeAncestralNucleotides() requires a sequence of length >= 1." << EidosTerminate();
	
	if (ancestral_seq_buffer_)
	{
		delete ancestral_seq_buffer_;
		ancestral_seq_buffer_ = nullptr;
	}
	
	if (sequence_value_type == EidosValueType::kValueInt)
	{
		// A vector of integers has been provided, where ACGT == 0123
		const int64_t *int_data = sequence_value->IntData();
		
		ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, int_data);
	}
	else if (sequence_value_type == EidosValueType::kValueString)
	{
		if (sequence_value_count != 1)
		{
			// A vector of characters has been provided, which must all be "A" / "C" / "G" / "T"
			const std::string *string_data = sequence_value->StringData();
			
			ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, string_data);
		}
		else	// sequence_value_count == 1
		{
			const std::string &sequence_string = sequence_value->StringData()[0];
			bool contains_only_nuc = true;
			
			// OK, we do a weird thing here.  We want to try to construct a NucleotideArray
			// from sequence_string, which throws with EIDOS_TERMINATION if it fails, but
			// we want to actually catch that exception even if we're running at the
			// command line, where EIDOS_TERMINATION normally calls exit().  So we actually
			// play around with the error-handling state to make it do what we want it to do.
			// This is very naughty and should be redesigned, but right now I'm not seeing
			// the right redesign strategy, so... hacking it for now.  Parallel code is at
			// Species::ExecuteContextFunction_initializeAncestralNucleotides()
			bool save_gEidosTerminateThrows = gEidosTerminateThrows;
			gEidosTerminateThrows = true;
			
			try {
				ancestral_seq_buffer_ = new NucleotideArray(sequence_string.length(), sequence_string.c_str());
			} catch (...) {
				contains_only_nuc = false;
				
				// clean up the error state since we don't want this throw to be reported
				gEidosTermination.clear();
				gEidosTermination.str("");
			}
			
			gEidosTerminateThrows = save_gEidosTerminateThrows;
			
			if (!contains_only_nuc)
			{
				// A singleton string has been provided that contains characters other than ACGT; we will interpret it as a filesystem path for a FASTA file
				std::string file_path = Eidos_ResolvedPath(sequence_string);
				std::ifstream file_stream(file_path.c_str());
				
				if (!file_stream.is_open())
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): the file at path " << sequence_string << " could not be opened or does not exist." << EidosTerminate();
				
				bool started_sequence = false;
				std::string line, fasta_sequence;
				
				while (getline(file_stream, line))
				{
					// skippable lines are blank or start with a '>' or ';'
					// we skip over them if they're at the start of the file; once we start a sequence, they terminate the sequence
					bool skippable = ((line.length() == 0) || (line[0] == '>') || (line[0] == ';'));
					
					if (!started_sequence && skippable)
						continue;
					if (skippable)
						break;
					
					// otherwise, append the nucleotides from this line, removing a \r if one is present at the end of the line
					if (line.back() == '\r')
						line.pop_back();
					
					fasta_sequence.append(line);
					started_sequence = true;
				}
				
				if (file_stream.bad())
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): a filesystem error occurred while reading the file at path " << sequence_string << "." << EidosTerminate();
				
				if (fasta_sequence.length() == 0)
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): no FASTA sequence found in " << sequence_string << "." << EidosTerminate();
				
				ancestral_seq_buffer_ = new NucleotideArray(fasta_sequence.length(), fasta_sequence.c_str());
			}
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): (internal error) unrecognized sequence type." << EidosTerminate();
	}
	
	// check that the length of the new sequence matches the chromosome length
	if (ancestral_seq_buffer_->size() != (std::size_t)(last_position_ + 1))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): The chromosome length (" << last_position_ + 1 << " base" << (last_position_ + 1 != 1 ? "s" : "") << ") does not match the ancestral sequence length (" << ancestral_seq_buffer_->size() << " base" << (ancestral_seq_buffer_->size() != 1 ? "s" : "") << ")." << EidosTerminate();
	
	// debugging
	//std::cout << "ancestral sequence set: " << *ancestral_seq_buffer_ << std::endl;
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(ancestral_seq_buffer_->size()));
}

//	*********************	– (void)setGeneConversion(numeric$ nonCrossoverFraction, numeric$ meanLength, numeric$ simpleConversionFraction, [numeric$ bias = 0])
//
EidosValue_SP Chromosome::ExecuteMethod_setGeneConversion(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() may not be called for a species with no genetics." << EidosTerminate();
	
	EidosValue *nonCrossoverFraction_value = p_arguments[0].get();
	EidosValue *meanLength_value = p_arguments[1].get();
	EidosValue *simpleConversionFraction_value = p_arguments[2].get();
	EidosValue *bias_value = p_arguments[3].get();
	
	double non_crossover_fraction = nonCrossoverFraction_value->NumericAtIndex_NOCAST(0, nullptr);
	double gene_conversion_avg_length = meanLength_value->NumericAtIndex_NOCAST(0, nullptr);
	double simple_conversion_fraction = simpleConversionFraction_value->NumericAtIndex_NOCAST(0, nullptr);
	double bias = bias_value->NumericAtIndex_NOCAST(0, nullptr);
	
	if ((non_crossover_fraction < 0.0) || (non_crossover_fraction > 1.0) || std::isnan(non_crossover_fraction))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() nonCrossoverFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(non_crossover_fraction) << " supplied)." << EidosTerminate();
	if ((gene_conversion_avg_length < 0.0) || std::isnan(gene_conversion_avg_length))		// intentionally no upper bound
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() meanLength must be >= 0.0 (" << EidosStringForFloat(gene_conversion_avg_length) << " supplied)." << EidosTerminate();
	if ((simple_conversion_fraction < 0.0) || (simple_conversion_fraction > 1.0) || std::isnan(simple_conversion_fraction))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() simpleConversionFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(simple_conversion_fraction) << " supplied)." << EidosTerminate();
	if ((bias < -1.0) || (bias > 1.0) || std::isnan(bias))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() bias must be between -1.0 and 1.0 inclusive (" << EidosStringForFloat(bias) << " supplied)." << EidosTerminate();
	if ((bias != 0.0) && !species_.IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() bias must be 0.0 in non-nucleotide-based models." << EidosTerminate();
	
	using_DSB_model_ = true;
	non_crossover_fraction_ = non_crossover_fraction;
	gene_conversion_avg_length_ = gene_conversion_avg_length;
	gene_conversion_inv_half_length_ = 1.0 / (gene_conversion_avg_length / 2.0);
	simple_conversion_fraction_ = simple_conversion_fraction;
	mismatch_repair_bias_ = bias;
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setHotspotMap(numeric multipliers, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setHotspotMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() may not be called for a species with no genetics." << EidosTerminate();
	if (!species_.IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() may only be called in nucleotide-based models (use setMutationRate() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	EidosValue *multipliers_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	
	int multipliers_count = multipliers_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex = IndividualSex::kUnspecified;
	std::string sex_string = sex_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requested sex '" << sex_string << "' unsupported." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && !single_mutation_map_) ||
		((requested_sex != IndividualSex::kUnspecified) && single_mutation_map_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? hotspot_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? hotspot_end_positions_M_ : hotspot_end_positions_F_));
	std::vector<double> &multipliers = ((requested_sex == IndividualSex::kUnspecified) ? hotspot_multipliers_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? hotspot_multipliers_M_ : hotspot_multipliers_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (multipliers_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requires multipliers to be a singleton if ends is not supplied." << EidosTerminate();
		
		double multiplier = multipliers_value->NumericAtIndex_NOCAST(0, nullptr);
		
		// check values
		if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() multiplier " << EidosStringForFloat(multiplier) << " out of range; multipliers must be >= 0." << EidosTerminate();
		
		// then adopt them
		multipliers.resize(0);
		positions.resize(0);
		
		multipliers.emplace_back(multiplier);
		positions.emplace_back(last_position_);
	}
	else
	{
		// ends is supplied
		int end_count = ends_value->Count();
		
		if ((end_count != multipliers_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requires ends and multipliers to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double multiplier = multipliers_value->NumericAtIndex_NOCAST(value_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(value_index, nullptr));
			
			if (value_index > 0)
				if (mutation_end_position <= ends_value->IntAtIndex_NOCAST(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() multiplier " << multiplier << " out of range; multipliers must be >= 0." << EidosTerminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before, otherwise our last_position_ cache is invalid.
		int64_t new_last_position = ends_value->IntAtIndex_NOCAST(end_count - 1, nullptr);
		
		if (new_last_position != last_position_)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() end " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << EidosTerminate();
		
		// then adopt them
		multipliers.resize(0);
		positions.resize(0);
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double multiplier = multipliers_value->NumericAtIndex_NOCAST(interval_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(interval_index, nullptr));
			
			multipliers.emplace_back(multiplier);
			positions.emplace_back(mutation_end_position);
		}
	}
	
	CreateNucleotideMutationRateMap();
	InitializeDraws();
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setMutationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setMutationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() may not be called for a species with no genetics." << EidosTerminate();
	if (species_.IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() may not be called in nucleotide-based models (use setHotspotMap() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex = IndividualSex::kUnspecified;
	
	std::string sex_string = sex_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requested sex '" << sex_string << "' unsupported." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && !single_mutation_map_) ||
		((requested_sex != IndividualSex::kUnspecified) && single_mutation_map_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? mutation_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? mutation_end_positions_M_ : mutation_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? mutation_rates_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? mutation_rates_M_ : mutation_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double mutation_rate = rates_value->NumericAtIndex_NOCAST(0, nullptr);
		
		// check values
		if ((mutation_rate < 0.0) || !std::isfinite(mutation_rate))		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() rate " << EidosStringForFloat(mutation_rate) << " out of range; rates must be >= 0." << EidosTerminate();
		
		// then adopt them
		rates.resize(0);
		positions.resize(0);
		
		rates.emplace_back(mutation_rate);
		//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	else
	{
		// ends is supplied
		int end_count = ends_value->Count();
		
		if ((end_count != rate_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double mutation_rate = rates_value->NumericAtIndex_NOCAST(value_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(value_index, nullptr));
			
			if (value_index > 0)
				if (mutation_end_position <= ends_value->IntAtIndex_NOCAST(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((mutation_rate < 0.0) || !std::isfinite(mutation_rate))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() rate " << EidosStringForFloat(mutation_rate) << " out of range; rates must be >= 0." << EidosTerminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before, otherwise our last_position_ cache is invalid.
		int64_t new_last_position = ends_value->IntAtIndex_NOCAST(end_count - 1, nullptr);
		
		if (new_last_position != last_position_)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() end " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << EidosTerminate();
		
		// then adopt them
		rates.resize(0);
		positions.resize(0);
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double mutation_rate = rates_value->NumericAtIndex_NOCAST(interval_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(interval_index, nullptr));
			
			rates.emplace_back(mutation_rate);
			positions.emplace_back(mutation_end_position);
		}
	}
	
	InitializeDraws();
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setRecombinationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setRecombinationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	CheckPartialInitializationForMethod(p_method_id);
	
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() may not be called for a species with no genetics." << EidosTerminate();
	
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex = IndividualSex::kUnspecified;
	
	std::string sex_string = sex_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requested sex '" << sex_string << "' unsupported." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && !single_recombination_map_) ||
		((requested_sex != IndividualSex::kUnspecified) && single_recombination_map_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? recombination_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? recombination_end_positions_M_ : recombination_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? recombination_rates_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? recombination_rates_M_ : recombination_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double recombination_rate = rates_value->NumericAtIndex_NOCAST(0, nullptr);
		
		// check values
		if ((recombination_rate < 0.0) || (recombination_rate > 0.5) || std::isnan(recombination_rate))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be in [0.0, 0.5]." << EidosTerminate();
		
		// then adopt them
		rates.resize(0);
		positions.resize(0);
		
		rates.emplace_back(recombination_rate);
		//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	else
	{
		// ends is supplied
		int end_count = ends_value->Count();
		
		if ((end_count != rate_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double recombination_rate = rates_value->NumericAtIndex_NOCAST(value_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(value_index, nullptr));
			
			if (value_index > 0)
				if (recombination_end_position <= ends_value->IntAtIndex_NOCAST(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((recombination_rate < 0.0) || (recombination_rate > 0.5) || std::isnan(recombination_rate))
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be in [0.0, 0.5]." << EidosTerminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before.
		int64_t new_last_position = ends_value->IntAtIndex_NOCAST(end_count - 1, nullptr);
		
		if (new_last_position != last_position_)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << EidosTerminate();
		
		// then adopt them
		rates.resize(0);
		positions.resize(0);
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double recombination_rate = rates_value->NumericAtIndex_NOCAST(interval_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(interval_index, nullptr));
			
			rates.emplace_back(recombination_rate);
			positions.emplace_back(recombination_end_position);
		}
	}
	
	InitializeDraws();
	
	return gStaticEidosValueVOID;
}


//
//	Chromosome_Class
//
#pragma mark -
#pragma mark Chromosome_Class
#pragma mark -

EidosClass *gSLiM_Chromosome_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Chromosome_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Chromosome_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElements,						true,	kEidosValueMaskObject, gSLiM_GenomicElement_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,										true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isSexChromosome,						true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_intrinsicPloidy,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_lastPosition,							true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_length,							true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotEndPositions,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotEndPositionsM,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotEndPositionsF,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotMultipliers,						true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotMultipliersM,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotMultipliersF,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositions,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositionsM,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositionsF,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRates,							true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRatesM,							true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRatesF,							true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_name,									false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRate,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRateM,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRateF,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRate,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateM,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateF,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositions,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsM,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsF,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRates,						true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesM,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesF,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_species,								true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Species_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_symbol,									true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionEnabled,					true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionGCBias,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionNonCrossoverFraction,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionMeanLength,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionSimpleConversionFraction,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,									false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_type,								true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_colorSubstitution,						false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Chromosome_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Chromosome_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_ancestralNucleotides, kEidosValueMaskInt | kEidosValueMaskString))->AddInt_OSN(gEidosStr_start, gStaticEidosValueNULL)->AddInt_OSN(gEidosStr_end, gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("string"))));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawBreakpoints, kEidosValueMaskInt))->AddObject_OSN("parent", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddInt_OSN("n", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_genomicElementForPosition, kEidosValueMaskObject, gSLiM_GenomicElement_Class))->AddInt("positions"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_hasGenomicElementForPosition, kEidosValueMaskLogical))->AddInt("positions"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setAncestralNucleotides, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntString("sequence"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setGeneConversion, kEidosValueMaskVOID))->AddNumeric_S("nonCrossoverFraction")->AddNumeric_S("meanLength")->AddNumeric_S("simpleConversionFraction")->AddNumeric_OS("bias", gStaticEidosValue_Integer0));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setHotspotMap, kEidosValueMaskVOID))->AddNumeric("multipliers")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationRate, kEidosValueMaskVOID))->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setRecombinationRate, kEidosValueMaskVOID))->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}





































































