//
//  species.cpp
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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


#include "species.h"
#include "community.h"
#include "slim_functions.h"
#include "eidos_test.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"
#include "eidos_sorting.h"
#include "individual.h"
#include "polymorphism.h"
#include "subpopulation.h"
#include "interaction_type.h"
#include "mutation_block.h"
#include "log_file.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <typeinfo>
#include <memory>
#include <string>
#include <utility>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>
#include <unordered_map>
#include <float.h>
#include <ctime>

#include "eidos_globals.h"
#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
#endif

//TREE SEQUENCE
#include <stdio.h>
#include <stdlib.h>
#include "json.hpp"
#include <sys/utsname.h>
#include <sys/stat.h>
#include <dirent.h>

//TREE SEQUENCE
//INCLUDE MSPRIME.H FOR THE CROSSCHECK CODE; NEEDS TO BE MOVED TO TSKIT
// the tskit header is not designed to be included from C++, so we have to protect it...
#ifdef __cplusplus
extern "C" {
#endif
#include "kastore.h"
#include "../treerec/tskit/trees.h"
#include "../treerec/tskit/tables.h"
#include "../treerec/tskit/genotypes.h"
#include "../treerec/tskit/text_input.h"
#ifdef __cplusplus
}
#endif


// This is the version written to the provenance table of .trees files
static const char *SLIM_TREES_FILE_VERSION_INITIAL __attribute__((unused)) = "0.1";		// SLiM 3.0, before the Inidividual table, etc.; UNSUPPORTED
static const char *SLIM_TREES_FILE_VERSION_PRENUC = "0.2";		// before introduction of nucleotides
static const char *SLIM_TREES_FILE_VERSION_POSTNUC = "0.3";		// SLiM 3.3.x, with the added nucleotide field in MutationMetadataRec
static const char *SLIM_TREES_FILE_VERSION_HASH = "0.4";		// SLiM 3.4.x, with the new model_hash key in provenance
static const char *SLIM_TREES_FILE_VERSION_META = "0.5";		// SLiM 3.5.x onward, with information in metadata instead of provenance
static const char *SLIM_TREES_FILE_VERSION_PREPARENT = "0.6";	// SLiM 3.6.x onward, with SLIM_TSK_INDIVIDUAL_RETAINED instead of SLIM_TSK_INDIVIDUAL_FIRST_GEN
static const char *SLIM_TREES_FILE_VERSION_PRESPECIES = "0.7";	// SLiM 3.7.x onward, with parent pedigree IDs in the individuals table metadata
static const char *SLIM_TREES_FILE_VERSION_SPECIES = "0.8";		// SLiM 4.0.x onward, with species `name`/`description`, and `tick` in addition to `cycle`
static const char *SLIM_TREES_FILE_VERSION = "0.9";				// SLiM 5.0 onward, for multichrom (haplosomes not genomes, and `chromosomes` key)

#pragma mark -
#pragma mark Species
#pragma mark -

Species::Species(Community &p_community, slim_objectid_t p_species_id, const std::string &p_name) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(p_name), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_Species_Class))),
	species_haplosome_pool_("EidosObjectPool(Haplosome)", sizeof(Haplosome), 16384), species_individual_pool_("EidosObjectPool(Individual)", sizeof(Individual), 8192),
    model_type_(p_community.model_type_), community_(p_community), population_(*this), name_(p_name), species_id_(p_species_id)
{
	// self_symbol_ is always a constant, but can't be marked as such on construction
	self_symbol_.second->MarkAsConstant();
	
#ifdef SLIMGUI
	// Pedigree recording is always enabled when running under SLiMgui, so that the various graphs all work
	// However, as with tree-sequence recording, the fact that it is enabled is not user-visible unless the user enables it
	pedigrees_enabled_ = true;
	pedigrees_enabled_by_SLiM_ = true;
#endif
	
	// Make space for up to SLIM_MAX_CHROMOSOMES Chromosome objects, but don't make any for now
	// This prevents the storage underlying chromosomes_ from being reallocated
	chromosomes_.reserve(SLIM_MAX_CHROMOSOMES);
}

Species::~Species(void)
{
	//EIDOS_ERRSTREAM << "Species::~Species" << std::endl;
	
	// There shouldn't be any individuals in the graveyard here, but just in case
	EmptyGraveyard();		// needs to be done first; uses subpopulation references
	
	population_.RemoveAllSubpopulationInfo();
	population_.PurgeRemovedSubpopulations();
	
	DeleteAllMutationRuns();
	
	for (auto mutation_type : mutation_types_)
		delete mutation_type.second;
	for (auto &element : mutation_types_)
		element.second = nullptr;
	mutation_types_.clear();
	
	for (auto genomic_element_type : genomic_element_types_)
		delete genomic_element_type.second;
	for (auto &element : genomic_element_types_)
		element.second = nullptr;
	genomic_element_types_.clear();
	
	// Free the shuffle buffer
	if (shuffle_buffer_)
	{
		free(shuffle_buffer_);
		shuffle_buffer_ = nullptr;
	}
	
	// TREE SEQUENCE RECORDING
	if (RecordingTreeSequence())
		FreeTreeSequence();
	
	if (hap_metadata_1F_) {
		free(hap_metadata_1F_);
		hap_metadata_1F_ = nullptr;
	}
	if (hap_metadata_1M_) {
		free(hap_metadata_1M_);
		hap_metadata_1M_ = nullptr;
	}
	if (hap_metadata_2F_) {
		free(hap_metadata_2F_);
		hap_metadata_2F_ = nullptr;
	}
	if (hap_metadata_2M_) {
		free(hap_metadata_2M_);
		hap_metadata_2M_ = nullptr;
	}
	
	// Let go of our chromosome objects.  This is tricky, because other objects deleted later might try to use
	// the chromosome objects after they're gone, leading to undefined behavior.  To make such bugs easier to
	// catch, we zero out pointers to chromosomes everywhere we keep them, so usage of them hopefully crashes.
	for (Chromosome *chromosome : chromosomes_)
		chromosome->Release();
	
	std::fill(chromosomes_.begin(), chromosomes_.end(), nullptr);
	chromosomes_.clear();
	
	for (auto &element : chromosome_from_id_)
		element.second = nullptr;
	chromosome_from_id_.clear();
	
	for (auto &element : chromosome_from_symbol_)
		element.second = nullptr;
	chromosome_from_symbol_.clear();
	
	std::fill(chromosome_for_haplosome_index_.begin(), chromosome_for_haplosome_index_.end(), nullptr);
	chromosome_for_haplosome_index_.clear();
	
	// Free our Trait objects
	for (Trait *trait : traits_)
		delete trait;
	traits_.clear();
	
	// Free our MutationBlock, and make those with pointers to it forget; see CreateAndPromulgateMutationBlock()
	{
		delete mutation_block_;
		mutation_block_ = nullptr;
		
		for (auto muttype_iter : mutation_types_)
			muttype_iter.second->mutation_block_ = nullptr;
		
		for (Chromosome *chromosome : chromosomes_)
			chromosome->mutation_block_ = nullptr;
		
		population_.mutation_block_ = nullptr;
	}
}

void Species::_MakeHaplosomeMetadataRecords(void)
{
	// Set up our default metadata records for haplosomes, which are variable-length.  The default records
	// are used as the initial configuration of the nodes for new individuals; then, as haplosomes are
	// added to the new individual, the is_vacant_ bits get tweaked as needed in the recorded metadata, which
	// is a bit gross, but necessary; the node metadata is recorded before the haplosomes are created.
	// See HaplosomeMetadataRec for comments on this design.
	
	// First, calculate how many bytes we need
	size_t bits_needed_for_is_vacant = chromosomes_.size();					// each chromosome needs one bit per node table entry
	haplosome_metadata_size_ = sizeof(HaplosomeMetadataRec) - 1;			// -1 to subtract out the is_vacant_[1] in the record
	haplosome_metadata_is_vacant_bytes_ = ((bits_needed_for_is_vacant + 7) / 8);	// (x+7)/8 rounds up to the number of bytes
	haplosome_metadata_size_ += haplosome_metadata_is_vacant_bytes_;
	
	// Then allocate the buffers needed; the "male" versions are present only when sex is enabled
	hap_metadata_1F_ = (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1);
	hap_metadata_1M_ = (sex_enabled_ ? (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1) : nullptr);
	hap_metadata_2F_ = (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1);
	hap_metadata_2M_ = (sex_enabled_ ? (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1) : nullptr);
	
	// Then set the is_vacant_ bits for the default state for males and females; this is the state in which
	// all chromosomes that dictate the is_vacant_ state by sex have that dictated state, while all others
	// (types "A", "H", and "H-" only) are assumed to be non-null.  Any positions that are unused for a
	// given chromosome type (like the second position for type "Y") are given as 1 here, "vacant", by
	// definition; "vacant" is either "unused" or "null haplosome".  We go from least-significant bit
	// to most-significant bit, byte by byte, with each chromosome using two bits.  The less significant
	// of those two bits is is_vacant_ for haplosome 1 for that chromosome; the more significant of those
	// two bits is is_vacant_ for haplosome 2 for that chromosome.
	IndividualSex sex = IndividualSex::kFemale;
	HaplosomeMetadataRec *focal_metadata_1 = hap_metadata_1F_;
	HaplosomeMetadataRec *focal_metadata_2 = hap_metadata_2F_;
	
	while (true)
	{
		for (Chromosome *chromosome : chromosomes_)
		{
			slim_chromosome_index_t chromosome_index = chromosome->Index();
			bool haplosome_1_is_vacant = false, haplosome_2_is_vacant = false;
			
			switch (chromosome->Type())
			{
				case ChromosomeType::kA_DiploidAutosome:
					haplosome_1_is_vacant = false;								// always present (by default)
					haplosome_2_is_vacant = false;								// always present (by default)
					break;
					
				case ChromosomeType::kH_HaploidAutosome:
				case ChromosomeType::kHF_HaploidFemaleInherited:
				case ChromosomeType::kHM_HaploidMaleInherited:
					haplosome_1_is_vacant = false;								// always present (by default)
					haplosome_2_is_vacant = true;								// always unused
					break;
					
				case ChromosomeType::kHNull_HaploidAutosomeWithNull:
					haplosome_1_is_vacant = false;								// always present
					haplosome_2_is_vacant = true;								// always null
					break;
					
				case ChromosomeType::kX_XSexChromosome:
					haplosome_1_is_vacant = false;								// always present
					haplosome_2_is_vacant = (sex == IndividualSex::kMale);		// null in males
					break;
					
				case ChromosomeType::kY_YSexChromosome:
				case ChromosomeType::kML_HaploidMaleLine:
					haplosome_1_is_vacant = (sex == IndividualSex::kFemale);	// null in females
					haplosome_2_is_vacant = true;								// always unused
					break;
					
				case ChromosomeType::kZ_ZSexChromosome:
					haplosome_1_is_vacant = (sex == IndividualSex::kFemale);	// null in females
					haplosome_2_is_vacant = false;								// always present
					break;
					
				case ChromosomeType::kW_WSexChromosome:
				case ChromosomeType::kFL_HaploidFemaleLine:
					haplosome_1_is_vacant = (sex == IndividualSex::kMale);		// null in males
					haplosome_2_is_vacant = true;								// always unused
					break;
					
				case ChromosomeType::kNullY_YSexChromosomeWithNull:
					haplosome_1_is_vacant = true;								// always null
					haplosome_2_is_vacant = (sex == IndividualSex::kFemale);	// null in females
					break;
			}
			
			// set the appropriate bits in the focal metadata, which we know was cleared to zero initially
			int byte_index = chromosome_index / 8;
			int bit_shift = chromosome_index % 8;
			
			if (haplosome_1_is_vacant)
				focal_metadata_1->is_vacant_[byte_index] |= (0x01 << bit_shift);
			
			if (haplosome_2_is_vacant)
				focal_metadata_2->is_vacant_[byte_index] |= (0x01 << bit_shift);
		}
		
		// loop from female to male, then break out
		if (sex_enabled_ && (sex == IndividualSex::kFemale))
		{
			sex = IndividualSex::kMale;
			focal_metadata_1 = hap_metadata_1M_;
			focal_metadata_2 = hap_metadata_2M_;
			continue;
		}
		break;
	}
	
//	printf("hap_metadata_1F_ == %.2X\n", hap_metadata_1F_->is_vacant_[0]);
//	printf("hap_metadata_1M_ == %.2X\n", hap_metadata_1M_->is_vacant_[0]);
//	printf("hap_metadata_2F_ == %.2X\n", hap_metadata_2F_->is_vacant_[0]);
//	printf("hap_metadata_2M_ == %.2X\n", hap_metadata_2M_->is_vacant_[0]);
}

Chromosome *Species::ChromosomeFromID(int64_t p_id)
{
	auto iter = chromosome_from_id_.find(p_id);
	
	if (iter == chromosome_from_id_.end())
		return nullptr;
	
	return (*iter).second;
}

Chromosome *Species::ChromosomeFromSymbol(const std::string &p_symbol)
{
	auto iter = chromosome_from_symbol_.find(p_symbol);
	
	if (iter == chromosome_from_symbol_.end())
		return nullptr;
	
	return (*iter).second;
}

void Species::MakeImplicitChromosome(ChromosomeType p_type)
{
	if (has_implicit_chromosome_)
		EIDOS_TERMINATION << "ERROR (Species::MakeImplicitChromosome): (internal error) implicit chromosome already exists." << EidosTerminate();
	if (num_chromosome_inits_ != 0)
		EIDOS_TERMINATION << "ERROR (Species::MakeImplicitChromosome): (internal error) explicit chromosome already exists." << EidosTerminate();
	
	// Only these three chromosome types are supported for an implicitly defined chromosome.  The symbols used
	// here match the symbols that were output for chromosome types in various built-in output methods prior to
	// SLiM 5, for backward compatibility.
	std::string chromosome_symbol;
	
	if (p_type == ChromosomeType::kA_DiploidAutosome)
		chromosome_symbol = "A";
	else if (p_type == ChromosomeType::kX_XSexChromosome)
		chromosome_symbol = "X";
	else if (p_type == ChromosomeType::kNullY_YSexChromosomeWithNull)
		chromosome_symbol = "Y";
	else
		EIDOS_TERMINATION << "ERROR (Species::MakeImplicitChromosome): (internal error) unsupported implicit chromosome type." << EidosTerminate();
	
	// Create an implicit Chromosome object with a retain on it from EidosDictionaryRetained::EidosDictionaryRetained()
	Chromosome *chromosome = new Chromosome(*this, p_type, 1, chromosome_symbol, /* p_index */ 0, /* p_preferred_mutcount */ 0);
	
	// Add it to our registry; AddChromosome() takes its retain count
	AddChromosome(chromosome);
	has_implicit_chromosome_ = true;
	has_currently_initializing_chromosome_ = true;
}

Chromosome *Species::CurrentlyInitializingChromosome(void)
{
	if (!has_currently_initializing_chromosome_)
		EIDOS_TERMINATION << "ERROR (Species::CurrentlyInitializingChromosome): (internal error) no currently initializing chromosome exists; MakeImplicitChromosome() should be called first." << EidosTerminate();
	if (!has_implicit_chromosome_ && (num_chromosome_inits_ == 0))
		EIDOS_TERMINATION << "ERROR (Species::CurrentlyInitializingChromosome): (internal error) no currently initializing chromosome exists even though has_currently_initializing_chromosome_ is true." << EidosTerminate();
	
	return chromosomes_.back();
}

void Species::AddChromosome(Chromosome *p_chromosome)
{
	int64_t id = p_chromosome->ID();
	std::string symbol = p_chromosome->Symbol();
	
	// this is the main registry, and owns the retain count on every chromosome; it takes the caller's retain here
	chromosomes_.push_back(p_chromosome);
	
	// these are secondary indices that do not keep a retain on the chromosomes
	chromosome_from_id_.emplace(id, p_chromosome);
	chromosome_from_symbol_.emplace(symbol, p_chromosome);
	
	// keep track of our haplosome configuration
	if (p_chromosome->IntrinsicPloidy() == 2)
	{
		chromosome_for_haplosome_index_.push_back(p_chromosome);
		chromosome_for_haplosome_index_.push_back(p_chromosome);
		chromosome_subindex_for_haplosome_index_.push_back(0);
		chromosome_subindex_for_haplosome_index_.push_back(1);
		first_haplosome_index_.push_back(haplosome_count_per_individual_);
		last_haplosome_index_.push_back(haplosome_count_per_individual_ + 1);
		haplosome_count_per_individual_ += 2;
	}
	else // p_chromosome->IntrinsicPloidy() == 1
	{
		chromosome_for_haplosome_index_.push_back(p_chromosome);
		chromosome_subindex_for_haplosome_index_.push_back(0);
		first_haplosome_index_.push_back(haplosome_count_per_individual_);
		last_haplosome_index_.push_back(haplosome_count_per_individual_);
		haplosome_count_per_individual_ += 1;
	}
	
	// keep track of whether we contain null haplosomes or not (for optimizations)
	// if addRecombinant(), addMultiRecombinant(), etc. places a null haplosome in 'A' or 'H', it will set
	// the has_null_haplosomes_ flag, which tracks this at a finer level of detail than the chromosome type
	if (p_chromosome->AlwaysUsesNullHaplosomes())
		chromosomes_use_null_haplosomes_ = true;
}

Chromosome *Species::GetChromosomeFromEidosValue(EidosValue *chromosome_value)
{
	EidosValueType chromosome_value_type = chromosome_value->Type();
	int chromosome_value_count = chromosome_value->Count();
	
	// NULL means "no chromosome chosen"; caller must be prepared for nullptr
	if (chromosome_value_type == EidosValueType::kValueNULL)
		return nullptr;
	
	if (chromosome_value_count != 1)
		EIDOS_TERMINATION << "ERROR (Species::GetChromosomeFromEidosValue): (internal error) the chromosome parameter must be singleton." << EidosTerminate();
	
	switch (chromosome_value_type)
	{
		case EidosValueType::kValueInt:
		{
			int64_t id = chromosome_value->IntAtIndex_NOCAST(0, nullptr);
			Chromosome *chromosome = ChromosomeFromID(id);
			
			if (!chromosome)
				EIDOS_TERMINATION << "ERROR (Species::GetChromosomeFromEidosValue): could not find a chromosome with the given id (" << id << ") in the target species." << EidosTerminate();
			
			return chromosome;
		}
		case EidosValueType::kValueString:
		{
			const std::string &symbol = chromosome_value->StringAtIndex_NOCAST(0, nullptr);
			Chromosome *chromosome = ChromosomeFromSymbol(symbol);
			
			if (!chromosome)
				EIDOS_TERMINATION << "ERROR (Species::GetChromosomeFromEidosValue): could not find a chromosome with the given symbol (" << symbol << ") in the target species." << EidosTerminate();
			
			return chromosome;
		}
		case EidosValueType::kValueObject:
		{
			Chromosome *chromosome = (Chromosome *)chromosome_value->ObjectElementAtIndex_NOCAST(0, nullptr);
			
			if (&chromosome->species_ != this)
				EIDOS_TERMINATION << "ERROR (Species::GetChromosomeFromEidosValue): the chromosome passed does not belong to the target species." << EidosTerminate();
			
			return chromosome;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Species::GetChromosomeFromEidosValue): (internal error) unexpected type for parameter chromosome." << EidosTerminate();
	}
}

void Species::GetChromosomeIndicesFromEidosValue(std::vector<slim_chromosome_index_t> &chromosome_indices, EidosValue *chromosomes_value)
{
	EidosValueType chromosomes_value_type = chromosomes_value->Type();
	int chromosomes_value_count = chromosomes_value->Count();
	
	switch (chromosomes_value_type)
	{
		// NULL means "all chromosomes", unlike for GetChromosomeFromEidosValue()
		case EidosValueType::kValueNULL:
		{
			for (Chromosome *chromosome : Chromosomes())
				chromosome_indices.push_back(chromosome->Index());
			break;
		}
		case EidosValueType::kValueInt:
		{
			const int64_t *ids_data = chromosomes_value->IntData();
			
			for (int ids_index = 0; ids_index < chromosomes_value_count; ids_index++)
			{
				int64_t id = ids_data[ids_index];
				Chromosome *chromosome = ChromosomeFromID(id);
				
				if (!chromosome)
					EIDOS_TERMINATION << "ERROR (Species::GetChromosomeIndicesFromEidosValue): could not find a chromosome with the given id (" << id << ") in the target species." << EidosTerminate();
				
				chromosome_indices.push_back(chromosome->Index());
			}
			break;
		}
		case EidosValueType::kValueString:
		{
			const std::string *symbols_data = chromosomes_value->StringData();
			
			for (int symbols_index = 0; symbols_index < chromosomes_value_count; symbols_index++)
			{
				const std::string &symbol = symbols_data[symbols_index];
				Chromosome *chromosome = ChromosomeFromSymbol(symbol);
				
				if (!chromosome)
					EIDOS_TERMINATION << "ERROR (Species::GetChromosomeIndicesFromEidosValue): could not find a chromosome with the given symbol (" << symbol << ") in the target species." << EidosTerminate();
				
				chromosome_indices.push_back(chromosome->Index());
			}
			break;
		}
		case EidosValueType::kValueObject:
		{
			Chromosome * const *chromosomes_data = (Chromosome * const *)chromosomes_value->ObjectData();
			
			for (int chromosome_index = 0; chromosome_index < chromosomes_value_count; ++chromosome_index)
			{
				Chromosome *chromosome = chromosomes_data[chromosome_index];
				
				if (&chromosome->species_ != this)
					EIDOS_TERMINATION << "ERROR (Species::GetChromosomeIndicesFromEidosValue): the chromosome passed does not belong to the target species." << EidosTerminate();
				
				chromosome_indices.push_back(chromosome->Index());
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Species::GetChromosomeIndicesFromEidosValue): (internal error) unexpected type for parameter chromosome." << EidosTerminate();
	}
}

Trait *Species::TraitFromName(const std::string &p_name) const
{
	auto iter = trait_from_name.find(p_name);
	
	if (iter == trait_from_name.end())
		return nullptr;
	
	return (*iter).second;
}

void Species::MakeImplicitTrait(void)
{
	if (has_implicit_trait_)
		EIDOS_TERMINATION << "ERROR (Species::MakeImplicitTrait): (internal error) implicit trait already exists." << EidosTerminate();
	if (num_trait_inits_ != 0)
		EIDOS_TERMINATION << "ERROR (Species::MakeImplicitTrait): (internal error) explicit trait already exists." << EidosTerminate();
	
	// Create an implicit Trait object with a retain on it from EidosDictionaryRetained::EidosDictionaryRetained()
	// Mirroring SLiM versions prior to multi-trait support, the implicit trait is a multiplicative trait with
	// no baselines (1.0, since it is multiplicative) and a direct effect from phenotype on fitness.
	std::string trait_name = name_ + "T";
	Trait *trait = new Trait(*this, trait_name, TraitType::kMultiplicative, 1.0, 1.0, 0.0, true);
	
	// Add it to our registry; AddTrait() takes its retain count
	AddTrait(trait);
	has_implicit_trait_ = true;
}

void Species::AddTrait(Trait *p_trait)
{
	if (p_trait->Index() != -1)
		EIDOS_TERMINATION << "ERROR (Species::AddTrait): (internal error) attempt to add a trait with index != -1." << EidosTerminate();
	
	std::string name = p_trait->Name();
	EidosGlobalStringID name_string_id = EidosStringRegistry::GlobalStringIDForString(name);
	
	// this is the main registry, and owns the retain count on every trait; it takes the caller's retain here
	p_trait->SetIndex((slim_trait_index_t)(traits_.size()));
	traits_.push_back(p_trait);
	
	// these are secondary indices that do not keep a retain on the traits
	trait_from_name.emplace(name, p_trait);
	trait_from_string_id.emplace(name_string_id, p_trait);
}

// This returns the trait index for a single trait, represented by an EidosValue with an integer index or a Trait object
slim_trait_index_t Species::GetTraitIndexFromEidosValue(EidosValue *trait_value, const std::string &p_method_name)
{
	int64_t trait_index;
	
	if (trait_value->Type() == EidosValueType::kValueInt)
	{
		trait_index = trait_value->IntAtIndex_NOCAST(0, nullptr);
	}
	else
	{
		const Trait *trait = (const Trait *)trait_value->ObjectElementAtIndex_NOCAST(0, nullptr);
		
		if (&trait->species_ != this)
			EIDOS_TERMINATION << "ERROR (Species::GetTraitIndexFromEidosValue): " << p_method_name << "() requires trait to belong to the same species as the target mutation type." << EidosTerminate(nullptr);
		
		trait_index = trait->Index();
	}
	
	if ((trait_index < 0) || (trait_index >= TraitCount()))
		EIDOS_TERMINATION << "ERROR (Species::GetTraitIndexFromEidosValue): out-of-range trait index in " << p_method_name << "(); trait index " << trait_index << " is outside the range [0, " << (TraitCount() - 1) << "] for the species." << EidosTerminate(nullptr);
	
	return (slim_trait_index_t)trait_index;
}

// This returns trait indices, represented by an EidosValue with integer indices, string names, or Trait objects, or NULL for all traits
void Species::GetTraitIndicesFromEidosValue(std::vector<slim_trait_index_t> &trait_indices, EidosValue *traits_value, const std::string &p_method_name)
{
	EidosValueType traits_value_type = traits_value->Type();
	int traits_value_count = traits_value->Count();
	slim_trait_index_t trait_count = TraitCount();
	
	switch (traits_value_type)
	{
			// NULL means "all traits", unlike for GetTraitIndexFromEidosValue()
		case EidosValueType::kValueNULL:
		{
			for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
				trait_indices.push_back(trait_index);
			break;
		}
		case EidosValueType::kValueInt:
		{
			const int64_t *indices_data = traits_value->IntData();
			
			for (int indices_index = 0; indices_index < traits_value_count; indices_index++)
			{
				int64_t trait_index = indices_data[indices_index];
				
				if ((trait_index < 0) || (trait_index >= TraitCount()))
					EIDOS_TERMINATION << "ERROR (Species::GetTraitIndicesFromEidosValue): out-of-range trait index in " << p_method_name << "(); trait index " << trait_index << " is outside the range [0, " << (TraitCount() - 1) << "] for the species." << EidosTerminate(nullptr);
				
				trait_indices.push_back((slim_trait_index_t)trait_index);
			}
			break;
		}
		case EidosValueType::kValueString:
		{
			const std::string *indices_data = traits_value->StringData();
			
			for (int names_index = 0; names_index < traits_value_count; names_index++)
			{
				const std::string &trait_name = indices_data[names_index];
				Trait *trait = TraitFromName(trait_name);
				
				if (trait == nullptr)
					EIDOS_TERMINATION << "ERROR (Species::GetTraitIndicesFromEidosValue): unrecognized trait name in " << p_method_name << "(); trait name " << trait_name << " is not defined for the species." << EidosTerminate(nullptr);
				
				trait_indices.push_back(trait->Index());
			}
			break;
		}
		case EidosValueType::kValueObject:
		{
			Trait * const *traits_data = (Trait * const *)traits_value->ObjectData();
			
			for (int traits_index = 0; traits_index < traits_value_count; ++traits_index)
			{
				Trait *trait = traits_data[traits_index];
				
				if (&trait->species_ != this)
					EIDOS_TERMINATION << "ERROR (Species::GetTraitIndicesFromEidosValue): " << p_method_name << "() requires trait to belong to the same species as the target mutation type." << EidosTerminate(nullptr);
				
				trait_indices.push_back(trait->Index());
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Species::GetTraitIndicesFromEidosValue): (internal error) unexpected type for parameter trait." << EidosTerminate();
	}
}

// get one line of input, sanitizing by removing comments and whitespace; used only by Species::InitializePopulationFromTextFile
void GetInputLine(std::istream &p_input_file, std::string &p_line);
void GetInputLine(std::istream &p_input_file, std::string &p_line)
{
	getline(p_input_file, p_line);
	
	// remove all after "//", the comment start sequence
	// BCH 16 Dec 2014: note this was "/" in SLiM 1.8 and earlier, changed to allow full filesystem paths to be specified.
	if (p_line.find("//") != std::string::npos)
		p_line.erase(p_line.find("//"));
	
	// remove leading and trailing whitespace (spaces and tabs)
	p_line.erase(0, p_line.find_first_not_of(" \t"));
	p_line.erase(p_line.find_last_not_of(" \t") + 1);
}

SLiMFileFormat Species::FormatOfPopulationFile(const std::string &p_file_string)
{
	if (p_file_string.length())
	{
		// p_file should have had its trailing slash stripped already, and a leading ~ should have been resolved
		// we will check those assumptions here for safety...
		if (p_file_string[0] == '~')
			EIDOS_TERMINATION << "ERROR (Species::FormatOfPopulationFile): (internal error) leading ~ in path was not resolved." << EidosTerminate();
		if (p_file_string.back() == '/')
			EIDOS_TERMINATION << "ERROR (Species::FormatOfPopulationFile): (internal error) trailing / in path was not stripped." << EidosTerminate();
		
		// First determine if the path is for a file or a directory
		const char *file_cstr = p_file_string.c_str();
		struct stat statbuf;
		
		if (stat(file_cstr, &statbuf) != 0)
			return SLiMFileFormat::kFileNotFound;
		
		if (S_ISDIR(statbuf.st_mode))
		{
			// The path is for a whole directory.  This used to be the code path for a directory-based tskit text
			// (i.e. non-binary) format, but we no longer support that.  This is now the code path for reading in
			// a multi-chromosome archive of .trees files, which live inside a directory.  If the directory does
			// not contain the expected .trees files, we'll discover that later on and raise.
			return SLiMFileFormat::kFormatDirectory;
		}
		else if (S_ISREG(statbuf.st_mode))
		{
			// The path is for a file.  It could be a SLiM text file, SLiM binary file, or tskit binary file; we
			// determine which using the leading 4 bytes of the file.  This heuristic will need to be adjusted
			// if/when these file formats change (such as going off of HD5 in the tskit file format).
			std::ifstream infile(file_cstr, std::ios::in | std::ios::binary);
			
			if (!infile.is_open() || infile.eof())
				return SLiMFileFormat::kFileNotFound;
			
			// Determine the file length
			infile.seekg(0, std::ios_base::end);
			std::size_t file_size = infile.tellg();
			
			// Determine the file format
			if (file_size >= 4)
			{
				char file_chars[4] = {0, 0, 0, 0};
				uint32_t file_endianness_tag = 0;
				
				infile.seekg(0, std::ios_base::beg);
				infile.read(&file_chars[0], 4);
				
				infile.seekg(0, std::ios_base::beg);
				infile.read(reinterpret_cast<char *>(&file_endianness_tag), sizeof file_endianness_tag);
				
				if ((file_chars[0] == '#') && (file_chars[1] == 'O') && (file_chars[2] == 'U') && (file_chars[3] == 'T'))
					return SLiMFileFormat::kFormatSLiMText;
				else if (file_endianness_tag == 0x12345678)
					return SLiMFileFormat::kFormatSLiMBinary;
				else if (file_endianness_tag == 0x46444889)			// 'âHDF', the prefix for HDF5 files apparently; reinterpreted via endianness
					return SLiMFileFormat::kFormatTskitBinary_HDF5;
				else if (file_endianness_tag == 0x53414B89)			// 'âKAS', the prefix for kastore files apparently; reinterpreted via endianness
					return SLiMFileFormat::kFormatTskitBinary_kastore;
			}
		}
	}
	
	return SLiMFileFormat::kFormatUnrecognized;
}

void Species::_CleanAllReferencesToSpecies(EidosInterpreter *p_interpreter)
{
	// clear out all variables of type Subpopulation etc. from the symbol table; they will all be invalid momentarily
	// note that we do this not only in our constants table, but in the user's variables as well; we can leave no stone unturned
	// Note that we presently have no way of clearing out EidosScribe/SLiMgui references (the variable browser, in particular),
	// and so EidosConsoleWindowController has to do an ugly and only partly effective hack to work around this issue.
	if (p_interpreter)
	{
		EidosSymbolTable &symbols = p_interpreter->SymbolTable();
		std::vector<std::string> all_symbols = symbols.AllSymbols();
		std::vector<EidosGlobalStringID> symbols_to_remove;
		
		for (const std::string &symbol_name : all_symbols)
		{
			EidosGlobalStringID symbol_ID = EidosStringRegistry::GlobalStringIDForString(symbol_name);
			EidosValue_SP symbol_value = symbols.GetValueOrRaiseForSymbol(symbol_ID);
			
			if (symbol_value->Type() == EidosValueType::kValueObject)
			{
				EidosValue_Object *symbol_object = (static_pointer_cast<EidosValue_Object>(symbol_value)).get();
				const EidosClass *symbol_class = symbol_object->Class();
				
				if ((symbol_class == gSLiM_Subpopulation_Class) || (symbol_class == gSLiM_Haplosome_Class) || (symbol_class == gSLiM_Individual_Class) || (symbol_class == gSLiM_Mutation_Class) || (symbol_class == gSLiM_Substitution_Class))
				{
					// BCH 5/7/2022: For multispecies, we now have to be careful to clear out only state related to the target species!
					// This is truly disgusting, because it means we have to go down into the elements of the value to check their species
					// If *any* element of a value belongs to the target species, we remove the whole value (rather than editing out elements)
					// Unless/until we are able to let the user retain references to these objects beyond their natural lifetime, there is
					// just no alternative; the user may find it surprising that their local variable has disappeared, but... such is life
					bool refers_to_target_species = false;
					
					if (symbol_class == gSLiM_Subpopulation_Class)
					{
						for (int i = 0; i < symbol_object->Count(); ++i)
						{
							Subpopulation *element = (Subpopulation *)symbol_object->ObjectElementAtIndex_NOCAST(i, nullptr);
							
							if (&element->species_ == this)
							{
								refers_to_target_species = true;
								break;
							}
						}
					}
					else if (symbol_class == gSLiM_Haplosome_Class)
					{
						for (int i = 0; i < symbol_object->Count(); ++i)
						{
							Haplosome *element = (Haplosome *)symbol_object->ObjectElementAtIndex_NOCAST(i, nullptr);
							
							if (&element->individual_->subpopulation_->species_ == this)
							{
								refers_to_target_species = true;
								break;
							}
						}
					}
					else if (symbol_class == gSLiM_Individual_Class)
					{
						for (int i = 0; i < symbol_object->Count(); ++i)
						{
							Individual *element = (Individual *)symbol_object->ObjectElementAtIndex_NOCAST(i, nullptr);
							
							if (&element->subpopulation_->species_ == this)
							{
								refers_to_target_species = true;
								break;
							}
						}
					}
					else if (symbol_class == gSLiM_Mutation_Class)
					{
						for (int i = 0; i < symbol_object->Count(); ++i)
						{
							Mutation *element = (Mutation *)symbol_object->ObjectElementAtIndex_NOCAST(i, nullptr);
							
							if (&element->mutation_type_ptr_->species_ == this)
							{
								refers_to_target_species = true;
								break;
							}
						}
					}
					else if (symbol_class == gSLiM_Substitution_Class)
					{
						for (int i = 0; i < symbol_object->Count(); ++i)
						{
							Substitution *element = (Substitution *)symbol_object->ObjectElementAtIndex_NOCAST(i, nullptr);
							
							if (&element->mutation_type_ptr_->species_ == this)
							{
								refers_to_target_species = true;
								break;
							}
						}
					}
					
					if (refers_to_target_species)
						symbols_to_remove.emplace_back(symbol_ID);
				}
			}
		}
		
		for (EidosGlobalStringID symbol_ID : symbols_to_remove)
			symbols.RemoveConstantForSymbol(symbol_ID);
	}
}

slim_tick_t Species::InitializePopulationFromFile(const std::string &p_file_string, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_remap)
{
	SLiMFileFormat file_format = FormatOfPopulationFile(p_file_string);
	
	if (file_format == SLiMFileFormat::kFileNotFound)
		EIDOS_TERMINATION << "ERROR (Species::InitializePopulationFromFile): initialization file does not exist or is empty." << EidosTerminate();
	if (file_format == SLiMFileFormat::kFormatUnrecognized)
		EIDOS_TERMINATION << "ERROR (Species::InitializePopulationFromFile): initialization file is invalid." << EidosTerminate();
	
	// readPopulationFromFile() should define a long-term boundary; the user shouldn't keep references to non-retain-release objects across it
	CheckLongTermBoundary();
	
	// start by cleaning out all variable/constant references to the species or any population object underneath it
	_CleanAllReferencesToSpecies(p_interpreter);
	
	// invalidate interactions, since any cached interaction data depends on the subpopulations and individuals
	community_.InvalidateInteractionsForSpecies(this);
	
	// then we dispose of all existing subpopulations, mutations, etc.
	population_.RemoveAllSubpopulationInfo();
    
    // Forget remembered subpop IDs and names since we are resetting our state.  We need to do this
    // to add in subpopulations we will load after resetting; however, it does leave open a window
    // for incorrect usage since ids/names that were used previously but are no longer extant will
    // be forgotten as a side effect of reloading, and could then get reused.  This seems unlikely
    // to arise in practice, and if it does it should produce a downstream error in Python if it
    // matters, due to ambiguity of duplicated ids/names, so we won't worry about it here - we'd
    // have to persist the list of known ids/names in metadata, which isn't worth the effort.
	// BCH 3/13/2022: Note that now in multispecies, we forget only the names/ids that we ourselves
	// have used; the other species in the community still remember and block their own usages.
	used_subpop_ids_.clear();
	used_subpop_names_.clear();
	
	// Read in the file.  The SLiM file-reading methods are not tree-sequence-aware, so we bracket them
	// with calls that fix the tree sequence recording state around them.  The treeSeq output methods
	// are of course treeSeq-aware, so we don't need to do that for them.
    const char *file_cstr = p_file_string.c_str();
	slim_tick_t new_tick = 0;
    
	if ((file_format == SLiMFileFormat::kFormatSLiMText) || (file_format == SLiMFileFormat::kFormatSLiMBinary))
	{
		if (p_subpop_remap.size() > 0)
			EIDOS_TERMINATION << "ERROR (Species::InitializePopulationFromFile): the subpopMap parameter is currently supported only when reading .trees files; for other file types it must be NULL (or an empty Dictionary)." << EidosTerminate();
		
		// TREE SEQUENCE RECORDING
		if (RecordingTreeSequence())
		{
			FreeTreeSequence();
			AllocateTreeSequenceTables();
			
			if (!community_.warned_no_ancestry_read_ && !gEidosSuppressWarnings)
			{
				p_interpreter->ErrorOutputStream() << "#WARNING (Species::InitializePopulationFromFile): when tree-sequence recording is enabled, it is usually desirable to call readFromPopulationFile() with a tree-sequence file to provide ancestry; such a file can be produced with treeSeqOutput(), or from msprime/tskit in Python." << std::endl;
				community_.warned_no_ancestry_read_ = true;
			}
		}
		
		if (file_format == SLiMFileFormat::kFormatSLiMText)
			new_tick = _InitializePopulationFromTextFile(file_cstr, p_interpreter);
		else if (file_format == SLiMFileFormat::kFormatSLiMBinary)
			new_tick = _InitializePopulationFromBinaryFile(file_cstr, p_interpreter);
		
		// TREE SEQUENCE RECORDING
		if (RecordingTreeSequence())
		{
			// set up all of the mutations we just read in with the tree-seq recording code
			RecordAllDerivedStatesFromSLiM();
			
			// reset our tree-seq auto-simplification interval so we don't simplify immediately
			simplify_elapsed_ = 0;
			
			// reset our last coalescence state; we don't know whether we're coalesced now or not
			for (TreeSeqInfo &tsinfo : treeseq_)
				tsinfo.last_coalescence_state_ = false;
		}
	}
	else if (file_format == SLiMFileFormat::kFormatTskitBinary_kastore)
	{
		if (chromosomes_.size() != 1)
			EIDOS_TERMINATION << "ERROR (Species::InitializePopulationFromFile): the focal species defines " << chromosomes_.size() << " chromosomes.  A single-chromosome tree-sequence file cannot be read in for this species, because the number of chromosomes does not match." << EidosTerminate();
		
		// We have a single chromosome and a single-chromosome .trees file; we will validate downstream that the chromosome information matches
		new_tick = _InitializePopulationFromTskitBinaryFile(file_cstr, p_interpreter, p_subpop_remap, *chromosomes_[0]);
	}
	else if (file_format == SLiMFileFormat::kFormatDirectory)
	{
		// Here we assume that a directory is a multi-chromosome .trees archive; we will check downstream
		new_tick = _InitializePopulationFromTskitDirectory(p_file_string, p_interpreter, p_subpop_remap);
	}
	else if (file_format == SLiMFileFormat::kFormatTskitBinary_HDF5)
		EIDOS_TERMINATION << "ERROR (Species::InitializePopulationFromFile): msprime HDF5 binary files are not supported; that file format has been superseded by kastore." << EidosTerminate();
	else
		EIDOS_TERMINATION << "ERROR (Species::InitializePopulationFromFile): unrecognized format code." << EidosTerminate();
	
	return new_tick;
}

slim_tick_t Species::_InitializePopulationFromTextFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::_InitializePopulationFromTextFile(): SLiM global state read");
	
	slim_tick_t file_tick, file_cycle;
	std::string line, sub; 
	std::ifstream infile(p_file);
	int spatial_output_count = 0;
	int age_output_count = 0;
	bool has_individual_pedigree_IDs = false;
	bool has_nucleotides = false;
	bool output_ancestral_nucs = false;
	
	if (!infile.is_open())
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): could not open initialization file." << EidosTerminate();
	
	// BCH 2/5/2025: I am removing code for reading file versions older than version 8 (SLiM 5.0); keeping
	// the legacy reading code working has been a headache and I want a clean break for multichrom
	
	// Parse the first line, to get the tick and cycle
	{
		GetInputLine(infile, line);
	
		std::istringstream iss(line);
		
		iss >> sub;		// #OUT:
		
		iss >> sub;		// tick
		int64_t tick_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		file_tick = SLiMCastToTickTypeOrRaise(tick_long);
		
		iss >> sub;		// cycle; this used to be the "A" file type tag, so we try to emit a good error message
		
		if (sub == "A")
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): reading of population files older than version 8 (SLiM 5.0) is no longer supported." << EidosTerminate();
		
		int64_t cycle_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		file_cycle = SLiMCastToTickTypeOrRaise(cycle_long);
		
		iss >> sub;		// should be "A"
		if (sub != "A")
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): the file type identifier in the #OUT line should be 'A', but is '" << sub << "'." << EidosTerminate();
	}
	
	// As of SLiM 2.1, we change the generation as a side effect of loading; otherwise we can't correctly update our state here!
	// As of SLiM 3, we set the generation up here, before making any individuals, because we need it to be correct for the tree-seq recording code.
	// As of SLiM 4, we set both the tick and the cycle, which are both saved to the file for version 7 and after.
	community_.SetTick(file_tick);
	SetCycle(file_cycle);
	
	// Read and ignore initial stuff until we hit the Populations section
	int64_t file_version = 0;	// represents no version tag found
	
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		// Starting in SLiM 3, we handle a Version line if we see one in passing, and it is required below
		if (line.find("Version:") != std::string::npos)
		{
			std::istringstream iss(line);
			
			iss >> sub;		// Version:
			iss >> sub;		// version number
			
			file_version = (int64_t)EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
			continue;
		}
		
		// Starting in SLiM 5, we handle a Flags line if we see one in passing, but it is not required
		if (line.find("Flags:") != std::string::npos)
		{
			std::istringstream iss(line);
			
			iss >> sub;		// Flags:
			
			while (iss >> sub)
			{
				if (sub == "SPACE=0")
					spatial_output_count = 0;
				else if (sub == "SPACE=1")
					spatial_output_count = 1;
				else if (sub == "SPACE=2")
					spatial_output_count = 2;
				else if (sub == "SPACE=3")
					spatial_output_count = 3;
				else if (sub == "AGES")
					age_output_count = 1;
				else if (sub == "PEDIGREES")
					has_individual_pedigree_IDs = true;
				else if (sub == "NUC")
					has_nucleotides = true;
				else if (sub == "ANC_SEQ")
					output_ancestral_nucs = true;
				else if (sub == "OBJECT_TAGS")
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): readFromPopulationFile() does not support reading in object tags from text format; output of object tags should be turned off in outputFull(), or you should save in binary instead with binary=T." << EidosTerminate();
				else if (sub == "SUBSTITUTIONS")
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): readFromPopulationFile() does not support reading in substitutions from text format; output of substitutions should be turned off in outputFull(), or you should save in binary instead with binary=T." << EidosTerminate();
				else
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): unrecognized flag in Flags line: '" << sub << "'." << EidosTerminate();
			}
			
			continue;
		}
		
		if (line.find("Populations") != std::string::npos)
			break;
	}
	
	// validate the file version
	if (file_version <= 0)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): file version is missing or corrupted; reading of population files older than version 8 (SLiM 5.0) is no longer supported." << EidosTerminate();
	if (file_version < 8)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): file version " << file_version << " detected; reading of population files older than version 8 (SLiM 5.0) is no longer supported." << EidosTerminate();
	if (file_version != 8)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): unrecognized version (" << file_version << "); the last version recognized by this version of SLiM is 8 (this file may have been generated by a more recent version of SLiM)." << EidosTerminate();
	
	// validate flags that were found (or not found)
	if ((spatial_output_count != 0) && (spatial_output_count != SpatialDimensionality()))	// note that we allow spatial information to be missing
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): a non-zero spatial dimensionality of " << spatial_output_count << " is flagged, but the spatial dimensionality of this model is " << SpatialDimensionality() << "; that is inconsistent." << EidosTerminate();
	
	if (age_output_count && (model_type_ == SLiMModelType::kModelTypeWF))
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): age information is present but the simulation is using a WF model; that is inconsistent." << EidosTerminate();
	if (!age_output_count && (model_type_ == SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): age information is not present but the simulation is using a nonWF model; age information must be included." << EidosTerminate();
	
	if (has_nucleotides && !IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): nucleotides are flagged as present in this file, but this is a non-nucleotide model; that is inconsistent." << EidosTerminate();
	if (!has_nucleotides && IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): nucleotides are not flagged as present in this file, but this is a nucleotide model; that is inconsistent." << EidosTerminate();
	if (output_ancestral_nucs && !has_nucleotides)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): an ancestral sequence is flagged as present, but nucleotides are not flagged as present; that is inconsistent." << EidosTerminate();
	
	// Now we are in the Populations section; read and instantiate each population until we hit the Individuals section
	while (!infile.eof())
	{ 
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Individuals") != std::string::npos)
			break;
		
		std::istringstream iss(line);
		
		iss >> sub;
		slim_objectid_t subpop_index = SLiMEidosScript::ExtractIDFromStringWithPrefix(sub, 'p', nullptr);
		
		iss >> sub;
		int64_t subpop_size_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(subpop_size_long);
		
		// SLiM 2.0 output format has <H | S <ratio>> here; if that is missing or "H" is given, the population is hermaphroditic and the ratio given is irrelevant
		double sex_ratio = 0.0;
		
		if (iss >> sub)
		{
			if (sub == "S")
			{
				iss >> sub;
				sex_ratio = EidosInterpreter::FloatForString(sub, nullptr);
			}
		}
		
		// Create the population population
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_index, subpop_size, sex_ratio, false);
		
		// define a new Eidos variable to refer to the new subpopulation
		EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
		
		if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): new subpopulation symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
		
		community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
	}
	
	// Now we are in the Individuals section; handle spatial positions, etc. until we hit a Chromosome line
	const std::vector<Chromosome *> &chromosomes = Chromosomes();
	
	if (has_individual_pedigree_IDs)
		gSLiM_next_pedigree_id = 0;
	
	if (line.find("Individuals") != std::string::npos)
	{
		while (!infile.eof()) 
		{
			GetInputLine(infile, line);
			
			if (line.length() == 0)
				continue;
			if (line.find("Chromosome") != std::string::npos)
				break;
			
			std::istringstream iss(line);
			
			iss >> sub;		// pX:iY – individual identifier
			std::size_t pos = static_cast<int>(sub.find_first_of(':'));
			
			if (pos == std::string::npos)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): colon missing in individual specifier." << EidosTerminate();
			
			std::string &&subpop_id_string = sub.substr(0, pos);
			
			slim_objectid_t subpop_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_string, 'p', nullptr);
			std::string &&individual_index_string = sub.substr(pos + 1, std::string::npos);
			
			if (individual_index_string[0] != 'i')
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): reference to individual is malformed." << EidosTerminate();
			
			int64_t individual_index = EidosInterpreter::NonnegativeIntegerForString(individual_index_string.c_str() + 1, nullptr);
			
			Subpopulation *subpop = SubpopulationWithID(subpop_id);
			
			if (!subpop)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): referenced subpopulation p" << subpop_id << " not defined." << EidosTerminate();
			
			if (individual_index >= subpop->parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): referenced individual i" << individual_index << " is out of range." << EidosTerminate();
			
			Individual &individual = *subpop->parent_individuals_[individual_index];
			
			if (has_individual_pedigree_IDs)
			{
				// If pedigree IDs are present use them; if not, we'll get whatever the default IDs are from the subpop construction
				iss >> sub;
				int64_t pedigree_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
				slim_pedigreeid_t pedigree_id = SLiMCastToPedigreeIDOrRaise(pedigree_long);
				
				if (PedigreesEnabled())
				{
					individual.SetPedigreeID(pedigree_id);
					gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);
					
					// we need to fix the haplosome ids for all of the individual's haplosomes
					int haplosome_index = 0;
					
					for (Chromosome *chromosome : chromosomes)
					{
						individual.haplosomes_[haplosome_index++]->SetHaplosomeID(pedigree_id * 2);
						
						if (chromosome->IntrinsicPloidy() == 2)
							individual.haplosomes_[haplosome_index++]->SetHaplosomeID(pedigree_id * 2 + 1);
					}
				}
			}
			
			bool sex_mismatch = false;
			iss >> sub;			// individual sex identifier (F/M/H)
			
			if (sub == "F")
			{
				if (individual.sex_ != IndividualSex::kFemale)
					sex_mismatch = true;
			}
			else if (sub == "M")
			{
				if (individual.sex_ != IndividualSex::kMale)
					sex_mismatch = true;
			}
			else if (sub == "H")
			{
				if (individual.sex_ != IndividualSex::kHermaphrodite)
					sex_mismatch = true;
			}
			else
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): unrecognized individual sex '" << sub << "'." << EidosTerminate();
			
			if (sex_mismatch)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): the specified individual sex '" << sub << "' does not match the sex of the individual '" << individual.sex_ << "'." << EidosTerminate();
			
			// BCH 2/5/2025: Before version 8, we emitted haplosome identifiers here, like "p1:16" and
			// "p1:17", but now that we have multiple chromosomes that really isn't helpful; removing
			// them.  In the Haplosomes section we will now just identify the individual; that suffices.
			
			// Parse the optional fields at the end of each individual line.  This is a bit tricky.
			// First we read all of the fields in, then we decide how to use them.
			std::vector<std::string> opt_params;
			int expected_opt_param_count = spatial_output_count + age_output_count;
			int opt_param_index = 0;
			
			while (iss >> sub)
				opt_params.emplace_back(sub);
			
			if ((int)opt_params.size() != expected_opt_param_count)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): output file format does not contain the expected individual data, as specified by the Flags line." << EidosTerminate();
			
			if (spatial_output_count)
			{
				// age information is present, in addition to the correct number of spatial positions
				if (spatial_output_count >= 1)
					individual.spatial_x_ = EidosInterpreter::FloatForString(opt_params[opt_param_index++], nullptr);	// spatial position x
				if (spatial_output_count >= 2)
					individual.spatial_y_ = EidosInterpreter::FloatForString(opt_params[opt_param_index++], nullptr);	// spatial position y
				if (spatial_output_count >= 3)
					individual.spatial_z_ = EidosInterpreter::FloatForString(opt_params[opt_param_index++], nullptr);	// spatial position z
			}
			
			if (age_output_count)
			{
				individual.age_ = (slim_age_t)EidosInterpreter::NonnegativeIntegerForString(opt_params[opt_param_index++], nullptr);	// age
			}
		}
	}
	
	// Now we loop over chromosomes; each starts with a Chromosome line and then contains subsections
	for (Chromosome *chromosome : chromosomes)
	{
		// we should currently have a Chromosome line that matches the current chromosome
		std::istringstream chrom_iss(line);
		
		chrom_iss >> sub;	// Chromosome:
		
		// chromosome index; chromosomes should be given in the same order as in the model
		chrom_iss >> sub;
		int64_t raw_chromosome_index = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		
		if (raw_chromosome_index >= (int)chromosomes.size())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): chromosome index " << raw_chromosome_index << " out of range." << EidosTerminate();
		
		slim_chromosome_index_t chromosome_index = (slim_chromosome_index_t)raw_chromosome_index;
		
		if (chromosome_index != chromosome->Index())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): chromosome index " << chromosome_index << " does not match expected index " << (unsigned int)(chromosome->Index()) << "." << EidosTerminate();
		
		int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
		//int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
		
		// chromosome type
		chrom_iss >> sub;
		ChromosomeType chromosome_type = ChromosomeTypeForString(sub);
		
		if (chromosome_type != chromosome->Type())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): chromosome type " << chromosome_type << " does not match expected index " << chromosome->Type() << "." << EidosTerminate();
		
		// chromosome id
		chrom_iss >> sub;
		int64_t chromosome_id = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		
		if (chromosome_id != chromosome->ID())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): chromosome id " << chromosome_id << " does not match expected id " << chromosome->ID() << "." << EidosTerminate();
		
		// chromosome last position
		chrom_iss >> sub;
		int64_t chromosome_lastpos = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		
		if (chromosome_lastpos != chromosome->last_position_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): chromosome last position " << chromosome_lastpos << " does not match expected last position " << chromosome->last_position_ << "." << EidosTerminate();
		
		// chromosome symbol
		chrom_iss >> sub;
		std::string quoted_symbol = '\"' + chromosome->Symbol() + '\"';
		
		if (sub != quoted_symbol)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): chromosome symbol " << sub << " does not match expected symbol " << chromosome->Symbol() << "." << EidosTerminate();
		
		GetInputLine(infile, line);
		if (line.find("Mutations") == std::string::npos)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): a Mutations section must follow each Chromosome line." << EidosTerminate();
		
		// Now we are in the Mutations section; read and instantiate all mutations and add them to our map and to the registry
#if EIDOS_ROBIN_HOOD_HASHING
		robin_hood::unordered_flat_map<slim_polymorphismid_t,MutationIndex> mutations;
#elif STD_UNORDERED_MAP_HASHING
		std::unordered_map<slim_polymorphismid_t,MutationIndex> mutations;
#endif
		Mutation *mut_block_ptr = mutation_block_->mutation_buffer_;
		
		while (!infile.eof()) 
		{
			GetInputLine(infile, line);
			
			if (line.length() == 0)
				continue;
			if (line.find("Haplosomes") != std::string::npos)
				break;
			
			std::istringstream iss(line);
			
			iss >> sub;
			int64_t polymorphismid_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
			slim_polymorphismid_t polymorphism_id = SLiMCastToPolymorphismidTypeOrRaise(polymorphismid_long);
			
			// Added in version 2 output, starting in SLiM 2.1
			iss >> sub;
			slim_mutationid_t mutation_id;
			
			if (sub[0] == 'm')	// autodetect whether we are parsing version 1 or version 2 output
			{
				mutation_id = polymorphism_id;		// when parsing version 1 output, we use the polymorphism id as the mutation id
			}
			else
			{
				mutation_id = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
				
				iss >> sub;		// queue up sub for mutation_type_id
			}
			
			slim_objectid_t mutation_type_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(sub, 'm', nullptr);
			
			iss >> sub;
			int64_t position_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
			slim_position_t position = SLiMCastToPositionTypeOrRaise(position_long);
			
			iss >> sub;
			slim_effect_t selection_coeff = static_cast<slim_effect_t>(EidosInterpreter::FloatForString(sub, nullptr));
			
			iss >> sub;
			slim_effect_t dominance_coeff = static_cast<slim_effect_t>(EidosInterpreter::FloatForString(sub, nullptr));
			
			iss >> sub;
			slim_objectid_t subpop_index = SLiMEidosScript::ExtractIDFromStringWithPrefix(sub, 'p', nullptr);
			
			iss >> sub;
			int64_t tick_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
			slim_tick_t tick = SLiMCastToTickTypeOrRaise(tick_long);
			
			iss >> sub;		// prevalence, which we discard
			
			int8_t nucleotide = -1;
			if (iss && (iss >> sub))
			{
				// fetch the nucleotide field if it is present
				if (sub == "A") nucleotide = 0;
				else if (sub == "C") nucleotide = 1;
				else if (sub == "G") nucleotide = 2;
				else if (sub == "T") nucleotide = 3;
				else EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): unrecognized value '"<< sub << "' in nucleotide field." << EidosTerminate();
			}
			
			// look up the mutation type from its index
			MutationType *mutation_type_ptr = MutationTypeWithID(mutation_type_id);
			
			if (!mutation_type_ptr) 
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " has not been defined for this species." << EidosTerminate();
			
			// BCH 7/2/2025: We no longer check the dominance coefficient against the mutation type, because it is allowed to differ
			
			// BCH 9/22/2021: Note that mutation_type_ptr->hemizygous_dominance_coeff_ is not saved, or checked here; too edge to be bothered...
			// FIXME MULTITRAIT: This will now change, since the hemizygous dominance coefficient is becoming a first-class citizen
			
			if ((nucleotide == -1) && mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " is nucleotide-based, but a nucleotide value for a mutation of this type was not supplied." << EidosTerminate();
			if ((nucleotide != -1) && !mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " is not nucleotide-based, but a nucleotide value for a mutation of this type was supplied." << EidosTerminate();
			
			// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
			MutationIndex new_mut_index = mutation_block_->NewMutationFromBlock();
			
			Mutation *new_mut = new (mut_block_ptr + new_mut_index) Mutation(mutation_id, mutation_type_ptr, chromosome_index, position, selection_coeff, dominance_coeff, subpop_index, tick, nucleotide);
			
			// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
			mutations.emplace(polymorphism_id, new_mut_index);
			population_.MutationRegistryAdd(new_mut);
			
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
			if (population_.keeping_muttype_registries_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
			
			// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_neutral_mutations_
			if (selection_coeff != (slim_effect_t)0.0)
			{
				pure_neutral_ = false;
				mutation_type_ptr->all_neutral_mutations_ = false;
			}
		}
		
		population_.InvalidateMutationReferencesCache();
		
		// Now we are in the Haplosomes section, which should take us to the end of the chromosome unless there is an Ancestral Sequence section
#ifndef _OPENMP
		MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
		slim_popsize_t previous_individual_index = -1;	// detect the first/second haplosome for intrinsically diploid chromosomes
		
		while (!infile.eof())
		{
			GetInputLine(infile, line);
			
			if (line.length() == 0)
				continue;
			if (line.find("Ancestral sequence") != std::string::npos)
				break;
			if (line.find("Chromosome") != std::string::npos)
				break;
			
			std::istringstream iss(line);
			
			iss >> sub;
			std::size_t pos = static_cast<int>(sub.find_first_of(':'));
			
			if (pos == std::string::npos)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): colon missing in individual specifier." << EidosTerminate();
			
			std::string &&subpop_id_string = sub.substr(0, pos);
			
			slim_objectid_t subpop_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_string, 'p', nullptr);
			std::string &&individual_index_string = sub.substr(pos + 1, std::string::npos);
			
			if (individual_index_string[0] != 'i')
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): reference to individual is malformed." << EidosTerminate();
			
			// this used to be the haplosome index, now it is the individual index and we have to figure out the haplosome index
			int64_t individual_index_long = EidosInterpreter::NonnegativeIntegerForString(individual_index_string.c_str() + 1, nullptr);
			
			Subpopulation *subpop = SubpopulationWithID(subpop_id);
			
			if (!subpop)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): referenced subpopulation p" << subpop_id << " not defined." << EidosTerminate();
			
			if (individual_index_long >= subpop->parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): referenced individual i" << individual_index_long << " is out of range." << EidosTerminate();
			slim_popsize_t individual_index = static_cast<slim_popsize_t>(individual_index_long);
			
			// detect when this is the second haplosome line for a given individual, and validate that
			// FIXME this code is brittle in various ways -- a second line might be needed but omitted, or a third line might be given
			bool is_individual_index_repeat = (individual_index == previous_individual_index);
			
			if (is_individual_index_repeat && (chromosome->IntrinsicPloidy() != 2))
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): a second haplosome was specified for a chromosome that is intrinsically haploid." << EidosTerminate();
			
			previous_individual_index = individual_index;
			
			// look up the individual and haplosome
			Individual *ind = subpop->parent_individuals_[individual_index];
			int haplosome_index = first_haplosome_index + is_individual_index_repeat;
			Haplosome &haplosome = *(ind->haplosomes_[haplosome_index]);
			
			if (haplosome.chromosome_index_ != chromosome->Index())
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): (internal error) haplosome does not belong to the focal chromosome." << EidosTerminate();
			
			if (iss >> sub)
			{
				// BCH 2/5/2025: We instantiate null haplosomes only where expect them to be, based upon
				// the chromosome type.  For chromosome types 'A' and 'H', null haplosomes can occur anywhere;
				// when that happens, we transform the instantiated haplosome to a null haplosome if necessary.
				// AddSubpopulation() created the haplosomes above, before we knew which would be null.
				if (sub == "<null>")
				{
					if (!haplosome.IsNull())
					{
						if ((model_type_ == SLiMModelType::kModelTypeNonWF) && ((chromosome_type == ChromosomeType::kA_DiploidAutosome) || (chromosome_type == ChromosomeType::kH_HaploidAutosome)))
						{
							haplosome.MakeNull();
							subpop->has_null_haplosomes_ = true;
						}
						else
							EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): haplosome is specified as null, but the instantiated haplosome is non-null." << EidosTerminate();
					}
					
					continue;	// this line is over
				}
				else
				{
					if (haplosome.IsNull())
						EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): haplosome is specified as non-null, but the instantiated haplosome is null." << EidosTerminate();
					
					// drop through, and sub will be interpreted as a mutation id below
				}
			}
			else
				continue;	// no mutations
				
			slim_position_t mutrun_length_ = haplosome.mutrun_length_;
			slim_mutrun_index_t current_mutrun_index = -1;
			MutationRun *current_mutrun = nullptr;
			
			do
			{
				int64_t polymorphismid_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
				slim_polymorphismid_t polymorphism_id = SLiMCastToPolymorphismidTypeOrRaise(polymorphismid_long);
				
				auto found_mut_pair = mutations.find(polymorphism_id);
				
				if (found_mut_pair == mutations.end()) 
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): polymorphism " << polymorphism_id << " has not been defined." << EidosTerminate();
				
				MutationIndex mutation = found_mut_pair->second;
				slim_mutrun_index_t mutrun_index = (slim_mutrun_index_t)((mut_block_ptr + mutation)->position_ / mutrun_length_);
				
				assert(mutrun_index != -1);		// to clue in the static analyzer
				
				if (mutrun_index != current_mutrun_index)
				{
#ifdef _OPENMP
					// When parallel, the MutationRunContext depends upon the position in the haplosome
					MutationRunContext &mutrun_context = chromosome.ChromosomeMutationRunContextForMutationRunIndex(mutrun_index);
#endif
					
					current_mutrun_index = mutrun_index;
					
					// We use WillModifyRun_UNSHARED() because we know that these runs are unshared (unless empty);
					// we created them empty, nobody has modified them but us, and we process each haplosome separately.
					// However, using WillModifyRun() would generally be fine since we hit this call only once
					// per mutrun per haplosome anyway, as long as the mutations are sorted by position.
					current_mutrun = haplosome.WillModifyRun_UNSHARED(current_mutrun_index, mutrun_context);
				}
				
				current_mutrun->emplace_back(mutation);
			}
			while (iss >> sub);
		}
		
		// Now we are in the Ancestral sequence section, which should take us to the end of the chromosome
		// (or file).  Conveniently, NucleotideArray supports operator>> to read nucleotides until the EOF.
		// BCH 2/5/2025: that operator>> code now stops if it sees two newlines, also, which we rely on here
		// to recognize the end of the sequence and then begin a new Chromosome section.
		if (line.find("Ancestral sequence") != std::string::npos)
		{
			infile >> *(chromosome->AncestralSequence());
		}
		else if (output_ancestral_nucs)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): an ancestral sequence is flagged as present, but was not found." << EidosTerminate();
	}
	
	// It's a little unclear how we ought to clean up after ourselves, and this is a continuing source of bugs.  We could be loading
	// a new population in an early() event, in a late() event, or in between cycles in SLiMgui, e.g. in the Eidos console.
	// The safest avenue seems to be to just do all the bookkeeping we can think of: tally frequencies, calculate fitnesses, and
	// survey the population for SLiMgui.  This will lead to some of these actions being done at an unusual time in the cycle,
	// though, and will cause some things to be done unnecessarily (because they are not normally up-to-date at the current
	// cycle stage anyway) or done twice (which could be particularly problematic for mutationEffect() callbacks).  Nevertheless, this seems
	// like the best policy, at least until shown otherwise...  BCH 11 June 2016
	
	// BCH 5 April 2017: Well, it has been shown otherwise.  Now that interactions have been added, mutationEffect() callbacks often depend on
	// them, which means the interactions need to be evaluated, which means we can't evaluate fitness values yet; we need to give the
	// user's script a chance to evaluate the interactions.  This was always a problem, really; mutationEffect() callbacks might have needed
	// some external state to be set up that would be on the population state.  But now it is a glaring problem, and forces us to revise
	// our policy.  All we do now is unique mutation runs and retally mutrun/mutation counts.
	
	// Re-tally mutation references so we have accurate frequency counts for our new mutations
	population_.UniqueMutationRuns();
	population_.InvalidateMutationReferencesCache();	// force a retally
	population_.TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ false);
	
	return file_tick;
}

#ifndef __clang_analyzer__
slim_tick_t Species::_InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::_InitializePopulationFromBinaryFile(): SLiM global state read");
	
	std::size_t file_size = 0;
	slim_tick_t file_tick, file_cycle;
	
	// options in the flags field
	int32_t spatial_output_count = 0;
	int age_output_count = 0;
	int pedigree_output_count = 0;
	bool has_nucleotides = false;
	bool has_ancestral_nucs = false;
	bool has_object_tags = false;
	bool has_substitutions = false;
	
	// Read file into buf
	std::ifstream infile(p_file, std::ios::in | std::ios::binary);
	
	if (!infile.is_open() || infile.eof())
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): could not open initialization file." << EidosTerminate();
	
	// Determine the file length
	infile.seekg(0, std::ios_base::end);
	file_size = infile.tellg();
	
	// Read in the entire file; we assume we have enough memory, for now
	std::unique_ptr<char[]> raii_buf(new char[file_size]);
	char *buf = raii_buf.get();
	
	if (!buf)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): could not allocate input buffer." << EidosTerminate();
	
	char *buf_end = buf + file_size;
	char *p = buf;
	
	infile.seekg(0, std::ios_base::beg);
	infile.read(buf, file_size);
	
	// Close the file; we will work only with our buffer from here on
	// Note that we use memcpy() to read values from the buffer, since it takes care of alignment issues
	// for us that otherwise bother the UndefinedBehaviorSanitizer.  On platforms that don't care about
	// alignment this should compile down to the same code; on platforms that do care, it avoids a crash.
	infile.close();
	
	int32_t section_end_tag;
	int32_t file_version;
	
	// Header beginning, to check endianness and determine file version
	{
		int32_t endianness_tag, version_tag;
		
		if (p + sizeof(endianness_tag) + sizeof(version_tag) > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF while reading header." << EidosTerminate();
		
		memcpy(&endianness_tag, p, sizeof(endianness_tag));
		p += sizeof(endianness_tag);
		
		memcpy(&version_tag, p, sizeof(version_tag));
		p += sizeof(version_tag);
		
		if (endianness_tag != 0x12345678)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): endianness mismatch." << EidosTerminate();
		
		file_version = version_tag;
		
		if (file_version <= 0)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): file version is missing or corrupted; reading of population files older than version 8 (SLiM 5.0) is no longer supported." << EidosTerminate();
		if (file_version < 8)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): file version " << file_version << " detected; reading of population files older than version 8 (SLiM 5.0) is no longer supported." << EidosTerminate();
		if (file_version != 8)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unrecognized version (" << file_version << "); the last version recognized by this version of SLiM is 8 (this file may have been generated by a more recent version of SLiM)." << EidosTerminate();
	}
	
	// Header section
	{
		int32_t double_size;
		double double_test;
		int64_t flags = 0;
		// FIXME MULTITRAIT: add new sizes here like slim_fitness_t
		int32_t slim_tick_t_size, slim_position_t_size, slim_objectid_t_size, slim_popsize_t_size, slim_refcount_t_size, slim_effect_t_size, slim_mutationid_t_size, slim_polymorphismid_t_size, slim_age_t_size, slim_pedigreeid_t_size, slim_haplosomeid_t_size, slim_usertag_t_size;
		int header_length = sizeof(double_size) + sizeof(double_test) + sizeof(flags) + sizeof(slim_tick_t_size) + sizeof(slim_position_t_size) + sizeof(slim_objectid_t_size) + sizeof(slim_popsize_t_size) + sizeof(slim_refcount_t_size) + sizeof(slim_effect_t_size) + sizeof(slim_mutationid_t_size) + sizeof(slim_polymorphismid_t_size) + sizeof(slim_age_t_size) + sizeof(slim_pedigreeid_t_size) + sizeof(slim_haplosomeid_t_size) + sizeof(slim_usertag_t_size) + sizeof(file_tick) + sizeof(file_cycle) + sizeof(section_end_tag);
		
		// this is how to add more header tags in future versions
		//if (file_version >= 9)
		//	header_length += sizeof(new_header_variable);
		
		if (p + header_length > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF while reading header." << EidosTerminate();
		
		memcpy(&double_size, p, sizeof(double_size));
		p += sizeof(double_size);
		
		memcpy(&double_test, p, sizeof(double_test));
		p += sizeof(double_test);
		
		memcpy(&flags, p, sizeof(flags));
		p += sizeof(flags);
		
		if (flags & 0x0003) spatial_output_count = (flags & 0x03);
		if (flags & 0x0004) age_output_count = 1;
		if (flags & 0x0008) pedigree_output_count = 1;
		if (flags & 0x0010) has_nucleotides = true;
		if (flags & 0x0020) has_ancestral_nucs = true;
		if (flags & 0x0040) has_object_tags = true;
		if (flags & 0x0080) has_substitutions = true;
		
		memcpy(&slim_tick_t_size, p, sizeof(slim_tick_t_size));
		p += sizeof(slim_tick_t_size);
		
		memcpy(&slim_position_t_size, p, sizeof(slim_position_t_size));
		p += sizeof(slim_position_t_size);
		
		memcpy(&slim_objectid_t_size, p, sizeof(slim_objectid_t_size));
		p += sizeof(slim_objectid_t_size);
		
		memcpy(&slim_popsize_t_size, p, sizeof(slim_popsize_t_size));
		p += sizeof(slim_popsize_t_size);
		
		memcpy(&slim_refcount_t_size, p, sizeof(slim_refcount_t_size));
		p += sizeof(slim_refcount_t_size);
		
		memcpy(&slim_effect_t_size, p, sizeof(slim_effect_t_size));
		p += sizeof(slim_effect_t_size);
		
		memcpy(&slim_mutationid_t_size, p, sizeof(slim_mutationid_t_size));
		p += sizeof(slim_mutationid_t_size);
		
		memcpy(&slim_polymorphismid_t_size, p, sizeof(slim_polymorphismid_t_size));
		p += sizeof(slim_polymorphismid_t_size);
		
		memcpy(&slim_age_t_size, p, sizeof(slim_age_t_size));
		p += sizeof(slim_age_t_size);
		
		memcpy(&slim_pedigreeid_t_size, p, sizeof(slim_pedigreeid_t_size));
		p += sizeof(slim_pedigreeid_t_size);
		
		memcpy(&slim_haplosomeid_t_size, p, sizeof(slim_haplosomeid_t_size));
		p += sizeof(slim_haplosomeid_t_size);
		
		memcpy(&slim_usertag_t_size, p, sizeof(slim_usertag_t_size));
		p += sizeof(slim_usertag_t_size);
		
		memcpy(&file_tick, p, sizeof(file_tick));
		p += sizeof(file_tick);
		
		memcpy(&file_cycle, p, sizeof(file_cycle));
		p += sizeof(file_cycle);
		
		memcpy(&section_end_tag, p, sizeof(section_end_tag));
		p += sizeof(section_end_tag);
		
		if (double_size != sizeof(double))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): sizeof(double) mismatch." << EidosTerminate();
		if (double_test != 1234567890.0987654321)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): double format mismatch." << EidosTerminate();
		
		if ((spatial_output_count < 0) || (spatial_output_count > 3))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): spatial output count out of range." << EidosTerminate();
		if ((spatial_output_count > 0) && (spatial_output_count != spatial_dimensionality_))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): output spatial dimensionality does not match that of the simulation." << EidosTerminate();
		
		if (age_output_count && (model_type_ == SLiMModelType::kModelTypeWF))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): age information is present but the simulation is using a WF model." << EidosTerminate();
		if (!age_output_count && (model_type_ == SLiMModelType::kModelTypeNonWF))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): age information is not present but the simulation is using a nonWF model; age information must be included." << EidosTerminate();
		
		if (has_nucleotides && !nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): the output was generated by a nucleotide-based model, but the current model is not nucleotide-based." << EidosTerminate();
		if (!has_nucleotides && nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): the output was generated by a non-nucleotide-based model, but the current model is nucleotide-based." << EidosTerminate();
		if (has_ancestral_nucs && !has_nucleotides)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): an ancestral sequence is flagged as present, but the current model is not nucleotide-based." << EidosTerminate();
		
		if ((slim_tick_t_size != sizeof(slim_tick_t)) ||
			(slim_position_t_size != sizeof(slim_position_t)) ||
			(slim_objectid_t_size != sizeof(slim_objectid_t)) ||
			(slim_popsize_t_size != sizeof(slim_popsize_t)) ||
			(slim_refcount_t_size != sizeof(slim_refcount_t)) ||
			(slim_effect_t_size != sizeof(slim_effect_t)) ||
			(slim_mutationid_t_size != sizeof(slim_mutationid_t)) ||
			(slim_polymorphismid_t_size != sizeof(slim_polymorphismid_t)) ||
			(slim_age_t_size != sizeof(slim_age_t)) ||
			(slim_pedigreeid_t_size != sizeof(slim_pedigreeid_t)) ||
			(slim_haplosomeid_t_size != sizeof(slim_haplosomeid_t)) ||
			(slim_usertag_t_size != sizeof(slim_usertag_t)))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): SLiM datatype size mismatch." << EidosTerminate();
		
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after header." << EidosTerminate();
	}
	
	// As of SLiM 2.1, we change the generation as a side effect of loading; otherwise we can't correctly update our state here!
	// As of SLiM 3, we set the generation up here, before making any individuals, because we need it to be correct for the tree-seq recording code.
	// As of SLiM 4, we set both the tick and the cycle, which are both saved to the file for version 7 and after.
	community_.SetTick(file_tick);
	SetCycle(file_cycle);
	
	// Populations section
	while (true)
	{
		int32_t subpop_start_tag;
		slim_objectid_t subpop_id;
		slim_popsize_t subpop_size;
		int32_t sex_flag;
		double subpop_sex_ratio;
		
		// If there isn't enough buffer left to read a full subpop record, we assume we are done with this section
		if (p + sizeof(subpop_start_tag) + sizeof(subpop_id) + sizeof(subpop_size) + sizeof(sex_flag) + sizeof(subpop_sex_ratio) + (has_object_tags ? sizeof(slim_usertag_t) : 0) > buf_end)
			break;
		
		// If the first int32_t is not a subpop start tag, then we are done with this section
		memcpy(&subpop_start_tag, p, sizeof(subpop_start_tag));
		if (subpop_start_tag != (int32_t)0xFFFF0001)
			break;
		
		// Otherwise, we have a subpop record; read in the rest of it
		p += sizeof(subpop_start_tag);
		
		memcpy(&subpop_id, p, sizeof(subpop_id));
		p += sizeof(subpop_id);
		
		memcpy(&subpop_size, p, sizeof(subpop_size));
		p += sizeof(subpop_size);
		
		memcpy(&sex_flag, p, sizeof(sex_flag));
		p += sizeof(sex_flag);
		
		if (sex_flag != sex_enabled_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): sex vs. hermaphroditism mismatch between file and simulation." << EidosTerminate();
		
		memcpy(&subpop_sex_ratio, p, sizeof(subpop_sex_ratio));
		p += sizeof(subpop_sex_ratio);
		
		// Create the population population
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, subpop_sex_ratio, false);
		
		if (has_object_tags)
		{
			memcpy(&new_subpop->tag_value_, p, sizeof(new_subpop->tag_value_));
			p += sizeof(new_subpop->tag_value_);
		}
		
		// define a new Eidos variable to refer to the new subpopulation
		EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
		
		if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): new subpopulation symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
		
		community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
	}
	
	if (p + sizeof(section_end_tag) > buf_end)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after subpopulations." << EidosTerminate();
	else
	{
		memcpy(&section_end_tag, p, sizeof(section_end_tag));
		p += sizeof(section_end_tag);
		
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after subpopulations." << EidosTerminate();
	}
	
	// Individuals section
	const std::vector<Chromosome *> &chromosomes = Chromosomes();
	
	if (pedigree_output_count)
		gSLiM_next_pedigree_id = 0;
	
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		slim_popsize_t subpop_size = subpop->parent_subpop_size_;
		
		for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)	// go through all children
		{
			// If there isn't enough buffer left to read a full subpop record, we have an error
			if (p + sizeof(IndividualSex) + pedigree_output_count * sizeof(slim_pedigreeid_t) + spatial_output_count * sizeof(double) + age_output_count * sizeof(slim_age_t) + (has_object_tags ? sizeof(slim_usertag_t) + sizeof(double) + 5*sizeof(char) : 0) > buf_end)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF in individuals section." << EidosTerminate();
			
			Individual &individual = *(subpop->parent_individuals_[individual_index]);
			
			memcpy(&individual.sex_, p, sizeof(individual.sex_));
			p += sizeof(individual.sex_);
			
			if (pedigree_output_count)
			{
				if (PedigreesEnabled())
				{
					slim_pedigreeid_t pedigree_id;
					
					memcpy(&pedigree_id, p, sizeof(pedigree_id));
					
					individual.SetPedigreeID(pedigree_id);
					gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);
					
					// we need to fix the haplosome ids for all of the individual's haplosomes
					int haplosome_index = 0;
					
					for (Chromosome *chromosome : chromosomes)
					{
						individual.haplosomes_[haplosome_index++]->SetHaplosomeID(pedigree_id * 2);
						
						if (chromosome->IntrinsicPloidy() == 2)
							individual.haplosomes_[haplosome_index++]->SetHaplosomeID(pedigree_id * 2 + 1);
					}
				}
				
				p += sizeof(slim_pedigreeid_t);
			}
			
			if (spatial_output_count)
			{
				if (spatial_output_count >= 1)
				{
					memcpy(&individual.spatial_x_, p, sizeof(individual.spatial_x_));
					p += sizeof(double);
				}
				if (spatial_output_count >= 2)
				{
					memcpy(&individual.spatial_y_, p, sizeof(individual.spatial_y_));
					p += sizeof(double);
				}
				if (spatial_output_count >= 3)
				{
					memcpy(&individual.spatial_z_, p, sizeof(individual.spatial_z_));
					p += sizeof(double);
				}
			}
			
			if (age_output_count)
			{
				memcpy(&individual.age_, p, sizeof(individual.age_));
				p += sizeof(slim_age_t);
			}
			
			if (has_object_tags)
			{
				memcpy(&individual.tag_value_, p, sizeof(individual.tag_value_));
				p += sizeof(tag_value_);
				
				memcpy(&individual.tagF_value_, p, sizeof(individual.tagF_value_));
				p += sizeof(individual.tagF_value_);
				
				{
					char tagL0_value;
					memcpy(&tagL0_value, p, sizeof(tagL0_value));
					p += sizeof(char);
					
					if (tagL0_value == 0) { individual.tagL0_set_ = 1; individual.tagL0_value_ = 0; }
					else if (tagL0_value == 1) { individual.tagL0_set_ = 1; individual.tagL0_value_ = 1; }
					else { individual.tagL0_set_ = 0; }
				}
				{
					char tagL1_value;
					memcpy(&tagL1_value, p, sizeof(tagL1_value));
					p += sizeof(char);
					
					if (tagL1_value == 0) { individual.tagL1_set_ = 1; individual.tagL1_value_ = 0; }
					else if (tagL1_value == 1) { individual.tagL1_set_ = 1; individual.tagL1_value_ = 1; }
					else { individual.tagL1_set_ = 0; }
				}
				{
					char tagL2_value;
					memcpy(&tagL2_value, p, sizeof(tagL2_value));
					p += sizeof(char);
					
					if (tagL2_value == 0) { individual.tagL2_set_ = 1; individual.tagL2_value_ = 0; }
					else if (tagL2_value == 1) { individual.tagL2_set_ = 1; individual.tagL2_value_ = 1; }
					else { individual.tagL2_set_ = 0; }
				}
				{
					char tagL3_value;
					memcpy(&tagL3_value, p, sizeof(tagL3_value));
					p += sizeof(char);
					
					if (tagL3_value == 0) { individual.tagL3_set_ = 1; individual.tagL3_value_ = 0; }
					else if (tagL3_value == 1) { individual.tagL3_set_ = 1; individual.tagL3_value_ = 1; }
					else { individual.tagL3_set_ = 0; }
				}
				{
					char tagL4_value;
					memcpy(&tagL4_value, p, sizeof(tagL4_value));
					p += sizeof(char);
					
					if (tagL4_value == 0) { individual.tagL4_set_ = 1; individual.tagL4_value_ = 0; }
					else if (tagL4_value == 1) { individual.tagL4_set_ = 1; individual.tagL4_value_ = 1; }
					else { individual.tagL4_set_ = 0; }
				}
			}
		}
	}
	
	if (p + sizeof(section_end_tag) > buf_end)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after individuals." << EidosTerminate();
	else
	{
		memcpy(&section_end_tag, p, sizeof(section_end_tag));
		p += sizeof(section_end_tag);
		
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after individuals." << EidosTerminate();
	}
	
	// Loop over the chromosomes.  Each chromosome gets a section end tag.  We begin with a chromosome count.
	int32_t chromosome_count;
	
	if (p + sizeof(chromosome_count) > buf_end)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF at chromosome count." << EidosTerminate();
	else
	{
		memcpy(&chromosome_count, p, sizeof(chromosome_count));
		p += sizeof(chromosome_count);
		
		if (chromosome_count != (int)chromosomes.size())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): chromosome count does not match the model state." << EidosTerminate();
	}
	
	for (Chromosome *chromosome : chromosomes)
	{
		// Read and validate information about the chromosome
		int32_t raw_chromosome_index;
		int32_t raw_chromosome_type;
		int64_t chromosome_id;
		int64_t chromosome_lastpos;
		int32_t mutation_map_size;
		
		if (p + sizeof(raw_chromosome_index) + sizeof(raw_chromosome_type) + sizeof(chromosome_id) + sizeof(chromosome_lastpos) + (has_object_tags ? sizeof(slim_usertag_t) : 0) > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF in chromosome information." << EidosTerminate();
		
		memcpy(&raw_chromosome_index, p, sizeof(raw_chromosome_index));
		p += sizeof(raw_chromosome_index);

		if (raw_chromosome_index != chromosome->Index())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): chromosome index mismatch." << EidosTerminate();
		
		slim_chromosome_index_t chromosome_index = (slim_chromosome_index_t)raw_chromosome_index;
		
		int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
		
		memcpy(&raw_chromosome_type, p, sizeof(raw_chromosome_type));
		p += sizeof(raw_chromosome_type);
		
		ChromosomeType chromosome_type = (ChromosomeType)raw_chromosome_type;
		
		if (chromosome_type != chromosome->Type())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): chromosome type mismatch." << EidosTerminate();
		
		memcpy(&chromosome_id, p, sizeof(chromosome_id));
		p += sizeof(chromosome_id);
		
		if (chromosome_id != chromosome->ID())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): chromosome id mismatch." << EidosTerminate();
		
		memcpy(&chromosome_lastpos, p, sizeof(chromosome_lastpos));
		p += sizeof(chromosome_lastpos);
		
		if (chromosome_lastpos != chromosome->last_position_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): chromosome last position mismatch." << EidosTerminate();
		
		if (has_object_tags)
		{
			memcpy(&chromosome->tag_value_, p, sizeof(chromosome->tag_value_));
			p += sizeof(chromosome->tag_value_);
		}
		
		// Read in the size of the mutation map, so we can allocate a vector rather than utilizing std::map
		if (p + sizeof(mutation_map_size) > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF at mutation map size." << EidosTerminate();
		else
		{
			memcpy(&mutation_map_size, p, sizeof(mutation_map_size));
			p += sizeof(mutation_map_size);
		}
		
		// Mutations section
		std::unique_ptr<MutationIndex[]> raii_mutations(new MutationIndex[mutation_map_size]);
		MutationIndex *mutations = raii_mutations.get();
		Mutation *mut_block_ptr = mutation_block_->mutation_buffer_;
		
		if (!mutations)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): could not allocate mutations buffer." << EidosTerminate();
		
		while (true)
		{
			int32_t mutation_start_tag;
			slim_polymorphismid_t polymorphism_id;
			slim_mutationid_t mutation_id;
			slim_objectid_t mutation_type_id;
			slim_position_t position;
			slim_effect_t selection_coeff;
			slim_effect_t dominance_coeff;
			slim_objectid_t subpop_index;
			slim_tick_t tick;
			slim_refcount_t prevalence;
			int8_t nucleotide = -1;
			
			// If there isn't enough buffer left to read a full mutation record, we assume we are done with this section
			int record_size = sizeof(mutation_start_tag) + sizeof(polymorphism_id) + sizeof(mutation_id) + sizeof(mutation_type_id) + sizeof(position) + sizeof(selection_coeff) + sizeof(dominance_coeff) + sizeof(subpop_index) + sizeof(tick) + sizeof(prevalence);
			
			if (has_nucleotides)
				record_size += sizeof(nucleotide);
			if (has_object_tags)
				record_size += sizeof(slim_usertag_t);
			
			if (p + record_size > buf_end)
				break;
			
			// If the first int32_t is not a mutation start tag, then we are done with this section
			memcpy(&mutation_start_tag, p, sizeof(mutation_start_tag));
			if (mutation_start_tag != (int32_t)0xFFFF0002)
				break;
			
			// Otherwise, we have a mutation record; read in the rest of it
			p += sizeof(mutation_start_tag);
			
			memcpy(&polymorphism_id, p, sizeof(polymorphism_id));
			p += sizeof(polymorphism_id);
			
			memcpy(&mutation_id, p, sizeof(mutation_id));
			p += sizeof(mutation_id);
			
			memcpy(&mutation_type_id, p, sizeof(mutation_type_id));
			p += sizeof(mutation_type_id);
			
			memcpy(&position, p, sizeof(position));
			p += sizeof(position);
			
			memcpy(&selection_coeff, p, sizeof(selection_coeff));
			p += sizeof(selection_coeff);
			
			memcpy(&dominance_coeff, p, sizeof(dominance_coeff));
			p += sizeof(dominance_coeff);
			
			memcpy(&subpop_index, p, sizeof(subpop_index));
			p += sizeof(subpop_index);
			
			memcpy(&tick, p, sizeof(tick));
			p += sizeof(tick);
			
			memcpy(&prevalence, p, sizeof(prevalence));
			(void)prevalence;	// we don't use the frequency when reading the pop data back in; let the static analyzer know that's OK
			p += sizeof(prevalence);
			
			if (has_nucleotides)
			{
				memcpy(&nucleotide, p, sizeof(nucleotide));
				p += sizeof(nucleotide);
			}
			
			// look up the mutation type from its index
			MutationType *mutation_type_ptr = MutationTypeWithID(mutation_type_id);
			
			if (!mutation_type_ptr) 
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " has not been defined for this species." << EidosTerminate();
			
			// BCH 7/2/2025: We no longer check the dominance coefficient against the mutation type, because it is allowed to differ
			
			// BCH 9/22/2021: Note that mutation_type_ptr->hemizygous_dominance_coeff_ is not saved, or checked here; too edge to be bothered...
			// FIXME MULTITRAIT: This will now change, since the hemizygous dominance coefficient is becoming a first-class citizen
			
			if ((nucleotide == -1) && mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " is nucleotide-based, but a nucleotide value for a mutation of this type was not supplied." << EidosTerminate();
			if ((nucleotide != -1) && !mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " is not nucleotide-based, but a nucleotide value for a mutation of this type was supplied." << EidosTerminate();
			
			// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
			MutationIndex new_mut_index = mutation_block_->NewMutationFromBlock();
			
			Mutation *new_mut = new (mut_block_ptr + new_mut_index) Mutation(mutation_id, mutation_type_ptr, chromosome_index, position, selection_coeff, dominance_coeff, subpop_index, tick, nucleotide);
			
			// read the tag value, if present
			if (has_object_tags)
			{
				memcpy(&new_mut->tag_value_, p, sizeof(slim_usertag_t));
				p += sizeof(slim_usertag_t);
			}
			
			// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
			mutations[polymorphism_id] = new_mut_index;
			population_.MutationRegistryAdd(new_mut);
			
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
			if (population_.keeping_muttype_registries_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
			
			// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_neutral_mutations_
			if (selection_coeff != (slim_effect_t)0.0)
			{
				pure_neutral_ = false;
				mutation_type_ptr->all_neutral_mutations_ = false;
			}
		}
		
		population_.InvalidateMutationReferencesCache();
		
		if (p + sizeof(section_end_tag) > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after mutations." << EidosTerminate();
		else
		{
			memcpy(&section_end_tag, p, sizeof(section_end_tag));
			p += sizeof(section_end_tag);
			
			if (section_end_tag != (int32_t)0xFFFF0000)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after mutations." << EidosTerminate();
		}
		
		// Haplosomes section
		bool use_16_bit = (mutation_map_size <= UINT16_MAX - 1);	// 0xFFFF is reserved as the start of our various tags
		std::unique_ptr<MutationIndex[]> raii_haplosomebuf(new MutationIndex[mutation_map_size]);	// allowing us to use emplace_back_bulk() for speed
		MutationIndex *haplosomebuf = raii_haplosomebuf.get();
#ifndef _OPENMP
		MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome &haplosome = *haplosomes[haplosome_index];
					slim_objectid_t subpop_id;
					int32_t total_mutations;
					
					// If there isn't enough buffer left to read a full haplosome record, we have an error
					if (p + sizeof(subpop_id) + sizeof(total_mutations) + (has_object_tags ? sizeof(slim_usertag_t) : 0) > buf_end)
						EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF in haplosome header." << EidosTerminate();
					
					memcpy(&subpop_id, p, sizeof(subpop_id));
					p += sizeof(subpop_id);
					
					if (subpop_id != subpop_pair.first + 1)		// + 1 to avoid colliding with section_end_tag
						EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): subpop id mismatch." << EidosTerminate();
					
					if (has_object_tags)
					{
						memcpy(&haplosome.tag_value_, p, sizeof(slim_usertag_t));
						p += sizeof(slim_usertag_t);
					}
					
					memcpy(&total_mutations, p, sizeof(total_mutations));
					p += sizeof(total_mutations);
					
					// Check the null haplosome state
					// BCH 2/5/2025: We instantiate null haplosomes only where expect them to be, based upon
					// the chromosome type.  For chromosome types 'A' and 'H', null haplosomes can occur anywhere;
					// when that happens, we transform the instantiated haplosome to a null haplosome if necessary.
					// AddSubpopulation() created the haplosomes above, before we knew which would be null.
					if (total_mutations == (int32_t)0xFFFF1000)
					{
						if (!haplosome.IsNull())
						{
							if ((model_type_ == SLiMModelType::kModelTypeNonWF) && ((chromosome_type == ChromosomeType::kA_DiploidAutosome) || (chromosome_type == ChromosomeType::kH_HaploidAutosome)))
							{
								haplosome.MakeNull();
								subpop->has_null_haplosomes_ = true;
							}
							else
								EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): haplosome is specified as null, but the instantiated haplosome is non-null." << EidosTerminate();
						}
					}
					else
					{
						if (haplosome.IsNull())
							EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): haplosome is specified as non-null, but the instantiated haplosome is null." << EidosTerminate();
						
						// Read in the mutation list
						int32_t mutcount = 0;
						
						if (use_16_bit)
						{
							// reading 16-bit mutation tags
							uint16_t mutation_id;
							
							if (p + sizeof(mutation_id) * total_mutations > buf_end)
								EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF while reading haplosome." << EidosTerminate();
							
							for (; mutcount < total_mutations; ++mutcount)
							{
								memcpy(&mutation_id, p, sizeof(mutation_id));
								p += sizeof(mutation_id);
								
								// Add mutation to haplosome
								if (/*(mutation_id < 0) ||*/ (mutation_id >= mutation_map_size)) 
									EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation " << mutation_id << " has not been defined." << EidosTerminate();
								
								haplosomebuf[mutcount] = mutations[mutation_id];
							}
						}
						else
						{
							// reading 32-bit mutation tags
							int32_t mutation_id;
							
							if (p + sizeof(mutation_id) * total_mutations > buf_end)
								EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF while reading haplosome." << EidosTerminate();
							
							for (; mutcount < total_mutations; ++mutcount)
							{
								memcpy(&mutation_id, p, sizeof(mutation_id));
								p += sizeof(mutation_id);
								
								// Add mutation to haplosome
								if ((mutation_id < 0) || (mutation_id >= mutation_map_size)) 
									EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation " << mutation_id << " has not been defined." << EidosTerminate();
								
								haplosomebuf[mutcount] = mutations[mutation_id];
							}
						}
						
						slim_position_t mutrun_length_ = haplosome.mutrun_length_;
						slim_mutrun_index_t current_mutrun_index = -1;
						MutationRun *current_mutrun = nullptr;
						
						for (int mut_index = 0; mut_index < mutcount; ++mut_index)
						{
							MutationIndex mutation = haplosomebuf[mut_index];
							slim_mutrun_index_t mutrun_index = (slim_mutrun_index_t)((mut_block_ptr + mutation)->position_ / mutrun_length_);
							
							if (mutrun_index != current_mutrun_index)
							{
#ifdef _OPENMP
								// When parallel, the MutationRunContext depends upon the position in the haplosome
								MutationRunContext &mutrun_context = chromosome.ChromosomeMutationRunContextForMutationRunIndex(mutrun_index);
#endif
								
								current_mutrun_index = mutrun_index;
								
								// We use WillModifyRun_UNSHARED() because we know that these runs are unshared (unless empty);
								// we created them empty, nobody has modified them but us, and we process each haplosome separately.
								// However, using WillModifyRun() would generally be fine since we hit this call only once
								// per mutrun per haplosome anyway, as long as the mutations are sorted by position.
								current_mutrun = haplosome.WillModifyRun_UNSHARED(current_mutrun_index, mutrun_context);
							}
							
							current_mutrun->emplace_back(mutation);
						}
					}
				}
			}
		}
		
		if (p + sizeof(section_end_tag) > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after haplosomes." << EidosTerminate();
		else
		{
			memcpy(&section_end_tag, p, sizeof(section_end_tag));
			p += sizeof(section_end_tag);
			
			if (section_end_tag != (int32_t)0xFFFF0000)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after haplosomes." << EidosTerminate();
		}
		
		// Ancestral sequence section, for nucleotide-based models
		// The ancestral sequence can be suppressed at save time, to decrease file size etc.  If it is missing,
		// we do not consider that an error at present.  This is a little weird – it's more useful to suppress
		// the ancestral sequence when writing text – but maybe the user really doesn't want it.  So do nothing.
		if (has_ancestral_nucs)
		{
			if (p + sizeof(int64_t) > buf_end)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): ancestral sequence was expected but is missing." << EidosTerminate();
			
			chromosome->AncestralSequence()->ReadCompressedNucleotides(&p, buf_end);
			
			if (p + sizeof(section_end_tag) > buf_end)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after ancestral sequence." << EidosTerminate();
			else
			{
				memcpy(&section_end_tag, p, sizeof(section_end_tag));
				p += sizeof(section_end_tag);
				
				if (section_end_tag != (int32_t)0xFFFF0000)
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after ancestral sequence." << EidosTerminate();
			}
		}
	}
	
	if (has_substitutions)
	{
		while (true)
		{
			int32_t substitution_start_tag;
			slim_mutationid_t mutation_id;
			slim_objectid_t mutation_type_id;
			slim_position_t position;
			slim_effect_t selection_coeff;
			slim_effect_t dominance_coeff;
			slim_objectid_t subpop_index;
			slim_tick_t origin_tick;
			slim_tick_t fixation_tick;
			slim_chromosome_index_t chromosome_index;
			int8_t nucleotide = -1;
			
			// If there isn't enough buffer left to read a full mutation record, we assume we are done with this section
			int record_size = sizeof(substitution_start_tag) + sizeof(mutation_id) + sizeof(mutation_type_id) + sizeof(position) + sizeof(selection_coeff) + sizeof(dominance_coeff) + sizeof(subpop_index) + sizeof(origin_tick) + sizeof(fixation_tick) + sizeof(chromosome_index);
			
			if (has_nucleotides)
				record_size += sizeof(nucleotide);
			if (has_object_tags)
				record_size += sizeof(slim_usertag_t);
			
			if (p + record_size > buf_end)
				break;
			
			// If the first int32_t is not a mutation start tag, then we are done with this section
			memcpy(&substitution_start_tag, p, sizeof(substitution_start_tag));
			if (substitution_start_tag != (int32_t)0xFFFF0003)
				break;
			
			// Otherwise, we have a mutation record; read in the rest of it
			p += sizeof(substitution_start_tag);
			
			memcpy(&mutation_id, p, sizeof(mutation_id));
			p += sizeof(mutation_id);
			
			memcpy(&mutation_type_id, p, sizeof(mutation_type_id));
			p += sizeof(mutation_type_id);
			
			memcpy(&position, p, sizeof(position));
			p += sizeof(position);
			
			memcpy(&selection_coeff, p, sizeof(selection_coeff));
			p += sizeof(selection_coeff);
			
			memcpy(&dominance_coeff, p, sizeof(dominance_coeff));
			p += sizeof(dominance_coeff);
			
			memcpy(&subpop_index, p, sizeof(subpop_index));
			p += sizeof(subpop_index);
			
			memcpy(&origin_tick, p, sizeof(origin_tick));
			p += sizeof(origin_tick);
			
			memcpy(&fixation_tick, p, sizeof(fixation_tick));
			p += sizeof(fixation_tick);
			
			memcpy(&chromosome_index, p, sizeof(chromosome_index));
			(void)chromosome_index;
			p += sizeof(chromosome_index);
			
			if (has_nucleotides)
			{
				memcpy(&nucleotide, p, sizeof(nucleotide));
				p += sizeof(nucleotide);
			}
			
			// note the tag is read below, after the substitution is created
			
			// look up the mutation type from its index
			MutationType *mutation_type_ptr = MutationTypeWithID(mutation_type_id);
			
			if (!mutation_type_ptr) 
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " has not been defined for this species." << EidosTerminate();
			
			// BCH 7/2/2025: We no longer check the dominance coefficient against the mutation type, because it is allowed to differ
			
			// BCH 9/22/2021: Note that mutation_type_ptr->hemizygous_dominance_coeff_ is not saved, or checked here; too edge to be bothered...
			// FIXME MULTITRAIT: This will now change, since the hemizygous dominance coefficient is becoming a first-class citizen
			
			if ((nucleotide == -1) && mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " is nucleotide-based, but a nucleotide value for a mutation of this type was not supplied." << EidosTerminate();
			if ((nucleotide != -1) && !mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " is not nucleotide-based, but a nucleotide value for a mutation of this type was supplied." << EidosTerminate();
			
			// construct the new substitution
			Substitution *new_substitution = new Substitution(mutation_id, mutation_type_ptr, chromosome_index, position, selection_coeff, dominance_coeff, subpop_index, origin_tick, fixation_tick, nucleotide);
			
			// read its tag, if requested
			if (has_object_tags)
			{
				memcpy(&new_substitution->tag_value_, p, sizeof(slim_usertag_t));
				p += sizeof(slim_usertag_t);
			}
			
			// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
			// TREE SEQUENCE RECORDING
			// When doing tree recording, we additionally keep all fixed mutations (their ids) in a multimap indexed by their position
			// This allows us to find all the fixed mutations at a given position quickly and easily, for calculating derived states
			if (RecordingTreeSequence())
				population_.treeseq_substitutions_map_.emplace(new_substitution->position_, new_substitution);
			
			population_.substitutions_.emplace_back(new_substitution);
		}
		
		if (p + sizeof(section_end_tag) > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after mutations." << EidosTerminate();
		else
		{
			memcpy(&section_end_tag, p, sizeof(section_end_tag));
			p += sizeof(section_end_tag);
			
			if (section_end_tag != (int32_t)0xFFFF0000)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after mutations." << EidosTerminate();
		}
	}
	
	// It's a little unclear how we ought to clean up after ourselves, and this is a continuing source of bugs.  We could be loading
	// a new population in an early() event, in a late() event, or in between cycles in SLiMgui, e.g. in the Eidos console.
	// The safest avenue seems to be to just do all the bookkeeping we can think of: tally frequencies, calculate fitnesses, and
	// survey the population for SLiMgui.  This will lead to some of these actions being done at an unusual time in the cycle,
	// though, and will cause some things to be done unnecessarily (because they are not normally up-to-date at the current
	// cycle stage anyway) or done twice (which could be particularly problematic for mutationEffect() callbacks).  Nevertheless, this seems
	// like the best policy, at least until shown otherwise...  BCH 11 June 2016
	
	// BCH 5 April 2017: Well, it has been shown otherwise.  Now that interactions have been added, mutationEffect() callbacks often depend on
	// them, which means the interactions need to be evaluated, which means we can't evaluate fitness values yet; we need to give the
	// user's script a chance to evaluate the interactions.  This was always a problem, really; mutationEffect() callbacks might have needed
	// some external state to be set up that would be on the population state.  But now it is a glaring problem, and forces us to revise
	// our policy.  For backward compatibility, we will keep the old behavior if reading a file that is version 2 or earlier; a bit
	// weird, but probably nobody will ever even notice...
	
	// Re-tally mutation references so we have accurate frequency counts for our new mutations
	population_.UniqueMutationRuns();
	population_.InvalidateMutationReferencesCache();	// force a retally
	population_.TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ false);
	
	if (file_version <= 2)
	{
		// Now that we have the info on everybody, update fitnesses so that we're ready to run the next cycle
		// used to be generation + 1; removing that 18 Feb 2016 BCH
		
		nonneutral_change_counter_++;			// trigger unconditional nonneutral mutation caching inside UpdateFitness()
		last_nonneutral_regime_ = 3;			// this means "unpredictable callbacks", will trigger a recache next cycle
		
		for (auto muttype_iter : mutation_types_)
			(muttype_iter.second)->subject_to_mutationEffect_callback_ = true;	// we're not doing RecalculateFitness()'s work, so play it safe
		
		SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
		community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosMutationEffectCallback;	// used for both mutationEffect() and fitnessEffect() for simplicity
		community_.executing_species_ = this;
		
		// we need to recalculate phenotypes for traits that have a direct effect on fitness
		std::vector<slim_trait_index_t> p_direct_effect_trait_indices;
		const std::vector<Trait *> &traits = Traits();
		
		for (slim_trait_index_t trait_index = 0; trait_index < TraitCount(); ++trait_index)
			if (traits[trait_index]->HasDirectFitnessEffect())
				p_direct_effect_trait_indices.push_back(trait_index);
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> mutationEffect_callbacks = CallbackBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosMutationEffectCallback, -1, -1, subpop_id, -1, -1);
			std::vector<SLiMEidosBlock*> fitnessEffect_callbacks = CallbackBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosFitnessEffectCallback, -1, -1, subpop_id, -1, -1);
			
			subpop->UpdateFitness(mutationEffect_callbacks, fitnessEffect_callbacks, p_direct_effect_trait_indices, /* p_force_trait_recalculation */ true);
		}
		
		community_.executing_block_type_ = old_executing_block_type;
		community_.executing_species_ = nullptr;
		
#ifdef SLIMGUI
		// Let SLiMgui survey the population for mean fitness and such, if it is our target
		population_.SurveyPopulation();
#endif
	}
	
	return file_tick;
}
#else
// the static analyzer has a lot of trouble understanding this method
slim_tick_t Species::_InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	return 0;
}
#endif

void Species::DeleteAllMutationRuns(void)
{
	// This traverses the free and in-use MutationRun pools and frees them all
	// Note that the allocation pools themselves, and the MutationRunContexts, remain intact
	for (Chromosome *chromosome : chromosomes_)
	{
		for (int threadnum = 0; threadnum < chromosome->ChromosomeMutationRunContextCount(); ++threadnum)
		{
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(threadnum);
			MutationRun::DeleteMutationRunContextContents(mutrun_context);
		}
	}
}

Subpopulation *Species::SubpopulationWithName(const std::string &p_subpop_name) {
	for (auto subpop_iter : population_.subpops_)
	{
		Subpopulation *subpop = subpop_iter.second;
		if (subpop->name_ == p_subpop_name)
			return subpop;
	}
	return nullptr;
}


//
// Running cycles
//
#pragma mark -
#pragma mark Running cycles
#pragma mark -

std::vector<SLiMEidosBlock*> Species::CallbackBlocksMatching(slim_tick_t p_tick, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id, slim_trait_index_t p_trait_index, int64_t p_chromosome_id)
{
	// Callbacks are species-specific; this method calls up to the community, which manages script blocks,
	// but does a species-specific search.
	return community_.ScriptBlocksMatching(p_tick, p_event_type, p_mutation_type_id, p_interaction_type_id, p_subpopulation_id, p_trait_index, p_chromosome_id, this);
}

void Species::RunInitializeCallbacks(void)
{
	// zero out the initialization check counts
	// FIXME: doing this here is error-prone; the species object should zero-initialize all of this stuff instead!
	num_species_inits_ = 0;
	num_slimoptions_inits_ = 0;
	num_mutation_type_inits_ = 0;
	num_ge_type_inits_ = 0;
	num_sex_inits_ = 0;
	num_treeseq_inits_ = 0;
	num_trait_inits_ = 0;
	num_chromosome_inits_ = 0;
	
	num_mutrate_inits_ = 0;
	num_recrate_inits_ = 0;
	num_genomic_element_inits_ = 0;
	num_gene_conv_inits_ = 0;
	num_ancseq_inits_ = 0;
	num_hotmap_inits_ = 0;
	
	has_implicit_trait_ = false;
	has_implicit_chromosome_ = false;
	
	// execute initialize() callbacks, which should always have a tick of 0 set
	std::vector<SLiMEidosBlock*> init_blocks = CallbackBlocksMatching(0, SLiMEidosBlockType::SLiMEidosInitializeCallback, -1, -1, -1, -1, -1);
	
	for (auto script_block : init_blocks)
		community_.ExecuteEidosEvent(script_block);
	
	//
	// check for complete initialization
	//
	
	if ((num_mutrate_inits_ == 0) && (num_mutation_type_inits_ == 0) && (num_ge_type_inits_ == 0) &&
		(num_genomic_element_inits_ == 0) && (num_recrate_inits_ == 0) && (num_gene_conv_inits_ == 0) &&
		(num_chromosome_inits_ == 0) && (!has_implicit_chromosome_))
	{
		// BCH 26 April 2022: In SLiM 4, as a special case, we allow *all* of the genetic structure boilerplate to be omitted.
		// This gives a species with no genetics, no mutations, no recombination, no declared chromosomes, and so forth.
		// In that case, here we set up the default empty genetic structure and pretend to have been initialized, so we have
		// little bits of several initialization functions excerpted here.  Note that the state achieved by this code path
		// cannot be achieved any other way; in particular, we have no genomic element types, no mutation types, and no
		// genomic elements; normally that is illegal, but we deliberately carve out this special case.
		// BCH 22 May 2022: No-genetics species cannot use tree-sequence recording or be nucleotide-based, for simplicity.
		// They always use null haplosomes, so any attempt to access their genetics is illegal.  They have no mutruns.
		// BCH 18 September 2024: They also cannot have any declared chromosomes, or do anything that would cause an
		// implicit chromosome to be defined.
		// BCH 10 October 2024: No-genetics models now have no Chromosome object at all
		if (recording_tree_)
			EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): no-genetics species cannot use tree-sequence recording; either add genetic initialization calls, or disable tree-sequence recording." << EidosTerminate();
		if (nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): no-genetics species cannot be nucleotide-based; either add genetic initialization calls, or turn off nucleotides." << EidosTerminate();
		
		has_genetics_ = false;
	}

	if (has_genetics_ && (!has_implicit_chromosome_) && (num_chromosome_inits_ == 0))
		EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): (internal error) a chromosome has not been set up properly." << EidosTerminate();
	if (!has_genetics_ && (has_implicit_chromosome_ || (num_chromosome_inits_ > 0)))
		EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): (internal error) a chromosome was set up in a no-genetics model." << EidosTerminate();
	
	// From the initialization that has occurred, there should now be a currently initializing chromosome,
	// whether implicitly or explicitly defined.  We now close out its definition and check it for
	// correctness.  If this is a multichromosome model, this has already been done for the previous ones.
	if (has_genetics_)
		EndCurrentChromosome(/* starting_new_chromosome */ false);
	
	// set a default avatar string if one was not provided; these will be A, B, etc.
	if (avatar_.length() == 0)
		avatar_ = std::string(1, (char)('A' + species_id_));
	
	community_.scripts_changed_ = true;		// avatars changed, either here or with initializeSpecies(), so redisplay the script block table
	
	// In single-species models, we are responsible for finalizing the model type decision at the end of our initialization
	// In multispecies models, the Community will have already made this decision and propagated it down to us.
	if (!community_.is_explicit_species_)
	{
		// We default to WF, but here we explicitly declare our model type so everybody knows the default was not changed
		// This cements the choice of WF if the first species initialized does not declare a model type explicitly
		if (!community_.model_type_set_)
			community_.SetModelType(SLiMModelType::kModelTypeWF);
	}
	
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
	{
		std::vector<SLiMEidosBlock*> script_blocks = community_.AllScriptBlocksForSpecies(this);
		
		for (auto script_block : script_blocks)
			if (script_block->type_ == SLiMEidosBlockType::SLiMEidosMateChoiceCallback)
				EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): mateChoice() callbacks may not be defined in nonWF models." << EidosTerminate(script_block->identifier_token_);
	}
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		std::vector<SLiMEidosBlock*> script_blocks = community_.AllScriptBlocksForSpecies(this);
		
		for (auto script_block : script_blocks)
		{
			if (script_block->type_ == SLiMEidosBlockType::SLiMEidosReproductionCallback)
				EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): reproduction() callbacks may not be defined in WF models." << EidosTerminate(script_block->identifier_token_);
			if (script_block->type_ == SLiMEidosBlockType::SLiMEidosSurvivalCallback)
				EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): survival() callbacks may not be defined in WF models." << EidosTerminate(script_block->identifier_token_);
		}
	}
	if (!sex_enabled_)
	{
		std::vector<SLiMEidosBlock*> script_blocks = community_.AllScriptBlocksForSpecies(this);
		
		for (auto script_block : script_blocks)
			if ((script_block->type_ == SLiMEidosBlockType::SLiMEidosReproductionCallback) && (script_block->sex_specificity_ != IndividualSex::kUnspecified))
				EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): reproduction() callbacks may not be limited by sex in non-sexual models." << EidosTerminate(script_block->identifier_token_);
	}
	{
		std::vector<SLiMEidosBlock*> script_blocks = community_.AllScriptBlocksForSpecies(this);
		
		for (auto script_block : script_blocks)
		{
			if (script_block->type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback)
			{
				if (has_implicit_chromosome_ && ((script_block->chromosome_id_ != -1) || (script_block->chromosome_symbol_.length() > 0)))
					EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): recombination() callbacks may only use a chromosome specifier in models with explicitly declared chromosomes." << EidosTerminate(script_block->identifier_token_);
				
				if (script_block->chromosome_symbol_.length() > 0)
				{
					Chromosome *chrom = ChromosomeFromSymbol(script_block->chromosome_symbol_);
					
					// In general we allow callbacks to reference subpops, mutation types, etc. that do not exist,
					// giving the user broad latitude, but with string chromosome symbols a typo seems likely
					if (!chrom)
						EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): recombination() callback declaration references a chromosome with symbol '" << script_block->chromosome_symbol_ << "' that has not been declared." << EidosTerminate(script_block->identifier_token_);
					
					// translate the symbol into an id, which is what ApplyRecombinationCallbacks() checks
					script_block->chromosome_id_ = chrom->ID();
				}
			}
		}
	}
	
	if (nucleotide_based_)
	{
		if (num_ancseq_inits_ == 0)
			EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): Nucleotide-based models must provide an ancestral nucleotide sequence with initializeAncestralNucleotides()." << EidosTerminate();
		
		for (Chromosome *chromosome : chromosomes_)
			if (!chromosome->ancestral_seq_buffer_)
				EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): (internal error) No ancestral sequence!" << EidosTerminate();
	}
	
	CheckMutationStackPolicy();
	
	// Except in no-genetics species, make a MutationBlock object to keep our mutations in
	if (has_genetics_)
		CreateAndPromulgateMutationBlock();
	
	// In nucleotide-based models, process the mutationMatrix parameters for genomic element types to calculate the maximum mutation rate
	if (nucleotide_based_)
		CacheNucleotideMatrices();
	
	// initialize pre-allocated default Haplosome metadata records (HaplosomeMetadataRec) based on the chromosome configuration
	_MakeHaplosomeMetadataRecords();
	
	// initialize chromosomes
	for (Chromosome *chromosome : chromosomes_)
	{
		// In nucleotide-based models, construct a mutation rate map
		if (nucleotide_based_)
			chromosome->CreateNucleotideMutationRateMap();
		
		chromosome->InitializeDraws();
	}
	
	// set up mutation runs for all chromosomes
	for (Chromosome *chromosome : chromosomes_)
	{
		chromosome->ChooseMutationRunLayout();
		chromosome->SetUpMutationRunContexts();
	}
	
	// Defining a neutral mutation type when tree-recording is on (with mutation recording) and the mutation rate is non-zero is legal, but causes a warning
	// I'm not sure this is a good idea, but maybe it will help people avoid doing dumb things; added at the suggestion of Peter Ralph...
	// BCH 26 Jan. 2020; refining the test here so it only logs if the neutral mutation type is used by a genomic element type
	if (recording_tree_ && recording_mutations_)
	{
		bool mut_rate_zero = true;
		
		for (Chromosome *chromosome : chromosomes_)
		{
			for (double rate : chromosome->mutation_rates_H_)
				if (rate != 0.0) { mut_rate_zero = false; break; }
			if (mut_rate_zero)
				for (double rate : chromosome->mutation_rates_M_)
					if (rate != 0.0) { mut_rate_zero = false; break; }
			if (mut_rate_zero)
				for (double rate : chromosome->mutation_rates_F_)
					if (rate != 0.0) { mut_rate_zero = false; break; }
		}
		
		if (!mut_rate_zero)
		{
			bool using_neutral_muttype = false;
			
			for (auto getype_iter : genomic_element_types_)
			{
				GenomicElementType *getype = getype_iter.second;
				
				for (auto muttype : getype->mutation_type_ptrs_)
				{
					if (muttype->all_neutral_DES_)
						using_neutral_muttype = true;
				}
			}
			
			if (using_neutral_muttype && !gEidosSuppressWarnings)
				SLIM_ERRSTREAM << "#WARNING (Species::RunInitializeCallbacks): with tree-sequence recording enabled and a non-zero mutation rate, a neutral mutation type was defined and used; this is legal, but usually undesirable, since neutral mutations can be overlaid later using the tree-sequence information." << std::endl;
		}
	}
	
	// Ancestral sequence check; this has to wait until after the chromosome has been initialized
	if (nucleotide_based_)
	{
		for (Chromosome *chromosome : chromosomes_)
			if (chromosome->ancestral_seq_buffer_->size() != (std::size_t)(chromosome->last_position_ + 1))
			EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): The chromosome length (" << chromosome->last_position_ + 1 << " base" << (chromosome->last_position_ + 1 != 1 ? "s" : "") << ") does not match the ancestral sequence length (" << chromosome->ancestral_seq_buffer_->size() << " base" << (chromosome->ancestral_seq_buffer_->size() != 1 ? "s" : "") << ")." << EidosTerminate();
	}
	
	// always start at cycle 1, regardless of what the starting tick value might be
	SetCycle(1);
	
	// kick off mutation run experiments, if needed
	for (Chromosome *chromosome : chromosomes_)
		chromosome->InitiateMutationRunExperiments();
	
	// TREE SEQUENCE RECORDING
	if (RecordingTreeSequence())
		AllocateTreeSequenceTables();
}

void Species::CreateAndPromulgateMutationBlock(void)
{
	// This creates a new MutationBlock and gives pointers to it to various sub-components of the species.  This
	// is called toward the end of initialize() callbacks; note that pointers will be nullptr until then.  That
	// is because we can't allocate the MutationBlock until we know how many traits there are.
	if (mutation_block_)
		EIDOS_TERMINATION << "ERROR (Species::CreateAndPromulgateMutationBlock): (internal error) a mutation block has already been allocated." << EidosTerminate();
	
	// first we make a new MutationBlock object for ourselves
	mutation_block_ = new MutationBlock(*this, TraitCount());
	
	// then we promulgate it to the masses, so that they have it on hand (avoiding the non-local memory access
	// of getting it from us), since it is referred to very actively in many places
	
	// give it to all MutationType objects in this species
	for (auto muttype_iter : mutation_types_)
		muttype_iter.second->mutation_block_ = mutation_block_;
	
	// give it to all Chromosome objects in this species
	for (Chromosome *chromosome : chromosomes_)
		chromosome->mutation_block_ = mutation_block_;
	
	// give it to the Population object in this species
	population_.mutation_block_ = mutation_block_;
}

void Species::EndCurrentChromosome(bool starting_new_chromosome)
{
	// Check for complete/correct initialization of the currently initializing chromosome.  The error messages emitted are tailored
	// to whether the user has an implicitly defined chromosome, or is explicitly defining chromosomes with initializeChromosome()
	// calls; we want to avoid confusion over the new vs. old paradigm of defining the chromosome.
	Chromosome *chromosome = CurrentlyInitializingChromosome();
	bool explicit_chromosome = (num_chromosome_inits_ > 0);
	std::string chromosomeStr = (explicit_chromosome ? "current chromosome" : "chromosome");
	std::string contextStr = (explicit_chromosome ? "for the current chromosome" : "in an initialize() callback");
	std::string finalStr = ((explicit_chromosome && starting_new_chromosome) ? "  The current chromosome's initialization must be completed before initialization of the next chromosome, with a new call to initializeChromosome(), can begin." : "");
	
	if (!nucleotide_based_ && (num_mutrate_inits_ == 0))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): The initialization of the " << chromosomeStr << " is incomplete.  At least one mutation rate interval must be defined " << contextStr << " with initializeMutationRate(), although the rate given can be zero." << finalStr << EidosTerminate();
	if (nucleotide_based_ && (num_mutrate_inits_ > 0))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): initializeMutationRate() may not be called in nucleotide-based models (use initializeHotspotMap() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	if ((num_mutation_type_inits_ == 0) && has_genetics_)
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): At least one mutation type must be defined in an initialize() callback with initializeMutationType() (or initializeMutationTypeNuc(), in nucleotide-based models)." << EidosTerminate();
	
	if ((num_ge_type_inits_ == 0) && has_genetics_)
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): At least one genomic element type must be defined in an initialize() callback with initializeGenomicElementType()." << EidosTerminate();
	
	if ((num_genomic_element_inits_ == 0) && has_genetics_)
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): The initialization of the " << chromosomeStr << " is incomplete.  At least one genomic element must be defined " << contextStr << " with initializeGenomicElement()." << finalStr << EidosTerminate();
	
	if (num_recrate_inits_ == 0)
	{
		if (chromosome->DefaultsToZeroRecombination())
		{
			// For chromosomes that require zero recombination, we allow the
			// initializeRecombinationRate() call to be omitted for brevity.
			// Derived from ExecuteContextFunction_initializeRecombinationRate().
			std::vector<slim_position_t> &positions = chromosome->recombination_end_positions_H_;
			std::vector<double> &rates = chromosome->recombination_rates_H_;
			rates.clear();
			positions.clear();
			
			rates.emplace_back(0.0);
			//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
			
			num_recrate_inits_++;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): The initialization of the " << chromosomeStr << " is incomplete.  At least one recombination rate interval must be defined " << contextStr << " with initializeRecombinationRate(), although the rate given can be zero." << finalStr << EidosTerminate();
		}
	}
	
	if ((chromosome->recombination_rates_H_.size() != 0) && ((chromosome->recombination_rates_M_.size() != 0) || (chromosome->recombination_rates_F_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): Cannot define both sex-specific and sex-nonspecific recombination rates." << EidosTerminate();
	
	if (((chromosome->recombination_rates_M_.size() == 0) && (chromosome->recombination_rates_F_.size() != 0)) ||
		((chromosome->recombination_rates_M_.size() != 0) && (chromosome->recombination_rates_F_.size() == 0)))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): The initialization of the " << chromosomeStr << " is incomplete.  Both sex-specific recombination rates must be defined, not just one (but one may be defined as zero)." << finalStr << EidosTerminate();
	
	
	if ((chromosome->mutation_rates_H_.size() != 0) && ((chromosome->mutation_rates_M_.size() != 0) || (chromosome->mutation_rates_F_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): Cannot define both sex-specific and sex-nonspecific mutation rates." << EidosTerminate();
	
	if (((chromosome->mutation_rates_M_.size() == 0) && (chromosome->mutation_rates_F_.size() != 0)) ||
		((chromosome->mutation_rates_M_.size() != 0) && (chromosome->mutation_rates_F_.size() == 0)))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): The initialization of the " << chromosomeStr << " is incomplete.  Both sex-specific mutation rates must be defined, not just one (but one may be defined as zero)." << finalStr << EidosTerminate();
	
	
	if ((chromosome->hotspot_multipliers_H_.size() != 0) && ((chromosome->hotspot_multipliers_M_.size() != 0) || (chromosome->hotspot_multipliers_F_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): Cannot define both sex-specific and sex-nonspecific hotspot maps." << EidosTerminate();
	
	if (((chromosome->hotspot_multipliers_M_.size() == 0) && (chromosome->hotspot_multipliers_F_.size() != 0)) ||
		((chromosome->hotspot_multipliers_M_.size() != 0) && (chromosome->hotspot_multipliers_F_.size() == 0)))
		EIDOS_TERMINATION << "ERROR (Species::EndCurrentChromosome): The initialization of the " << chromosomeStr << " is incomplete.  Both sex-specific hotspot maps must be defined, not just one (but one may be defined as 1.0)." << finalStr << EidosTerminate();
	
	has_currently_initializing_chromosome_ = false;
}

bool Species::HasDoneAnyInitialization(void)
{
	// This is used by Community to make sure that initializeModelType() executes before any other init
	return ((num_mutation_type_inits_ > 0) || (num_mutrate_inits_ > 0) || (num_ge_type_inits_ > 0) || (num_genomic_element_inits_ > 0) || (num_recrate_inits_ > 0) || (num_gene_conv_inits_ > 0) || (num_sex_inits_ > 0) || (num_slimoptions_inits_ > 0) || (num_treeseq_inits_ > 0) || (num_ancseq_inits_ > 0) || (num_hotmap_inits_ > 0) || (num_species_inits_ > 0) || (num_chromosome_inits_ > 0) || has_implicit_chromosome_);
}

void Species::PrepareForCycle(void)
{
	// Called by Community at the very start of each cycle, whether WF or nonWF (but not before initialize() callbacks)
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		// Optimization; see mutation_type.h for an explanation of what this counter is used for
		if (population_.any_muttype_call_count_used_)
		{
			for (auto muttype_iter : mutation_types_)
				(muttype_iter.second)->muttype_registry_call_count_ = 0;
			
			population_.any_muttype_call_count_used_ = false;
		}
#endif
	
	// zero out clock accumulators for mutation run experiments; we will add to these as we do work later
	for (Chromosome *chromosome : chromosomes_)
		chromosome->ZeroMutationRunExperimentClock();
}

void Species::MaintainMutationRegistry(void)
{
	if (has_genetics_)
	{
		population_.MaintainMutationRegistry();
		
		// Every hundredth cycle we unique mutation runs to optimize memory usage and efficiency.  The number 100 was
		// picked out of a hat – often enough to perhaps be useful in keeping SLiM slim, but infrequent enough that if it
		// is a time sink it won't impact the simulation too much.  This call is really quite fast, though – on the order
		// of 0.015 seconds for a pop of 10000 with a 1e5 chromosome and lots of mutations.  So although doing this every
		// cycle would seem like overkill – very few duplicates would be found per call – every 100 should be fine.
		// Anyway, if we start seeing this call in performance analysis, we should probably revisit this; the benefit is
		// likely to be pretty small for most simulations, so if the cost is significant then it may be a lose.
		if (cycle_ % 100 == 0)
			population_.UniqueMutationRuns();
	}
}

void Species::RecalculateFitness(bool p_force_trait_recalculation)
{
	population_.RecalculateFitness(cycle_, p_force_trait_recalculation);	// used to be cycle_ + 1 in the WF cycle; removing that 18 Feb 2016 BCH
}

void Species::MaintainTreeSequence(void)
{
	// TREE SEQUENCE RECORDING
	if (recording_tree_)
	{
#if DEBUG
		// check the integrity of the tree sequence in every cycle in Debug mode only
		CheckTreeSeqIntegrity();
#endif
					
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		CheckAutoSimplification();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(community_.profile_stage_totals_[8]);
#endif
		
		// note that this causes simplification, so it will confuse the auto-simplification code
		if (running_treeseq_crosschecks_ && (cycle_ % treeseq_crosschecks_interval_ == 0))
			CrosscheckTreeSeqIntegrity();
	}
}

void Species::EmptyGraveyard(void)
{
	// Individuals end up in graveyard_ due to killIndividuals(); they get disposed of here.
	// It's not necessary that FreeSubpopIndividual() be called on the correct subpopulation, really,
	// but that API is at the Subpopulation level instead of in Species for efficiency, so...
	for (Individual *individual : graveyard_)
		individual->subpopulation_->FreeSubpopIndividual(individual);
	
	graveyard_.resize(0);
}

void Species::WF_GenerateOffspring(void)
{
	slim_tick_t tick = community_.Tick();
	std::vector<SLiMEidosBlock*> mate_choice_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosMateChoiceCallback, -1, -1, -1, -1, -1);
	std::vector<SLiMEidosBlock*> modify_child_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosModifyChildCallback, -1, -1, -1, -1, -1);
	std::vector<SLiMEidosBlock*> recombination_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, -1, -1, -1);
	std::vector<SLiMEidosBlock*> mutation_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosMutationCallback, -1, -1, -1, -1, -1);
	bool mate_choice_callbacks_present = mate_choice_callbacks.size();
	bool modify_child_callbacks_present = modify_child_callbacks.size();
	bool recombination_callbacks_present = recombination_callbacks.size();
	bool mutation_callbacks_present = mutation_callbacks.size();
	bool no_active_callbacks = true;
	
	// a type 's' DES needs to count as an active callback; it could activate other callbacks,
	// and in any case we need EvolveSubpopulation() to take the non-parallel code path
	if (type_s_DESs_present_)
		no_active_callbacks = false;
	
	// if there are no active callbacks of any type, we can pretend there are no callbacks at all
	// if there is a callback of any type, however, then inactive callbacks could become active
	if (mate_choice_callbacks_present || modify_child_callbacks_present || recombination_callbacks_present || mutation_callbacks_present)
	{
		if (no_active_callbacks)
			for (SLiMEidosBlock *callback : mate_choice_callbacks)
				if (callback->block_active_)
				{
					no_active_callbacks = false;
					break;
				}
		
		if (no_active_callbacks)
			for (SLiMEidosBlock *callback : modify_child_callbacks)
				if (callback->block_active_)
				{
					no_active_callbacks = false;
					break;
				}
		
		if (no_active_callbacks)
			for (SLiMEidosBlock *callback : recombination_callbacks)
				if (callback->block_active_)
				{
					no_active_callbacks = false;
					break;
				}
		
		if (no_active_callbacks)
			for (SLiMEidosBlock *callback : mutation_callbacks)
				if (callback->block_active_)
				{
					no_active_callbacks = false;
					break;
				}
	}
	
	if (no_active_callbacks)
	{
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			population_.EvolveSubpopulation(*subpop_pair.second, false, false, false, false, false);
	}
	else
	{
		// cache a list of callbacks registered for each subpop
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			
			// Get mateChoice() callbacks that apply to this subpopulation
			subpop->registered_mate_choice_callbacks_.resize(0);
			
			for (SLiMEidosBlock *callback : mate_choice_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_mate_choice_callbacks_.emplace_back(callback);
			}
			
			// Get modifyChild() callbacks that apply to this subpopulation
			subpop->registered_modify_child_callbacks_.resize(0);
			
			for (SLiMEidosBlock *callback : modify_child_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_modify_child_callbacks_.emplace_back(callback);
			}
			
			// Get recombination() callbacks that apply to this subpopulation
			subpop->registered_recombination_callbacks_.resize(0);
			
			for (SLiMEidosBlock *callback : recombination_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_recombination_callbacks_.emplace_back(callback);
			}
			
			// Get mutation() callbacks that apply to this subpopulation
			subpop->registered_mutation_callbacks_.resize(0);
			
			for (SLiMEidosBlock *callback : mutation_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_mutation_callbacks_.emplace_back(callback);
			}
		}
		
		// then evolve each subpop
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			population_.EvolveSubpopulation(*subpop_pair.second, mate_choice_callbacks_present, modify_child_callbacks_present, recombination_callbacks_present, mutation_callbacks_present, type_s_DESs_present_);
	}
}

void Species::WF_SwitchToChildGeneration(void)
{
	// switch to the child generation; we don't want to do this until all callbacks have executed for all subpops
	population_.child_generation_valid_ = true;
	
	// added 30 November 2016 so MutationRun refcounts reflect their usage count in the simulation
	// moved up to SLiMCycleStage::kWFStage2GenerateOffspring, 9 January 2018, so that the
	// population is in a standard state for CheckIndividualIntegrity() at the end of this stage
	// BCH 4/22/2023: this is no longer relevant in terms of accurate MutationRun refcounts, since
	// we no longer refcount those, but they still need to be zeroed out so they're ready for reuse
	// BCH 10/15/2024: I realized that clearing the haplosomes is no longer needed at all; we can
	// just remove our requirement that the haplosomes be cleared, and overwrite the stale pointers
	// when we reuse a haplosome.  I am relegating haplosome clearing to a debugging flag.
#if SLIM_CLEAR_HAPLOSOMES
	population_.ClearParentalHaplosomes();
#endif
}

void Species::WF_SwapGenerations(void)
{
	population_.SwapGenerations();
}

void Species::nonWF_GenerateOffspring(void)
{
	slim_tick_t tick = community_.Tick();
	std::vector<SLiMEidosBlock*> reproduction_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosReproductionCallback, -1, -1, -1, -1, -1);
	std::vector<SLiMEidosBlock*> modify_child_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosModifyChildCallback, -1, -1, -1, -1, -1);
	std::vector<SLiMEidosBlock*> recombination_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, -1, -1, -1);
	std::vector<SLiMEidosBlock*> mutation_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosMutationCallback, -1, -1, -1, -1, -1);
	
	// choose templated variants for GenerateIndividualsX() methods of Subpopulation, called during reproduction() callbacks
	// this is an optimization technique that lets us optimize away unused cruft at compile time
	// some relevant posts that were helpful in figuring out the correct syntax:
	// 	http://goodliffe.blogspot.com/2011/07/c-declaring-pointer-to-template-method.html
	// 	https://stackoverflow.com/questions/115703/storing-c-template-function-definitions-in-a-cpp-file
	// 	https://stackoverflow.com/questions/22275786/change-boolean-flags-into-template-arguments
	// and a Godbolt experiment I did to confirm that this really works: https://godbolt.org/z/Mva4Kbhrd
	//
	// callbacks are "on" if they exist for any subpopulation, since nonWF allows parents to belong to any subpop
	// note this optimization depends upon the fact that none of these flags can change during one reproduction() stage!
	bool pedigrees_enabled = PedigreesEnabled();
	bool recording_tree_sequence = RecordingTreeSequence();
	bool has_reproduction_callbacks = ((reproduction_callbacks.size() > 0) || (modify_child_callbacks.size() > 0) || (recombination_callbacks.size() > 0) || (mutation_callbacks.size() > 0));
	bool is_spatial = (SpatialDimensionality() >= 1);
	
	if (DoingAnyMutationRunExperiments())
	{
		if (pedigrees_enabled)
		{
			if (recording_tree_sequence)
			{
				if (has_reproduction_callbacks)	// has any of the callbacks that the GenerateIndividuals...() methods care about; this can be refined later
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, true, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, true, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, true, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, true, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, true, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, true, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, true, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, true, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, true, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, true, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, true, false, false>;
					}
				}
			}
			else
			{
				if (has_reproduction_callbacks)
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, false, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, false, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, false, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, false, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, false, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, false, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, false, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, false, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, true, false, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, true, false, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, true, false, false, false>;
					}
				}
			}
		}
		else
		{
			if (recording_tree_sequence)
			{
				if (has_reproduction_callbacks)
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, true, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, true, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, true, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, true, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, true, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, true, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, true, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, true, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, true, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, true, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, true, false, false>;
					}
				}
			}
			else
			{
				if (has_reproduction_callbacks)
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, false, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, false, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, false, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, false, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, false, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, false, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, false, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, false, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<true, false, false, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<true, false, false, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<true, false, false, false, false>;
					}
				}
			}
		}
	}
	else
	{
		if (pedigrees_enabled)
		{
			if (recording_tree_sequence)
			{
				if (has_reproduction_callbacks)
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, true, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, true, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, true, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, true, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, true, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, true, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, true, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, true, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, true, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, true, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, true, false, false>;
					}
				}
			}
			else
			{
				if (has_reproduction_callbacks)
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, false, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, false, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, false, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, false, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, false, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, false, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, false, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, false, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, true, false, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, true, false, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, true, false, false, false>;
					}
				}
			}
		}
		else
		{
			if (recording_tree_sequence)
			{
				if (has_reproduction_callbacks)
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, true, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, true, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, true, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, true, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, true, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, true, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, true, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, true, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, true, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, true, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, true, false, false>;
					}
				}
			}
			else
			{
				if (has_reproduction_callbacks)
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, false, true, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, false, true, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, false, true, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, false, true, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, false, true, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, false, false, true>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, false, false, true>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, false, false, true>;
					}
					else
					{
						population_.GenerateIndividualCrossed_TEMPLATED = &Subpopulation::GenerateIndividualCrossed<false, false, false, false, false>;
						population_.GenerateIndividualSelfed_TEMPLATED = &Subpopulation::GenerateIndividualSelfed<false, false, false, false, false>;
						population_.GenerateIndividualCloned_TEMPLATED = &Subpopulation::GenerateIndividualCloned<false, false, false, false, false>;
					}
				}
			}
		}
	}
	
	// similarly, choose templated variants for the HaplosomeCrossed()/HaplosomeCloned()/HaplosomeRecombined() methods of Population
	if (recording_tree_sequence)
	{
		if (has_reproduction_callbacks)	// has any of the callbacks that the GenerateIndividuals...() methods care about; this can be refined later
		{
			population_.HaplosomeCrossed_TEMPLATED = &Population::HaplosomeCrossed<true, true>;
			population_.HaplosomeCloned_TEMPLATED = &Population::HaplosomeCloned<true, true>;
			population_.HaplosomeRecombined_TEMPLATED = &Population::HaplosomeRecombined<true, true>;
		}
		else
		{
			population_.HaplosomeCrossed_TEMPLATED = &Population::HaplosomeCrossed<true, false>;
			population_.HaplosomeCloned_TEMPLATED = &Population::HaplosomeCloned<true, false>;
			population_.HaplosomeRecombined_TEMPLATED = &Population::HaplosomeRecombined<true, false>;
		}
	}
	else
	{
		if (has_reproduction_callbacks)
		{
			population_.HaplosomeCrossed_TEMPLATED = &Population::HaplosomeCrossed<false, true>;
			population_.HaplosomeCloned_TEMPLATED = &Population::HaplosomeCloned<false, true>;
			population_.HaplosomeRecombined_TEMPLATED = &Population::HaplosomeRecombined<false, true>;
		}
		else
		{
			population_.HaplosomeCrossed_TEMPLATED = &Population::HaplosomeCrossed<false, false>;
			population_.HaplosomeCloned_TEMPLATED = &Population::HaplosomeCloned<false, false>;
			population_.HaplosomeRecombined_TEMPLATED = &Population::HaplosomeRecombined<false, false>;
		}
	}
	
	// cache a list of callbacks registered for each subpop
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
	{
		slim_objectid_t subpop_id = subpop_pair.first;
		Subpopulation *subpop = subpop_pair.second;
		
		// Get reproduction() callbacks that apply to this subpopulation
		subpop->registered_reproduction_callbacks_.resize(0);
		
		for (SLiMEidosBlock *callback : reproduction_callbacks)
		{
			slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
			
			if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
				subpop->registered_reproduction_callbacks_.emplace_back(callback);
		}
		
		// Get modifyChild() callbacks that apply to this subpopulation
		subpop->registered_modify_child_callbacks_.resize(0);
		
		for (SLiMEidosBlock *callback : modify_child_callbacks)
		{
			slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
			
			if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
				subpop->registered_modify_child_callbacks_.emplace_back(callback);
		}
		
		// Get recombination() callbacks that apply to this subpopulation
		subpop->registered_recombination_callbacks_.resize(0);
		
		for (SLiMEidosBlock *callback : recombination_callbacks)
		{
			slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
			
			if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
				subpop->registered_recombination_callbacks_.emplace_back(callback);
		}
		
		// Get mutation() callbacks that apply to this subpopulation
		subpop->registered_mutation_callbacks_.resize(0);
		
		for (SLiMEidosBlock *callback : mutation_callbacks)
		{
			slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
			
			if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
				subpop->registered_mutation_callbacks_.emplace_back(callback);
		}
	}
	
	// then evolve each subpop
	SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
	community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosReproductionCallback;
	
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->ReproduceSubpopulation();
	
	community_.executing_block_type_ = old_executing_block_type;
	
	// This completes the first half of the reproduction process; see Species::nonWF_MergeOffspring() for the second half
}

void Species::nonWF_MergeOffspring(void)
{
	// Species::nonWF_GenerateOffspring() completed the first half of the reproduction process; this does the second half
	// This defers the merging of offspring until all species have reproduced, allowing multispecies interactions to remain valid
	
	// Invalidate interactions, now that the generation they were valid for is disappearing
	community_.InvalidateInteractionsForSpecies(this);
	
	// then merge in the generated offspring; we don't want to do this until all callbacks have executed for all subpops
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->MergeReproductionOffspring();
	
	// then generate any deferred haplosomes; note that the deferred offspring got merged in above already
#if DEFER_BROKEN
	// The "defer" flag is simply disregarded at the moment; its design has rotted away,
	// and needs to be remade anew once things have settled down.
	population_.DoDeferredReproduction();
#endif
	
	// clear the "migrant" property on all individuals
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		std::vector<Individual *> &parents = subpop->parent_individuals_;
		size_t parent_count = parents.size();
		
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_MIGRANT_CLEAR);
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_MIGRANT_CLEAR);
#pragma omp parallel for schedule(static) default(none) shared(parent_count) firstprivate(parents)  if(parent_count >= EIDOS_OMPMIN_MIGRANT_CLEAR) num_threads(thread_count)
		for (size_t parent_index = 0; parent_index < parent_count; ++parent_index)
		{
			parents[parent_index]->migrant_ = false;
		}
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_MIGRANT_CLEAR);
	}
	
	// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
	population_.InvalidateMutationReferencesCache();
}

void Species::nonWF_ViabilitySurvival(void)
{
	slim_tick_t tick = community_.Tick();
	std::vector<SLiMEidosBlock*> survival_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosSurvivalCallback, -1, -1, -1, -1, -1);
	bool survival_callbacks_present = survival_callbacks.size();
	bool no_active_callbacks = true;
	
	// if there are no active callbacks, we can pretend there are no callbacks at all
	if (survival_callbacks_present)
	{
		for (SLiMEidosBlock *callback : survival_callbacks)
			if (callback->block_active_)
			{
				no_active_callbacks = false;
				break;
			}
	}
	
	if (no_active_callbacks)
	{
		// Survival is simple viability selection without callbacks
		std::vector<SLiMEidosBlock*> no_survival_callbacks;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			subpop_pair.second->ViabilitySurvival(no_survival_callbacks);
	}
	else
	{
		// Survival is governed by callbacks, per subpopulation
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> subpop_survival_callbacks;
			
			// Get survival callbacks that apply to this subpopulation
			for (SLiMEidosBlock *callback : survival_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop_survival_callbacks.emplace_back(callback);
			}
			
			// Handle survival, using the callbacks
			subpop->ViabilitySurvival(subpop_survival_callbacks);
		}
		
		// Callbacks could have requested that individuals move rather than dying; check for that
		bool any_moved = false;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			if ((subpop_pair.second)->nonWF_survival_moved_individuals_.size())
			{
				any_moved = true;
				break;
			}
		}
		
		if (any_moved)
			population_.ResolveSurvivalPhaseMovement();
	}
	
	// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
	population_.InvalidateMutationReferencesCache();
}

void Species::FinishMutationRunExperimentTimings(void)
{
	for (Chromosome *chromosome : chromosomes_)
		chromosome->FinishMutationRunExperimentTiming();
}

void Species::SetCycle(slim_tick_t p_new_cycle)
{
	cycle_ = p_new_cycle;
	
	// Note that the tree sequence tick depends upon the tick, not the cycle,
	// so that it is is sync for all species in the community.
}

void Species::AdvanceCycleCounter(void)
{
	// called by Community at the end of the cycle
	SetCycle(cycle_ + 1);
}

void Species::SimulationHasFinished(void)
{
	// This is an opportunity for final calculation/output when a simulation finishes
	// This is called by Community::SimulationHasFinished() for each species
	
	// Print mutation run experiment results
	if (SLiM_verbosity_level >= 2)
	{
		int summary_count = 0;
		
		for (Chromosome *chromosome : chromosomes_)
			summary_count += chromosome->MutationRunExperimentsEnabled();
		
		if (summary_count > 0)
		{
			SLIM_OUTSTREAM << std::endl;
			
			SLIM_OUTSTREAM << "// Mutation run experiment data:" << std::endl;
			SLIM_OUTSTREAM << "//" << std::endl;
			SLIM_OUTSTREAM << "// For each chromosome that conducted experiments, the optimal" << std::endl;
			SLIM_OUTSTREAM << "// mutation run count is given, with the percentage of cycles" << std::endl;
			SLIM_OUTSTREAM << "// in which that number was used.  The number of mutation run" << std::endl;
			SLIM_OUTSTREAM << "// experiments conducted is also given; if that is small (less" << std::endl;
			SLIM_OUTSTREAM << "// than 200 or so), or if the percentage of cycles is close to" << std::endl;
			SLIM_OUTSTREAM << "// or below 50%, the optimal count may not be accurate, since" << std::endl;
			SLIM_OUTSTREAM << "// insufficient data was gathered.  In that case, you might" << std::endl;
			SLIM_OUTSTREAM << "// wish to conduct your own timing experiments using different" << std::endl;
			SLIM_OUTSTREAM << "// counts.  Profile output also has more detail on this data." << std::endl;
			SLIM_OUTSTREAM << "//" << std::endl;
			
			for (Chromosome *chromosome : chromosomes_)
				chromosome->PrintMutationRunExperimentSummary();
			
			SLIM_OUTSTREAM << "//" << std::endl;
			SLIM_OUTSTREAM << "// It might (or might not) speed up your model to add:" << std::endl;
			SLIM_OUTSTREAM << "//" << std::endl;
			SLIM_OUTSTREAM << "//    mutationRuns=X" << std::endl;
			SLIM_OUTSTREAM << "//" << std::endl;
			SLIM_OUTSTREAM << "// to the initializeChromosome() call" << (summary_count > 1 ? "s" : "") << " in your initialize()" << std::endl;
			SLIM_OUTSTREAM << "// callback, where X is the optimal count for the chromosome." << std::endl;
			SLIM_OUTSTREAM << "// (If your model does not call initializeChromosome(), you" << std::endl;
			SLIM_OUTSTREAM << "// would need to add " << (summary_count > 1 ? "those calls" : "that call") <<
				".)  Optimal " << (summary_count > 1 ? "counts" : "count") << " may change" << std::endl;
			SLIM_OUTSTREAM << "// if your model changes, or even if the model is just run on" << std::endl;
			SLIM_OUTSTREAM << "// different hardware.  See the SLiM manual for more details." << std::endl;
			SLIM_OUTSTREAM << std::endl;
		}
	}
}

void Species::InferInheritanceForClone(Chromosome *chromosome, Individual *parent, IndividualSex sex, Haplosome **strand1, Haplosome **strand3, const char *caller_name)
{
#if DEBUG
	if (!chromosome || !parent || !strand1 || !strand3 || !caller_name)
		EIDOS_TERMINATION << "ERROR (Species::InferInheritanceForClone): (internal error) parameter is nullptr." << EidosTerminate();
#endif
	
	ChromosomeType chromosome_type = chromosome->Type();
	unsigned int chromosome_index = chromosome->Index();
	int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
	int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
	
	// validate the offspring's sex; note that we allow kHF_HaploidFemaleInherited and
	// kHM_HaploidMaleInherited to be inherited from the "wrong" sex, as does addCloned();
	// those inheritance patterns are for biparental crosses specifically
	IndividualSex parent_sex = parent->sex_;
	
	if (sex == IndividualSex::kUnspecified)
		sex = parent_sex;
	
	if (sex != parent_sex)
		if ((chromosome_type == ChromosomeType::kX_XSexChromosome) ||
			(chromosome_type == ChromosomeType::kY_YSexChromosome) ||
			(chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
			(chromosome_type == ChromosomeType::kW_WSexChromosome) ||
			(chromosome_type == ChromosomeType::kFL_HaploidFemaleLine) ||
			(chromosome_type == ChromosomeType::kML_HaploidMaleLine) ||
			(chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull))
		EIDOS_TERMINATION << "ERROR (Species::InferInheritanceForClone): clonal inheritance inference for " << caller_name << " requires that sex match the sex of the parent for chromosome type '" << chromosome_type << "' (symbol '" << chromosome->Symbol() << "'), since the haplosome configuration of that chromosome type depends upon sex.  You can pass NULL for sex to match the parent automatically." << EidosTerminate();
	
	// all returned entries not set are NULL
	*strand1 = nullptr;
	*strand3 = nullptr;
	
	// for simplicity, we just test for a null haplosome and clone whatever is not null;
	// if the parent is legal, the offspring will be legal too, given the sex check above
	if (chromosome->IntrinsicPloidy() == 2)
	{
		Haplosome *hap1 = parent->haplosomes_[first_haplosome_index];
		Haplosome *hap2 = parent->haplosomes_[last_haplosome_index];
		
		if (!hap1->IsNull())	*strand1 = hap1;
		if (!hap2->IsNull())	*strand3 = hap2;
	}
	else	// chromosome->IntrinsicPloidy() == 1
	{		
		Haplosome *hap = parent->haplosomes_[first_haplosome_index];
		
		if (!hap->IsNull())		*strand1 = hap;
	}
}

void Species::InferInheritanceForCross(Chromosome *chromosome, Individual *parent1, Individual *parent2, IndividualSex sex, Haplosome **strand1, Haplosome **strand2, Haplosome **strand3, Haplosome **strand4, const char *caller_name)
{
#if DEBUG
	if (!chromosome || !parent1 || !parent2 || !strand1 || !strand2 || !strand3 || !strand4 || !caller_name)
		EIDOS_TERMINATION << "ERROR (Species::InferInheritanceForCross): (internal error) parameter is nullptr." << EidosTerminate();
#endif
	
	ChromosomeType chromosome_type = chromosome->Type();
	unsigned int chromosome_index = chromosome->Index();
	int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
	int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
	
	// validate the offspring's sex; note that we allow kHF_HaploidFemaleInherited and
	// kHM_HaploidMaleInherited to be inherited from the "wrong" sex, as does addCloned();
	// those inheritance patterns are for biparental crosses specifically
	IndividualSex parent1_sex = parent1->sex_;
	IndividualSex parent2_sex = parent2->sex_;
	
	if (sex_enabled_ && ((parent1_sex != IndividualSex::kFemale) || (parent2_sex != IndividualSex::kMale)))
		EIDOS_TERMINATION << "ERROR (Species::InferInheritanceForCross): " << caller_name << " requires that parent1 be female and parent2 male, in a sexual model.  If you require more flexibility than this, turn off separate sexes and track the sex of individuals yourself, or use addPatternForRecombinant() instead." << EidosTerminate();
	
	if (sex == IndividualSex::kUnspecified)
		if ((chromosome_type == ChromosomeType::kX_XSexChromosome) ||
			(chromosome_type == ChromosomeType::kY_YSexChromosome) ||
			(chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
			(chromosome_type == ChromosomeType::kW_WSexChromosome) ||
			(chromosome_type == ChromosomeType::kFL_HaploidFemaleLine) ||
			(chromosome_type == ChromosomeType::kML_HaploidMaleLine) ||
			(chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull))
		EIDOS_TERMINATION << "ERROR (Species::InferInheritanceForCross): crossed inheritance inference for " << caller_name << " requires that sex is specified explicitly as 'M' or 'F' for chromosome type '" << chromosome_type << "' (symbol '" << chromosome->Symbol() << "'), since the haplosome configuration of that chromosome type depends upon sex." << EidosTerminate();
	
	// all returned entries not set are NULL
	*strand1 = nullptr;
	*strand2 = nullptr;
	*strand3 = nullptr;
	*strand4 = nullptr;
	
	// figure out the inheritance patterns, which are complex!
	switch (chromosome_type)
	{
			// diploid types
		case ChromosomeType::kA_DiploidAutosome:
		{
			// we require all haplosomes non-null; if the user is playing games, they need to control them
			Haplosome *hap1 = parent1->haplosomes_[first_haplosome_index];
			Haplosome *hap2 = parent1->haplosomes_[last_haplosome_index];
			Haplosome *hap3 = parent2->haplosomes_[first_haplosome_index];
			Haplosome *hap4 = parent2->haplosomes_[last_haplosome_index];
			
			if (hap1->IsNull() || hap2->IsNull() || hap3->IsNull() || hap4->IsNull())
				EIDOS_TERMINATION << "ERROR (Species::InferInheritanceForCross): crossed inheritance inference for " << caller_name << " requires that all four parental strands are not null haplosomes for chromosome type 'A'), since the parental strands are supposed to be crossed.  Use addPatternForRecombinant() to control more complex inheritance patterns." << EidosTerminate();
			
			*strand1 = hap1;
			*strand2 = hap2;
			*strand3 = hap3;
			*strand4 = hap4;
			break;
		}
		case ChromosomeType::kH_HaploidAutosome:
		{
			// we require all haplosomes non-null; if the user is playing games, they need to control them
			Haplosome *hap1 = parent1->haplosomes_[first_haplosome_index];
			Haplosome *hap3 = parent2->haplosomes_[first_haplosome_index];
			
			if (hap1->IsNull() || hap3->IsNull())
				EIDOS_TERMINATION << "ERROR (Species::InferInheritanceForCross): crossed inheritance inference for " << caller_name << " requires that both parental strands are not null haplosomes for chromosome type 'H'), since the strands from the two parents are supposed to be crossed.  Use addPatternForRecombinant() to control more complex inheritance patterns." << EidosTerminate();
			
			*strand1 = hap1;
			*strand2 = hap3;
			break;
		}
		case ChromosomeType::kX_XSexChromosome:
		{
			// females are XX, males are X-
			Haplosome *hap1 = parent1->haplosomes_[first_haplosome_index];
			Haplosome *hap2 = parent1->haplosomes_[last_haplosome_index];
			Haplosome *hap3 = parent2->haplosomes_[first_haplosome_index];	// hap4 is null
			
			if (sex == IndividualSex::kMale)
			{
				// first offspring X is crossed from the female, second is null (a Y was inherited instead)
				*strand1 = hap1;
				*strand2 = hap2;
			}
			else
			{
				// first offspring X is crossed from the female, second is clonal from the male
				*strand1 = hap1;
				*strand2 = hap2;
				*strand3 = hap3;
			}
			break;
		}
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kML_HaploidMaleLine:
		{
			// females are -, males are Y
			Haplosome *hap3 = parent2->haplosomes_[first_haplosome_index];
			
			if (sex == IndividualSex::kMale)
			{
				// offspring Y is inherited from the male
				*strand1 = hap3;
			}
			break;
		}
		case ChromosomeType::kZ_ZSexChromosome:
		{
			// females are -Z, males are ZZ
			Haplosome *hap2 = parent1->haplosomes_[last_haplosome_index];	// hap1 is null
			Haplosome *hap3 = parent2->haplosomes_[first_haplosome_index];
			Haplosome *hap4 = parent2->haplosomes_[last_haplosome_index];
			
			if (sex == IndividualSex::kMale)
			{
				// first offspring Z is clonal from the female, second is crossed from the male
				*strand1 = hap2;
				*strand3 = hap3;
				*strand4 = hap4;
			}
			else
			{
				// first offspring Z is null (a W was inherited instead), second is crossed from the male
				*strand3 = hap3;
				*strand4 = hap4;
			}
			break;
		}
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kFL_HaploidFemaleLine:
		{
			// females are W, males are -
			Haplosome *hap1 = parent1->haplosomes_[first_haplosome_index];
			
			if (sex == IndividualSex::kFemale)
			{
				// offspring W is inherited from the female
				*strand1 = hap1;
			}
			break;
		}
		case ChromosomeType::kHF_HaploidFemaleInherited:
		{
			Haplosome *hap1 = parent1->haplosomes_[first_haplosome_index];
			*strand1 = hap1;
			break;
		}
		case ChromosomeType::kHM_HaploidMaleInherited:
		{
			Haplosome *hap3 = parent2->haplosomes_[first_haplosome_index];
			*strand1 = hap3;
			break;
		}
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
		{
			// females are --, males are -Y
			Haplosome *hap4 = parent2->haplosomes_[last_haplosome_index];
			
			if (sex == IndividualSex::kMale)
			{
				// offspring Y is inherited from the male, to the second offspring haplosome
				*strand3 = hap4;
			}
			break;
		}
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:
		{
			EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCrossed): chromosome type 'H-' does not allow reproduction by biparental cross (only cloning); chromosome type 'H' provides greater flexibility for modeling haploids." << EidosTerminate();
		}
	}
	
	// this method always randomizes the initial copy strand; even if randomizeStrands=F is passed
	// to addMultiRecombinant(), inferred crosses should still behave like regular crosses
	Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
	
	if (*strand1 && *strand2 && Eidos_RandomBool(rng_state))
		std::swap(*strand1, *strand2);
	if (*strand3 && *strand4 && Eidos_RandomBool(rng_state))
		std::swap(*strand3, *strand4);
}

void Species::Species_CheckIntegrity(void)
{
#if DEBUG
	// Check for consistency in the chromosome setup first
	const std::vector<Chromosome *> &chromosomes = Chromosomes();
	size_t chromosomes_count = chromosomes.size();
	int haplosome_index = 0;
	bool null_haplosomes_used = false;
	
	if (has_genetics_ && (chromosomes_count == 0))
		EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) no chromosome present in genetic species." << EidosTerminate();
	
	for (size_t chromosome_index = 0; chromosome_index < chromosomes_count; chromosome_index++)
	{
		Chromosome *chromosome = chromosomes[chromosome_index];
		ChromosomeType chromosome_type = chromosome->Type();
		
		if (chromosome->Index() != chromosome_index)
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) chromosome->Index() mismatch." << EidosTerminate();
		
		if (ChromosomeFromID(chromosome->ID()) != chromosome)
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) chromosome->ID() lookup error." << EidosTerminate();
		
		if (ChromosomeFromSymbol(chromosome->Symbol()) != chromosome)
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) chromosome->Symbol() lookup error." << EidosTerminate();
		
		if (!sex_enabled_ &&
			((chromosome_type == ChromosomeType::kX_XSexChromosome) ||
			 (chromosome_type == ChromosomeType::kY_YSexChromosome) ||
			 (chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
			 (chromosome_type == ChromosomeType::kW_WSexChromosome) ||
			 //(chromosome_type == ChromosomeType::kHF_HaploidFemaleInherited) ||		// now allowing; see issue #534
			 (chromosome_type == ChromosomeType::kFL_HaploidFemaleLine) ||
			 //(chromosome_type == ChromosomeType::kHM_HaploidMaleInherited) ||			// now allowing; see issue #534
			 (chromosome_type == ChromosomeType::kML_HaploidMaleLine) ||
			 (chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull)))
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) chromosome type '" << chromosome_type << "' not allowed in non-sexual models." << EidosTerminate();
		
		// check haplosome indices
		int haplosome_count = chromosome->IntrinsicPloidy();
		
		if (first_haplosome_index_[chromosome_index] != haplosome_index)
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) first_haplosome_index_ mismatch." << EidosTerminate();
		if (last_haplosome_index_[chromosome_index] != haplosome_index + haplosome_count - 1)
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) last_haplosome_index_ mismatch." << EidosTerminate();
		
		haplosome_index += haplosome_count;
		
		// check null haplosome optimization
		if (chromosome->AlwaysUsesNullHaplosomes())
			null_haplosomes_used = true;
	}
	
	if (haplosome_index != haplosome_count_per_individual_)
		EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) haplosome_count_per_individual_ does not match chromosomes." << EidosTerminate();
	
	if (null_haplosomes_used != chromosomes_use_null_haplosomes_)
		EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) chromosomes_use_null_haplosomes_ mismatch." << EidosTerminate();
#endif
	
#if DEBUG
	// Then check each individual and its haplosomes
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
}

void Species::_CheckMutationStackPolicy(void)
{
	// Check mutation stacking policy for consistency.  This is called periodically during the simulation.
	
	// First do a fast check for the standard case, that each mutation type is in its own stacking group
	// with an index equal to its mutation_type_id_.  Unless the user has configured stacking groups this
	// will verify the setup very quickly.
	bool stacking_nonstandard = false;
	
	for (auto muttype_iter : mutation_types_)
	{
		MutationType *muttype = muttype_iter.second;
		
		if (muttype->stack_group_ != muttype->mutation_type_id_)
		{
			stacking_nonstandard = true;
			break;
		}
	}
	
	if (stacking_nonstandard)
	{
		// If there are N mutation types that participate in M stacking groups, the runtime of the code below
		// is approximately O(N*M), so it can take quite a long time with many distinct stacking groups.  It
		// could perhaps be made faster by first putting the mutation types into a data structure that sorted
		// them by stacking group; a std::map, or just sorting them by stacking group in a vector.  However,
		// I have yet to encounter a model that triggers this case badly (now that the nucleotide model has
		// been fixed to use a single mutation stacking group).
		std::vector<int64_t> checked_groups;
		
		for (auto muttype_iter : mutation_types_)
		{
			MutationType *muttype = muttype_iter.second;
			int64_t stack_group = muttype->stack_group_;
			
			if (std::find(checked_groups.begin(), checked_groups.end(), stack_group) == checked_groups.end())
			{
				// This stacking group has not been checked yet
				MutationStackPolicy stack_policy = muttype->stack_policy_;
				
				for (auto muttype_iter2 : mutation_types_)
				{
					MutationType *muttype2 = muttype_iter2.second;
					
					if ((muttype2->stack_group_ == stack_group) && (muttype2->stack_policy_ != stack_policy))
						EIDOS_TERMINATION << "ERROR (Species::_CheckMutationStackPolicy): inconsistent mutationStackPolicy values within one mutationStackGroup." << EidosTerminate();
				}
				
				checked_groups.emplace_back(stack_group);
			}
		}
	}
	
	// we're good until the next change
	mutation_stack_policy_changed_ = false;
}

void Species::MaxNucleotideMutationRateChanged(void)
{
	CacheNucleotideMatrices();
	
	for (Chromosome *chromosome : chromosomes_)
	{
		chromosome->CreateNucleotideMutationRateMap();
		chromosome->InitializeDraws();
	}
}

void Species::CacheNucleotideMatrices(void)
{
	// Go through all genomic element types in a nucleotide-based model, analyze their mutation matrices,
	// and find the maximum mutation rate expressed by any genomic element type for any genomic background.
	max_nucleotide_mut_rate_ = 0.0;
	
	for (auto type_entry : genomic_element_types_)
	{
		GenomicElementType *ge_type = type_entry.second;
		
		if (ge_type->mm_thresholds)
			free(ge_type->mm_thresholds);
		
		if (ge_type->mutation_matrix_)
		{
			EidosValue_Float *mm = ge_type->mutation_matrix_.get();
			const double *mm_data = mm->data();
			
			if (mm->Count() == 16)
			{
				for (int nuc = 0; nuc < 4; ++nuc)
				{
					double rateA = mm_data[nuc];
					double rateC = mm_data[nuc + 4];
					double rateG = mm_data[nuc + 8];
					double rateT = mm_data[nuc + 12];
					double total_rate = rateA + rateC + rateG + rateT;
					
					if (total_rate > max_nucleotide_mut_rate_)
						max_nucleotide_mut_rate_ = total_rate;
				}
			}
			else if (mm->Count() == 256)
			{
				for (int trinuc = 0; trinuc < 64; ++trinuc)
				{
					double rateA = mm_data[trinuc];
					double rateC = mm_data[trinuc + 64];
					double rateG = mm_data[trinuc + 128];
					double rateT = mm_data[trinuc + 192];
					double total_rate = rateA + rateC + rateG + rateT;
					
					if (total_rate > max_nucleotide_mut_rate_)
						max_nucleotide_mut_rate_ = total_rate;
				}
			}
			else
				EIDOS_TERMINATION << "ERROR (Species::CacheNucleotideMatrices): (internal error) unsupported mutation matrix size." << EidosTerminate();
		}
	}
	
	// Now go through the genomic element types again, and calculate normalized mutation rate
	// threshold values that will allow fast decisions on which derived nucleotide to create
	for (auto type_entry : genomic_element_types_)
	{
		GenomicElementType *ge_type = type_entry.second;
		
		if (ge_type->mutation_matrix_)
		{
			EidosValue_Float *mm = ge_type->mutation_matrix_.get();
			const double *mm_data = mm->data();
			
			if (mm->Count() == 16)
			{
				ge_type->mm_thresholds = (double *)malloc(16 * sizeof(double));
				if (!ge_type->mm_thresholds)
					EIDOS_TERMINATION << "ERROR (Species::CacheNucleotideMatrices): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
				
				for (int nuc = 0; nuc < 4; ++nuc)
				{
					double rateA = mm_data[nuc];
					double rateC = mm_data[nuc + 4];
					double rateG = mm_data[nuc + 8];
					double rateT = mm_data[nuc + 12];
					double total_rate = rateA + rateC + rateG + rateT;
					double fraction_of_max_rate = total_rate / max_nucleotide_mut_rate_;
					double *nuc_thresholds = ge_type->mm_thresholds + (size_t)nuc * 4;
					
					nuc_thresholds[0] = (rateA / total_rate) * fraction_of_max_rate;
					nuc_thresholds[1] = ((rateA + rateC) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[2] = ((rateA + rateC + rateG) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[3] = fraction_of_max_rate;
				}
			}
			else if (mm->Count() == 256)
			{
				ge_type->mm_thresholds = (double *)malloc(256 * sizeof(double));
				if (!ge_type->mm_thresholds)
					EIDOS_TERMINATION << "ERROR (Species::CacheNucleotideMatrices): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
				
				for (int trinuc = 0; trinuc < 64; ++trinuc)
				{
					double rateA = mm_data[trinuc];
					double rateC = mm_data[trinuc + 64];
					double rateG = mm_data[trinuc + 128];
					double rateT = mm_data[trinuc + 192];
					double total_rate = rateA + rateC + rateG + rateT;
					double fraction_of_max_rate = total_rate / max_nucleotide_mut_rate_;
					double *nuc_thresholds = ge_type->mm_thresholds + (size_t)trinuc * 4;
					
					nuc_thresholds[0] = (rateA / total_rate) * fraction_of_max_rate;
					nuc_thresholds[1] = ((rateA + rateC) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[2] = ((rateA + rateC + rateG) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[3] = fraction_of_max_rate;
				}
			}
			else
				EIDOS_TERMINATION << "ERROR (Species::CacheNucleotideMatrices): (internal error) unsupported mutation matrix size." << EidosTerminate();
		}
	}
}

void Species::TabulateSLiMMemoryUsage_Species(SLiMMemoryUsage_Species *p_usage)
{
	EIDOS_BZERO(p_usage, sizeof(SLiMMemoryUsage_Species));
	
	// Gather haplosomes in preparation for the work below
	std::vector<Haplosome *> all_haplosomes_in_use, all_haplosomes_not_in_use;
	size_t haplosome_pool_usage = 0, individual_pool_usage = 0;
	int haplosome_count_per_individual = HaplosomeCountPerIndividual();
	
	for (auto iter : population_.subpops_)
	{
		Subpopulation &subpop = *iter.second;
		
		for (Individual *ind : subpop.parent_individuals_)
		{
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				all_haplosomes_in_use.push_back(haplosomes[haplosome_index]);
		}
		
		for (Individual *ind : subpop.child_individuals_)
		{
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				all_haplosomes_in_use.push_back(haplosomes[haplosome_index]);
		}
		
		for (Individual *ind : subpop.nonWF_offspring_individuals_)
		{
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				all_haplosomes_in_use.push_back(haplosomes[haplosome_index]);
		}
		
	}
	
	for (Chromosome *chromosome : chromosomes_)
	{
		all_haplosomes_not_in_use.insert(all_haplosomes_not_in_use.end(), chromosome->HaplosomesJunkyardNonnull().begin(), chromosome->HaplosomesJunkyardNonnull().end());
		all_haplosomes_not_in_use.insert(all_haplosomes_not_in_use.end(), chromosome->HaplosomesJunkyardNull().begin(), chromosome->HaplosomesJunkyardNull().end());
	}
	
	haplosome_pool_usage = species_haplosome_pool_.MemoryUsageForAllNodes();
	individual_pool_usage = species_individual_pool_.MemoryUsageForAllNodes();
	
	// Chromosome
	{
		p_usage->chromosomeObjects_count = chromosomes_.size();
		p_usage->chromosomeObjects = sizeof(Chromosome) * p_usage->chromosomeObjects_count;
		p_usage->chromosomeMutationRateMaps = 0;
		p_usage->chromosomeRecombinationRateMaps = 0;
		p_usage->chromosomeAncestralSequence = 0;
		
		for (Chromosome *chromosome : chromosomes_)
		{
			p_usage->chromosomeMutationRateMaps += chromosome->MemoryUsageForMutationMaps();
			p_usage->chromosomeRecombinationRateMaps += chromosome->MemoryUsageForRecombinationMaps();
			p_usage->chromosomeAncestralSequence += chromosome->MemoryUsageForAncestralSequence();
		}
	}
	
	// Haplosome
	{
		p_usage->haplosomeObjects_count = all_haplosomes_in_use.size();
		p_usage->haplosomeObjects = sizeof(Haplosome) * p_usage->haplosomeObjects_count;
		
		for (Haplosome *haplosome : all_haplosomes_in_use)
			p_usage->haplosomeExternalBuffers += haplosome->MemoryUsageForMutrunBuffers();
		
		p_usage->haplosomeUnusedPoolSpace = haplosome_pool_usage - p_usage->haplosomeObjects;	// includes junkyard objects and unused space
		
		for (Haplosome *haplosome : all_haplosomes_not_in_use)
			p_usage->haplosomeUnusedPoolBuffers += haplosome->MemoryUsageForMutrunBuffers();
	}
	
	// GenomicElement
	{
		p_usage->genomicElementObjects_count = 0;
		
		for (Chromosome *chromosome : chromosomes_)
			p_usage->genomicElementObjects_count += chromosome->GenomicElementCount();
		
		p_usage->genomicElementObjects = sizeof(GenomicElement) * p_usage->genomicElementObjects_count;
	}
	
	// GenomicElementType
	{
		p_usage->genomicElementTypeObjects_count = genomic_element_types_.size();
		p_usage->genomicElementTypeObjects = sizeof(GenomicElementType) * p_usage->genomicElementTypeObjects_count;
	}
	
	// Individual
	{
		int64_t objectCount = 0;
		
		for (auto iter : population_.subpops_)
		{
			Subpopulation &subpop = *iter.second;
			
			objectCount += subpop.parent_individuals_.size();
			objectCount += subpop.child_individuals_.size();
			objectCount += subpop.nonWF_offspring_individuals_.size();
		}
		
		p_usage->individualObjects_count = objectCount;
		p_usage->individualObjects = sizeof(Individual) * p_usage->individualObjects_count;
		
		// externally allocated haplosome buffers; don't count if the internal buffer (capacity 2) is in use
		if (haplosome_count_per_individual > 2)
			p_usage->individualHaplosomeVectors = p_usage->individualObjects_count * haplosome_count_per_individual * sizeof(Haplosome*);
		
		// individuals in the junkyard, awaiting reuse, including their haplosome buffers
		p_usage->individualJunkyardAndHaplosomes = sizeof(Individual) * population_.species_individuals_junkyard_.size();
		if (haplosome_count_per_individual > 2)
			p_usage->individualJunkyardAndHaplosomes = population_.species_individuals_junkyard_.size() * haplosome_count_per_individual * sizeof(Haplosome*);
		
		// unused pool space; this is memory for new individuals that has never been used, and has no haplosome buffers
		p_usage->individualUnusedPoolSpace = individual_pool_usage - p_usage->individualObjects;
	}
	
	// Mutation
	{
		int registry_size;
		
		population_.MutationRegistry(&registry_size);
		p_usage->mutationObjects_count = (int64_t)registry_size;
		p_usage->mutationObjects = sizeof(Mutation) * registry_size;
	}
	
	// MutationRun
	{
		{
			int64_t mutrun_objectCount = 0;
			int64_t mutrun_externalBuffers = 0;
			int64_t mutrun_nonneutralCaches = 0;
			
			// each thread has its own inuse pool
			for (Chromosome *chromosome : chromosomes_)
			{
				for (int threadnum = 0; threadnum < chromosome->ChromosomeMutationRunContextCount(); ++threadnum)
				{
					MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(threadnum);
					
					for (const MutationRun *inuse_mutrun : mutrun_context.in_use_pool_)
					{
						mutrun_objectCount++;
						mutrun_externalBuffers += inuse_mutrun->MemoryUsageForMutationIndexBuffers();
						mutrun_nonneutralCaches += inuse_mutrun->MemoryUsageForNonneutralCaches();
					}
				}
			}
			
			p_usage->mutationRunObjects_count = mutrun_objectCount;
			p_usage->mutationRunObjects = sizeof(MutationRun) * mutrun_objectCount;
			
			p_usage->mutationRunExternalBuffers = mutrun_externalBuffers;
			p_usage->mutationRunNonneutralCaches = mutrun_nonneutralCaches;
		}
		
		{
			int64_t mutrun_unusedCount = 0;
			int64_t mutrun_unusedBuffers = 0;
			
			// each thread has its own free pool
			for (Chromosome *chromosome : chromosomes_)
			{
				for (int threadnum = 0; threadnum < chromosome->ChromosomeMutationRunContextCount(); ++threadnum)
				{
					MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(threadnum);
					
					for (const MutationRun *free_mutrun : mutrun_context.freed_pool_)
					{
						mutrun_unusedCount++;
						mutrun_unusedBuffers += free_mutrun->MemoryUsageForMutationIndexBuffers();
						mutrun_unusedBuffers += free_mutrun->MemoryUsageForNonneutralCaches();
					}
				}
			}
			
			p_usage->mutationRunUnusedPoolSpace = sizeof(MutationRun) * mutrun_unusedCount;
			p_usage->mutationRunExternalBuffers = mutrun_unusedBuffers;
		}
	}
	
	// MutationType
	{
		p_usage->mutationTypeObjects_count = mutation_types_.size();
		p_usage->mutationTypeObjects = sizeof(MutationType) * p_usage->mutationTypeObjects_count;
	}
	
	// Species (including the Population object)
	{
		p_usage->speciesObjects_count = 1;
		p_usage->speciesObjects = (sizeof(Species) - sizeof(Chromosome)) * p_usage->speciesObjects_count;	// Chromosome is handled separately above
		
		// this now adds up usage across all table collections, avoiding overcounting of shared tables
		p_usage->speciesTreeSeqTables = 0;
		bool first = true;
		
		for (TreeSeqInfo &tsinfo : treeseq_)
		{
			p_usage->speciesTreeSeqTables += MemoryUsageForTreeSeqInfo(tsinfo, /* p_count_shared_tables */ first);
			first = false;
		}
	}
	
	// Subpopulation
	{
		p_usage->subpopulationObjects_count = population_.subpops_.size();
		p_usage->subpopulationObjects = sizeof(Subpopulation) * p_usage->subpopulationObjects_count;
		
		for (auto iter : population_.subpops_)
		{
			Subpopulation &subpop = *iter.second;
			
			if (subpop.cached_parental_fitness_)
				p_usage->subpopulationFitnessCaches += subpop.cached_fitness_capacity_ * sizeof(double);
			if (subpop.mate_choice_weights_)
				p_usage->subpopulationFitnessCaches += subpop.mate_choice_weights_->Count() * sizeof(double);
			
			p_usage->subpopulationParentTables += subpop.MemoryUsageForParentTables();
			
			for (const auto &iter_map : subpop.spatial_maps_)
			{
				SpatialMap &map = *iter_map.second;
				
				if (map.values_)
				{
					if (map.spatiality_ == 1)
						p_usage->subpopulationSpatialMaps += map.grid_size_[0] * sizeof(double);
					else if (map.spatiality_ == 2)
						p_usage->subpopulationSpatialMaps += map.grid_size_[0] * map.grid_size_[1] * sizeof(double);
					else if (map.spatiality_ == 3)
						p_usage->subpopulationSpatialMaps += map.grid_size_[0] * map.grid_size_[1] * map.grid_size_[2] * sizeof(double);
				}
				if (map.red_components_)
					p_usage->subpopulationSpatialMaps += map.n_colors_ * sizeof(float) * 3;
#if defined(SLIMGUI)
				if (map.display_buffer_)
					p_usage->subpopulationSpatialMapsDisplay += (size_t)map.buffer_width_ * (size_t)map.buffer_height_ * sizeof(uint8_t) * 3;
				
				// FIXME: the usage due to image_ should be added here
#endif
			}
		}
	}
	
	/*
	 Subpopulation:
	 
	gsl_ran_discrete_t *lookup_parent_ = nullptr;
	gsl_ran_discrete_t *lookup_female_parent_ = nullptr;
	gsl_ran_discrete_t *lookup_male_parent_ = nullptr;

	 */
	
	// Substitution
	{
		p_usage->substitutionObjects_count = population_.substitutions_.size();
		p_usage->substitutionObjects = sizeof(Substitution) * p_usage->substitutionObjects_count;
	}
	
	// missing: EidosCallSignature, EidosPropertySignature, EidosScript, EidosToken, function map, global strings and ids and maps, std::strings in various objects
	// that sort of overhead should be fairly constant, though, and should be dwarfed by the overhead of the objects above in bigger models
	
	// also missing: LogFile
	
	SumUpMemoryUsage_Species(*p_usage);
}

slim_popsize_t *Species::BorrowShuffleBuffer(slim_popsize_t p_buffer_size)
{
	if (shuffle_buf_borrowed_)
		EIDOS_TERMINATION << "ERROR (Species::BorrowShuffleBuffer): (internal error) shuffle buffer already borrowed." << EidosTerminate();
	
#if DEBUG_SHUFFLE_BUFFER
	// guarantee allocation of a buffer, even with a requested size of 0, so we have a place to put our overrun barriers
	if ((p_buffer_size > shuffle_buf_capacity_) || !shuffle_buffer_)
#else
	if (p_buffer_size > shuffle_buf_capacity_)
#endif
	{
		if (shuffle_buffer_)
			free(shuffle_buffer_);
		shuffle_buf_capacity_ = p_buffer_size * 2;		// double capacity so we reallocate less often
#if DEBUG_SHUFFLE_BUFFER
		// room for an extra value at the start and end
		shuffle_buffer_ = (slim_popsize_t *)malloc((shuffle_buf_capacity_ + 2) * sizeof(slim_popsize_t));
#else
		shuffle_buffer_ = (slim_popsize_t *)malloc(shuffle_buf_capacity_ * sizeof(slim_popsize_t));
#endif
		shuffle_buf_size_ = 0;
		
		if (!shuffle_buffer_)
			EIDOS_TERMINATION << "ERROR (Species::BorrowShuffleBuffer): allocation failed (requested size " << p_buffer_size << " entries, allocation size " << (shuffle_buf_capacity_ * sizeof(slim_popsize_t)) << " bytes); you may need to raise the memory limit for SLiM." << EidosTerminate();
	}
	
#if DEBUG_SHUFFLE_BUFFER
	// put flag values in to detect an overrun
	slim_popsize_t *buffer_contents = shuffle_buffer_ + 1;
	
	shuffle_buffer_[0] = (slim_popsize_t)0xDEADD00D;
	shuffle_buffer_[p_buffer_size + 1] = (slim_popsize_t)0xDEADD00D;
#else
	slim_popsize_t *buffer_contents = shuffle_buffer_;
#endif
	
	if (shuffle_buf_is_enabled_)
	{
		// The shuffle buffer is enabled, so we need to reinitialize it with sequential values if it has
		// changed size (unnecessary if it has not changed size, since the values are just rearranged),
		// and then shuffle it into a new order.
		
		if (p_buffer_size != shuffle_buf_size_)
		{
			for (slim_popsize_t i = 0; i < p_buffer_size; ++i)
				buffer_contents[i] = i;
			
			shuffle_buf_size_ = p_buffer_size;
		}
		
		if (shuffle_buf_size_ > 0)
		{
			EidosRNG_32_bit &rng_32 = EIDOS_32BIT_RNG(omp_get_thread_num());
			Eidos_ran_shuffle_uint32(rng_32, buffer_contents, shuffle_buf_size_);
		}
	}
	else
	{
		// The shuffle buffer is disabled, so we can assume that existing entries are already sequential,
		// and we only need to "top off" the buffer with new sequential values if it has grown.
		if (p_buffer_size > shuffle_buf_size_)
		{
			for (slim_popsize_t i = shuffle_buf_size_; i < p_buffer_size; ++i)
				buffer_contents[i] = i;
			
			shuffle_buf_size_ = p_buffer_size;
		}
	}
	
#if DEBUG_SHUFFLE_BUFFER
	// check for correct setup of flag values; entries 1:shuffle_buf_size_ are used
	if (shuffle_buffer_[0] != (slim_popsize_t)0xDEADD00D)
		EIDOS_TERMINATION << "ERROR (Species::BorrowShuffleBuffer): (internal error) shuffle buffer overrun at start." << EidosTerminate();
	if (shuffle_buffer_[shuffle_buf_size_ + 1] != (slim_popsize_t)0xDEADD00D)
		EIDOS_TERMINATION << "ERROR (Species::BorrowShuffleBuffer): (internal error) shuffle buffer overrun at end." << EidosTerminate();
#endif
	
	shuffle_buf_borrowed_ = true;
	return buffer_contents;
}

void Species::ReturnShuffleBuffer(void)
{
	if (!shuffle_buf_borrowed_)
		EIDOS_TERMINATION << "ERROR (Species::ReturnShuffleBuffer): (internal error) shuffle buffer was not borrowed." << EidosTerminate();
	
#if DEBUG_SHUFFLE_BUFFER
	// check for correct setup of flag values; entries 1:shuffle_buf_size_ are used
	if (shuffle_buffer_[0] != (slim_popsize_t)0xDEADD00D)
		EIDOS_TERMINATION << "ERROR (Species::ReturnShuffleBuffer): (internal error) shuffle buffer overrun at start." << EidosTerminate();
	if (shuffle_buffer_[shuffle_buf_size_ + 1] != (slim_popsize_t)0xDEADD00D)
		EIDOS_TERMINATION << "ERROR (Species::ReturnShuffleBuffer): (internal error) shuffle buffer overrun at end." << EidosTerminate();
#endif
	
	shuffle_buf_borrowed_ = false;
}

#if (SLIMPROFILING == 1)
// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
void Species::CollectMutationProfileInfo(void)
{
	// maintain our history of the nonneutral regime
	profile_nonneutral_regime_history_.emplace_back(last_nonneutral_regime_);
	
	// track the maximum number of mutations in existence at one time
	int registry_size;
	
	population_.MutationRegistry(&registry_size);
	profile_max_mutation_index_ = std::max(profile_max_mutation_index_, (int64_t)registry_size);
	
	// tally per-chromosome information
	int64_t operation_id = MutationRun::GetNextOperationID();
	
	for (Chromosome *chromosome : Chromosomes())
	{
		int first_haplosome_index = FirstHaplosomeIndices()[chromosome->Index()];
		int last_haplosome_index = LastHaplosomeIndices()[chromosome->Index()];
		
		// maintain our history of the number of mutruns per haplosome
		chromosome->profile_mutcount_history_.emplace_back(chromosome->mutrun_count_);
		
		// tally up the number of mutation runs, mutation usage metrics, etc.
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					const MutationRun **mutruns = haplosome->mutruns_;
					int32_t mutrun_count = haplosome->mutrun_count_;
					
					chromosome->profile_mutrun_total_usage_ += mutrun_count;
					
					for (int32_t mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
					{
						const MutationRun *mutrun = mutruns[mutrun_index];
						
						if (mutrun)
						{
							if (mutrun->operation_id_ != operation_id)
							{
								mutrun->operation_id_ = operation_id;
								chromosome->profile_unique_mutrun_total_++;
							}
							
							// tally the total and nonneutral mutations
							mutrun->tally_nonneutral_mutations(&chromosome->profile_mutation_total_usage_,
															   &chromosome->profile_nonneutral_mutation_total_,
															   &chromosome->profile_mutrun_nonneutral_recache_total_);
						}
					}
				}
			}
		}
	}
}
#endif
#endif


//
// TREE SEQUENCE RECORDING
//
#pragma mark -
#pragma mark Tree sequence recording
#pragma mark -

void Species::AboutToSplitSubpop(void)
{
	// see Population::AddSubpopulationSplit()
	community_.tree_seq_tick_offset_ += 0.00001;
}

void Species::CopySharedTablesIn(tsk_table_collection_t &p_tables)
{
	// This directly copies the shared tables (nodes, individuals, and populations) into the
	// table collection p_tables.  This means that p_tables will point to the same table
	// column buffers as the main table collection does, BUT will have its own separate row
	// counts for those buffers.  This is an extraordinarily dangerous state to be in; if
	// either table collection adds/removes rows from a shared table, the two collections
	// will get out of synch, and buffer overruns and other problems will soon follow.
	// As soon as possible, DisconnectCopiedSharedTables() should be called to undo this.
#if DEBUG
	if (&p_tables == &treeseq_[0].tables_)
		EIDOS_TERMINATION << "ERROR (Species::CopySharedTablesIn): (internal error) trying to copy shared tables into the main table collection!" << EidosTerminate();
#endif
	
	tsk_table_collection_t &main_tables = treeseq_[0].tables_;
	
	p_tables.nodes = main_tables.nodes;
	p_tables.individuals = main_tables.individuals;
	p_tables.populations = main_tables.populations;
}

void Species::DisconnectCopiedSharedTables(tsk_table_collection_t &p_tables)
{
	// This zeroes out copies of shared tables (nodes, individuals, and populations) set up
	// by CopySharedTablesIn().  Note that changes to shared column data will persist, but
	// changes to row counts will *not* persist; they get zeroed here.  Be careful!
	
	// The tskit example at https://github.com/tskit-dev/tskit/pull/2665/files only
	// disconnects at the end, in free_tables(), but that seems very dangerous; any
	// accidental use of a tskit API that modifies a copied table will make things
	// go out of sync.  Our design here means we have to copy in and then disconnect
	// around every operation that references the contents of a given table, but it seems
	// safer to me.
#if DEBUG
	if (&p_tables == &treeseq_[0].tables_)
		EIDOS_TERMINATION << "ERROR (Species::DisconnectCopiedSharedTables): (internal error) trying to disconnect the main table collection!" << EidosTerminate();
#endif
	
	EIDOS_BZERO(&p_tables.nodes, sizeof(p_tables.nodes));
	EIDOS_BZERO(&p_tables.individuals, sizeof(p_tables.individuals));
	EIDOS_BZERO(&p_tables.populations, sizeof(p_tables.populations));
}

void Species::handle_error(const std::string &msg, int err)
{
	std::cout << "Error:" << msg << ": " << tsk_strerror(err) << std::endl;
	EIDOS_TERMINATION << msg << ": " << tsk_strerror(err) << EidosTerminate();
}

void Species::ReorderIndividualTable(tsk_table_collection_t *p_tables, std::vector<int> p_individual_map, bool p_keep_unmapped)
{
	// Modifies the tables in place so that individual number individual_map[k] becomes the k-th individual in the new tables.
	// Discard unmapped individuals unless p_keep_unmapped is true, in which case put them at the end.
	size_t num_individuals = p_tables->individuals.num_rows;
	std::vector<tsk_id_t> inverse_map(num_individuals, TSK_NULL);
	
	for (tsk_id_t j = 0; (size_t)j < p_individual_map.size(); j++)
		inverse_map[p_individual_map[j]] = j;
	
	// If p_keep_unmapped is true, use the inverse table to add all unmapped individuals to the end of p_individual_map
	if (p_keep_unmapped)
	{
		for (tsk_id_t j = 0; (size_t)j < inverse_map.size(); j++)
		{
			if (inverse_map[j] == TSK_NULL)
			{
				inverse_map[j] = (tsk_id_t)p_individual_map.size();
				p_individual_map.emplace_back(j);
			}
		}
		assert(p_individual_map.size() == p_tables->individuals.num_rows);
	}
	
	// Make a copy of p_tables->individuals, from which we will copy rows back to p_tables->individuals
	tsk_individual_table_t individuals_copy;
	int ret = tsk_individual_table_copy(&p_tables->individuals, &individuals_copy, 0);
	if (ret < 0) handle_error("reorder_individuals", ret);
	
	// Clear p_tables->individuals and copy rows into it in the requested order
	tsk_individual_table_clear(&p_tables->individuals);
	
	for (tsk_id_t k : p_individual_map)
	{
		assert((size_t) k < individuals_copy.num_rows);
		
		uint32_t flags = individuals_copy.flags[k];
		double *location = individuals_copy.location + individuals_copy.location_offset[k];
		size_t location_length = individuals_copy.location_offset[k+1] - individuals_copy.location_offset[k];
		const char *metadata = individuals_copy.metadata + individuals_copy.metadata_offset[k];
		size_t metadata_length = individuals_copy.metadata_offset[k+1] - individuals_copy.metadata_offset[k];
		
		ret = tsk_individual_table_add_row(&p_tables->individuals, flags, location, (tsk_size_t)location_length,
                NULL, 0, // individual parents
                metadata, (tsk_size_t)metadata_length);
		if (ret < 0) handle_error("tsk_individual_table_add_row", ret);
	}
	
	assert(p_tables->individuals.num_rows == p_individual_map.size());
	
	// Free the contents of the individual table copy we made (but not the table itself, which is stack-allocated)
	tsk_individual_table_free(&individuals_copy);
	
	// Fix the individual indices in the nodes table to point to the new rows
	for (size_t j = 0; j < p_tables->nodes.num_rows; j++)
	{
		tsk_id_t old_indiv = p_tables->nodes.individual[j];
		
		if (old_indiv >= 0)
			p_tables->nodes.individual[j] = inverse_map[old_indiv];
	}
}

void Species::AddParentsColumnForOutput(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash)
{
	// Build a parents column in the individuals table for output, from the pedigree IDs in the metadata
	// We create the parents column and fill it with info.  Note that we always know the pedigree ID if a parent
	// existed, so a parent pedigree ID of -1 means "there was no parent", and should result in no parent table entry.
	// A parent pedigree ID that is not present in the individuals table translates to TSK_NULL, which means
	// "this parent did exist, but was not put in the table, or was simplified away".  We allocate two entries
	// per individual, which might be an overallocation but is unlikely to matter.
	size_t num_rows = p_tables->individuals.num_rows;
	size_t parents_buffer_size = num_rows * 2 * sizeof(tsk_id_t);
	tsk_id_t *parents_buffer = (tsk_id_t *)malloc(parents_buffer_size);
	tsk_size_t *parents_offset_buffer = (tsk_size_t *)malloc((p_tables->individuals.max_rows + 1) * sizeof(tsk_size_t));	// +1 for the trailing length entry
	
	if (!parents_buffer || !parents_offset_buffer)
		EIDOS_TERMINATION << "ERROR (Species::AddParentsColumnForOutput): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	tsk_id_t *parents_buffer_ptr = parents_buffer;
	
	for (tsk_size_t individual_index = 0; individual_index < num_rows; individual_index++)
	{
		tsk_id_t tsk_individual = (tsk_id_t)individual_index;
		IndividualMetadataRec *metadata_rec = (IndividualMetadataRec *)(p_tables->individuals.metadata + p_tables->individuals.metadata_offset[tsk_individual]);
		slim_pedigreeid_t pedigree_p1 = metadata_rec->pedigree_p1_;
		slim_pedigreeid_t pedigree_p2 = metadata_rec->pedigree_p2_;
		
		parents_offset_buffer[individual_index] = parents_buffer_ptr - parents_buffer;
		
		if (pedigree_p1 != -1)
		{
			auto p1_iter = p_individuals_hash->find(pedigree_p1);
			tsk_id_t p1_tskid = (p1_iter == p_individuals_hash->end()) ? TSK_NULL : p1_iter->second;
			
			//std::cout << "first parent pedigree ID " << pedigree_p1 << " is tskid " << p1_tskid << std::endl;
			*(parents_buffer_ptr++) = p1_tskid;
		}
		
		if (pedigree_p2 != -1)
		{
			auto p2_iter = p_individuals_hash->find(pedigree_p2);
			tsk_id_t p2_tskid = (p2_iter == p_individuals_hash->end()) ? TSK_NULL : p2_iter->second;
			
			//std::cout << "second parent pedigree ID " << pedigree_p2 << " is tskid " << p2_tskid << std::endl;
			*(parents_buffer_ptr++) = p2_tskid;
		}
	}
	
	parents_offset_buffer[num_rows] = parents_buffer_ptr - parents_buffer;
	
	// Put the new parents buffers into the individuals table
	if (p_tables->individuals.parents)
		free(p_tables->individuals.parents);
	p_tables->individuals.parents = parents_buffer;
	
	if (p_tables->individuals.parents_offset)
		free(p_tables->individuals.parents_offset);
	p_tables->individuals.parents_offset = parents_offset_buffer;
	
	p_tables->individuals.parents_length = parents_buffer_ptr - parents_buffer;
	p_tables->individuals.max_parents_length = parents_buffer_size;
}

void Species::BuildTabledIndividualsHash(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash)
{
	// Here we rebuild a hash table for fast lookup of individuals table rows.
	// The key is the pedigree ID, so we can look up tabled individuals quickly; the value
	// is the index of that pedigree ID in the list of tabled individuals.  This code
	// used to live in AddNewIndividualsToTable(), building a temporary table; now it can
	// rebuild a permanent table (tabled_individuals_hash_), or make a temporary table
	// for local use.
	p_individuals_hash->clear();
	
	tsk_size_t num_rows = p_tables->individuals.num_rows;
	char *metadata_base = p_tables->individuals.metadata;
	tsk_size_t *metadata_offset = p_tables->individuals.metadata_offset;
	
	for (tsk_size_t individual_index = 0; individual_index < num_rows; individual_index++)
	{
		IndividualMetadataRec *metadata_rec = (IndividualMetadataRec *)(metadata_base + metadata_offset[individual_index]);
		slim_pedigreeid_t pedigree_id = metadata_rec->pedigree_id_;
		tsk_id_t tsk_individual = (tsk_id_t)individual_index;
		
		p_individuals_hash->emplace(pedigree_id, tsk_individual);
	}
}

struct edge_plus_time {
	double time;
	tsk_id_t parent, child;
	double left, right;
};

// This parallel sorter is basically a clone of _Eidos_ParallelQuicksort_ASCENDING() in eidos_sorting.inc
// The only difference (and the only reason we can't use that code directly) is we want to inline our comparator
#ifdef _OPENMP
static void _Eidos_ParallelQuicksort_ASCENDING(edge_plus_time *values, int64_t lo, int64_t hi, int64_t fallthrough)
{
	if (lo >= hi)
		return;
	
	if (hi - lo + 1 <= fallthrough) {
		// fall through to sorting with std::sort() below our threshold size
		std::sort(values + lo, values + hi + 1,
			[](const edge_plus_time &lhs, const edge_plus_time &rhs) {
				if (lhs.time == rhs.time) {
					if (lhs.parent == rhs.parent) {
						if (lhs.child == rhs.child) {
							return lhs.left < rhs.left;
						}
						return lhs.child < rhs.child;
					}
					return lhs.parent < rhs.parent;
				}
				return lhs.time < rhs.time;
			});
	} else {
		// choose the middle of three pivots, in an attempt to avoid really bad pivots
		edge_plus_time &pivot1 = *(values + lo);
		edge_plus_time &pivot2 = *(values + hi);
		edge_plus_time &pivot3 = *(values + ((lo + hi) >> 1));
		edge_plus_time pivot;
		
		// we just use times to choose the middle pivot; pivots with the same time probably won't be very
		// different in their sorted position anyway, except pathological models that record vast numbers
		// of edges in very few ticks; for those, this will revert to random-ish pivot choice (not so bad?)
		if (pivot1.time > pivot2.time)
		{
			if (pivot2.time > pivot3.time)		pivot = pivot2;
			else if (pivot1.time > pivot3.time)	pivot = pivot3;
			else								pivot = pivot1;
		}
		else
		{
			if (pivot1.time > pivot3.time)		pivot = pivot1;
			else if (pivot2.time > pivot3.time)	pivot = pivot3;
			else								pivot = pivot2;
		}
		
		// note that std::partition is not guaranteed to leave the pivot value in position
		// we do a second partition to exclude all duplicate pivot values, which seems to be one standard strategy
		// this works particularly well when duplicate values are very common; it helps avoid O(n^2) performance
		// note the partition is not parallelized; that is apparently a difficult problem for parallel quicksort
		edge_plus_time *middle1 = std::partition(values + lo, values + hi + 1, [pivot](const edge_plus_time& em) {
			//return em < pivot;
			if (em.time == pivot.time) {
				if (em.parent == pivot.parent) {
					if (em.child == pivot.child) {
						return em.left < pivot.left;
					}
					return em.child < pivot.child;
				}
				return em.parent < pivot.parent;
			}
			return em.time < pivot.time;
		});
		edge_plus_time *middle2 = std::partition(middle1, values + hi + 1, [pivot](const edge_plus_time& em) {
			//return !(pivot < em);
			if (pivot.time == em.time) {
				if (pivot.parent == em.parent) {
					if (pivot.child == em.child) {
						return !(pivot.left < em.left);
					}
					return !(pivot.child < em.child);
				}
				return !(pivot.parent < em.parent);
			}
			return !(pivot.time < em.time);
		});
		int64_t mid1 = middle1 - values;
		int64_t mid2 = middle2 - values;
		#pragma omp task default(none) firstprivate(values, lo, mid1, fallthrough)
		{ _Eidos_ParallelQuicksort_ASCENDING(values, lo, mid1 - 1, fallthrough); }	// Left branch
		#pragma omp task default(none) firstprivate(values, hi, mid2, fallthrough)
		{ _Eidos_ParallelQuicksort_ASCENDING(values, mid2, hi, fallthrough); }		// Right branch
	}
}
#endif

static int
slim_sort_edges(tsk_table_sorter_t *sorter, tsk_size_t start)
{
	// this is the same as slim_sort_edges_PARALLEL(), but the loops are not parallelized.
	// this is used for multi-chromosome models, since we then parallelize across chromosomes.
	// so this function can be run *inside* a parallel region, but does not make a parallel region.
	
	if (sorter->tables->edges.metadata_length != 0)
		throw std::invalid_argument("the sorter does not currently handle edge metadata");
	if (start != 0)
		throw std::invalid_argument("the sorter requires start==0");
	
	std::size_t num_rows = static_cast<std::size_t>(sorter->tables->edges.num_rows);
	//std::cout << num_rows << " edge table rows to be sorted" << std::endl;
	
	edge_plus_time *temp_edge_data = (edge_plus_time *)malloc(num_rows * sizeof(edge_plus_time));
	if (!temp_edge_data)
		EIDOS_TERMINATION << "ERROR (slim_sort_edges): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	tsk_edge_table_t *edges = &sorter->tables->edges;
	double *node_times = sorter->tables->nodes.time;
	
	// pre-sort: assemble the temp_edge_data vector
	for (tsk_size_t i = 0; i < num_rows; ++i)
	{
		temp_edge_data[i] = edge_plus_time{ node_times[edges->parent[i]], edges->parent[i], edges->child[i], edges->left[i], edges->right[i] };
	}
	
	// sort with std::sort
	std::sort(temp_edge_data, temp_edge_data + num_rows,
			  [](const edge_plus_time &lhs, const edge_plus_time &rhs) {
		if (lhs.time == rhs.time) {
			if (lhs.parent == rhs.parent) {
				if (lhs.child == rhs.child) {
					return lhs.left < rhs.left;
				}
				return lhs.child < rhs.child;
			}
			return lhs.parent < rhs.parent;
		}
		return lhs.time < rhs.time;
	});
	
	// post-sort: copy the sorted temp_edge_data vector back into the edge table
	for (std::size_t i = 0; i < num_rows; ++i)
	{
		edges->left[i] = temp_edge_data[i].left;
		edges->right[i] = temp_edge_data[i].right;
		edges->parent[i] = temp_edge_data[i].parent;
		edges->child[i] = temp_edge_data[i].child;
	}
	
	free(temp_edge_data);
	
	return 0;
}

#ifdef _OPENMP
static int
slim_sort_edges_PARALLEL(tsk_table_sorter_t *sorter, tsk_size_t start)
{
	// this is the same as slim_sort_edges(), but the loops are (potentially) parallelized.
	// this is used for single-chromosome models, to get some parallelization benefit.
	
	if (sorter->tables->edges.metadata_length != 0)
		throw std::invalid_argument("the sorter does not currently handle edge metadata");
	if (start != 0)
		throw std::invalid_argument("the sorter requires start==0");
	
	std::size_t num_rows = static_cast<std::size_t>(sorter->tables->edges.num_rows);
	//std::cout << num_rows << " edge table rows to be sorted" << std::endl;
	
	edge_plus_time *temp_edge_data = (edge_plus_time *)malloc(num_rows * sizeof(edge_plus_time));
	if (!temp_edge_data)
		EIDOS_TERMINATION << "ERROR (slim_sort_edges_PARALLEL): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	tsk_edge_table_t *edges = &sorter->tables->edges;
	double *node_times = sorter->tables->nodes.time;
	
	// pre-sort: assemble the temp_edge_data vector
	{
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_SIMPLIFY_SORT_PRE);
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_SIMPLIFY_SORT_PRE);
#pragma omp parallel for schedule(static) default(none) shared(num_rows, temp_edge_data, edges, node_times) if(num_rows >= EIDOS_OMPMIN_SIMPLIFY_SORT_PRE) num_threads(thread_count)
		for (tsk_size_t i = 0; i < num_rows; ++i)
		{
			temp_edge_data[i] = edge_plus_time{ node_times[edges->parent[i]], edges->parent[i], edges->child[i], edges->left[i], edges->right[i] };
		}
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_SIMPLIFY_SORT_PRE);
	}
	
	// sort with std::sort when not running parallel, or if the task is small;
	// sort in parallel for big tasks if we can; see Eidos_ParallelSort() which
	// this is patterned after, but we want the (faster) inlined comparator...
	{
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_SIMPLIFY_SORT);
		
#ifdef _OPENMP
		if (num_rows >= EIDOS_OMPMIN_SIMPLIFY_SORT)
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_SIMPLIFY_SORT);
#pragma omp parallel default(none) shared(num_rows, temp_edge_data) num_threads(thread_count)
			{
				// We fall through to using std::sort when below a threshold interval size.
				// The larger the threshold, the less time we spend thrashing tasks on small
				// intervals, which is good; but it also sets a limit on how many threads we
				// we bring to bear on relatively small sorts, which is bad.  We try to
				// calculate the optimal fall-through heuristically here; basically we want
				// to subdivide with tasks enough that the workload is shared well, and then
				// do the rest of the work with std::sort().  The more threads there are,
				// the smaller we want to subdivide.
				int64_t fallthrough = num_rows / (EIDOS_FALLTHROUGH_FACTOR * omp_get_num_threads());
				
				if (fallthrough < 1000)
					fallthrough = 1000;
				
#pragma omp single nowait
				{
					_Eidos_ParallelQuicksort_ASCENDING(temp_edge_data, 0, num_rows - 1, fallthrough);
				}
			} // End of parallel region
			
			goto didParallelSort;
		}
#endif
		
		std::sort(temp_edge_data, temp_edge_data + num_rows,
				  [](const edge_plus_time &lhs, const edge_plus_time &rhs) {
			if (lhs.time == rhs.time) {
				if (lhs.parent == rhs.parent) {
					if (lhs.child == rhs.child) {
						return lhs.left < rhs.left;
					}
					return lhs.child < rhs.child;
				}
				return lhs.parent < rhs.parent;
			}
			return lhs.time < rhs.time;
		});
		
#ifdef _OPENMP
		// If we did a parallel sort, we jump here to skip the single-threaded sort
	didParallelSort:
#endif
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_SIMPLIFY_SORT);
	}
	
	// post-sort: copy the sorted temp_edge_data vector back into the edge table
	{
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_SIMPLIFY_SORT_POST);
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_SIMPLIFY_SORT_POST);
#pragma omp parallel for schedule(static) default(none) shared(num_rows, temp_edge_data, edges) if(num_rows >= EIDOS_OMPMIN_SIMPLIFY_SORT_POST) num_threads(thread_count)
		for (std::size_t i = 0; i < num_rows; ++i)
		{
			edges->left[i] = temp_edge_data[i].left;
			edges->right[i] = temp_edge_data[i].right;
			edges->parent[i] = temp_edge_data[i].parent;
			edges->child[i] = temp_edge_data[i].child;
		}
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_SIMPLIFY_SORT_POST);
	}
	
	free(temp_edge_data);
	
	return 0;
}
#endif

void Species::_SimplifyTreeSequence(TreeSeqInfo &tsinfo, const std::vector<tsk_id_t> &samples)
{
	// BEWARE!  This is an internal method, and should only be called from SimplifyAllTreeSequences()!
	// It assumes that a variety of things will be done by the caller, and those things are not optional!
	// With multiple chromosomes when running parallel, this will be called from inside a parallel region!
	
	// sort the table collection
	{
		tsk_flags_t flags = TSK_NO_CHECK_INTEGRITY;
#if DEBUG
		// in DEBUG mode, we do a standard consistency check for tree-seq integrity after each simplify; unlike in
		// CheckTreeSeqIntegrity(), this does not need TSK_NO_CHECK_POPULATION_REFS since we have a valid population table
		// we don't need/want order checks for the tables, since we sort them here; if that doesn't do the right thing,
		// that would be a bug in tskit, and would be caught by their tests, presumably, so no point in wasting time on it...
		flags = 0;
#endif
		
#if 0
		// sort the tables using tsk_table_collection_sort() to get the default behavior
		int ret = tsk_table_collection_sort(&tsinfo.tables_, /* edge_start */ NULL, /* flags */ flags);
		if (ret < 0) handle_error("tsk_table_collection_sort", ret);
#else
		// sort the tables using our own custom edge sorter, for additional speed through inlining of the comparison function
		// see https://github.com/tskit-dev/tskit/pull/627, https://github.com/tskit-dev/tskit/pull/711
		// FIXME for additional speed we could perhaps be smart about only sorting the portions of the edge table
		// that need it, but the tricky thing is that all the old stuff has to be at the bottom of the table, not the top...
		tsk_table_sorter_t sorter;
		int ret = tsk_table_sorter_init(&sorter, &tsinfo.tables_, /* flags */ flags);
		if (ret != 0) handle_error("tsk_table_sorter_init", ret);
		
#ifdef _OPENMP
		// When running multithreaded, we can parallelize the sorting work.  We do so only for single-chromosome models,
		// however.  With multiple chromosomes we parallelize across chromosomes, allowing simplification in parallel too.
		if (chromosomes_.size() > 1)
			sorter.sort_edges = slim_sort_edges;
		else
			sorter.sort_edges = slim_sort_edges_PARALLEL;
#else
		sorter.sort_edges = slim_sort_edges;
#endif	// _OPENMP
		
		try {
			ret = tsk_table_sorter_run(&sorter, NULL);
		} catch (std::exception &e) {
			EIDOS_TERMINATION << "ERROR (Species::_SimplifyTreeSequence): (internal error) exception raised during tsk_table_sorter_run(): " << e.what() << "." << EidosTerminate();
		}
		if (ret != 0) handle_error("tsk_table_sorter_run", ret);
		
		tsk_table_sorter_free(&sorter);
		if (ret != 0) handle_error("tsk_table_sorter_free", ret);
#endif
	}
	
	// remove redundant sites we added
	{
		int ret = tsk_table_collection_deduplicate_sites(&tsinfo.tables_, 0);
		if (ret < 0) handle_error("tsk_table_collection_deduplicate_sites", ret);
	}
	
	// simplify
	{
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_SIMPLIFY_CORE);
		
		// BCH 12/9/2024: Removing TSK_SIMPLIFY_FILTER_INDIVIDUALS here, because we now need to filter the individuals
		// table ourselves after simplifying all the tree sequences (perhaps in parallel); see SimplifyAllTreeSequences().
		tsk_flags_t flags = TSK_SIMPLIFY_FILTER_SITES | TSK_SIMPLIFY_KEEP_INPUT_ROOTS;
		
		// BCH 12/10/2024: This should still work, with our own node table filtering code.  As Jerome explains, "simplify
		// will still keep the *edges* that are unary, and that's all that matters. The downstream filtering code you
		// have just looks to see what nodes have references, and filters out those that are not used in any edges."
		// BCH 3/13/2025: changing TSK_SIMPLIFY_KEEP_UNARY to TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS,
		// since it is the correct flag; see discussion in https://github.com/MesserLab/SLiM/issues/487
		if (!retain_coalescent_only_) flags |= TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS;
		
		// BCH 12/9/2024: These flags are added for multichromosome support; we want to simplify all the tree sequences
		// (perhaps in parallel), without touching the node table at all, and then we clean up the node table afterwards.
		flags |= TSK_SIMPLIFY_NO_FILTER_NODES | TSK_SIMPLIFY_NO_UPDATE_SAMPLE_FLAGS;
		
		int ret = tsk_table_collection_simplify(&tsinfo.tables_, samples.data(), (tsk_size_t)samples.size(), flags, NULL);
		if (ret != 0) handle_error("tsk_table_collection_simplify", ret);
		
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_SIMPLIFY_CORE);
	}
	
	// note that we leave things in a partially completed state; the nodes and individuals tables still
	// need to be filtered!  that is the responsibility of the caller -- i.e., SimplifyAllTreeSequences().
}

void Species::SimplifyAllTreeSequences(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::SimplifyAllTreeSequences): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// if we have no recorded nodes, there is nothing to simplify; note that the nodes table is shared
	if (treeseq_[0].tables_.nodes.num_rows == 0)
		return;
	
	std::vector<tsk_id_t> samples;
	
	// BCH 7/27/2019: We now build a hash table containing all of the entries of remembered_nodes_,
	// so that the find() operations in the loop below can be done in constant time instead of O(N) time.
	// We need to be able to find out the index of an entry, in remembered_nodes_, once we have found it;
	// that is what the mapped value provides, whereas the key value is the tsk_id_t we need to find below.
	// We do all this inside a block so the map gets deallocated as soon as possible, to minimize footprint.
	// BCH 12/9/2024: The point of all this kerfuffle with the lookup table is that an extant individual
	// might also be a remembered individual, and we don't want to put it into the samples vector twice,
	// I think; otherwise we could just throw the remembered nodes and extant individuals into `samples`
	// with no lookup table complication.
	{
#if EIDOS_ROBIN_HOOD_HASHING
		robin_hood::unordered_flat_map<tsk_id_t, uint32_t> remembered_nodes_lookup;
		//typedef robin_hood::pair<tsk_id_t, uint32_t> MAP_PAIR;
#elif STD_UNORDERED_MAP_HASHING
		std::unordered_map<tsk_id_t, uint32_t> remembered_nodes_lookup;
		//typedef std::pair<tsk_id_t, uint32_t> MAP_PAIR;
#endif
		
		// the remembered_nodes_ come first in the list of samples
		uint32_t index = 0;
		
		for (tsk_id_t sample_id : remembered_nodes_)
		{
			samples.emplace_back(sample_id);
			remembered_nodes_lookup.emplace(sample_id, index);
			index++;
		}
		
		// and then come all the nodes of the extant individuals
		for (auto subpop_iter : population_.subpops_)
		{
			Subpopulation *subpop = subpop_iter.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				// all the haplosomes for an individual share the same two tskit node ids (shared node table)
				// since both nodes for an individual are always remembered together, we only need to do
				// one hash table lookup to determine whether this individual's haplosomes are remembered
				tsk_id_t tsk_node_id_base = ind->TskitNodeIdBase();
				
				auto iter = remembered_nodes_lookup.find(tsk_node_id_base);
				bool not_remembered = (iter == remembered_nodes_lookup.end());
				
				if (not_remembered)
				{
					samples.emplace_back(tsk_node_id_base);
					samples.emplace_back(tsk_node_id_base + 1);
				}
				
#if DEBUG
				// check that both of the individual's haplosomes are (or are not) remembered together
				auto iter_2 = remembered_nodes_lookup.find(tsk_node_id_base + 1);
				bool not_remembered_2 = (iter_2 == remembered_nodes_lookup.end());
				
				if (not_remembered != not_remembered_2)
					EIDOS_TERMINATION << "ERROR (Species::SimplifyAllTreeSequences): one node remembered, one node not!." << EidosTerminate(nullptr);
#endif
			}
		}
	}
	
	// the tables need to have a population table to be able to sort it; we make this in index 0's table
	// collection, and the other table collections will share it temporarily using CopySharedTablesIn()
	tsk_table_collection_t &main_tables = treeseq_[0].tables_;
	
	WritePopulationTable(&main_tables);
	
	// simplify all of the tree sequences
	// FIXME MULTICHROM: parallelize simplification here!
	for (Chromosome *chromosome : chromosomes_)
	{
		slim_chromosome_index_t chromosome_index = chromosome->Index();
		TreeSeqInfo &chromosome_tsinfo = treeseq_[chromosome_index];
		tsk_table_collection_t &chromosome_tables = chromosome_tsinfo.tables_;
		
		// swap in the shared tables from the main tree sequence; we need them for simplify to work, but
		// simplify should not touch any of them, so it should be safe to simplify using them directly
		if (chromosome_index > 0)
			CopySharedTablesIn(chromosome_tables);
		
		// simplify
		_SimplifyTreeSequence(chromosome_tsinfo, samples);
		
		// swap out the shared tables immediately after; the filtering code below does not need the shared tables
		if (chromosome_index > 0)
			DisconnectCopiedSharedTables(chromosome_tables);
	}
	
	// the node table needs to be filtered now; we turned that off for simplification, so it could be parallelized.
	// this code is copied from https://github.com/tskit-dev/tskit/pull/2665/files (multichrom_wright_fisher.c)
	const tsk_size_t num_nodes = main_tables.nodes.num_rows;
	
	if (num_nodes > 0)
	{
		tsk_size_t sample_count = (tsk_size_t)samples.size();
		int ret;
		tsk_bool_t *keep_nodes = (tsk_bool_t *)calloc(num_nodes, sizeof(tsk_bool_t));	// note: cleared by calloc
		tsk_id_t *node_id_map = (tsk_id_t *)malloc(num_nodes * sizeof(tsk_id_t));
		
		if (!keep_nodes || !node_id_map)
			EIDOS_TERMINATION << "ERROR (Species::SimplifyAllTreeSequences): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		// mark the nodes we want to keep: samples (including remembered nodes), plus all nodes referenced by edges
		for (tsk_size_t j = 0; j < sample_count; j++)
			keep_nodes[samples[j]] = true;

		// update the 'sample' flags on the nodes (which simplify didn't update because we used the NO_FILTER_NODES flag)
		for (tsk_size_t j = 0; j < num_nodes; j++)
			main_tables.nodes.flags[j] &= (~TSK_NODE_IS_SAMPLE);
		
		for (tsk_size_t j = 0; j < sample_count; j++)
			main_tables.nodes.flags[samples[j]] |= TSK_NODE_IS_SAMPLE;
		
		for (Chromosome *chromosome : chromosomes_)
		{
			slim_chromosome_index_t chromosome_index = chromosome->Index();
			TreeSeqInfo &chromosome_tsinfo = treeseq_[chromosome_index];
			tsk_table_collection_t &chromosome_tables = chromosome_tsinfo.tables_;
			tsk_id_t *edges_child = chromosome_tables.edges.child;
			tsk_id_t *edges_parent = chromosome_tables.edges.parent;
			tsk_size_t edges_num_rows = chromosome_tables.edges.num_rows;
			
			for (tsk_size_t k = 0; k < edges_num_rows; k++) {
				keep_nodes[edges_child[k]] = true;
				keep_nodes[edges_parent[k]] = true;
			}
		}
		
		// tskit does the work for us and provides an index map
		ret = tsk_node_table_keep_rows(&main_tables.nodes, keep_nodes, 0, node_id_map);
		if (ret < 0) handle_error("tsk_node_table_keep_rows", ret);
		
		// remap node references
		for (Chromosome *chromosome : chromosomes_)
		{
			slim_chromosome_index_t chromosome_index = chromosome->Index();
			TreeSeqInfo &chromosome_tsinfo = treeseq_[chromosome_index];
			tsk_table_collection_t &chromosome_tables = chromosome_tsinfo.tables_;
			
			// remap in the edges table
			tsk_id_t *edges_child = chromosome_tables.edges.child;
			tsk_id_t *edges_parent = chromosome_tables.edges.parent;
			tsk_size_t edges_num_rows = chromosome_tables.edges.num_rows;
			
			for (tsk_size_t k = 0; k < edges_num_rows; k++) {
				edges_child[k] = node_id_map[edges_child[k]];
				edges_parent[k] = node_id_map[edges_parent[k]];
			}
			
			// remap in the mutations table also; Jerome's example didn't have mutations so it didn't do this
			tsk_id_t *mutations_node = chromosome_tables.mutations.node;
			tsk_size_t mutations_num_rows = chromosome_tables.mutations.num_rows;
			
			for (tsk_size_t k = 0; k < mutations_num_rows; k++) {
				tsk_id_t remapped_id = node_id_map[mutations_node[k]];
				
				// Peter says: You might also think we need to loop through the mutation table to add nodes that are
				// referred to there to keep_nodes, but I don't think that's true - we should be able to assert
				// node_id_map[mutations_node[k]] >= 0. (it'll be -1 if the node has been removed).  So, doing that.
				assert(remapped_id >= 0);
				
				mutations_node[k] = remapped_id;
			}
			
#if DEBUG
			// BCH 2/25/2025: We need to swap in the shared tables around the integrity check
			if (chromosome_index > 0)
				CopySharedTablesIn(chromosome_tables);
			
			ret = tsk_table_collection_check_integrity(&chromosome_tables, 0);
			if (ret < 0) handle_error("SimplifyAllTreeSequences() tsk_table_collection_check_integrity after node remapping", ret);
			
			if (chromosome_index > 0)
				DisconnectCopiedSharedTables(chromosome_tables);
#endif
		}
		
		// update map of remembered_nodes_; with a single chromosome and a standard simplify,
		// they would now be the first n entries in the node table (and we used to assume that),
		// but now that is not guaranteed, and we need to remap them using node_id_map
		for (tsk_id_t j = 0; j < (tsk_id_t)remembered_nodes_.size(); j++)
			remembered_nodes_[j] = node_id_map[samples[j]];
		
		// and update the tskit node id base for all extant individuals, similarly
		for (auto subpop_iter : population_.subpops_)
		{
			Subpopulation *subpop = subpop_iter.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				// all the haplosomes for an individual share the same two tskit node ids (shared node table)
				// we thus need to just remap the base id, and the second id should always remap with it
				tsk_id_t tsk_node_id_base = ind->TskitNodeIdBase();
				tsk_id_t remapped_base = node_id_map[tsk_node_id_base];
				
				ind->SetTskitNodeIdBase(remapped_base);
				
#if DEBUG
				// check that the second id did remap alongside the first id
				if (node_id_map[tsk_node_id_base + 1] != remapped_base + 1)
					EIDOS_TERMINATION << "ERROR (Species::SimplifyAllTreeSequences): node table filtering did not preserve order!" << EidosTerminate(nullptr);
#endif
			}
		}
		
		free(keep_nodes);
		free(node_id_map);
	}
	
	// the individual table needs to be filtered now; we no longer pass TSK_SIMPLIFY_FILTER_INDIVIDUALS for simplification,
	// so it could be parallelized.  The code here is based on the node table filtering above, mutatis mutandis
	const tsk_size_t num_individuals = main_tables.individuals.num_rows;
	
	if (num_individuals > 0)
	{
		int ret;
		tsk_bool_t *keep_individuals = (tsk_bool_t *)calloc(num_individuals, sizeof(tsk_bool_t));	// note: cleared by calloc
		tsk_id_t *individual_id_map = (tsk_id_t *)malloc(num_individuals * sizeof(tsk_id_t));
		
		if (!keep_individuals || !individual_id_map)
			EIDOS_TERMINATION << "ERROR (Species::SimplifyAllTreeSequences): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		// mark the individuals we want to keep: all individuals referenced by nodes; note that the node table is shared,
		// so we only need to loop through that one shared node table that is kept by the main table collection
		{
			tsk_id_t *nodes_individual = main_tables.nodes.individual;
			tsk_size_t nodes_num_rows = main_tables.nodes.num_rows;
			
			for (tsk_size_t k = 0; k < nodes_num_rows; k++) {
				tsk_id_t individual_index = nodes_individual[k];
				
				if (individual_index != TSK_NULL)
					keep_individuals[individual_index] = true;
			}
		}
		
		// tskit does the work for us and provides an index map
		ret = tsk_individual_table_keep_rows(&main_tables.individuals, keep_individuals, 0, individual_id_map);
		if (ret < 0) handle_error("tsk_individual_table_keep_rows", ret);
		
		// remap individual references; again, this is a shared table so we only need to modify it for the main tables
		{
			tsk_id_t *nodes_individual = main_tables.nodes.individual;
			tsk_size_t nodes_num_rows = main_tables.nodes.num_rows;
			
			for (tsk_size_t k = 0; k < nodes_num_rows; k++) {
				tsk_id_t individual_index = nodes_individual[k];
				
				if (individual_index != TSK_NULL)
					nodes_individual[k] = individual_id_map[individual_index];
			}
			
#if DEBUG
			// BCH 2/25/2025: We don't need to swap in the shared tables, because this call is only on main_tables
			ret = tsk_table_collection_check_integrity(&main_tables, 0);
			if (ret < 0) handle_error("SimplifyAllTreeSequences() tsk_table_collection_check_integrity after individual remapping", ret);
#endif
		}
		
		free(keep_individuals);
		free(individual_id_map);
		
		// remake our hash table of pedigree ids to tsk_ids, since we have reordered the (shared) individuals table
		BuildTabledIndividualsHash(&main_tables, &tabled_individuals_hash_);
	}
	
	// note that simplify does not mess with the population table, at least with the flags we pass it,
	// so we don't need to filter it as we filtered the node and individual tables above
	
	// reset current position, used to rewind individuals that are rejected by modifyChild()
	RecordTablePosition();
	
	// and reset our elapsed time since last simplification, for auto-simplification
	simplify_elapsed_ = 0;
	
	// as a side effect of simplification, update a "model has coalesced" flag that the user can consult, if requested
	// this could potentially be parallelized, but it's kind of a fringe feature, and not that slow...
	if (running_coalescence_checks_)
	{
		for (TreeSeqInfo &tsinfo : treeseq_)
		{
			// BCH 2/25/2025: Copy shared tables in across the coalescence check
			if (tsinfo.chromosome_index_ != 0)
				CopySharedTablesIn(tsinfo.tables_);
			
			CheckCoalescenceAfterSimplification(tsinfo);
			
			if (tsinfo.chromosome_index_ != 0)
				DisconnectCopiedSharedTables(tsinfo.tables_);
		}
	}
}

void Species::CheckCoalescenceAfterSimplification(TreeSeqInfo &tsinfo)
{
#if DEBUG
	if (!recording_tree_ || !running_coalescence_checks_)
		EIDOS_TERMINATION << "ERROR (Species::CheckCoalescenceAfterSimplification): (internal error) coalescence check called with recording or checking off." << EidosTerminate();
#endif
	
	// Note that this method assumes that tsinfo has had the shared tables copied in!
	
	// Copy the table collection, which will (if it is not the main table collection) have empty tables for
	// the shared node, individual, and population tables.  We copy *first*, because we don't want to make
	// a copy of the shared tables, we just want to share them at the pointer level.  (Jerome said at one
	// point that this copy is unnecessary since tsk_table_collection_build_index() does not modify the core
	// information in the table collection, but just adds some separate indices.  However, we also need to
	// add a population table, so really it is best to make a copy I think.)
	tsk_table_collection_t tables_copy;
	int ret;
	
	ret = tsk_table_collection_copy(&tsinfo.tables_, &tables_copy, 0);
	if (ret < 0) handle_error("tsk_table_collection_copy", ret);
	
	// If tsinfo is not the main table collection (which has the shared tables), copy the shared tables in now.
	// If it is the main table collection, it now has a deep copy of the population table, so it is fine.
	if (tsinfo.chromosome_index_ > 0)
	{
		CopySharedTablesIn(tables_copy);
		
		// Now we have a pointer-level copy of the main table collection's population table; if we modify it,
		// which we need to do, we would actually modify the original table in the main table collection,
		// which we don't want.  So now we make a deep copy of it that we can modify safely.  We own that
		// deep copy, and will free it at the end.
		tsk_population_table_t deep_copy_pop_table;
		
		ret = tsk_population_table_copy(&tables_copy.populations, &deep_copy_pop_table, 0);
		if (ret < 0) handle_error("tsk_population_table_copy", ret);
		
		tables_copy.populations = deep_copy_pop_table;		// overwrite with the copy
	}
	
	// Our tables copy needs to have a population table now, since this is required to build a tree sequence.
	// We could build this once and reuse it across all the calls to this method for different chromosomes,
	// but I think's probably not worth the trouble; the overhead should be small.
	WritePopulationTable(&tables_copy);
	
	// Build an index (which does not modify the main tables) and make a tree sequence.
	ret = tsk_table_collection_build_index(&tables_copy, 0);
	if (ret < 0) handle_error("tsk_table_collection_build_index", ret);
	
	tsk_treeseq_t ts;
	
	ret = tsk_treeseq_init(&ts, &tables_copy, 0);
	if (ret < 0) handle_error("tsk_treeseq_init", ret);
	
	// Collect a vector of all extant haplosome node IDs belonging to the chromosome that tsinfo records
	int first_haplosome_index = FirstHaplosomeIndices()[tsinfo.chromosome_index_];
	int last_haplosome_index = LastHaplosomeIndices()[tsinfo.chromosome_index_];
	std::vector<tsk_id_t> all_extant_nodes;
	
	for (auto subpop_iter : population_.subpops_)
	{
		Subpopulation *subpop = subpop_iter.second;
		
		for (Individual *ind : subpop->parent_individuals_)
		{
			// all the haplosomes for an individual share the same two tskit node ids (shared node table)
			// we only want to trace back from haplosomes that are used by the focal chromosome, however;
			// and only from haplosomes that are non-null (a test which was missing before, a bug I think)
			tsk_id_t tsk_node_id_base = ind->TskitNodeIdBase();
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				if (!haplosome->IsNull())
				{
					// the tskit node id for a haplosome is the base ID from the individual, plus 0 or 1
					all_extant_nodes.emplace_back(tsk_node_id_base + haplosome->chromosome_subposition_);
				}
			}
		}
	}
	
	int64_t extant_node_count = (int64_t)all_extant_nodes.size();
	
	// Iterate through the trees to check coalescence; this is a bit tricky because of keeping first-gen nodes and nodes
	// in remembered individuals.  We use the sparse tree's "tracked samples" feature, tracking extant individuals
	// only, to find out whether all extant individuals are under a single root (coalesced), or under multiple roots
	// (not coalesced).  Doing this requires a scan through all the roots at each site, which is very slow if we have
	// indeed coalesced, but if we are far from coalescence we will usually be able to determine that in the scan of the
	// first tree (because every site will probably be uncoalesced), which seems like the right performance trade-off.
	tsk_tree_t t;
	bool fully_coalesced = true;
	
	tsk_tree_init(&t, &ts, 0);
	if (ret < 0) handle_error("tsk_tree_init", ret);
	
	tsk_tree_set_tracked_samples(&t, extant_node_count, all_extant_nodes.data());
	if (ret < 0) handle_error("tsk_tree_set_tracked_samples", ret);
	
	ret = tsk_tree_first(&t);
	if (ret < 0) handle_error("tsk_tree_first", ret);
	
	for (; (ret == 1) && fully_coalesced; ret = tsk_tree_next(&t))
	{
#if 0
		// If we didn't keep first-generation lineages, or remember haplosomes, >1 root would mean not coalesced
		if (tsk_tree_get_num_roots(&t) > 1)
		{
			fully_coalesced = false;
			break;
		}
#else
		// But we do have retained/remembered nodes in the tree, so we need to be smarter; nodes for the first gen
		// ancestors will always be present, giving >1 root in each tree even when we have coalesced, and the
		// remembered individuals may mean that more than one root node has children, too, even when we have
		// coalesced.  What we need to know is: how many roots are there that have >0 *extant* children?  This
		// is what we use the tracked samples for; they are extant individuals.
		for (tsk_id_t root = tsk_tree_get_left_root(&t); root != TSK_NULL; root = t.right_sib[root])
		{
			int64_t num_tracked = t.num_tracked_samples[root];
			
			if ((num_tracked > 0) && (num_tracked < extant_node_count))
			{
				fully_coalesced = false;
				break;
			}
		}
#endif
	}
	if (ret < 0) handle_error("tsk_tree_next", ret);
	
	ret = tsk_tree_free(&t);
	if (ret < 0) handle_error("tsk_tree_free", ret);
	
	ret = tsk_treeseq_free(&ts);
	if (ret < 0) handle_error("tsk_treeseq_free", ret);
	
	if (tsinfo.chromosome_index_ > 0)
	{
		// we made a new deep copy of the population table above, so we need to free it before disconnecting
		ret = tsk_population_table_free(&tables_copy.populations);
		if (ret < 0) handle_error("tsk_population_table_free", ret);
		
		// now we can disconnect, zeroing out the other shared tables that we made pointer-level copies of
		DisconnectCopiedSharedTables(tables_copy);
	}
	
	ret = tsk_table_collection_free(&tables_copy);
	if (ret < 0) handle_error("tsk_table_collection_free", ret);
	
	//std::cout << "tick " << community->Tick() << ": fully_coalesced == " << (fully_coalesced ? "TRUE" : "false") << std::endl;
	tsinfo.last_coalescence_state_ = fully_coalesced;
}

bool Species::_SubpopulationIDInUse(slim_objectid_t p_subpop_id)
{
	// Called by Community::SubpopulationIDInUse(); do not call directly!
	
	// This checks the tree-sequence population table, if there is one. We'll
	// assume that *any* metadata means we can't use the subpop, which means we
	// won't clobber any existing metadata, although there might be subpops
	// with metadata not put in by SLiM.
	if (RecordingTreeSequence() && treeseq_.size())
	{
		// We only need to consult the first (shared) populations table
		tsk_population_table_t &shared_populations_table = treeseq_[0].tables_.populations;
		
		if (p_subpop_id < (int)shared_populations_table.num_rows) {
			int ret;
			tsk_population_t row;

			ret = tsk_population_table_get_row(&shared_populations_table, p_subpop_id, &row);
			if (ret != 0) handle_error("tsk_population_table_get_row", ret);
			if (row.metadata_length > 0) {
				// Check the metadata is not "null". It would maybe be better
				// to parse the metadata, though.
				if ((row.metadata_length != 4) || (strncmp(row.metadata, "null", 4) != 0)) {
					return true;
				}
			}
		}
	}
	
	return false;
}

void Species::RecordTablePosition(void)
{
	// keep the current position in each table collection for rewinding if a proposed child is rejected
	// note that for freed tables (because of table sharing), this will record/restore a position of 0
	for (TreeSeqInfo &tsinfo : treeseq_)
		tsk_table_collection_record_num_rows(&tsinfo.tables_, &tsinfo.table_position_);
}

void Species::AllocateTreeSequenceTables(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::AllocateTreeSequenceTables): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	if (tables_initialized_)
		EIDOS_TERMINATION << "ERROR (Species::AllocateTreeSequenceTables): (internal error) tree sequence tables already initialized." << EidosTerminate();
	
	// Set up the table collections before loading a saved population or starting a simulation
	// We have one TreeSeqInfo struct for each chromosome, and allocate and initialize them all here
	treeseq_.resize(chromosomes_.size());
	
	bool first = true;
	for (Chromosome *chromosome : chromosomes_)
	{
		slim_chromosome_index_t index = chromosome->Index();
		TreeSeqInfo &tsinfo = treeseq_[index];
		
		//INITIALIZE NODE AND EDGE TABLES.
		int ret = tsk_table_collection_init(&tsinfo.tables_, TSK_TC_NO_EDGE_METADATA);
		if (ret != 0) handle_error("AllocateTreeSequenceTables()", ret);
		
		if (!first)
		{
			// the node, individual, and population tables are shared; only the first TreeSeqInfo
			// contains them at most times, and the tables are shared with the others when needed
			// attempting to access these freed tables will probably crash, beware
			tsk_node_table_free(&tsinfo.tables_.nodes);
			tsk_individual_table_free(&tsinfo.tables_.individuals);
			tsk_population_table_free(&tsinfo.tables_.populations);
		}
		
		tsinfo.tables_.sequence_length = (double)chromosome->last_position_ + 1;
		tsinfo.chromosome_index_ = chromosome->Index();
		tsinfo.last_coalescence_state_ = false;
		
		first = false;
	}
	
	RecordTablePosition();
	tables_initialized_ = true;
}

void Species::SetCurrentNewIndividual(__attribute__((unused))Individual *p_individual)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::SetCurrentNewIndividual): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This is called by code where new individuals are created
	
	// Remember the new individual being defined; we don't need this right now,
	// but it seems to keep coming back, so I've kept the code for it...
	//current_new_individual_ = p_individual;
	
	// Remember the current table position so we can return to it later in RetractNewIndividual()
	RecordTablePosition();
	
	// Record the usage of the next two node table entries for this individual, for (up to) two
	// haplosomes in each tree sequence.  Some chromosomes will involve only one haplosome, because
	// they are haploid; and sometimes null haplosomes will mean that fewer (or none) of these
	// node table entries will actually be used.  That's OK; we want to use the same node table
	// entries for a given individual in every tree sequences, so we (in general) have to reserve
	// two entries in any case, and tskit will ignore the ones we don't use.  Note that this work
	// used to be done in RecordNewHaplosome(), but it needs to be done just once for each new
	// individual, whereas RecordNewHaplosome() has to record each new haplosome created.
	
	// Add haplosome nodes; we mark all nodes with TSK_NODE_IS_SAMPLE here because we have full
	// genealogical information on all of them (until simplify, which clears TSK_NODE_IS_SAMPLE
	// from nodes that are not kept in the sample).
	double time = (double) -1 * (community_.tree_seq_tick_ + community_.tree_seq_tick_offset_);	// see Population::AddSubpopulationSplit() regarding tree_seq_tick_offset_
	tsk_flags_t flags = TSK_NODE_IS_SAMPLE;
	tsk_node_table_t &shared_node_table = treeseq_[0].tables_.nodes;
	
	// Figure out the metadata to use, which is a version of the default metadata.  We patch in
	// the correct haplosome pedigree IDs, directly into the default metadata records, so
	// this code is not thread-safe!  The design is this way because the size of HaplosomeMetadataRec
	// is determined dynamically at runtime, depending on the number of chromosomes in the model.
	// (If we want this to run in parallel across chromosomes eventually, we could keep separate
	// copies of the default haplosome metadata for each chromosome, to make this thread-safe...)
	THREAD_SAFETY_IN_ACTIVE_PARALLEL();
	static_assert(sizeof(HaplosomeMetadataRec) == 9, "HaplosomeMetadataRec has changed size; this code probably needs to be updated");
	HaplosomeMetadataRec *metadata1, *metadata2;
	
	if (p_individual->sex_ == IndividualSex::kMale)
	{
		// this case covers only males
		metadata1 = hap_metadata_1M_;
		metadata2 = hap_metadata_2M_;
	}
	else
	{
		// this case covers both females and hermaphrodites
		metadata1 = hap_metadata_1F_;
		metadata2 = hap_metadata_2F_;
	}
	
	metadata1->haplosome_id_ = p_individual->PedigreeID() * 2;
	metadata2->haplosome_id_ = p_individual->PedigreeID() * 2 + 1;
	
	// Make the node table entries, with default metadata for now
	tsk_id_t nodeTSKID1 = tsk_node_table_add_row(&shared_node_table, flags, time,
												 (tsk_id_t)p_individual->subpopulation_->subpopulation_id_,
												 TSK_NULL, (char *)metadata1, (tsk_size_t)haplosome_metadata_size_);
	if (nodeTSKID1 < 0) handle_error("tsk_node_table_add_row", nodeTSKID1);
	
	tsk_id_t nodeTSKID2 = tsk_node_table_add_row(&shared_node_table, flags, time,
												 (tsk_id_t)p_individual->subpopulation_->subpopulation_id_,
												 TSK_NULL, (char *)metadata2, (tsk_size_t)haplosome_metadata_size_);
	if (nodeTSKID2 < 0) handle_error("tsk_node_table_add_row", nodeTSKID2);
	
	// The individual remembers the tskid of the first node (which is the same across all haplosomes
	// in 1st position).  For haplosomes in 2nd position, it is first_tsk_node_id + 1.
	p_individual->SetTskitNodeIdBase(nodeTSKID1);
	
	// The haplosome metadata is presently all zero.  FinalizeCurrentNewIndividual() will clean it up.
}

void Species::RetractNewIndividual()
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::RetractNewIndividual): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This is called when a new child, introduced by SetCurrentNewIndividual(), gets rejected by a modifyChild()
	// callback.  We will have logged recombination breakpoints and new mutations into our tables, and now want
	// to back those changes out by re-setting the active row index for the tables.
	
	// We presently don't use current_new_individual_ any more, but I've kept
	// around the code since it seems to keep coming back...
	//current_new_individual_ = nullptr;
	
	size_t trees_count = treeseq_.size();
	
	if (trees_count > 0)
	{
		TreeSeqInfo &tsinfo_0 = treeseq_[0];
		
		// BCH 12/1/2025: The base table collection can restore its bookmarked position directly;
		// that will reset the bookmarked positions in all of the shared tables as well.
		tsk_table_collection_truncate(&tsinfo_0.tables_, &tsinfo_0.table_position_);
		
		// BCH 12/1/2025: In the multichrom case we need to protect against a segfault inside
		// tsk_table_collection_truncate() for the secondary table collections.  This is because
		// they have NULL for their various column pointers, and tsk_table_collection_truncate()
		// accesses index 0 of every offset column to get the offset for row 0.  (It is always
		// for row 0 in the shared tables because they are zeroed out; their num_rows was zero
		// in RecordTablePosition().)  See https://github.com/MesserLab/SLiM/issues/579 for details.
		// BEWARE: This code will need updating if new shared tables are added, or new columns
		// are added within the existing shared table.  Any offset column that is accessed in the
		// ..._truncate() functions for the shared tables needs to be protected here.
		tsk_size_t zero_value = 0;
		tsk_size_t *pointer_to_zero_value = &zero_value;
		
		for (size_t trees_index = 1; trees_index < trees_count; ++trees_index)
		{
			TreeSeqInfo &tsinfo_i = treeseq_[trees_index];
			
#if DEBUG
			// This protection scheme relies upon the bookmarked row being zero for shared tables;
			// only the zeroth element of each offset column is set up by the hack here.
			if ((tsinfo_i.table_position_.nodes != 0) ||
				(tsinfo_i.table_position_.individuals != 0) ||
				(tsinfo_i.table_position_.populations != 0))
				EIDOS_TERMINATION << "ERROR (Species::RetractNewIndividual): (internal error) tree sequence bookmark for a shared table in a secondary table collection is non-zero." << EidosTerminate();
#endif
			
			tsinfo_i.tables_.nodes.metadata_offset = pointer_to_zero_value;
			tsinfo_i.tables_.individuals.location_offset = pointer_to_zero_value;
			tsinfo_i.tables_.individuals.parents_offset = pointer_to_zero_value;
			tsinfo_i.tables_.individuals.metadata_offset = pointer_to_zero_value;
			tsinfo_i.tables_.populations.metadata_offset = pointer_to_zero_value;
			
			tsk_table_collection_truncate(&tsinfo_i.tables_, &tsinfo_i.table_position_);
			
			tsinfo_i.tables_.nodes.metadata_offset = NULL;
			tsinfo_i.tables_.individuals.location_offset = NULL;
			tsinfo_i.tables_.individuals.parents_offset = NULL;
			tsinfo_i.tables_.individuals.metadata_offset = NULL;
			tsinfo_i.tables_.populations.metadata_offset = NULL;
		}
	}
	
	// The above code boils down to this, which was the old code before #579 came along.
	// It is the same apart from the complicated protection against segfaults:
	//
	//	for (TreeSeqInfo &tsinfo : treeseq_)
	//		tsk_table_collection_truncate(&tsinfo.tables_, &tsinfo.table_position_);
}

void Species::RecordNewHaplosome(slim_position_t *p_breakpoints, int p_breakpoints_count, Haplosome *p_new_haplosome, 
		const Haplosome *p_initial_parental_haplosome, const Haplosome *p_second_parental_haplosome)
{
	// This method records a new non-null haplosome; see also RecordNewHaplosome_NULL().
	
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
	
	if (!p_new_haplosome)
		EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome): (internal error) p_new_haplosome is nullptr." << EidosTerminate();
	if (p_new_haplosome->IsNull())
		EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome): (internal error) p_new_haplosome is a null haplosome." << EidosTerminate();
#endif
	
	slim_chromosome_index_t chromosome_index = p_new_haplosome->chromosome_index_;
	Chromosome &chromosome = *chromosomes_[chromosome_index];
	TreeSeqInfo &tsinfo = treeseq_[chromosome_index];
	
	// This records information about an individual in the edge table.  BCH 12/6/2024: Note that recording the
	// new node table entries is now done by SetCurrentNewIndividual().  That method determines the tskit node
	// ids for the two haplosome positions of the individual, as tsk_node_id_base_ (+ 1).
	
	// Note that the breakpoints vector provided may (or may not) contain a breakpoint, as the final breakpoint
	// in the vector, that is past the end of the chromosome.  This is for bookkeeping in the crossover-mutation
	// code and should be ignored, as the code below does.  The breakpoints vector may be nullptr (indicating no
	// recombination), but if it exists it will be sorted in ascending order.

	// if there is no parent then no need to record edges
	if (!p_initial_parental_haplosome && !p_second_parental_haplosome)
		return;
	
	assert(p_initial_parental_haplosome);	// this cannot be nullptr if p_second_parental_haplosome is non-null, so now it is guaranteed non-null
	
	// get the TSK IDs for all the haplosomes involved; they are the tsk_node_id_base_ of the owning
	// individual, plus 0 or 1 depending on whether they are the first or second haplosome for their
	// associated chromosome
	tsk_id_t offspringTSKID, haplosome1TSKID, haplosome2TSKID;
	
	offspringTSKID = p_new_haplosome->OwningIndividual()->TskitNodeIdBase() + p_new_haplosome->chromosome_subposition_;
	haplosome1TSKID = p_initial_parental_haplosome->OwningIndividual()->TskitNodeIdBase() + p_initial_parental_haplosome->chromosome_subposition_;
	if (!p_second_parental_haplosome)
		haplosome2TSKID = haplosome1TSKID;
	else
		haplosome2TSKID = p_second_parental_haplosome->OwningIndividual()->TskitNodeIdBase() + p_second_parental_haplosome->chromosome_subposition_;
	
	// fix possible excess past-the-end breakpoint
	if (p_breakpoints_count && (p_breakpoints[p_breakpoints_count - 1] > chromosome.last_position_))
		p_breakpoints_count--;
	
	// add an edge for each interval between breakpoints
	double left = 0.0;
	double right;
	bool polarity = true;
	
	for (int i = 0; i < p_breakpoints_count; i++)
	{
		right = p_breakpoints[i];
		
		tsk_id_t parent = (tsk_id_t) (polarity ? haplosome1TSKID : haplosome2TSKID);
		polarity = !polarity;
		
		// Sometimes the user might add a breakpoint at 0, to flip the initial copy strand, as in the meiotic
		// drive recipe.  If they do that, a left==right breakpoint might make it in to here.  That would be
		// a bug in the caller.  This has never been seen in the wild, so I'll make it DEBUG only.  In non-
		// DEBUG runs the tree sequence will fail to pass integrity checks, with TSK_ERR_BAD_EDGE_INTERVAL.
#if DEBUG
		if (left >= right)
			EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome): (internal error) a left==right breakpoint was passed to RecordNewHaplosome()." << EidosTerminate();
#endif
		
		int ret = tsk_edge_table_add_row(&tsinfo.tables_.edges, left, right, parent, offspringTSKID, NULL, 0);
		if (ret < 0) handle_error("tsk_edge_table_add_row", ret);
		
		left = right;
	}
	
	right = (double)chromosome.last_position_+1;
	tsk_id_t parent = (tsk_id_t) (polarity ? haplosome1TSKID : haplosome2TSKID);
	int ret = tsk_edge_table_add_row(&tsinfo.tables_.edges, left, right, parent, offspringTSKID, NULL, 0);
	if (ret < 0) handle_error("tsk_edge_table_add_row", ret);
}

void Species::RecordNewHaplosome_NULL(Haplosome *p_new_haplosome)
{
	// This method records a new null haplosome (no edges to record); see also RecordNewHaplosome().
	
	// BCH 12/10/2024: With the new metadata scheme for haplosome, we also need to fix the is_vacant_ metadata if
	// the new haplosome is a null haplosome *and* it belongs to a chromosome type where that is notable.  In
	// the present design, that can only be chromosome types "A" and "H"; the other chromosome types do not
	// allow deviation from the default null-haplosome configuration.
	
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome_NULL): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
	
	if (!p_new_haplosome)
		EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome): (internal error) p_new_haplosome is nullptr." << EidosTerminate();
	if (!p_new_haplosome->IsNull())
		EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome): (internal error) p_new_haplosome is not a null haplosome." << EidosTerminate();
#endif
	
	{
		slim_chromosome_index_t chromosome_index = p_new_haplosome->chromosome_index_;
		Chromosome &chromosome = *chromosomes_[chromosome_index];
		ChromosomeType chromosome_type = chromosome.Type();
		
		if ((chromosome_type == ChromosomeType::kA_DiploidAutosome) || (chromosome_type == ChromosomeType::kH_HaploidAutosome))
		{
			// it is null and that was unexpected; we need to flip the corresponding is_vacant_ bit
			// each chromosome has two node table entries; entry 1 is for haplosome 1, entry 2 is
			// for haplosome 2, so there is only one bit per chromosome in a given is_vacant_ vector
			tsk_id_t offspringTSKID = p_new_haplosome->OwningIndividual()->TskitNodeIdBase() + p_new_haplosome->chromosome_subposition_;
			tsk_node_table_t &shared_node_table = treeseq_[0].tables_.nodes;
			HaplosomeMetadataRec *metadata = (HaplosomeMetadataRec *)(shared_node_table.metadata + shared_node_table.metadata_offset[offspringTSKID]);
			uint8_t *metadata_is_vacant = metadata->is_vacant_;
			int byte_index = chromosome_index / 8;
			int bit_shift = chromosome_index % 8;
			
			metadata_is_vacant[byte_index] |= (0x01 << bit_shift);
		}
	}
}

void Species::RecordNewDerivedState(const Haplosome *p_haplosome, slim_position_t p_position, const std::vector<Mutation *> &p_derived_mutations)
{
#if DEBUG
	if (!recording_mutations_)
		EIDOS_TERMINATION << "ERROR (Species::RecordNewDerivedState): (internal error) tree sequence mutation recording method called with recording off." << EidosTerminate();
#endif
	
	// This records information in the Site and Mutation tables.
	// This is called whenever a new mutation is added to a haplosome.  Because
	// mutation stacking makes things complicated, this hook supplies not just
	// the new mutation, but the entire new derived state – all of the
	// mutations that exist at the given position in the given haplosome,
	// post-addition.  This derived state may involve the removal of some
	// ancestral mutations (or may not), in addition to the new mutation that
	// was added.  The new state is not even guaranteed to be different from
	// the ancestral state; because of the way new mutations are added in some
	// paths (with bulk operations) we may not know.  This method will also be
	// called when a mutation is removed from a given haplosome; if no mutations
	// remain at the given position, p_derived_mutations will be empty.  The
	// vector of mutations passed in here is reused internally, so this method
	// must not keep a pointer to it; any information that needs to be kept
	// from it must be copied out.  See treerec/implementation.md for more.
	
	// BCH 4/29/2018: Null haplosomes should never contain any mutations at all,
	// including fixed mutations; the simplest thing is to just disallow derived
	// states for them altogether.
	if (p_haplosome->IsNull())
		EIDOS_TERMINATION << "ERROR (Species::RecordNewDerivedState): new derived states cannot be recorded for null haplosomes." << EidosTerminate();
	
	tsk_id_t haplosomeTSKID = p_haplosome->OwningIndividual()->TskitNodeIdBase() + p_haplosome->chromosome_subposition_;
	slim_chromosome_index_t index = p_haplosome->chromosome_index_;
	TreeSeqInfo &tsinfo = treeseq_[index];

	// Identify any previous mutations at this site in this haplosome, and add a new site.
	// This site may already exist, but we add it anyway, and deal with that in deduplicate_sites().
	double tsk_position = (double) p_position;

	tsk_id_t site_id = tsk_site_table_add_row(&tsinfo.tables_.sites, tsk_position, NULL, 0, NULL, 0);
	if (site_id < 0) handle_error("tsk_site_table_add_row", site_id);
	
	// form derived state
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::RecordNewDerivedState(): usage of statics");
	
	static std::vector<slim_mutationid_t> derived_mutation_ids;
	static std::vector<MutationMetadataRec> mutation_metadata;
	MutationMetadataRec metadata_rec;
	
	derived_mutation_ids.resize(0);
	mutation_metadata.resize(0);
	for (Mutation *mutation : p_derived_mutations)
	{
		derived_mutation_ids.emplace_back(mutation->mutation_id_);
		MetadataForMutation(mutation, &metadata_rec);
		mutation_metadata.emplace_back(metadata_rec);
	}
	
	// find and incorporate any fixed mutations at this position, which exist in all new derived states but are not included by SLiM
	// BCH 5/14/2019: Note that this means that derived states will be recorded that look "stacked" even when those mutations would
	// not have stacked, by the stacking policy, had they occurred in the same haplosome at the same time.  So this is a bit weird.
	// For example, you can end up with a derived state that appears to show two nucleotides stacked at the same position; but one
	// fixed before the other one occurred, so they aren't stacked really, the new one just occurred on the ancestral background of
	// the old one.  Possibly we ought to do something different about this (and not record a stacked derived state), but that
	// would be a big change since it has implications for crosscheck, etc.  FIXME
	auto position_range_iter = population_.treeseq_substitutions_map_.equal_range(p_position);
	
	for (auto position_iter = position_range_iter.first; position_iter != position_range_iter.second; ++position_iter)
	{
		Substitution *substitution = position_iter->second;
		
		derived_mutation_ids.emplace_back(substitution->mutation_id_);
		MetadataForSubstitution(substitution, &metadata_rec);
		mutation_metadata.emplace_back(metadata_rec);
	}
	
	// check for time consistency, using the shared node table in treeseq_[0]; this used to be a DEBUG check, but
	// it turns out that it happens in real models, so it should be checked in release builds also; it occurs
	// when a new mutation is added to a subpop that just split off a new subpop, due to tree_seq_tick_offset_;
	// see https://github.com/MesserLab/SLiM/issues/473 for a model that reproduces the problem, and now raises.
	double time = -(double) (community_.tree_seq_tick_ + community_.tree_seq_tick_offset_);	// see Population::AddSubpopulationSplit() regarding tree_seq_tick_offset_
	TreeSeqInfo &tsinfo0 = treeseq_[0];
	
	if (time < tsinfo0.tables_.nodes.time[haplosomeTSKID])
		EIDOS_TERMINATION << "ERROR (Species::RecordNewDerivedState): a mutation is being added with an invalid timestamp, greater than the time of the tree sequence node to which it belongs.  This can happen if you use addSubpopSplit() to split a new subpop from an old subpop, and then try to add a new mutation to the old subpop in the same tick.  That would imply that descendants of the old subpop ought to possess the new mutation -- but they don't, because the new subpop was already split off.  It therefore creates an inconsistency in the tree sequence.  Either add the new mutation prior to the split, or wait until the next tick to add the new mutation at a time that is clearly post-split.  (Details: invalid derived state recorded in tick " << community_.Tick() << ", haplosome " << haplosomeTSKID << ", id " << p_haplosome->haplosome_id_ << ", with time " << time << " >= " << tsinfo0.tables_.nodes.time[haplosomeTSKID] << ")." << EidosTerminate();
	
	// add the mutation table row with the final derived state and metadata
	char *derived_muts_bytes = (char *)(derived_mutation_ids.data());
	size_t derived_state_length = derived_mutation_ids.size() * sizeof(slim_mutationid_t);
	char *mutation_metadata_bytes = (char *)(mutation_metadata.data());
	size_t mutation_metadata_length = mutation_metadata.size() * sizeof(MutationMetadataRec);
	
	int ret = tsk_mutation_table_add_row(&tsinfo.tables_.mutations, site_id, haplosomeTSKID, TSK_NULL, 
					time,
					derived_muts_bytes, (tsk_size_t)derived_state_length,
					mutation_metadata_bytes, (tsk_size_t)mutation_metadata_length);
	if (ret < 0) handle_error("tsk_mutation_table_add_row", ret);
}

void Species::CheckAutoSimplification(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::CheckAutoSimplification): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This is called at the end of each cycle, at an appropriate time to simplify.  This method decides
	// whether to simplify or not, based upon how long it has been since the last time we simplified.  Each
	// time we simplify, we ask whether we simplified too early, too late, or just the right time by comparing
	// the pre:post ratio of the tree recording table sizes to the desired pre:post ratio, simplification_ratio_,
	// as set up in initializeTreeSeq().  Note that a simplification_ratio_ value of INF means "never simplify
	// automatically"; we check for that up front.
	++simplify_elapsed_;
	
	if (simplification_interval_ != -1)
	{
		// BCH 4/5/2019: Adding support for a chosen simplification interval rather than a ratio.  A value of -1
		// means the simplification ratio is being used, as implemented below; any other value is a target interval.
		if ((simplify_elapsed_ >= 1) && (simplify_elapsed_ >= simplification_interval_))
		{
			SimplifyAllTreeSequences();
		}
	}
	else if (!std::isinf(simplification_ratio_))
	{
		if (simplify_elapsed_ >= simplify_interval_)
		{
			// We could, in principle, calculate actual memory used based on number of rows * sizeof(column), etc.,
			// but that seems like overkill; adding together the number of rows in all the tables should be a
			// reasonable proxy, and this whole thing is just a heuristic that needs to be tailored anyway.
			// Note that this overcounts the rows for shared tables, but since the ratio of old:new is what matters
			// for the decision below, it seems to me that that overcounting is unimportant, and simpler to code.
			uint64_t old_table_size = 0;
			uint64_t new_table_size = 0;
			
			for (TreeSeqInfo &tsinfo : treeseq_)
			{
				old_table_size += (uint64_t)tsinfo.tables_.nodes.num_rows;
				old_table_size += (uint64_t)tsinfo.tables_.edges.num_rows;
				old_table_size += (uint64_t)tsinfo.tables_.sites.num_rows;
				old_table_size += (uint64_t)tsinfo.tables_.mutations.num_rows;
			}
			
			SimplifyAllTreeSequences();
				
			for (TreeSeqInfo &tsinfo : treeseq_)
			{
				new_table_size += (uint64_t)tsinfo.tables_.nodes.num_rows;
				new_table_size += (uint64_t)tsinfo.tables_.edges.num_rows;
				new_table_size += (uint64_t)tsinfo.tables_.sites.num_rows;
				new_table_size += (uint64_t)tsinfo.tables_.mutations.num_rows;
			}
			
			double ratio = old_table_size / (double)new_table_size;
			
			//std::cout << "auto-simplified in tick " << community->Tick() << "; old size " << old_table_size << ", new size " << new_table_size;
			//std::cout << "; ratio " << ratio << ", target " << simplification_ratio_ << std::endl;
			//std::cout << "old interval " << simplify_interval_ << ", new interval ";
			
			// Adjust our automatic simplification interval based upon the observed change in storage space used.
			// Not sure if this is exactly what we want to do; this will hunt around a lot without settling on a value,
			// but that seems harmless.  The scaling factor of 1.2 is chosen somewhat arbitrarily; we want it to be
			// large enough that we will arrive at the optimum interval before too terribly long, but small enough
			// that we have some granularity, so that once we reach the optimum we don't fluctuate too much.
			if (ratio < simplification_ratio_)
			{
				// We simplified too soon; wait a little longer next time
				simplify_interval_ *= 1.2;
				
				// Impose a maximum interval of 1000, so we don't get caught flat-footed if model demography changes
				if (simplify_interval_ > 1000.0)
					simplify_interval_ = 1000.0;
			}
			else if (ratio > simplification_ratio_)
			{
				// We simplified too late; wait a little less long next time
				simplify_interval_ /= 1.2;
				
				// Impose a minimum interval of 1.0, just to head off weird underflow issues
				if (simplify_interval_ < 1.0)
					simplify_interval_ = 1.0;
			}
			
			//std::cout << simplify_interval_ << std::endl;
		}
	}
}

void Species::DerivedStatesFromAscii(tsk_table_collection_t *p_tables)
{
	// This modifies p_tables in place, replacing the derived_state column of p_tables with a binary version.
	tsk_mutation_table_t mutations_copy;
	int ret = tsk_mutation_table_copy(&p_tables->mutations, &mutations_copy, 0);
	if (ret < 0) handle_error("derived_from_ascii", ret);
	
	{
		const char *derived_state = p_tables->mutations.derived_state;
		tsk_size_t *derived_state_offset = p_tables->mutations.derived_state_offset;
		std::vector<slim_mutationid_t> binary_derived_state;
		std::vector<tsk_size_t> binary_derived_state_offset;
		size_t derived_state_total_part_count = 0;
		
		binary_derived_state_offset.emplace_back(0);
		
		try {
			for (size_t j = 0; j < p_tables->mutations.num_rows; j++)
			{
				std::string string_derived_state(derived_state + derived_state_offset[j], derived_state_offset[j+1] - derived_state_offset[j]);
				
				if (string_derived_state.size() == 0)
				{
					// nothing to do for an empty derived state
				}
				else if (string_derived_state.find(',') == std::string::npos)
				{
					// a single mutation can be handled more efficiently, and this is the common case so it's worth optimizing
					binary_derived_state.emplace_back((slim_mutationid_t)std::stoll(string_derived_state));
					derived_state_total_part_count++;
				}
				else
				{
					// stacked mutations require that the derived state be separated to parse it
					std::vector<std::string> derived_state_parts = Eidos_string_split(string_derived_state, ",");
					
					for (std::string &derived_state_part : derived_state_parts)
						binary_derived_state.emplace_back((slim_mutationid_t)std::stoll(derived_state_part));
					
					derived_state_total_part_count += derived_state_parts.size();
				}
				
				binary_derived_state_offset.emplace_back((tsk_size_t)(derived_state_total_part_count * sizeof(slim_mutationid_t)));
			}
		} catch (...) {
			EIDOS_TERMINATION << "ERROR (Species::DerivedStatesFromAscii): a mutation derived state was not convertible into an int64_t mutation id.  The tree-sequence data may not be annotated for SLiM, or may be corrupted.  If mutations were added in msprime, do you want to use the msprime.SLiMMutationModel?" << EidosTerminate();
		}
		
		if (binary_derived_state.size() == 0)
			binary_derived_state.resize(1);
		
		ret = tsk_mutation_table_set_columns(&p_tables->mutations,
										 mutations_copy.num_rows,
										 mutations_copy.site,
										 mutations_copy.node,
										 mutations_copy.parent,
										 mutations_copy.time,
										 (char *)binary_derived_state.data(),
										 binary_derived_state_offset.data(),
										 mutations_copy.metadata,
										 mutations_copy.metadata_offset);
		if (ret < 0) handle_error("derived_from_ascii", ret);
	}
	
	tsk_mutation_table_free(&mutations_copy);
}

void Species::DerivedStatesToAscii(tsk_table_collection_t *p_tables)
{
	// This modifies p_tables in place, replacing the derived_state column of p_tables with an ASCII version.
	tsk_mutation_table_t mutations_copy;
	int ret = tsk_mutation_table_copy(&p_tables->mutations, &mutations_copy, 0);
	if (ret < 0) handle_error("derived_to_ascii", ret);
	
	{
		const char *derived_state = p_tables->mutations.derived_state;
		tsk_size_t *derived_state_offset = p_tables->mutations.derived_state_offset;
		std::string text_derived_state;
		std::vector<tsk_size_t> text_derived_state_offset;
		
		text_derived_state_offset.emplace_back(0);
		
		for (size_t j = 0; j < p_tables->mutations.num_rows; j++)
		{
			slim_mutationid_t *int_derived_state = (slim_mutationid_t *)(derived_state + derived_state_offset[j]);
			size_t cur_derived_state_length = (derived_state_offset[j+1] - derived_state_offset[j])/sizeof(slim_mutationid_t);
			
			for (size_t i = 0; i < cur_derived_state_length; i++)
			{
				if (i != 0) text_derived_state.append(",");
				text_derived_state.append(std::to_string(int_derived_state[i]));
			}
			text_derived_state_offset.emplace_back((tsk_size_t)text_derived_state.size());
		}
		
		ret = tsk_mutation_table_set_columns(&p_tables->mutations,
										 mutations_copy.num_rows,
										 mutations_copy.site,
										 mutations_copy.node,
										 mutations_copy.parent,
										 mutations_copy.time,
										 text_derived_state.c_str(),
										 text_derived_state_offset.data(),
										 mutations_copy.metadata,
										 mutations_copy.metadata_offset);
		if (ret < 0) handle_error("derived_to_ascii", ret);
	}
	
	tsk_mutation_table_free(&mutations_copy);
}

void Species::AddIndividualsToTable(Individual * const *p_individual, size_t p_num_individuals, tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash, tsk_flags_t p_flags)
{
	// We use currently use this function in two ways, depending on p_flags:
	//  1. (SLIM_TSK_INDIVIDUAL_REMEMBERED) for individuals to be permanently
	//      remembered, or
	//  2. (SLIM_TSK_INDIVIDUAL_RETAINED) for individuals to be retained only while
	//      some of their genome (i.e. any of their nodes) exists in the tree sequence, or
	//  3. (SLIM_TSK_INDIVIDUAL_ALIVE) to output the final generation in the tree sequence.
	// So, in case (1) we set the REMEMBERED flag, in case (2) we set the RETAINED flag,
	// and in case (3) we set the ALIVE flag.
	// Note that this function can be called multiple times for the same set of
	// individuals. In the most extreme case, individuals who are remembered, then
	// permanently remembered but still alive when the tree sequence is written out will
	// have this method called on them three times, and they get all flags set.
	
	// Passing nullptr used to be allowed as a shorthand for "use the internal tables", but that is
	// no longer allowed since we now might have multiple internal table collections, per chromosome.
	if (p_tables == nullptr)
		EIDOS_TERMINATION << "ERROR (Species::AddIndividualsToTable): (internal error) p_tables is nullptr!" << EidosTerminate();
	
	// loop over individuals and add entries to the individual table; if they are already
	// there, we just need to update their flags, metadata, location, etc.
	for (size_t j = 0; j < p_num_individuals; j++)
	{
		Individual *ind = p_individual[j];
		slim_pedigreeid_t ped_id = ind->PedigreeID();

		std::vector<double> location;
		location.emplace_back(ind->spatial_x_);
		location.emplace_back(ind->spatial_y_);
		location.emplace_back(ind->spatial_z_);
		
		IndividualMetadataRec metadata_rec;
		MetadataForIndividual(ind, &metadata_rec);
		
		// do a fast lookup to see whether this individual is already in the individuals table
		auto ind_pos = p_individuals_hash->find(ped_id);
		
		if (ind_pos == p_individuals_hash->end()) {
			// This individual is not already in the tables.
			tsk_id_t tsk_individual = tsk_individual_table_add_row(&p_tables->individuals,
					p_flags, location.data(), (uint32_t)location.size(), 
                    NULL, 0, // individual parents
					(char *)&metadata_rec, (uint32_t)sizeof(IndividualMetadataRec));
			if (tsk_individual < 0) handle_error("tsk_individual_table_add_row", tsk_individual);
			
			// Add the new individual to our hash table, for fast lookup as done above
			p_individuals_hash->emplace(ped_id, tsk_individual);

			// Update node table to have the individual's tskit id in its individual column
			tsk_id_t tsk_node_id_base = ind->TskitNodeIdBase();
			
			assert(tsk_node_id_base + 1 < (tsk_id_t) p_tables->nodes.num_rows);		// base and base+1 must both be in range
			p_tables->nodes.individual[tsk_node_id_base] = tsk_individual;
			p_tables->nodes.individual[tsk_node_id_base + 1] = tsk_individual;

			// Update remembered nodes; there are just two entries, base and base+1, for all haplosomes
			if (p_flags & SLIM_TSK_INDIVIDUAL_REMEMBERED)
			{
				remembered_nodes_.emplace_back(tsk_node_id_base);
				remembered_nodes_.emplace_back(tsk_node_id_base + 1);
			}
		} else {
			// This individual is already there; we need to update the information.
			tsk_id_t tsk_individual =  ind_pos->second;

			assert(((size_t)tsk_individual < p_tables->individuals.num_rows)
				   && (location.size()
					   == (p_tables->individuals.location_offset[tsk_individual + 1]
						   - p_tables->individuals.location_offset[tsk_individual]))
				   && (sizeof(IndividualMetadataRec)
					   == (p_tables->individuals.metadata_offset[tsk_individual + 1]
						   - p_tables->individuals.metadata_offset[tsk_individual])));
			
			// It could have been previously inserted but not with the SLIM_TSK_INDIVIDUAL_REMEMBERED
			// flag: if so, it now needs adding to the list of remembered nodes
			tsk_id_t tsk_node_id_base = ind->TskitNodeIdBase();
			
			if (((p_tables->individuals.flags[tsk_individual] & SLIM_TSK_INDIVIDUAL_REMEMBERED) == 0)
				&& (p_flags & SLIM_TSK_INDIVIDUAL_REMEMBERED))
			{
				remembered_nodes_.emplace_back(tsk_node_id_base);
				remembered_nodes_.emplace_back(tsk_node_id_base + 1);
			}

			memcpy(p_tables->individuals.location
				   + p_tables->individuals.location_offset[tsk_individual],
				   location.data(), location.size() * sizeof(double));
			memcpy(p_tables->individuals.metadata
					+ p_tables->individuals.metadata_offset[tsk_individual],
					&metadata_rec, sizeof(IndividualMetadataRec));
			p_tables->individuals.flags[tsk_individual] |= p_flags;
			
			// Check node table
			assert(ind->TskitNodeIdBase() + 1 < (tsk_id_t) p_tables->nodes.num_rows);	// base and base+1 must both be in range
			
			// BCH 4/29/2019: These asserts are, we think, not technically necessary – the code
			// would work even if they were violated.  But they're a nice invariant to guarantee,
			// and right now they are always true.
			assert(p_tables->nodes.individual[tsk_node_id_base] == (tsk_id_t)tsk_individual);
			assert(p_tables->nodes.individual[tsk_node_id_base + 1] == (tsk_id_t)tsk_individual);
		}
	}
}

void Species::AddLiveIndividualsToIndividualsTable(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash)
{
	// add currently alive individuals to the individuals table, so they persist
	// through simplify and can be revived when loading saved state
	for (auto subpop_iter : population_.subpops_)
	{
		AddIndividualsToTable(subpop_iter.second->parent_individuals_.data(), subpop_iter.second->parent_individuals_.size(), p_tables, p_individuals_hash, SLIM_TSK_INDIVIDUAL_ALIVE);
	}
}

void Species::FixAliveIndividuals(tsk_table_collection_t *p_tables)
{
	// This clears the alive flags of the remaining entries; our internal tables never say "alive",
	// since that changes from cycle to cycle, so after loading saved state we want to strip
	for (size_t j = 0; j < p_tables->individuals.num_rows; j++)
		p_tables->individuals.flags[j] &= (~SLIM_TSK_INDIVIDUAL_ALIVE);
}

// check whether population metadata is SLiM metadata or not, without raising; if it is, return the slim_id, >= 0, if not, return -1
// see also Species::__PrepareSubpopulationsFromTables(), which does similar checks but raises if something is wrong
static slim_objectid_t CheckSLiMPopulationMetadata(const char *p_metadata, size_t p_metadata_length)
{
	std::string metadata_string(p_metadata, p_metadata_length);
	nlohmann::json subpop_metadata;
	
	try {
		subpop_metadata = nlohmann::json::parse(metadata_string);
	} catch (...) {
		return -1;
	}

	if (subpop_metadata.is_null())
		return -1;
	if (!subpop_metadata.is_object())
		return -1;
	
	if (!subpop_metadata.contains("slim_id"))
		return -1;
	if (!subpop_metadata["slim_id"].is_number_integer())
		return -1;
	
	return subpop_metadata["slim_id"].get<slim_objectid_t>();
}

void Species::WritePopulationTable(tsk_table_collection_t *p_tables)
{
	int ret;
	tsk_id_t tsk_population_id = -1;
	tsk_population_table_t *population_table_copy;
	population_table_copy = (tsk_population_table_t *)malloc(sizeof(tsk_population_table_t));
	if (!population_table_copy)
		EIDOS_TERMINATION << "ERROR (Species::WritePopulationTable): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	ret = tsk_population_table_copy(&p_tables->populations, population_table_copy, 0);
	if (ret != 0) handle_error("WritePopulationTable tsk_population_table_copy()", ret);
	ret = tsk_population_table_clear(&p_tables->populations);
	if (ret != 0) handle_error("WritePopulationTable tsk_population_table_clear()", ret);

	// figure out the last subpop id we need to write out to the table; this is the greatest value from (a) the number
	// of rows in the current population table (to carry over non-SLiM pop table entries we loaded in), (b) the subpop
	// references found in the node table, which might reference subpops that no longer exist, and (c) the subpop ids
	// found in our "previously used" information, whcih references every subpop id we have seen during execution.
	slim_objectid_t last_subpop_id = (slim_objectid_t)population_table_copy->num_rows - 1;	// FIXME note this assumes the number of rows fits into 32 bits
	for (size_t j = 0; j < p_tables->nodes.num_rows; j++)
		last_subpop_id = std::max(last_subpop_id, p_tables->nodes.population[j]);
	for (const auto &used_id_name : used_subpop_ids_)
		last_subpop_id = std::max(last_subpop_id, used_id_name.first);
	
	// write out an entry for each subpop
	slim_objectid_t last_id_written = -1;
	
	for (auto subpop_iter : population_.subpops_)
	{
		Subpopulation *subpop = subpop_iter.second;
		slim_objectid_t subpop_id = subpop->subpopulation_id_;
		
		// first, write out empty entries for unused subpop ids before this one; note metadata should always be JSON here,
		// binary metadata got translated to JSON by _InstantiateSLiMObjectsFromTables() on read
		while (last_id_written < subpop_id - 1)
		{
			bool got_metadata = false;
			std::string new_metadata_string("null");
			
			if (++last_id_written < (slim_objectid_t) population_table_copy->num_rows)
			{
				tsk_population_t tsk_population_object;
				ret = tsk_population_table_get_row(population_table_copy, last_id_written, &tsk_population_object);
				if (ret != 0) handle_error("WritePopulationTable tsk_population_table_get_row()", ret);
				
				if (CheckSLiMPopulationMetadata(tsk_population_object.metadata, tsk_population_object.metadata_length) == -1)
				{
					// The metadata present, if any, is not SLiM metadata, so it should be carried over.
					new_metadata_string = std::string(tsk_population_object.metadata, tsk_population_object.metadata_length);
					got_metadata = true;
				}
				else
				{
					// SLiM metadata for non-extant subpops gets removed at write time for consistency.
					// See issue #317 for discussion.  However, we keep *names* from SLiM populations
					// that are found in the table and have been removed, because names provide a
					// useful way for the user to check that everything is as they expect,
					// lets them find particular populations without error-prone bookkeeping,
					// and is the basis for the more user-friendly interfaces in msprime
					// (e.g., it's important to have names when setting up a more complex model
					// to use in recapitation). At present, the only way that
					// entries can have SLiM metadata in the population table
					// but not correspond to extant populations is if the populations were present
					// in a tree sequence that was loaded in to SLiM but they were
					// subsequently removed.
					std::string metadata_string(
							tsk_population_object.metadata,
							tsk_population_object.metadata_length);
					nlohmann::json old_metadata;
					old_metadata = nlohmann::json::parse(metadata_string);
					if (old_metadata.contains("name")) {
						nlohmann::json new_metadata = nlohmann::json::object();
						new_metadata["name"] = old_metadata["name"];
						new_metadata_string = new_metadata.dump();
						got_metadata = true;
					}
				}
			}
			// BCH 7/20/2024: To fix #447, we have some new logic here.  If we didn't get any useful
			// metadata from the population table, we're on our own.  If we have previously seen a
			// subpop with this id at any point, we use the name we last saw for that id.
			if (!got_metadata)
			{
				auto used_id_name_iter = used_subpop_ids_.find(last_id_written);
				
				if (used_id_name_iter != used_subpop_ids_.end())
				{
					nlohmann::json new_metadata = nlohmann::json::object();
					new_metadata["name"] = (*used_id_name_iter).second;
					new_metadata_string = new_metadata.dump();
					//got_metadata = true;	// BCH 4/15/2025: value stored is never used; commenting out, I don't *think* this represents a bug, just an unnecessary store...
				}
			}
			
			// otherwise, we will use the "null" metadata we set as the default above,
			// producing a simple placeholder row that implies the id has never been used.
			
			tsk_population_id = tsk_population_table_add_row(
					&p_tables->populations,
					new_metadata_string.data(),
					new_metadata_string.length()
			);
			if (tsk_population_id < 0) handle_error("tsk_population_table_add_row", tsk_population_id);
			
			assert(tsk_population_id == last_id_written);
		}
		
		// now we're at the slot for this subpopulation, so construct it and write it out
		nlohmann::json pop_metadata = nlohmann::json::object();
		
		pop_metadata["slim_id"] = subpop->subpopulation_id_;
		
		if (spatial_dimensionality_ >= 1)
		{
			pop_metadata["bounds_x0"] = subpop->bounds_x0_;
			pop_metadata["bounds_x1"] = subpop->bounds_x1_;
		}
		if (spatial_dimensionality_ >= 2)
		{
			pop_metadata["bounds_y0"] = subpop->bounds_y0_;
			pop_metadata["bounds_y1"] = subpop->bounds_y1_;
		}
		if (spatial_dimensionality_ >= 3)
		{
			pop_metadata["bounds_z0"] = subpop->bounds_z0_;
			pop_metadata["bounds_z1"] = subpop->bounds_z1_;
		}
		
		pop_metadata["name"] = subpop->name_;
		if (subpop->description_.length())
			pop_metadata["description"] = subpop->description_;
		
		if (model_type_ == SLiMModelType::kModelTypeWF)
		{
			if (subpop->selfing_fraction_ > 0.0)
				pop_metadata["selfing_fraction"] = subpop->selfing_fraction_;
			if (subpop->female_clone_fraction_ > 0.0)
				pop_metadata["female_cloning_fraction"] = subpop->female_clone_fraction_;
			if (subpop->male_clone_fraction_ > 0.0)
				pop_metadata["male_cloning_fraction"] = subpop->male_clone_fraction_;
			if (subpop->parent_sex_ratio_ != 0.5)
				pop_metadata["sex_ratio"] = subpop->parent_sex_ratio_;
			
			nlohmann::json migration_records = nlohmann::json::array();
			
			for (std::pair<slim_objectid_t,double> migration_pair : subpop->migrant_fractions_)
			{
				nlohmann::json migration_record = nlohmann::json::object();
				
				migration_record["source_subpop"] = migration_pair.first;
				migration_record["migration_rate"] = migration_pair.second;
				migration_records.emplace_back(std::move(migration_record));
			}
			
			pop_metadata["migration_records"] = std::move(migration_records);
		}
		
		std::string metadata_rec = pop_metadata.dump();
		
		tsk_population_id = tsk_population_table_add_row(&p_tables->populations, (char *)metadata_rec.c_str(), (uint32_t)metadata_rec.length());
		if (tsk_population_id < 0) handle_error("tsk_population_table_add_row", tsk_population_id);
		
		last_id_written++;
		assert(tsk_population_id == last_id_written);
	}
	
	// finally, write out entries for the rest of the table; entries are needed up to
	// largest_subpop_id_ because there could be ancestral nodes that reference them
	while (last_id_written < (slim_objectid_t) last_subpop_id)
	{
		bool got_metadata = false;
		std::string new_metadata_string("null");
		
		if (++last_id_written < (slim_objectid_t) population_table_copy->num_rows)
		{
			tsk_population_t tsk_population_object;
			tsk_population_table_get_row(population_table_copy, last_id_written, &tsk_population_object);
			if (ret != 0) handle_error("WritePopulationTable tsk_population_table_get_row()", ret);
			
			if (CheckSLiMPopulationMetadata(tsk_population_object.metadata, tsk_population_object.metadata_length) == -1)
			{
				// The metadata present, if any, is not SLiM metadata, so it should be carried over; note that
				new_metadata_string = std::string(tsk_population_object.metadata, tsk_population_object.metadata_length);
				got_metadata = true;
			}
			else
			{
				// As above, retain only names from SLiM metadata for non-extant subpops.
				std::string metadata_string(
						tsk_population_object.metadata,
						tsk_population_object.metadata_length);
				nlohmann::json old_metadata;
				old_metadata = nlohmann::json::parse(metadata_string);
				if (old_metadata.contains("name")) {
					nlohmann::json new_metadata = nlohmann::json::object();
					new_metadata["name"] = old_metadata["name"];
					new_metadata_string = new_metadata.dump();
					got_metadata = true;
				}
			}
		}
		// BCH 7/20/2024: To fix #447, we have some new logic here.  If we didn't get any useful
		// metadata from the population table, we're on our own.  If we have previously seen a
		// subpop with this id at any point, we use the name we last saw for that id.
		if (!got_metadata)
		{
			auto used_id_name_iter = used_subpop_ids_.find(last_id_written);
			
			if (used_id_name_iter != used_subpop_ids_.end())
			{
				nlohmann::json new_metadata = nlohmann::json::object();
				new_metadata["name"] = (*used_id_name_iter).second;
				new_metadata_string = new_metadata.dump();
				//got_metadata = true;	// BCH 4/15/2025: value stored is never used; commenting out, I don't *think* this represents a bug, just an unnecessary store...
			}
		}
		
		tsk_population_id = tsk_population_table_add_row(
				&p_tables->populations,
				new_metadata_string.data(),
				new_metadata_string.length()
		);
		if (tsk_population_id < 0) handle_error("tsk_population_table_add_row", tsk_population_id);
		
		assert(tsk_population_id == last_id_written);
	}
	
	ret = tsk_population_table_free(population_table_copy);
	if (ret != 0) handle_error("tsk_population_table_free", ret);
	free(population_table_copy);
}

void Species::WriteTreeSequenceMetadata(tsk_table_collection_t *p_tables, EidosDictionaryUnretained *p_metadata_dict, slim_chromosome_index_t p_chromosome_index)
{
	int ret = 0;

	//////
	// Top-level (tree sequence) metadata:
	// In the future, we might need to *add* to the metadata *and also* the schema,
	// leaving other keys that might already be there.
	// But that's being a headache, so we're skipping it.
	
	// BCH 3/9/2025: This is now wrapped in try/catch because the JSON library might raise, especially if it dislikes
	// the model string we try to put in metadata for p_include_model; see https://github.com/MesserLab/SLiM/issues/488
	std::string new_metadata_str;
	
	try {
		nlohmann::json metadata;
		
		// Add user-defined metadata under the SLiM key, if it was supplied by the user
		// See https://github.com/MesserLab/SLiM/issues/122
		if (p_metadata_dict)
		{
			nlohmann::json user_metadata = p_metadata_dict->JSONRepresentation();
			
			metadata["SLiM"]["user_metadata"] = user_metadata;
			
			//std::cout << "JSON metadata: " << std::endl << user_metadata.dump(4) << std::endl;
		}
		
		// We could support per-chromosome top-level metadata, too, that would only be saved out
		// to that chromosome's file, but let's wait to see whether somebody asks for it...
		
		if (model_type_ == SLiMModelType::kModelTypeWF) {
			metadata["SLiM"]["model_type"] = "WF";
			if (community_.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) {
				metadata["SLiM"]["stage"] = "first";
			} else if (community_.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts) {
				metadata["SLiM"]["stage"] = "early";
			} else {
				assert(community_.CycleStage() == SLiMCycleStage::kWFStage5ExecuteLateScripts);
				metadata["SLiM"]["stage"] = "late";
			}
		} else {
			assert(model_type_ == SLiMModelType::kModelTypeNonWF);
			metadata["SLiM"]["model_type"] = "nonWF";
			if (community_.CycleStage() == SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) {
				metadata["SLiM"]["stage"] = "first";
			} else if (community_.CycleStage() == SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) {
				metadata["SLiM"]["stage"] = "early";
			} else {
				assert(community_.CycleStage() == SLiMCycleStage::kNonWFStage6ExecuteLateScripts);
				metadata["SLiM"]["stage"] = "late";
			}
		}
		metadata["SLiM"]["cycle"] = Cycle();
		metadata["SLiM"]["tick"] = community_.Tick();
		metadata["SLiM"]["file_version"] = SLIM_TREES_FILE_VERSION;
		
		metadata["SLiM"]["name"] = name_;
		if (description_.length())
			metadata["SLiM"]["description"] = description_;
		
		if (spatial_dimensionality_ == 0) {
			metadata["SLiM"]["spatial_dimensionality"] = "";
		} else if (spatial_dimensionality_ == 1) {
			metadata["SLiM"]["spatial_dimensionality"] = "x";
		} else if (spatial_dimensionality_ == 2) {
			metadata["SLiM"]["spatial_dimensionality"] = "xy";
		} else {
			metadata["SLiM"]["spatial_dimensionality"] = "xyz";
		}
		if (periodic_x_ & periodic_y_ & periodic_z_) {
			metadata["SLiM"]["spatial_periodicity"] = "xyz";
		} else if (periodic_x_ & periodic_y_) {
			metadata["SLiM"]["spatial_periodicity"] = "xy";
		} else if (periodic_x_ & periodic_z_) {
			metadata["SLiM"]["spatial_periodicity"] = "xz";
		} else if (periodic_y_ & periodic_z_) {
			metadata["SLiM"]["spatial_periodicity"] = "yz";
		} else if (periodic_x_) {
			metadata["SLiM"]["spatial_periodicity"] = "x";
		} else if (periodic_y_) {
			metadata["SLiM"]["spatial_periodicity"] = "y";
		} else if (periodic_z_) {
			metadata["SLiM"]["spatial_periodicity"] = "z";
		} else {
			metadata["SLiM"]["spatial_periodicity"] = "";
		}
		metadata["SLiM"]["separate_sexes"] = sex_enabled_ ? true : false;
		metadata["SLiM"]["nucleotide_based"] = nucleotide_based_ ? true : false;
		
		metadata["SLiM"]["chromosomes"] = nlohmann::json::array();
		for (Chromosome *chromosome : chromosomes_)
		{
			nlohmann::json chromosome_info;
			
			chromosome_info["index"] = chromosome->Index();
			chromosome_info["id"] = chromosome->ID();
			chromosome_info["symbol"] = chromosome->Symbol();
			if (chromosome->Name().length() > 0)
				chromosome_info["name"] = chromosome->Name();
			chromosome_info["type"] = StringForChromosomeType(chromosome->Type());
			
			metadata["SLiM"]["chromosomes"].push_back(chromosome_info);
			
			if (p_chromosome_index == chromosome->Index())		// true for the chromosome being written
			{
				// write out all the same information again in a key called "this_chromosome"; this way the
				// user can trivially get the info for the chromosome represented by the file; note that a
				// no-genetics model will have a chromosomes key with an empty array, and no this_chromosome,
				// but a no-genetics model can't write a tree sequence anyway, so that is moot.
				metadata["SLiM"]["this_chromosome"] = chromosome_info;
			}
		}
		
		new_metadata_str = metadata.dump();
	} catch (const std::exception &e) {
		EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequenceMetadata): a JSON string could not be generated for tree-sequence metadata due to an error: '" << e.what() << "'." << EidosTerminate();
	} catch (...) {
		EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequenceMetadata): a JSON string could not be generated for tree-sequence metadata due to an unknown error." << EidosTerminate();
	}
	
	ret = tsk_table_collection_set_metadata(
			p_tables, new_metadata_str.c_str(), (tsk_size_t)new_metadata_str.length());
	if (ret != 0)
		handle_error("tsk_table_collection_set_metadata", ret);

	// As above, we maybe ought to edit the metadata schema adding our keys,
	// but then comparing tables is a headache; see tskit#763
	ret = tsk_table_collection_set_metadata_schema(
			p_tables, gSLiM_tsk_metadata_schema.c_str(), (tsk_size_t)gSLiM_tsk_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_table_collection_set_metadata_schema", ret);

	////////////
	// Set metadata schema on each table
	ret = tsk_edge_table_set_metadata_schema(&p_tables->edges,
			gSLiM_tsk_edge_metadata_schema.c_str(),
			(tsk_size_t)gSLiM_tsk_edge_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_edge_table_set_metadata_schema", ret);
	ret = tsk_site_table_set_metadata_schema(&p_tables->sites,
			gSLiM_tsk_site_metadata_schema.c_str(),
			(tsk_size_t)gSLiM_tsk_site_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_site_table_set_metadata_schema", ret);
	ret = tsk_mutation_table_set_metadata_schema(&p_tables->mutations,
			gSLiM_tsk_mutation_metadata_schema.c_str(),
			(tsk_size_t)gSLiM_tsk_mutation_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_mutation_table_set_metadata_schema", ret);
	ret = tsk_individual_table_set_metadata_schema(&p_tables->individuals,
			gSLiM_tsk_individual_metadata_schema.c_str(),
			(tsk_size_t)gSLiM_tsk_individual_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_individual_table_set_metadata_schema", ret);
	ret = tsk_population_table_set_metadata_schema(&p_tables->populations,
			gSLiM_tsk_population_metadata_schema.c_str(),
			(tsk_size_t)gSLiM_tsk_population_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_population_table_set_metadata_schema", ret);
	
	// For the node table the schema we save out depends upon the number of
	// bits needed to represent the null haplosome structure of the model.
	// We allocate one bit per chromosome, in each node table entry (note
	// there are two entries per individual, so it ends up being two bits
	// of information per chromosome, across the two node table entries.)
	// See the big comment on gSLiM_tsk_node_metadata_schema_FORMAT.
	std::string tsk_node_metadata_schema = gSLiM_tsk_node_metadata_schema_FORMAT;
	size_t pos = tsk_node_metadata_schema.find("\"%d\"");
	std::string count_string = std::to_string(haplosome_metadata_is_vacant_bytes_);
	
	if (pos == std::string::npos)
		EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequenceMetadata): (internal error) `%d` substring missing from gSLiM_tsk_node_metadata_schema_FORMAT." << EidosTerminate();
	
	tsk_node_metadata_schema.replace(pos, 4, count_string);		// replace %d in the format string with the byte count
	
	ret = tsk_node_table_set_metadata_schema(&p_tables->nodes,
			tsk_node_metadata_schema.c_str(),
			(tsk_size_t)tsk_node_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_node_table_set_metadata_schema", ret);
}

void Species::WriteProvenanceTable(tsk_table_collection_t *p_tables, bool p_use_newlines, bool p_include_model, slim_chromosome_index_t p_chromosome_index)
{
	int ret = 0;
	time_t timer;
	size_t timestamp_size = 64;
	char buffer[timestamp_size];
	struct tm* tm_info;
	// NOTE: since file version 0.5, we do *not* read information
	// back out of the provenance table, but get it from metadata instead.
	// But, we still want to record how the tree sequence was produced in
	// provenance, so the code remains much the same.
	
	// New provenance writing code, using the JSON for Modern C++ library (json.hpp); this is file_version 0.2 (and up)
	// BCH 3/9/2025: This is now wrapped in try/catch because the JSON library might raise, especially if it dislikes
	// the model string we try to put in metadata for p_include_model; see https://github.com/MesserLab/SLiM/issues/488
	std::string provenance_str;
	
	try {
		nlohmann::json j;
		
		// BCH 3/10/2024: Moving from schema version 1.0.0 to 1.1.0, which I believe shipped in tskit 0.5.9.
		// This adds the optional `resources` key.  See https://github.com/MesserLab/SLiM/issues/478.
		j["schema_version"] = "1.1.0";
		
		struct utsname name;
		ret = uname(&name);
		
		if (ret == -1)
			EIDOS_TERMINATION << "ERROR (Species::WriteProvenanceTable): (internal error) uname() failed." << EidosTerminate();
		
		j["environment"]["os"]["version"] = std::string(name.version);
		j["environment"]["os"]["node"] = std::string(name.nodename);
		j["environment"]["os"]["release"] = std::string(name.release);
		j["environment"]["os"]["system"] = std::string(name.sysname);
		j["environment"]["os"]["machine"] = std::string(name.machine);
		
		j["software"]["name"] = "SLiM";		// note this key was named "program" in provenance version 0.1
		j["software"]["version"] = SLIM_VERSION_STRING;
		
		j["slim"]["file_version"] = SLIM_TREES_FILE_VERSION;	// see declaration of SLIM_TREES_FILE_VERSION for comments on prior versions
		j["slim"]["cycle"] = Cycle();
		j["slim"]["tick"] = community_.Tick();
		j["slim"]["name"] = name_;
		if (description_.length())
			j["slim"]["description"] = description_;
		//j["slim"]["remembered_node_count"] = (long)remembered_nodes_.size();	// no longer writing this key!
		
		// compute the SHA-256 hash of the script string
		const std::string &scriptString = community_.ScriptString();
		const char *script_chars = scriptString.c_str();
		uint8_t script_hash[32];
		char script_hash_string[65];
		
		Eidos_calc_sha_256(script_hash, script_chars, strlen(script_chars));
		Eidos_hash_to_string(script_hash_string, script_hash);
		
		std::string scriptHashString(script_hash_string);
		
		//std::cout << "script hash: " << scriptHashString << std::endl;
		
		j["parameters"]["command"] = community_.cli_params_;
		
		// note high overlap with WriteTreeSequenceMetadata
		if (model_type_ == SLiMModelType::kModelTypeWF) {
			j["parameters"]["model_type"] = "WF";
			if (community_.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) {
				j["parameters"]["stage"] = "first";
			} else if (community_.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts) {
				j["parameters"]["stage"] = "early";
			} else {
				assert(community_.CycleStage() == SLiMCycleStage::kWFStage5ExecuteLateScripts);
				j["parameters"]["stage"] = "late";
			}
		} else {
			assert(model_type_ == SLiMModelType::kModelTypeNonWF);
			j["parameters"]["model_type"] = "nonWF";
			if (community_.CycleStage() == SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) {
				j["parameters"]["stage"] = "first";
			} else if (community_.CycleStage() == SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) {
				j["parameters"]["stage"] = "early";
			} else {
				assert(community_.CycleStage() == SLiMCycleStage::kNonWFStage6ExecuteLateScripts);
				j["parameters"]["stage"] = "late";
			}
		}
		if (spatial_dimensionality_ == 0) {
			j["parameters"]["spatial_dimensionality"] = "";
		} else if (spatial_dimensionality_ == 1) {
			j["parameters"]["spatial_dimensionality"] = "x";
		} else if (spatial_dimensionality_ == 2) {
			j["parameters"]["spatial_dimensionality"] = "xy";
		} else {
			j["parameters"]["spatial_dimensionality"] = "xyz";
		}
		if (periodic_x_ & periodic_y_ & periodic_z_) {
			j["parameters"]["spatial_periodicity"] = "xyz";
		} else if (periodic_x_ & periodic_y_) {
			j["parameters"]["spatial_periodicity"] = "xy";
		} else if (periodic_x_ & periodic_z_) {
			j["parameters"]["spatial_periodicity"] = "xz";
		} else if (periodic_y_ & periodic_z_) {
			j["parameters"]["spatial_periodicity"] = "yz";
		} else if (periodic_x_) {
			j["parameters"]["spatial_periodicity"] = "x";
		} else if (periodic_y_) {
			j["parameters"]["spatial_periodicity"] = "y";
		} else if (periodic_z_) {
			j["parameters"]["spatial_periodicity"] = "z";
		} else {
			j["parameters"]["spatial_periodicity"] = "";
		}
		j["parameters"]["separate_sexes"] = sex_enabled_ ? true : false;
		j["parameters"]["nucleotide_based"] = nucleotide_based_ ? true : false;
		
		j["parameters"]["chromosomes"] = nlohmann::json::array();
		for (Chromosome *chromosome : chromosomes_)
		{
			nlohmann::json chromosome_info;
			
			chromosome_info["index"] = chromosome->Index();
			chromosome_info["id"] = chromosome->ID();
			chromosome_info["symbol"] = chromosome->Symbol();
			if (chromosome->Name().length() > 0)
				chromosome_info["name"] = chromosome->Name();
			chromosome_info["type"] = StringForChromosomeType(chromosome->Type());
			
			j["parameters"]["chromosomes"].push_back(chromosome_info);
			
			if (p_chromosome_index == chromosome->Index())		// true for the chromosome being written
			{
				// write out all the same information again in a key called "this_chromosome"; this way the
				// user can trivially get the info for the chromosome represented by the file; note that a
				// no-genetics model will have a chromosomes key with an empty array, and no this_chromosome,
				// but a no-genetics model can't write a tree sequence anyway, so that is moot.
				j["parameters"]["this_chromosome"] = chromosome_info;
			}
		}
		
		if (p_include_model)
			j["parameters"]["model"] = scriptString;				// made model optional in file_version 0.4
		j["parameters"]["model_hash"] = scriptHashString;			// added model_hash in file_version 0.4
		j["parameters"]["seed"] = community_.original_seed_;
		
		j["metadata"]["individuals"]["flags"]["16"]["name"] = "SLIM_TSK_INDIVIDUAL_ALIVE";
		j["metadata"]["individuals"]["flags"]["16"]["description"] = "the individual was alive at the time the file was written";
		j["metadata"]["individuals"]["flags"]["17"]["name"] = "SLIM_TSK_INDIVIDUAL_REMEMBERED";
		j["metadata"]["individuals"]["flags"]["17"]["description"] = "the individual was requested by the user to be permanently remembered";
		j["metadata"]["individuals"]["flags"]["18"]["name"] = "SLIM_TSK_INDIVIDUAL_RETAINED";
		j["metadata"]["individuals"]["flags"]["18"]["description"] = "the individual was requested by the user to be retained only if its nodes continue to exist in the tree sequence";
		
		// We save this information out only for runs at the command line.  This data might not be available on
		// all platforms; when it is unavailable, the key will be omitted.  We always have elapsed wall time.
#ifndef SLIMGUI
		double user_time, sys_time;
		size_t peak_RSS = Eidos_GetPeakRSS();
		
		Eidos_GetUserSysTime(&user_time, &sys_time);
		
		j["resources"]["elapsed_time"] = Eidos_WallTimeSeconds();
		if (user_time > 0.0)
			j["resources"]["user_time"] = user_time;
		if (sys_time > 0.0)
			j["resources"]["sys_time"] = sys_time;
		if (peak_RSS > 0)
			j["resources"]["max_memory"] = peak_RSS;
#endif
		
		if (p_use_newlines)
			provenance_str = j.dump(4);
		else
			provenance_str = j.dump();
	} catch (const std::exception &e) {
		EIDOS_TERMINATION << "ERROR (Species::WriteProvenanceTable): a JSON string could not be generated for tree-sequence provenance due to an error: '" << e.what() << "'." << EidosTerminate();
	} catch (...) {
		EIDOS_TERMINATION << "ERROR (Species::WriteProvenanceTable): a JSON string could not be generated for tree-sequence provenance due to an unknown error." << EidosTerminate();
	}
	
	//std::cout << "Provenance output: \n" << provenance_str << std::endl;
	
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buffer, timestamp_size, "%Y-%m-%dT%H:%M:%S", tm_info);
	
	ret = tsk_provenance_table_add_row(&p_tables->provenances, buffer, (tsk_size_t)strlen(buffer), provenance_str.c_str(), (tsk_size_t)provenance_str.length());
	if (ret < 0) handle_error("tsk_provenance_table_add_row", ret);
}

void Species::_MungeIsNullNodeMetadataToIndex0(TreeSeqInfo &p_treeseq, int original_chromosome_index)
{
	// This shifts is_vacant metadata bits in the node table from an original index (the chromosome index
	// being loaded from a file) to a final index of 0 (destined for a single-chromosome model).  This
	// is done by allocating a whole new metadata buffer, because in the general case the size of the
	// metadata records might actually be changing -- if the file has more than one byte of is_vacant
	// information per record.  So we will make new metadata and replace the old.  The new metadata
	// buffer uses one byte of is_vacant data, always, since we're loading into a single-chromosome model.
	// Note that this means the metadata schema might change too!
	tsk_table_collection_t &tables = p_treeseq.tables_;
	tsk_node_table_t &node_table = tables.nodes;
	HaplosomeMetadataRec *new_metadata_buffer = (HaplosomeMetadataRec *)calloc(node_table.num_rows, sizeof(HaplosomeMetadataRec));
	
	// these are for accessing the is_vacant bit in the original metadata
	int byte_index = original_chromosome_index / 8;
	int bit_shift = original_chromosome_index % 8;
	
	for (tsk_size_t row = 0; row < node_table.num_rows; ++row)
	{
		size_t node_metadata_length = node_table.metadata_offset[row + 1] - node_table.metadata_offset[row];
		size_t expected_min_metadata_length = sizeof(HaplosomeMetadataRec) + byte_index;	// 1 byte already counted in HaplosomeMetadataRec
		
		// check that the length is sufficient for the bits of original_index
		if (node_metadata_length < expected_min_metadata_length)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): unexpected node metadata length; this file cannot be read." << EidosTerminate();
		
		HaplosomeMetadataRec *node_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[row]);
		HaplosomeMetadataRec *new_metadata = new_metadata_buffer + row;
		
		new_metadata->haplosome_id_ = node_metadata->haplosome_id_;
		
		if ((node_metadata->is_vacant_[byte_index] >> bit_shift) & 0x01)
			new_metadata->is_vacant_[0] = 0x01;
	}
	
	// Now change the offsets to the new offsets; we do not allocate a new buffer,
	// because we just need the same number of rows that we already have.
	for (tsk_size_t row = 0; row <= node_table.num_rows; ++row)
		node_table.metadata_offset[row] = row * sizeof(HaplosomeMetadataRec);
	
	tsk_safe_free(node_table.metadata);
	
	node_table.metadata = (char *)new_metadata_buffer;
	node_table.metadata_length = node_table.num_rows * sizeof(HaplosomeMetadataRec);
	node_table.max_metadata_length = node_table.metadata_length;
	
	// need to fix the schema, because the number of bytes may have changed
	std::string tsk_node_metadata_schema = gSLiM_tsk_node_metadata_schema_FORMAT;
	size_t pos = tsk_node_metadata_schema.find("\"%d\"");
	std::string count_string = std::to_string(haplosome_metadata_is_vacant_bytes_);
	
	if (pos == std::string::npos)
		EIDOS_TERMINATION << "ERROR (Species::_MungeIsNullNodeMetadataToIndex0): (internal error) `%d` substring missing from gSLiM_tsk_node_metadata_schema_FORMAT." << EidosTerminate();
	
	tsk_node_metadata_schema.replace(pos, 4, count_string);		// replace %d in the format string with the byte count
	
	int ret = tsk_node_table_set_metadata_schema(&node_table,
												 tsk_node_metadata_schema.c_str(),
												 (tsk_size_t)tsk_node_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_node_table_set_metadata_schema", ret);
}

void Species::ReadTreeSequenceMetadata(TreeSeqInfo &p_treeseq, slim_tick_t *p_tick, slim_tick_t *p_cycle, SLiMModelType *p_model_type, int *p_file_version)
{
	// New provenance reading code, using the JSON for Modern C++ library (json.hpp); this
	// applies to file versions > 0.1.  The version 0.1 code was removed 24 Feb. 2025.
	
	tsk_table_collection_t &p_tables = p_treeseq.tables_;
	std::string model_type_str;
	std::string cycle_stage_str;
	long long tick_ll, gen_ll;
	int this_chromosome_id;
	int this_chromosome_index;
	std::string this_chromosome_symbol;
	std::string this_chromosome_type;
	bool chomosomes_key_present = false;
	
	try {
		////////////
		// Format 0.5 and later: using top-level metadata
		
		// Note: we *could* parse the metadata schema, but instead we'll just try parsing the metadata.
		// std::string metadata_schema_str(p_tables->metadata_schema, p_tables->metadata_schema_length);
		// nlohmann::json metadata_schema = nlohmann::json::parse(metadata_schema_str);

		std::string metadata_str(p_tables.metadata, p_tables.metadata_length);
		auto metadata = nlohmann::json::parse(metadata_str);
		
		//std::cout << metadata.dump(4) << std::endl;
		
		if (!metadata["SLiM"].contains("file_version"))
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the required metadata key 'file_version' is missing; this file cannot be read." << EidosTerminate();
		
		auto file_version = metadata["SLiM"]["file_version"];
		
		if ((file_version == SLIM_TREES_FILE_VERSION_PRENUC) ||
			(file_version == SLIM_TREES_FILE_VERSION_POSTNUC) ||
			(file_version == SLIM_TREES_FILE_VERSION_HASH) ||
			(file_version == SLIM_TREES_FILE_VERSION_META) ||
			(file_version == SLIM_TREES_FILE_VERSION_PREPARENT) ||
			(file_version == SLIM_TREES_FILE_VERSION_PRESPECIES) ||
			(file_version == SLIM_TREES_FILE_VERSION_SPECIES))
		{
			// SLiM 5.0 breaks backward compatibility with earlier file versions
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the version of this file appears to be too old to be read, or the file is corrupted; you can try using pyslim to bring an old file version forward to the current version, or generate a new file with the current version of SLiM or pyslim." << EidosTerminate();
		}
		else if (file_version == SLIM_TREES_FILE_VERSION)
		{
			*p_file_version = 9;
		}
		else
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): this .trees file was generated by an unrecognized version of SLiM or pyslim (internal file version " << file_version << "); this file cannot be read." << EidosTerminate();
		
		// We test for some keys if they are new or optional, but assume that others must be there, such as "model_type".
		// If we fetch a key and it is missing, nhlohmann::json raises and we end up in the provenance fallback code below.
		// That indicates that we're reading an old file version, which we no longer support in SLiM 5.
		// BCH 2/24/2025: I'm shifting towards testing for every key before fetching, in order to give better error messages.
		if (!metadata["SLiM"].contains("model_type"))
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the required metadata key 'model_type' is missing; this file cannot be read." << EidosTerminate();
		
		model_type_str = metadata["SLiM"]["model_type"];
		
		if (!metadata["SLiM"].contains("tick"))
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the required metadata key 'tick' is missing; this file cannot be read." << EidosTerminate();
		
		tick_ll = metadata["SLiM"]["tick"];
		
		// "cycle" is optional and now defaults to the tick (it used to fall back to the old "generation" key)
		if (metadata["SLiM"].contains("cycle"))
			gen_ll = metadata["SLiM"]["cycle"];
		else
			gen_ll = tick_ll;
		
		// "stage" is optional, and is used below only for validation; it provides an extra layer of safety
		if (metadata["SLiM"].contains("stage"))
			cycle_stage_str = metadata["SLiM"]["stage"];
		
		/*if (metadata["SLiM"].contains("name"))
		{
			// If a species name is present, it must match our own name; can't load data across species, as a safety measure
			// If users find this annoying, it can be relaxed; nothing really depends on it
			// BCH 5/12/2022: OK, it is already annoying; disabling this check for now
			std::string metadata_name = metadata["SLiM"]["name"];
			
			if (metadata_name != name_)
				EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): this .trees file represents a species named " << metadata_name << ", which does not match the name of the target species, " << name_ << "; species names must match." << EidosTerminate();
		}*/
		
		if (metadata["SLiM"].contains("description"))
		{
			// If a species description is present and non-empty, it replaces our own description
			std::string metadata_description = metadata["SLiM"]["description"];
			
			if (metadata_description.length())
				description_ = metadata_description;
		}
		
		// The "this_chromosome" key is required, as are the keys within it
		auto &this_chromosome_metadata = metadata["SLiM"]["this_chromosome"];
		
		if (!this_chromosome_metadata.is_object())
			SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the 'this_chromosome' metadata key must be a JSON object." << std::endl;
		if (!this_chromosome_metadata.contains("id"))
			SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the required metadata key 'id' is missing from the 'this_chromosome' metadata entry." << std::endl;
		if (!this_chromosome_metadata.contains("index"))
			SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the required metadata key 'index' is missing from the 'this_chromosome' metadata entry." << std::endl;
		if (!this_chromosome_metadata.contains("symbol"))
			SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the required metadata key 'symbol' is missing from the 'this_chromosome' metadata entry." << std::endl;
		if (!this_chromosome_metadata.contains("type"))
			SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the required metadata key 'type' is missing from the 'this_chromosome' metadata entry." << std::endl;
		
		this_chromosome_id = this_chromosome_metadata["id"];
		this_chromosome_index = this_chromosome_metadata["index"];
		this_chromosome_symbol = this_chromosome_metadata["symbol"];
		this_chromosome_type = this_chromosome_metadata["type"];
		
		// The "chromosomes" key is optional, but if provided, it has to make sense
		if (metadata["SLiM"].contains("chromosomes"))
		{
			chomosomes_key_present = true;
			
			// We validate the whole "chromosomes" key against the whole model, to make sure everything is as expected
			auto &chromosomes_metadata = metadata["SLiM"]["chromosomes"];
			
			if (!chromosomes_metadata.is_array())
				SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the 'chromosomes' metadata key must be an array." << std::endl;
			if (chromosomes_metadata.size() != Chromosomes().size())
				SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the number of entries in the 'chromosomes' metadata key does not match the number of chromosomes in the model." << std::endl;
			
			for (std::size_t chromosomes_index = 0; chromosomes_index < Chromosomes().size(); ++chromosomes_index)
			{
				Chromosome *chromosome = Chromosomes()[chromosomes_index];
				auto &one_chromosome_metadata = chromosomes_metadata[chromosomes_index];
				
				if (!one_chromosome_metadata.contains("id"))
					SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the required metadata key 'id' is missing from a 'chromosomes' metadata entry; if 'chromosomes' is provided at all, it must be complete." << std::endl;
				if (!one_chromosome_metadata.contains("symbol"))
					SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the required metadata key 'symbol' is missing from a 'chromosomes' metadata entry; if 'chromosomes' is provided at all, it must be complete." << std::endl;
				if (!one_chromosome_metadata.contains("type"))
					SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the required metadata key 'type' is missing from a 'chromosomes' metadata entry; if 'chromosomes' is provided at all, it must be complete." << std::endl;
				
				int one_chromosome_id = one_chromosome_metadata["id"];
				std::string one_chromosome_symbol = one_chromosome_metadata["symbol"];
				std::string one_chromosome_type = one_chromosome_metadata["type"];
				
				if (one_chromosome_id != chromosome->ID())
					SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the id for the entry at index " << chromosomes_index << " in the 'chromosomes' metadata key does not match the corresponding chromosome in the model." << std::endl;
				if (one_chromosome_symbol != chromosome->Symbol())
					SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the symbol for the entry at index " << chromosomes_index << " in the 'chromosomes' metadata key does not match the corresponding chromosome in the model." << std::endl;
				if (one_chromosome_type != chromosome->TypeString())
					SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the type for the entry at index " << chromosomes_index << " in the 'chromosomes' metadata key does not match the corresponding chromosome in the model." << std::endl;
			}
		}
	} catch (...) {
		///////////////////////
		// Previous formats: everything is in provenance
		
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the version of this file appears to be too old to be read, or the file is corrupted; you can try using pyslim to bring an old file version forward to the current version, or generate a new file with the current version of SLiM." << EidosTerminate();
	}
	
	// check the model type; at the moment we do not require the model type to match what we are running, but we issue a warning on a mismatch
	if (((model_type_str == "WF") && (model_type_ != SLiMModelType::kModelTypeWF)) ||
		((model_type_str == "nonWF") && (model_type_ != SLiMModelType::kModelTypeNonWF)))
	{
		if (!gEidosSuppressWarnings)
			SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the model type of the .trees file (" << model_type_str << ") does not match the current model type." << std::endl;
	}
	
	if (model_type_str == "WF")
		*p_model_type = SLiMModelType::kModelTypeWF;
	else if (model_type_str == "nonWF")
		*p_model_type = SLiMModelType::kModelTypeNonWF;
	else
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): unrecognized model type ('" << model_type_str << "'); this file cannot be read." << EidosTerminate();
	
	// bounds-check the cycle and tick
	if ((gen_ll < 1) || (gen_ll > SLIM_MAX_TICK))
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): cycle value (" << gen_ll << ") out of range; this file cannot be read." << EidosTerminate();
	if ((tick_ll < 1) || (tick_ll > SLIM_MAX_TICK))
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): tick value (" << tick_ll << ") out of range; this file cannot be read." << EidosTerminate();
	
	*p_tick = (slim_tick_t)tick_ll;
	*p_cycle = (slim_tick_t)gen_ll;
	
	// check the cycle stage for a match, warn on mismatch; this is new in SLiM 4, seems like a good idea
	if (cycle_stage_str.length())
	{
		if (cycle_stage_str == "first")
		{
			if ((community_.CycleStage() != SLiMCycleStage::kWFStage0ExecuteFirstScripts) &&
				(community_.CycleStage() != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts))
				SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the cycle stage of the .trees file ('first') does not match the current cycle stage." << std::endl;
		}
		else if (cycle_stage_str == "early")
		{
			if ((community_.CycleStage() != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) &&
				(community_.CycleStage() != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts))
				SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the cycle stage of the .trees file ('early') does not match the current cycle stage." << std::endl;
		}
		else if (cycle_stage_str == "late")
		{
			if ((community_.CycleStage() != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
				(community_.CycleStage() != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
				SLIM_ERRSTREAM << "#WARNING (Species::ReadTreeSequenceMetadata): the cycle stage of the .trees file ('late') does not match the current cycle stage." << std::endl;
		}
		else
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): a cycle stage is given in metadata, but its value ('" << cycle_stage_str << "') is unrecognized." << EidosTerminate();
	}
	
	// check the "this_chromosome" metadata information against the chromosome that p_treeseq says we're reading
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	Chromosome *chromosome = Chromosomes()[chromosome_index];
	
	if (this_chromosome_id != chromosome->ID())
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the chromosome id provided in the 'this_chromosome' key (" << this_chromosome_id << ") does not match the id (" << chromosome->ID() << ") of the corresponding chromosome in the model." << EidosTerminate();
	if (this_chromosome_type != chromosome->TypeString())
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the chromosome type provided in the 'this_chromosome' key (" << this_chromosome_type << ") does not match the type (" << chromosome->TypeString() << ") of the corresponding chromosome in the model." << EidosTerminate();
	if (this_chromosome_symbol != chromosome->Symbol())
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the chromosome symbol provided in the 'this_chromosome' key (" << this_chromosome_symbol << ") does not match the symbol (" << chromosome->Symbol() << ") of the corresponding chromosome in the model." << EidosTerminate();
	
	// Check the chromosome index; when loading a multi-chromosome set, we normally require indices to match
	// - one exception is that you can load a chromosome from any index into a single-chromosome model
	// - another exception is that, as a special nod to assembling a set of externally-generated (i.e., msprime)
	//   simulations into a multi-chromosome set, we allow the index to be 0 in "this_chromosome", if and only if
	//   the "chromosomes" key is not present; the file then represents a single chromosome that doesn't know
	//   that it's part of a larger set.
	// In both of these exceptional cases, we need to make sure that we look up bits in the is_vacant flags of
	// node metadata using the chromosome index stated in the file being loaded, NOT the chromosome index that the
	// data is being loaded into!  In the first case, we can simply munge the node table metadata right now, to
	// have the is_vacant bits in the position for index 0.  In the second case, it is much trickier because we
	// have a shared node table, and we need to fix the is_vacant bits in the shared table as well.
	if (this_chromosome_index != chromosome->Index())
	{
		if (Chromosomes().size() == 1)
		{
			// We are loading into a single-chromosome model.  We need to munge the incoming is_vacant 
			// metadata to move is_vacant flags from the file's index down to index 0.
			_MungeIsNullNodeMetadataToIndex0(p_treeseq, this_chromosome_index);
		}
		else if ((this_chromosome_index == 0) && !chomosomes_key_present)
		{
			// We are loading a file that has is_vacant information at index 0, into a different index
			// in a multi-chromosome model.  This is allowed when chomosomes_key_present is false,
			// because this is the pattern we get from loading an msprime simulation in without
			// setting up all the multi-chrom metadata completely.  Doing this requires that we do more
			// complex munging, which is not yet supported.  In particular, we will need to modify the
			// shared node table, not just the node table being loaded; and we will need to in some way
			// manage the equality check between the shared node table and our node table, which would
			// fail since they will not match (due to the presence of other chromosomes).
			// FIXME MULTICHROM: I need a test case before I can do this; waiting for one from Peter.
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): (internal error) loading into a different chromosome index is not yet supported." << EidosTerminate();
		}
		else
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): the chromosome index provided in the 'this_chromosome' key (" << this_chromosome_index << ") does not match the index (" << (unsigned int)(chromosome->Index()) << ") of the corresponding chromosome in the model." << EidosTerminate();
	}
}

void Species::_CreateDirectoryForMultichromArchive(std::string resolved_user_path, bool p_overwrite_directory)
{
	// Eidos_CreateDirectory() errors if the path already exists, but for WriteTreeSequence(),
	// we want to replace an existing directory (but not a file); it would be too annoying
	// if we didn't, for successive runs of the same model.  The archive is, in effect, one
	// file.  However, we want to be very careful in doing this, since it is dangerous!
	int resolved_user_path_length = (int)resolved_user_path.length();
	bool resolved_user_path_ends_in_slash = (resolved_user_path_length > 0) && (resolved_user_path[resolved_user_path_length-1] == '/');
	
	struct stat file_info;
	bool path_exists = (stat(resolved_user_path.c_str(), &file_info) == 0);
	
	if (path_exists)
	{
		bool is_directory = !!(file_info.st_mode & S_IFDIR);
		
		if (is_directory)
		{
			if (!p_overwrite_directory)
				EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path; you may pass overwriteDirectory=T to override this error and replace the existing directory, but note that this is quite a dangerous operation (treeSeqOutput() will still refuse to overwrite the existing directory if it contains any files besides .trees files, for additional safety)." << EidosTerminate();
			
			// remove() requires that the directory be empty, so we need to remove all files inside it.
			// For safety, we first pass over all files and verify that they are not directories, and
			// that their filenames all end in .trees.  If there is any other cruft inside the directory,
			// we will refuse to delete it.
			DIR *dp = opendir(resolved_user_path.c_str());
			
			if (dp != NULL)
			{
				while (true)
				{
					errno = 0;
					struct dirent *ep = readdir(dp);
					int error = errno;
					
					if (!ep)
					{
						if (error)
						{
							(void)closedir(dp);
							EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and could not be read." << EidosTerminate();
						}
						break;
					}
					
					std::string interior_filename_base = ep->d_name;
					
					if ((interior_filename_base == ".") || (interior_filename_base == ".."))
						continue;
					
					std::string interior_filename = resolved_user_path + (resolved_user_path_ends_in_slash ? "" : "/") + interior_filename_base;		// NOLINT(*-inefficient-string-concatenation) : this is fine
					
					bool file_exists = (stat(interior_filename.c_str(), &file_info) == 0);
					
					if (!file_exists)
					{
						(void)closedir(dp);
						EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and could not be read." << EidosTerminate();
					}
					
					is_directory = !!(file_info.st_mode & S_IFDIR);
					
					if (is_directory)
					{
						(void)closedir(dp);
						EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and contains a subdirectory within it (" << interior_filename_base << "); overwriting the path is not safe." << EidosTerminate();
					}
					if (!Eidos_string_hasSuffix(interior_filename, ".trees") && (interior_filename_base != ".DS_Store"))
					{
						(void)closedir(dp);
						EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and contains a file within it (" << interior_filename_base << ") that is not a .trees file; overwriting the path is not safe." << EidosTerminate();
					}
				}
				
				(void)closedir(dp);
				
				// OK, everything in the directory seems eligible for removal; let's try.  Note that the logic
				// below duplicates the logic above, to avoid race conditions in which the filesystem changes.
				dp = opendir(resolved_user_path.c_str());
				
				if (dp != NULL)
				{
					while (true)
					{
						errno = 0;
						struct dirent *ep = readdir(dp);
						int error = errno;
						
						if (!ep)
						{
							if (error)
							{
								(void)closedir(dp);
								EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and could not be read." << EidosTerminate();
							}
							break;
						}
						
						std::string interior_filename_base = ep->d_name;
						
						if ((interior_filename_base == ".") || (interior_filename_base == ".."))
							continue;
						
						std::string interior_filename = resolved_user_path + (resolved_user_path_ends_in_slash ? "" : "/") + interior_filename_base;		// NOLINT(*-inefficient-string-concatenation) : this is fine
						
						bool file_exists = (stat(interior_filename.c_str(), &file_info) == 0);
						
						if (!file_exists)
						{
							(void)closedir(dp);
							EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and could not be read." << EidosTerminate();
						}
						
						is_directory = !!(file_info.st_mode & S_IFDIR);
						
						if (is_directory)
						{
							(void)closedir(dp);
							EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and contains a subdirectory within it (" << interior_filename_base << "); overwriting the path is not safe." << EidosTerminate();
						}
						if (!Eidos_string_hasSuffix(interior_filename, ".trees") && (interior_filename_base != ".DS_Store"))
						{
							(void)closedir(dp);
							EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and contains a file within it (" << interior_filename_base << ") that is not a .trees file; overwriting the path is not safe." << EidosTerminate();
						}
						
						bool success = (remove(interior_filename.c_str()) == 0);
						
						if (!success)
						{
							(void)closedir(dp);
							EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and contains a file within it (" << interior_filename_base << ") that could not be removed." << EidosTerminate();
						}
					}
					
					(void)closedir(dp);
				}
				else
					EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and could not be overwritten." << EidosTerminate();
			}
			else
				EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and could not be overwritten." << EidosTerminate();
			
			bool success = (remove(resolved_user_path.c_str()) == 0);
			
			if (!success)
				EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a directory already exists at that path and could not be removed." << EidosTerminate();
		}
		else
			EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because a file already exists at that path." << EidosTerminate();
	}
	
	// If we made it to here, there should no longer be a directory at resolved_user_path
	std::string error_string;
	bool success = Eidos_CreateDirectory(resolved_user_path, &error_string);
	
	// Fatal error if we can't create the directory
	if (error_string.length())
		EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", because of error: " << error_string << "." << EidosTerminate();
	else if (!success)
		EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", for unknown reasons." << EidosTerminate();
}

void Species::WriteTreeSequence(std::string &p_recording_tree_path, bool p_simplify, bool p_include_model, EidosDictionaryUnretained *p_metadata_dict, bool p_overwrite_directory)
{
	int ret = 0;
	bool is_multichrom = (chromosomes_.size() > 1);
	
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// If this is a single-chromosome species, then write out the single tree sequence to the path;
	// otherwise, create p_recording_tree_path as a directory, and write out to that directory
	// Standardize the path, resolving a leading ~ and maybe other things
	std::string resolved_user_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(p_recording_tree_path));
	
	if (is_multichrom)
	{
		// For a multichromosome archive, we need to create the directory to hold it.  This call
		// will raise if there are any problems in doing so.
		_CreateDirectoryForMultichromArchive(resolved_user_path, p_overwrite_directory);
	}
	
	// Add a population (i.e., subpopulation) table to the table collection; subpopulation information
	// comes from the time of output.  This needs to happen before simplify/sort.  We write the population
	// table once, into treeseq_[0], and then share it into the other tree sequences below.  Note that
	// SimplifyAllTreeSequences() also writes the population table, so this call is redundant when
	// p_simplify is true, but I'm leaving it this way for redundancy, to prevent future bugs, and
	// because I'm not 100% certain that we didn't do it this way originally for a good reason.  :->
	WritePopulationTable(&treeseq_[0].tables_);
	
	// First we simplify, on the original table collection; we considered doing this on the copy,
	// but then the copy takes longer and the simplify's work is lost, and there doesn't seem to
	// be a compelling case for leaving the original tables unsimplified.  Note that Peter has done
	// a check that calling treeSeqOutput() in the middle of a run does not change the result, although
	// it *does* change the order of the rows; see https://github.com/MesserLab/SLiM/issues/209
	if (p_simplify)
	{
		SimplifyAllTreeSequences();
	}
	
	for (Chromosome *chromosome : chromosomes_)
	{
		slim_chromosome_index_t chromosome_index = chromosome->Index();
		TreeSeqInfo &chromosome_tsinfo = treeseq_[chromosome_index];
		tsk_table_collection_t &chromosome_tables = chromosome_tsinfo.tables_;
		
		// Copy in the shared tables (node, individual, population) at this point, so the shared tables then get
		// copied below; we will be modifying the tables, and don't want our modification to go into the original
		// shared tables, which we are not allowed to change.
		if (chromosome_index > 0)
			CopySharedTablesIn(chromosome_tables);
		
		// Copy the table collection so that modifications we do for writing don't affect the original tables.
		// Note that there's a lot of work below to clean up the individuals table and node table for saving.
		// Those tables are shared.  We don't want to do this cleanup in the original tables, since that would
		// modify our recording state I guess; but I think this cleanup will be the same for every chromosome,
		// so technically we could do this work just once, I think (?), and share the processed tables across
		// all the chromosomes.  I've chosen not to pursue that idea, because I don't see a path to doing it
		// without increasing the high-water mark for the memory usage of this code, which is very important
		// to keep low.  Anyhow, maybe this is unimportant since it is only overhead at save time, and is
		// probably not a hotspot.
		tsk_table_collection_t output_tables;
		ret = tsk_table_collection_copy(&chromosome_tables, &output_tables, 0);
		if (ret < 0) handle_error("tsk_table_collection_copy", ret);
		
		// We can unshare the shared tables in the original table collection immediately, zeroing them out.
		if (chromosome_index > 0)
			DisconnectCopiedSharedTables(chromosome_tables);
		
		// Sort and deduplicate; we don't need to do this if we simplified above, since simplification does these steps
		if (!p_simplify)
		{
			int flags = TSK_NO_CHECK_INTEGRITY;
#if DEBUG
			flags = 0;
#endif
			ret = tsk_table_collection_sort(&output_tables, /* edge_start */ NULL, /* flags */ flags);
			if (ret < 0) handle_error("tsk_table_collection_sort", ret);
			
			// Remove redundant sites we added
			ret = tsk_table_collection_deduplicate_sites(&output_tables, 0);
			if (ret < 0) handle_error("tsk_table_collection_deduplicate_sites", ret);
		}
		
		// Add in the mutation.parent information; valid tree sequences need parents, but we don't keep them while running
		ret = tsk_table_collection_build_index(&output_tables, 0);
		if (ret < 0) handle_error("tsk_table_collection_build_index", ret);
		ret = tsk_table_collection_compute_mutation_parents(&output_tables, TSK_NO_CHECK_INTEGRITY);
		if (ret < 0) handle_error("tsk_table_collection_compute_mutation_parents", ret);
		
		{
			// Create a local hash table for pedigree IDs to individuals table indices.  If we simplified, that validated
			// tabled_individuals_hash_ as a side effect, so we can copy that as a base; otherwise, we make one from scratch.
			// Note that this hash table is used only for AddLiveIndividualsToIndividualsTable() below; after that we reorder
			// the individuals table, so we'll make another hash table for AddParentsColumnForOutput(), unfortunately.
			INDIVIDUALS_HASH local_individuals_lookup;
			
			if (p_simplify)
				local_individuals_lookup = tabled_individuals_hash_;		// copies
			else
				BuildTabledIndividualsHash(&output_tables, &local_individuals_lookup);
			
			// Add information about the current cycle to the individual table; 
			// this modifies "remembered" individuals, since information comes from the
			// time of output, not creation
			AddLiveIndividualsToIndividualsTable(&output_tables, &local_individuals_lookup);
		}
		
		// We need the individual table's order, for alive individuals, to match that of
		// SLiM so that when we read back in it doesn't cause a reordering as a side effect
		// all other individuals in the table will be retained, at the end
		std::vector<int> individual_map;
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *individual : subpop->parent_individuals_)
			{
				tsk_id_t node_id = individual->TskitNodeIdBase();
				tsk_id_t ind_id = output_tables.nodes.individual[node_id];
				
				individual_map.emplace_back(ind_id);
			}
		}
		
		ReorderIndividualTable(&output_tables, individual_map, true);
		
		// Now that the table is reordered, we can build the parents column of the individuals table
		// This requires a new pedigree id to tskid lookup table, which we construct here.
		{
			INDIVIDUALS_HASH local_individuals_lookup;
			
			BuildTabledIndividualsHash(&output_tables, &local_individuals_lookup);
			AddParentsColumnForOutput(&output_tables, &local_individuals_lookup);
		}
		
		// Rebase the times in the nodes to be in tskit-land; see _InstantiateSLiMObjectsFromTables() for the inverse operation
		// BCH 4/4/2019: switched to using tree_seq_tick_ to avoid a parent/child timestamp conflict
		// This makes sense; as far as tree-seq recording is concerned, tree_seq_tick_ is the time counter
		slim_tick_t time_adjustment = community_.tree_seq_tick_;
		
		for (size_t node_index = 0; node_index < output_tables.nodes.num_rows; ++node_index)
			output_tables.nodes.time[node_index] += time_adjustment;
		
		for (size_t mut_index = 0; mut_index < output_tables.mutations.num_rows; ++mut_index)
			output_tables.mutations.time[mut_index] += time_adjustment;
		
		// Add a row to the Provenance table to record current state; text format does not allow newlines in the entry,
		// so we don't prettyprint the JSON when going to text, as a quick fix that avoids quoting the newlines etc.
		WriteProvenanceTable(&output_tables, /* p_use_newlines */ true, p_include_model, chromosome->Index());
		
		// Add top-level metadata and metadata schema
		WriteTreeSequenceMetadata(&output_tables, p_metadata_dict, chromosome->Index());
		
		// Set the simulation time unit, in case that is useful to someone.  This is set up in initializeTreeSeq().
		ret = tsk_table_collection_set_time_units(&output_tables, community_.treeseq_time_unit_.c_str(), community_.treeseq_time_unit_.length());
		if (ret < 0) handle_error("tsk_table_collection_set_time_units", ret);
		
		// Write out the copied tables
		{
			// derived state data must be in ASCII (or unicode) on disk, according to tskit policy
			DerivedStatesToAscii(&output_tables);
			
			// In nucleotide-based models, put an ASCII representation of the reference sequence into the tables
			if (nucleotide_based_)
			{
				std::size_t buflen = chromosome->AncestralSequence()->size();
				char *buffer = (char *)malloc(buflen);
				
				if (!buffer)
					EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
				
				chromosome->AncestralSequence()->WriteNucleotidesToBuffer(buffer);
				
				ret = tsk_reference_sequence_takeset_data(&output_tables.reference_sequence, buffer, buflen);		// tskit now owns buffer
				if (ret < 0) handle_error("tsk_reference_sequence_takeset_data", ret);
			}
			
			// With one chromosome, we write out to resolved_user_path directly; with more than one, we
			// created a directory at resolved_user_path above, and now we generate a generic filename
			std::string output_path;
			
			if (chromosomes_.size() == 1)
				output_path = resolved_user_path;
			else
				output_path = resolved_user_path + "/chromosome_" + chromosome->Symbol() + ".trees";
			
			ret = tsk_table_collection_dump(&output_tables, output_path.c_str(), 0);
			if (ret < 0) handle_error("tsk_table_collection_dump", ret);
		}
		
		// Done with our tables copy
		ret = tsk_table_collection_free(&output_tables);
		if (ret < 0) handle_error("tsk_table_collection_free", ret);
	}
}


void Species::FreeTreeSequence()
{
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::FreeTreeSequence): (internal error) FreeTreeSequence() called when tree-sequence recording is not enabled." << EidosTerminate();
	
	if (tables_initialized_)
	{
		// Free any tree-sequence recording stuff that has been allocated; called when Species is getting deallocated,
		// and also when we're wiping the slate clean with something like readFromPopulationFile().
		bool first = true;
		for (TreeSeqInfo &tsinfo : treeseq_)
		{
			// the node, individual, and population tables are shared; avoid doing a double free
			// (I don't think any of the shared tables should be copied at this point anyway,
			// though; maybe there should be an assert here to that effect?)
			if (!first)
				DisconnectCopiedSharedTables(tsinfo.tables_);
			
			tsk_table_collection_free(&tsinfo.tables_);
			first = false;
		}
		
		treeseq_.resize(0);
		
		remembered_nodes_.clear();
		tabled_individuals_hash_.clear();
		tables_initialized_ = false;
	}
}

void Species::RecordAllDerivedStatesFromSLiM(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::RecordAllDerivedStatesFromSLiM): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This method does nothing but record mutations, so...
	if (!recording_mutations_)
		return;
	
	// This is called when new tree sequence tables need to be built to correspond to the current state of SLiM, such as
	// after handling a readFromPopulationFile() call.  It is guaranteed by the caller of this method that any old tree
	// sequence recording stuff has been freed with a call to FreeTreeSequence(), and then a new recording session has
	// been initiated with AllocateTreeSequenceTables(); it might be good for this method to do a sanity check that all
	// of the recording tables are indeed allocated but empty, I guess.  Every extant individual and haplosome has been
	// recorded already, with calls to SetCurrentNewIndividual() and RecordNewHaplosome(), in the readPopulationFile()
	// code. Our job is just to record the mutations ("derived states") in the SLiM data into the tree sequence.  Note
	// that new mutations will not be added one at a time, when they are stacked; each block of stacked mutations in a
	// haplosome will be added with a single derived state call here.
	int haplosome_count_per_individual = HaplosomeCountPerIndividual();
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		
		for (Individual *individual : subpop->parent_individuals_)
		{
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
			{
				Haplosome *haplosome = individual->haplosomes_[haplosome_index];
				
				if (!haplosome->IsNull())
					haplosome->record_derived_states(this);
			}
		}
	}
}

void Species::MetadataForMutation(Mutation *p_mutation, MutationMetadataRec *p_metadata)
{
	static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec has changed size; this code probably needs to be updated");
	
	if (!p_mutation || !p_metadata)
		EIDOS_TERMINATION << "ERROR (Species::MetadataForMutation): (internal error) bad parameters to MetadataForMutation()." << EidosTerminate();
	
	p_metadata->mutation_type_id_ = p_mutation->mutation_type_ptr_->mutation_type_id_;
	
	// FIXME MULTITRAIT: We need to figure out where we're going to put multitrait information in .trees
	// For now we just write out the effect for trait 0, but we need the dominance coeff too, and we need
	// it for all traits in the model not just trait 0; this design is not going to work. See
	// https://github.com/MesserLab/SLiM/issues/569
	MutationBlock *mutation_block = p_mutation->mutation_type_ptr_->mutation_block_;
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(p_mutation);
	
	p_metadata->selection_coeff_ = mut_trait_info[0].effect_size_;
	
	p_metadata->subpop_index_ = p_mutation->subpop_index_;
	p_metadata->origin_tick_ = p_mutation->origin_tick_;
	p_metadata->nucleotide_ = p_mutation->nucleotide_;
}

void Species::MetadataForSubstitution(Substitution *p_substitution, MutationMetadataRec *p_metadata)
{
	static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec has changed size; this code probably needs to be updated");
	
	if (!p_substitution || !p_metadata)
		EIDOS_TERMINATION << "ERROR (Species::MetadataForSubstitution): (internal error) bad parameters to MetadataForSubstitution()." << EidosTerminate();
	
	p_metadata->mutation_type_id_ = p_substitution->mutation_type_ptr_->mutation_type_id_;
	
	// FIXME MULTITRAIT: We need to figure out where we're going to put multitrait information in .trees
	// For now we just write out the effect for trait 0, but we need the dominance coeff too, and we need
	// it for all traits in the model not just trait 0; this design is not going to work.  See
	// https://github.com/MesserLab/SLiM/issues/569
	p_metadata->selection_coeff_ = p_substitution->trait_info_[0].effect_size_;
	
	p_metadata->subpop_index_ = p_substitution->subpop_index_;
	p_metadata->origin_tick_ = p_substitution->origin_tick_;
	p_metadata->nucleotide_ = p_substitution->nucleotide_;
}

void Species::MetadataForIndividual(Individual *p_individual, IndividualMetadataRec *p_metadata)
{
	static_assert(sizeof(IndividualMetadataRec) == 40, "IndividualMetadataRec has changed size; this code probably needs to be updated");
	
	if (!p_individual || !p_metadata)
		EIDOS_TERMINATION << "ERROR (Species::MetadataForIndividual): (internal error) bad parameters to MetadataForIndividual()." << EidosTerminate();
	
	p_metadata->pedigree_id_ = p_individual->PedigreeID();
	p_metadata->pedigree_p1_ = p_individual->Parent1PedigreeID();
	p_metadata->pedigree_p2_ = p_individual->Parent2PedigreeID();
	p_metadata->age_ = p_individual->age_;
	p_metadata->subpopulation_id_ = p_individual->subpopulation_->subpopulation_id_;
	p_metadata->sex_ = (int32_t)p_individual->sex_;		// IndividualSex, but int32_t in the record
	
	p_metadata->flags_ = 0;
	if (p_individual->migrant_)
		p_metadata->flags_ |= SLIM_INDIVIDUAL_METADATA_MIGRATED;
}

void Species::CheckTreeSeqIntegrity(void)
{
	// Here we call tskit to check the integrity of the tree-sequence tables themselves – not against
	// SLiM's parallel data structures (done in CrosscheckTreeSeqIntegrity()), just on their own.
	for (TreeSeqInfo &tsinfo : treeseq_)
	{
		// BCH 2/25/2025: We need to share tables in, for chromosomes after the first
		if (tsinfo.chromosome_index_ > 0)
			CopySharedTablesIn(tsinfo.tables_);
		
		int ret = tsk_table_collection_check_integrity(&tsinfo.tables_, TSK_NO_CHECK_POPULATION_REFS);
		if (ret < 0) handle_error("tsk_table_collection_check_integrity()", ret);
		
		if (tsinfo.chromosome_index_ > 0)
			DisconnectCopiedSharedTables(tsinfo.tables_);
	}
}

void Species::CrosscheckTreeSeqIntegrity(void)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Species::CrosscheckTreeSeqIntegrity(): illegal when parallel");
	
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// first crosscheck the substitutions multimap against SLiM's substitutions vector
	{
		std::vector<Substitution *> vector_subs = population_.substitutions_;
		std::vector<Substitution *> multimap_subs;
		
		for (auto entry : population_.treeseq_substitutions_map_)
			multimap_subs.emplace_back(entry.second);
		
		std::sort(vector_subs.begin(), vector_subs.end());
		std::sort(multimap_subs.begin(), multimap_subs.end());
		
		if (vector_subs != multimap_subs)
			EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) mismatch between SLiM substitutions and the treeseq substitution multimap." << EidosTerminate();
	}
	
	// crosscheck haplosomes and mutations one chromosome at a time
	for (Chromosome *chromosome : chromosomes_)
	{
		int chromosome_index = chromosome->Index();
		int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
		tsk_table_collection_t &chromosome_tables = treeseq_[chromosome_index].tables_;
		
		// get all haplosomes from all subpopulations for the focal chromosome; we will cross-check them all simultaneously
		static std::vector<Haplosome *> haplosomes;
		haplosomes.resize(0);
		
		for (auto pop_iter : population_.subpops_)
		{
			Subpopulation *subpop = pop_iter.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **ind_haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
					haplosomes.emplace_back(ind_haplosomes[haplosome_index]);
			}
		}
		
		// if we have no haplosomes to check, we return; we could check that the tree sequences are also empty, but we don't
		size_t haplosome_count = haplosomes.size();
		
		if (haplosome_count == 0)
			continue;
		
		// check for correspondence between SLiM's haplosomes and the tree_seq's nodes, including their metadata
		// FIXME unimplemented
		
		// if we're recording mutations, we can check all of them
		if (recording_mutations_)
		{
			// prepare to walk all the haplosomes by making HaplosomeWalker objects for them all
			static std::vector<HaplosomeWalker> haplosome_walkers;
			haplosome_walkers.clear();
			haplosome_walkers.reserve(haplosome_count);
			
			for (Haplosome *haplosome : haplosomes)
				haplosome_walkers.emplace_back(haplosome);
			
			// Copy in the shared tables (node, individual, population) at this point, so the shared tables then get
			// copied below; we will be modifying the tables, and don't want our modification to go into the original
			// shared tables, which we are not allowed to change.
			if (chromosome_index > 0)
				CopySharedTablesIn(chromosome_tables);
			
			// Copy the table collection so that modifications we do for crosscheck don't affect the original tables.
			// FIXME this could be stack-local rather than malloced; not making that change now to keep the diffs simpler
			int ret;
			tsk_table_collection_t *tables_copy;
			
			tables_copy = (tsk_table_collection_t *)malloc(sizeof(tsk_table_collection_t));
			if (!tables_copy)
				EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			ret = tsk_table_collection_copy(&chromosome_tables, tables_copy, 0);
			if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_table_collection_copy()", ret);
			
			// We can unshare the shared tables in the original table collection immediately, zeroing them out.
			if (chromosome_index > 0)
				DisconnectCopiedSharedTables(chromosome_tables);
			
			// our tables copy needs to have a population table now, since this is required to build a tree sequence
			// we could build this once and reuse it across all the calls to this method for different chromosomes,
			// but I think's probably not worth the trouble; the overhead should be small.
			WritePopulationTable(tables_copy);
			
			// simplify before making our tree_sequence object; the sort and deduplicate and compute parents are required for the crosscheck, whereas the simplify
			// could perhaps be removed, which would cause the iteration over variants to visit a bunch of stuff unrelated to the current individuals.
			// this code is adapted from Species::_SimplifyTreeSequence(), but we don't need to update the TSK map table or the table position,
			// and we simplify down to just the extant individuals since we can't cross-check older individuals anyway...
			if (tables_copy->nodes.num_rows != 0)
			{
				std::vector<tsk_id_t> samples;
				
				for (auto iter : population_.subpops_)
				{
					Subpopulation *subpop = iter.second;
					
					for (Individual *ind : subpop->parent_individuals_)
					{
						Haplosome **ind_haplosomes = ind->haplosomes_;
						
						for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
							samples.emplace_back(ind->TskitNodeIdBase() + ind_haplosomes[haplosome_index]->chromosome_subposition_);
					}
				}
				
				tsk_flags_t flags = TSK_NO_CHECK_INTEGRITY;
#if DEBUG
				flags = 0;
#endif
				ret = tsk_table_collection_sort(tables_copy, /* edge_start */ NULL, /* flags */ flags);
				if (ret < 0) handle_error("tsk_table_collection_sort", ret);
				
				ret = tsk_table_collection_deduplicate_sites(tables_copy, 0);
				if (ret < 0) handle_error("tsk_table_collection_deduplicate_sites", ret);
				
				// crosscheck is not going to be parallelized, so we use different flags for simplify here than in
				// Species::_SimplifyTreeSequence(); in particular, we let it filter nodes and individuals for us
				// BCH 3/13/2025: changing TSK_SIMPLIFY_KEEP_UNARY to TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS,
				// since it is the correct flag; see discussion in https://github.com/MesserLab/SLiM/issues/487
				flags = TSK_SIMPLIFY_FILTER_SITES | TSK_SIMPLIFY_FILTER_INDIVIDUALS | TSK_SIMPLIFY_KEEP_INPUT_ROOTS;
				if (!retain_coalescent_only_) flags |= TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS;
				
				ret = tsk_table_collection_simplify(tables_copy, samples.data(), (tsk_size_t)samples.size(), flags, NULL);
				if (ret != 0) handle_error("tsk_table_collection_simplify", ret);
				
				// must build indexes before compute mutation parents
				ret = tsk_table_collection_build_index(tables_copy, 0);
				if (ret < 0) handle_error("tsk_table_collection_build_index", ret);
				
				ret = tsk_table_collection_compute_mutation_parents(tables_copy, TSK_NO_CHECK_INTEGRITY);
				if (ret < 0) handle_error("tsk_table_collection_compute_mutation_parents", ret);
				
			}
			
			// allocate and set up the tree_sequence object that contains all the tree sequences
			tsk_treeseq_t *ts;
			
			ts = (tsk_treeseq_t *)malloc(sizeof(tsk_treeseq_t));
			if (!ts)
				EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			ret = tsk_treeseq_init(ts, tables_copy, TSK_TS_INIT_BUILD_INDEXES);
			if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_treeseq_init()", ret);
			
			// allocate and set up the variant object we'll update as we walk along the sequence
			tsk_variant_t variant;
			
			ret = tsk_variant_init(
								   &variant, ts, NULL, 0, NULL, TSK_ISOLATED_NOT_MISSING);
			if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_variant_init()", ret);
			
			// crosscheck by looping through variants
			for (tsk_size_t i = 0; i < ts->tables->sites.num_rows; i++)
			{
				ret = tsk_variant_decode(&variant, (tsk_id_t)i, 0);
				if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_variant_decode()", ret);
				
				// Check this variant against SLiM.  A variant represents a site at which a tracked mutation exists.
				// The tsk_variant_t will tell us all the allelic states involved at that site, what the alleles are, and which haplosomes
				// in the sample are using them.  We will then check that all the haplosomes that the variant claims to involve have
				// the allele the variant attributes to them, and that no haplosomes contain any alleles at the position that are not
				// described by the variant.  The variants are returned in sorted order by position, so we can keep pointers into
				// every extant haplosome's mutruns, advance those pointers a step at a time, and check that everything matches at every
				// step.  Keep in mind that some mutations may have been fixed (substituted) or lost.
				slim_position_t variant_pos_int = (slim_position_t)variant.site.position;		// should be no loss of precision, fingers crossed
				
				// Get all the substitutions involved at this site, which should be present in every sample
				auto substitution_range_iter = population_.treeseq_substitutions_map_.equal_range(variant_pos_int);
				static std::vector<slim_mutationid_t> fixed_mutids;
				
				fixed_mutids.resize(0);
				for (auto substitution_iter = substitution_range_iter.first; substitution_iter != substitution_range_iter.second; ++substitution_iter)
					fixed_mutids.emplace_back(substitution_iter->second->mutation_id_);
				
				// Check all the haplosomes against the variant's belief about this site
				for (size_t haplosome_index = 0; haplosome_index < haplosome_count; haplosome_index++)
				{
					HaplosomeWalker &haplosome_walker = haplosome_walkers[haplosome_index];
					int32_t haplosome_variant = variant.genotypes[haplosome_index];
					tsk_size_t haplosome_allele_length = variant.allele_lengths[haplosome_variant];
					
					if (haplosome_allele_length % sizeof(slim_mutationid_t) != 0)
						EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) variant allele had length that was not a multiple of sizeof(slim_mutationid_t)." << EidosTerminate();
					haplosome_allele_length /= sizeof(slim_mutationid_t);
					
					//std::cout << "variant for haplosome: " << (int)haplosome_variant << " (allele length == " << haplosome_allele_length << ")" << std::endl;
					
					// BCH 4/29/2018: null haplosomes shouldn't ever contain any mutations, including fixed mutations
					if (haplosome_walker.WalkerHaplosome()->IsNull())
					{
						if (haplosome_allele_length == 0)
							continue;
						
						EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) null haplosome has non-zero treeseq allele length " << haplosome_allele_length << "." << EidosTerminate();
					}
					
					// (1) if the variant's allele is zero-length, we do nothing (if it incorrectly claims that a haplosome contains no
					// mutation, we'll catch that later)  (2) if the variant's allele is the length of one mutation id, we can simply
					// check that the next mutation in the haplosome in question exists and has the right mutation id; (3) if the variant's
					// allele has more than one mutation id, we have to check them all against all the mutations at the given position
					// in the haplosome in question, which is a bit annoying since the lists may not be in the same order.  Note that if
					// the variant is for a mutation that has fixed, it will not be present in the haplosome; we check for a substitution
					// with the right ID.
					slim_mutationid_t *haplosome_allele = (slim_mutationid_t *)variant.alleles[haplosome_variant];
					
					if (haplosome_allele_length == 0)
					{
						// If there are no fixed mutations at this site, we can continue; haplosomes that have a mutation at this site will
						// raise later when they realize they have been skipped over, so we don't have to check for that now...
						if (fixed_mutids.size() == 0)
							continue;
						
						EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) the treeseq has 0 mutations at position " << variant_pos_int << ", SLiM has " << fixed_mutids.size() << " fixed mutation(s)." << EidosTerminate();
					}
					else if (haplosome_allele_length == 1)
					{
						// The tree has just one mutation at this site; this is the common case, so we try to handle it quickly
						slim_mutationid_t allele_mutid = *haplosome_allele;
						Mutation *current_mut = haplosome_walker.CurrentMutation();
						
						if (current_mut)
						{
							slim_position_t current_mut_pos = current_mut->position_;
							
							if (current_mut_pos < variant_pos_int)
								EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) haplosome mutation was not represented in trees (single case)." << EidosTerminate();
							if (current_mut->position_ > variant_pos_int)
								current_mut = nullptr;	// not a candidate for this position, we'll see it again later
						}
						
						if (!current_mut && (fixed_mutids.size() == 1))
						{
							// We have one fixed mutation and no segregating mutation, versus one mutation in the tree; crosscheck
							if (allele_mutid != fixed_mutids[0])
								EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) the treeseq has mutid " << allele_mutid << " at position " << variant_pos_int << ", SLiM has a fixed mutation of id " << fixed_mutids[0] << EidosTerminate();
							
							continue;	// the match was against a fixed mutation, so don't go to the next mutation
						}
						else if (current_mut && (fixed_mutids.size() == 0))
						{
							// We have one segregating mutation and no fixed mutation, versus one mutation in the tree; crosscheck
							if (allele_mutid != current_mut->mutation_id_)
								EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) the treeseq has mutid " << allele_mutid << " at position " << variant_pos_int << ", SLiM has a segregating mutation of id " << current_mut->mutation_id_ << EidosTerminate();
						}
						else
						{
							// We have a count mismatch; there is one mutation in the tree, but we have !=1 in SLiM including substitutions
							EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) haplosome/allele size mismatch at position " << variant_pos_int << ": the treeseq has 1 mutation of mutid " << allele_mutid << ", SLiM has " << (current_mut ? 1 : 0) << " segregating and " << fixed_mutids.size() << " fixed mutation(s)." << EidosTerminate();
						}
						
						haplosome_walker.NextMutation();
						
						// Check the next mutation to see if it's at this position as well, and is missing from the tree;
						// this would get caught downstream, but for debugging it is clearer to catch it here
						Mutation *next_mut = haplosome_walker.CurrentMutation();
						
						if (next_mut && next_mut->position_ == variant_pos_int)
							EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) the treeseq is missing a stacked mutation with mutid " << next_mut->mutation_id_ << " at position " << variant_pos_int << "." << EidosTerminate();
					}
					else // (haplosome_allele_length > 1)
					{
						static std::vector<slim_mutationid_t> allele_mutids;
						static std::vector<slim_mutationid_t> haplosome_mutids;
						allele_mutids.resize(0);
						haplosome_mutids.resize(0);
						
						// tabulate all tree mutations
						for (tsk_size_t mutid_index = 0; mutid_index < haplosome_allele_length; ++mutid_index)
							allele_mutids.emplace_back(haplosome_allele[mutid_index]);
						
						// tabulate segregating SLiM mutations
						while (true)
						{
							Mutation *current_mut = haplosome_walker.CurrentMutation();
							
							if (current_mut)
							{
								slim_position_t current_mut_pos = current_mut->position_;
								
								if (current_mut_pos < variant_pos_int)
									EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) haplosome mutation was not represented in trees (bulk case)." << EidosTerminate();
								else if (current_mut_pos == variant_pos_int)
								{
									haplosome_mutids.emplace_back(current_mut->mutation_id_);
									haplosome_walker.NextMutation();
								}
								else break;
							}
							else break;
						}
						
						// tabulate fixed SLiM mutations
						haplosome_mutids.insert(haplosome_mutids.end(), fixed_mutids.begin(), fixed_mutids.end());
						
						// crosscheck, sorting so there is no order-dependency
						if (allele_mutids.size() != haplosome_mutids.size())
							EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) haplosome/allele size mismatch at position " << variant_pos_int << ": the treeseq has " << allele_mutids.size() << " mutations, SLiM has " << (haplosome_mutids.size() - fixed_mutids.size()) << " segregating and " << fixed_mutids.size() << " fixed mutation(s)." << EidosTerminate();
						
						std::sort(allele_mutids.begin(), allele_mutids.end());
						std::sort(haplosome_mutids.begin(), haplosome_mutids.end());
						
						for (tsk_size_t mutid_index = 0; mutid_index < haplosome_allele_length; ++mutid_index)
							if (allele_mutids[mutid_index] != haplosome_mutids[mutid_index])
								EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) haplosome/allele bulk mutid mismatch." << EidosTerminate();
					}
				}
			}
			
			// we have finished all variants, so all the haplosomes we're tracking should be at their ends; any left-over mutations
			// should have been in the trees but weren't, so this is an error
			for (size_t haplosome_index = 0; haplosome_index < haplosome_count; haplosome_index++)
				if (!haplosome_walkers[haplosome_index].Finished())
					EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) mutations left in haplosome beyond those in tree." << EidosTerminate();
			
			// free
			ret = tsk_variant_free(&variant);
			if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_variant_free()", ret);
			
			ret = tsk_treeseq_free(ts);
			if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_treeseq_free()", ret);
			free(ts);
			
			ret = tsk_table_collection_free(tables_copy);
			if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_table_collection_free()", ret);
			free(tables_copy);
		}
	}
	
	// check that tabled_individuals_hash_ is the right size and has all the right entries
	if (recording_tree_)
	{
		tsk_individual_table_t &shared_individuals_table = treeseq_[0].tables_.individuals;
		
		if (shared_individuals_table.num_rows != tabled_individuals_hash_.size())
			EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) tabled_individuals_hash_ size (" << tabled_individuals_hash_.size() << ") does not match the individuals table size (" << shared_individuals_table.num_rows << ")." << EidosTerminate();
		
		for (tsk_size_t individual_index = 0; individual_index < shared_individuals_table.num_rows; individual_index++)
		{
			tsk_id_t tsk_individual = (tsk_id_t)individual_index;
			IndividualMetadataRec *metadata_rec = (IndividualMetadataRec *)(shared_individuals_table.metadata + shared_individuals_table.metadata_offset[tsk_individual]);
			slim_pedigreeid_t pedigree_id = metadata_rec->pedigree_id_;
			auto lookup = tabled_individuals_hash_.find(pedigree_id);

			if (lookup == tabled_individuals_hash_.end())
				EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) missing entry for a pedigree id in tabled_individuals_hash_." << EidosTerminate();

			tsk_id_t lookup_tskid = lookup->second;

			if (tsk_individual != lookup_tskid)
				EIDOS_TERMINATION << "ERROR (Species::CrosscheckTreeSeqIntegrity): (internal error) incorrect entry for a pedigree id in tabled_individuals_hash_." << EidosTerminate();
		}
	}
}

void Species::__CheckPopulationMetadata(TreeSeqInfo &p_treeseq)
{
	// check population table metadata
	tsk_table_collection_t &tables = p_treeseq.tables_;
	char *pop_schema_ptr = tables.populations.metadata_schema;
	tsk_size_t pop_schema_len = tables.populations.metadata_schema_length;
	std::string pop_schema(pop_schema_ptr, pop_schema_len);
	
	if (pop_schema == gSLiM_tsk_population_metadata_schema_PREJSON)
	{
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): the population metadata schema is old; this version of the .trees format is no longer supported by SLiM." << EidosTerminate(nullptr);
	}
	else
	{
		// If it is not in the pre-JSON format, check that it is JSON; we don't accept binary non-JSON metadata.
		// This is necessary because we will carry this metadata over when we output a new population table on save;
		// this metadata must be compatible with our schema, which is a JSON schema.  Note that we do not check that
		// the schema exactly matches our current schema string, however; we are permissive about that, by design.
		// See https://github.com/MesserLab/SLiM/issues/169 for discussion about schema checking/compatibility.
		nlohmann::json pop_schema_json;
		
		try {
			pop_schema_json = nlohmann::json::parse(pop_schema);
		} catch (...) {
			EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): the population metadata schema does not parse as a valid JSON string." << EidosTerminate(nullptr);
		}
		
		if (pop_schema_json["codec"] != "json")
			EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): the population metadata schema must be JSON." << EidosTerminate(nullptr);
	}
}

// We need a reverse hash to construct the remapped population table
#if EIDOS_ROBIN_HOOD_HASHING
typedef robin_hood::unordered_flat_map<slim_objectid_t, int64_t> SUBPOP_REMAP_REVERSE_HASH;
#elif STD_UNORDERED_MAP_HASHING
typedef std::unordered_map<slim_objectid_t, int64_t> SUBPOP_REMAP_REVERSE_HASH;
#endif

void Species::__RemapSubpopulationIDs(SUBPOP_REMAP_HASH &p_subpop_map, TreeSeqInfo &p_treeseq, __attribute__ ((unused)) int p_file_version)
{
	// If we have been given a remapping table, this method munges all of the data
	// and metadata in the treeseq tables to accomplish that remapping.  It is gross
	// to have to do this on the raw table data, but we need that data to be corrected
	// so that we can simulate forward from it.  Every subpop id referenced in the
	// tables must be remapped; if a map is given, it must remap everything.  We have
	// to check all metadata carefully, since this remap happens before other checks.
	// We handle both SLiM metadata and non-SLiM metadata correctly here if we can.
	if (p_subpop_map.size() > 0)
	{
		tsk_table_collection_t &tables = p_treeseq.tables_;
		SUBPOP_REMAP_REVERSE_HASH subpop_reverse_hash;	// from SLiM subpop id back to the table index read
		slim_objectid_t remapped_row_count = 0;			// the number of rows we need in the remapped population table
		int ret = 0;
		
		// When remapping, we may encounter -1 as a subpopulation id.  This is actually TSK_NULL,
		// which is used in various contexts to represent "unknown" - as a source in the migration
		// table, as the subpop of origin for a mutation, etc.  Whenever we encounter such a
		// TSK_NULL, we just want to map it back to itself; so we will map -1 to -1.  This is
		// necessary because we raise when we see an unmapped subpopulation id.  We make a copy
		// of p_subpop_map so we don't modify the caller's map.
		SUBPOP_REMAP_HASH subpop_map = p_subpop_map;
		
		subpop_map.emplace(-1, -1);
		
		// First we will scan the population table metadata to assess the situation
		{
			tsk_population_table_t &pop_table = tables.populations;
			tsk_size_t pop_count = pop_table.num_rows;
			
			// Start by checking that no remap entry references a population table index that is out of range
			if (pop_count == 0)
				EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): the population table is empty, and therefore cannot be remapped." << EidosTerminate(nullptr);
			
			for (auto &remap_entry : p_subpop_map)
			{
				int64_t table_index = remap_entry.first;
				//slim_objectid_t remapped_index = remap_entry.second;
				
				//std::cout << "index " << table_index << " being remapped to " << remapped_index << std::endl;
				
				if (table_index < 0)
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): (internal error) index " << table_index << " is out of range (less than zero)." << EidosTerminate(nullptr);
				if (table_index >= (int64_t)pop_count)
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): index " << table_index << " is out of range (last valid index " << ((int64_t)pop_count - 1) << ")." << EidosTerminate(nullptr);
			}
			
			// OK, population table indices are in range; check the population table entry remappings one by one
			for (tsk_size_t pop_index = 0; pop_index < pop_count; pop_index++)
			{
				// validate and parse metadata
				size_t metadata_length = pop_table.metadata_offset[pop_index + 1] - pop_table.metadata_offset[pop_index];
				
				char *metadata_char = pop_table.metadata + pop_table.metadata_offset[pop_index];
				std::string metadata_string(metadata_char, metadata_length);
				nlohmann::json subpop_metadata;
				slim_objectid_t subpop_id = (slim_objectid_t)pop_index, remapped_id;
				
				// we require that metadata for every row be valid JSON; we have no way of
				// understanding, much less remapping, metadata in other (binary) formats
				try {
					subpop_metadata = nlohmann::json::parse(metadata_string);
				} catch (...) {
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): population metadata does not parse as a valid JSON string; this file cannot be read." << EidosTerminate(nullptr);
				}
				
				if (subpop_metadata.is_null())
				{
					// 'null' rows in the population table correspond to unused subpop IDs
					auto remap_iter = subpop_map.find(subpop_id);
					
					// null lines are usually not remapped, so we don't require a remap here, but if
					// they are referenced by other data then they will have to be, so we allow it
					if (remap_iter == subpop_map.end())
						continue;
					
					remapped_id = remap_iter->second;
				}
				else if (!subpop_metadata.is_object())
				{
					// if a row's metadata is not 'null', we require it to be a JSON "object"
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): population metadata does not parse as a JSON object; this file cannot be read." << EidosTerminate(nullptr);
				}
				else if (!subpop_metadata.contains("slim_id"))
				{
					// this row has JSON metadata that does not have a "slim_id" key, so it is
					// not SLiM metadata; this is the "carryover" case and we will remap it
					// without any attempt to fix the contents of the metadata
					
					// since the metadata is not null, a remap is required; check for it and fetch it
					auto remap_iter = subpop_map.find(subpop_id);
					
					if (remap_iter == subpop_map.end())
						EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): subpopulation id " << subpop_id << " is used in the population table (for a non-SLiM 'carryover' subpopulation), but is not remapped in subpopMap." << EidosTerminate();
					
					remapped_id = remap_iter->second;
				}
				else if (!subpop_metadata["slim_id"].is_number_integer())
				{
					// if a row has JSON metadata with a "slim_id" key, its value must be an integer
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): population metadata key 'slim_id' is not the expected type (integer); this file cannot be read." << EidosTerminate(nullptr);
				}
				else
				{
					// This row has JSON metadata with an integer "slim_id" key; it is
					// SLiM metadata so this row will end up being a SLiM subpopulation
					// and we will remap it and fix up its metadata
					slim_objectid_t slim_id = subpop_metadata["slim_id"].get<slim_objectid_t>();
					
					// enforce the slim_id == index invariant here; removing this invariant would be
					// possible but would require a bunch of bookeeping and checks; see treerec/implementation.md
                    // for more discussion of this
					if (slim_id != subpop_id)
						EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): population metadata value for key 'slim_id' is not equal to the table index; this file cannot be read." << EidosTerminate(nullptr);
					
					// since the metadata is not null, a remap is required; check for it and fetch it
					auto remap_iter = subpop_map.find(subpop_id);
					
					if (remap_iter == subpop_map.end())
						EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): subpopulation id " << subpop_id << " is used in the population table, but is not remapped in subpopMap." << EidosTerminate();
					
					remapped_id = remap_iter->second;
				}
				
				// this remap seems good; do the associated bookkeeping
				if (remapped_id >= remapped_row_count)
					remapped_row_count = remapped_id + 1;	// +1 so the count encompasses [0, remapped_id]
				
				subpop_reverse_hash.emplace(std::pair<slim_objectid_t, int64_t>(remapped_id, subpop_id));
			}
		}
		
		// Next we reorder the actual rows of the population table, using a copy of the table
		{
			tsk_id_t tsk_population_id;
			tsk_population_table_t *population_table_copy;
			population_table_copy = (tsk_population_table_t *)malloc(sizeof(tsk_population_table_t));
			if (!population_table_copy)
				EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			ret = tsk_population_table_copy(&tables.populations, population_table_copy, 0);
			if (ret != 0) handle_error("__RemapSubpopulationIDs tsk_population_table_copy()", ret);
			ret = tsk_population_table_clear(&tables.populations);
			if (ret != 0) handle_error("__RemapSubpopulationIDs tsk_population_table_clear()", ret);
			
			for (slim_objectid_t remapped_row_index = 0; remapped_row_index < remapped_row_count; ++remapped_row_index)
			{
				auto reverse_iter = subpop_reverse_hash.find(remapped_row_index);
				
				if (reverse_iter == subpop_reverse_hash.end())
				{
					// No remap hash entry for this row index, so it must be an empty row
					tsk_population_id = tsk_population_table_add_row(&tables.populations, "null", 4);
				}
				else
				{
					// We have a remap entry; this could be an empty row, a SLiM subpop row, or a carryover row
					tsk_id_t original_row_index = (slim_objectid_t)reverse_iter->second;
					size_t metadata_length = population_table_copy->metadata_offset[original_row_index + 1] - population_table_copy->metadata_offset[original_row_index];
					char *metadata_char = population_table_copy->metadata + population_table_copy->metadata_offset[original_row_index];
					std::string metadata_string(metadata_char, metadata_length);
					nlohmann::json subpop_metadata = nlohmann::json::parse(metadata_string);
					
					if (subpop_metadata.is_null())
					{
						// There is a remap entry for this, but it is an empty row; no slim_id
						tsk_population_id = tsk_population_table_add_row(&tables.populations, "null", 4);
					}
					else if (!subpop_metadata.contains("slim_id"))
					{
						// There is a remap entry for this, with JSON metadata that has no slim_id;
						// this is carryover metadata, typically from msprime but who knows
						// We will remap msprime-style names like "pop_0", but *not* SLiM names like "p0"
						// We also permit the name to not be a string, in this code path, since
						// this metadata does not conform to our schema; we need to accept whatever it is
						std::string msprime_name = std::string("pop_").append(std::to_string(original_row_index));
						
						if (subpop_metadata.contains("name") && subpop_metadata["name"].is_string() && (subpop_metadata["name"].get<std::string>() == msprime_name))
						{
							subpop_metadata["name"] = SLiMEidosScript::IDStringWithPrefix('p', remapped_row_index);
							metadata_string = subpop_metadata.dump();
							tsk_population_id = tsk_population_table_add_row(&tables.populations, (char *)metadata_string.c_str(), (uint32_t)metadata_string.length());
						}
						else
						{
							tsk_population_id = tsk_population_table_add_row(&tables.populations, metadata_char, metadata_length);
						}
					}
					else
					{
						// There is a remap entry for this, with JSON metadata that has a slim_id;
						// this is a SLiM subpop, so we need to re-generate the metadata to fix slim_id
						subpop_metadata["slim_id"] = remapped_row_index;
						
						// We also need to fix the "name" metadata key when it equals the SLiM identifier
						// We fix msprime-style names like "pop_0" to the remapped "pX" name; see issue #173
						if (subpop_metadata.contains("name"))
						{
							nlohmann::json value = subpop_metadata["name"];
							if (!value.is_string())
								EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): population metadata key 'name' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
							std::string metadata_name = value.get<std::string>();
							std::string id_name = SLiMEidosScript::IDStringWithPrefix('p', original_row_index);
							std::string msprime_name = std::string("pop_").append(std::to_string(original_row_index));
							
							if ((metadata_name == id_name) || (metadata_name == msprime_name))
								subpop_metadata["name"] = SLiMEidosScript::IDStringWithPrefix('p', remapped_row_index);
						}
						
						// And finally, if there are migration records (for WF models) we need to remap them
						// We check only what we need to check; __ConfigureSubpopulationsFromTables() does more
						size_t migration_rec_count = 0;
						nlohmann::json migration_records;
						
						if (subpop_metadata.contains("migration_records"))
						{
							nlohmann::json value = subpop_metadata["migration_records"];
							if (!value.is_array())
								EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): population metadata key 'migration_records' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
							migration_records = value;
							migration_rec_count = migration_records.size();
						}
						
						if (migration_rec_count > 0)
						{
							for (size_t migration_index = 0; migration_index < migration_rec_count; ++migration_index)
							{
								nlohmann::json migration_rec = migration_records[migration_index];
								
								if (!migration_rec.is_object() || !migration_rec.contains("source_subpop") || !migration_rec["source_subpop"].is_number_integer())
									EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): population metadata migration record does not obey the metadata schema; this file cannot be read." << EidosTerminate(nullptr);
								
								slim_objectid_t old_subpop = migration_rec["source_subpop"].get<slim_objectid_t>();
								auto remap_iter = subpop_map.find(old_subpop);
								
								if (remap_iter == subpop_map.end())
									EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_subpop << ") used by the tree sequence data (migration record) was not remapped." << EidosTerminate();
								
								slim_objectid_t new_subpop = remap_iter->second;
								migration_rec["source_subpop"] = new_subpop;
								migration_records[migration_index] = migration_rec;
							}
							
							subpop_metadata["migration_records"] = migration_records;
						}
						
						// We've done all the necessary metadata tweaks; write it out
						metadata_string = subpop_metadata.dump();
						tsk_population_id = tsk_population_table_add_row(&tables.populations, (char *)metadata_string.c_str(), (uint32_t)metadata_string.length());
					}
				}
				
				// check the tsk_population_id returned by tsk_population_table_add_row() above
				if (tsk_population_id < 0) handle_error("tsk_population_table_add_row", tsk_population_id);
				assert(tsk_population_id == remapped_row_index);
			}
			
			ret = tsk_population_table_free(population_table_copy);
			if (ret != 0) handle_error("tsk_population_table_free", ret);
			free(population_table_copy);
		}
		
		// BCH 30 May 2022: OK, now we deal with the other tables.  We have a few stakes here.  The metadata on those tables is
		// guaranteed to be SLiM metadata.  I am told that it is not correct to check the schemas for the tables against known SLiM
		// schemas; the incoming file has a SLiM file version on it, and that means that it is guaranteed by whoever made it to be
		// SLiM-compliant, and that means SLiM metadata throughout (except in the population table itself, where the fact that our
		// metadata is JSON means we can distinguish foreign metadata and carry it over intact, as in the code above; that is not
		// possible in other tables because the metadata is binary).  The only compliance check we do is that the length of each chunk
		// of metadata matches what we expect it to be (based upon SLiM's binary metadata formats and the file version); and if a
		// length doesn't match, we throw.  That is not really for the benefit of the caller, or to validate the incoming data; it is
		// only for our own debugging purposes, as an assert of what we already know is guaranteed to be true.  So, given this
		// understanding, we will now go into the tables and munge all of their metadata to refer to the remapped subpopulation ids.
		
		// Remap subpop_index_ in the mutation metadata, in place
		{
			std::size_t metadata_rec_size = sizeof(MutationMetadataRec);
			tsk_mutation_table_t &mut_table = tables.mutations;
			tsk_size_t num_rows = mut_table.num_rows;
			
			for (tsk_size_t mut_index = 0; mut_index < num_rows; ++mut_index)
			{
				char *metadata_bytes = mut_table.metadata + mut_table.metadata_offset[mut_index];
				tsk_size_t metadata_length = mut_table.metadata_offset[mut_index + 1] - mut_table.metadata_offset[mut_index];
				
				if (metadata_length % metadata_rec_size != 0)
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): unexpected mutation metadata length; this file cannot be read." << EidosTerminate();
				
				int stack_count = (int)(metadata_length / metadata_rec_size);
				
				for (int stack_index = 0; stack_index < stack_count; ++stack_index)
				{
					// Here we have to deal with the metadata format
					MutationMetadataRec *metadata = (MutationMetadataRec *)metadata_bytes + stack_index;
					slim_objectid_t old_subpop = metadata->subpop_index_;
					auto remap_iter = subpop_map.find(old_subpop);
					
					if (remap_iter == subpop_map.end())
						EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_subpop << ") used by the tree sequence data (mutation metadata) was not remapped." << EidosTerminate();
					
					metadata->subpop_index_ = remap_iter->second;
				}
			}
		}
		
		// Next we remap subpopulation_id_ in the individual metadata, in place
		{
			tsk_individual_table_t &ind_table = tables.individuals;
			tsk_size_t num_rows = ind_table.num_rows;
			
			for (tsk_size_t ind_index = 0; ind_index < num_rows; ++ind_index)
			{
				char *metadata_bytes = ind_table.metadata + ind_table.metadata_offset[ind_index];
				tsk_size_t metadata_length = ind_table.metadata_offset[ind_index + 1] - ind_table.metadata_offset[ind_index];
				
				if (metadata_length != sizeof(IndividualMetadataRec))
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): unexpected individual metadata length; this file cannot be read." << EidosTerminate();
				
				IndividualMetadataRec *metadata = (IndividualMetadataRec *)metadata_bytes;
				slim_objectid_t old_subpop = metadata->subpopulation_id_;
				auto remap_iter = subpop_map.find(old_subpop);
				
				if (remap_iter == subpop_map.end())
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_subpop << ") used by the tree sequence data (individual metadata) was not remapped." << EidosTerminate();
				
				metadata->subpopulation_id_ = remap_iter->second;
			}
		}
		
		// Next we remap subpop ids in the population column of the node table, in place
		{
			tsk_node_table_t &node_table = tables.nodes;
			tsk_size_t num_rows = node_table.num_rows;
			
			for (tsk_size_t node_index = 0; node_index < num_rows; ++node_index)
			{
				tsk_id_t old_subpop = node_table.population[node_index];
				auto remap_iter = subpop_map.find(old_subpop);
				
				if (remap_iter == subpop_map.end())
					EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_subpop << ") used by the tree sequence data (node table) was not remapped." << EidosTerminate();
				
				node_table.population[node_index] = remap_iter->second;
			}
		}
		
		// SLiM does not use the migration table, but we should remap it just
		// to keep the internal state of the tree sequence consistent
		{
			tsk_migration_table_t &migration_table = tables.migrations;
			tsk_size_t num_rows = migration_table.num_rows;
			
			for (tsk_size_t node_index = 0; node_index < num_rows; ++node_index)
			{
				// remap source column
				{
					tsk_id_t old_source = migration_table.source[node_index];
					auto remap_iter = subpop_map.find(old_source);
					
					if (remap_iter == subpop_map.end())
						EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_source << ") used by the tree sequence data (migration table) was not remapped." << EidosTerminate();
					
					migration_table.source[node_index] = remap_iter->second;
				}
				
				// remap dest column
				{
					tsk_id_t old_dest = migration_table.dest[node_index];
					auto remap_iter = subpop_map.find(old_dest);
					
					if (remap_iter == subpop_map.end())
						EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_dest << ") used by the tree sequence data (migration table) was not remapped." << EidosTerminate();
					
					migration_table.dest[node_index] = remap_iter->second;
				}
			}
		}
	}
}

typedef struct ts_subpop_info {
	slim_popsize_t countMH_ = 0, countF_ = 0;
	std::vector<IndividualSex> sex_;
	std::vector<tsk_id_t> nodes_;
	std::vector<slim_pedigreeid_t> pedigreeID_;
	std::vector<slim_pedigreeid_t> pedigreeP1_;
	std::vector<slim_pedigreeid_t> pedigreeP2_;
	std::vector<slim_age_t> age_;
	std::vector<double> spatial_x_;
	std::vector<double> spatial_y_;
	std::vector<double> spatial_z_;
	std::vector<uint32_t> flags_;
} ts_subpop_info;

void Species::__PrepareSubpopulationsFromTables(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, TreeSeqInfo &p_treeseq)
{
	// This reads the subpopulation table and creates ts_subpop_info records for the non-empty subpopulations
	// Doing this first allows us to check that individuals are going into subpopulations that we understand
	// The code here is duplicated to some extent in __ConfigureSubpopulationsFromTables(), which finalizes things
	tsk_table_collection_t &tables = p_treeseq.tables_;
	tsk_population_table_t &pop_table = tables.populations;
	tsk_size_t pop_count = pop_table.num_rows;
	
	for (tsk_size_t pop_index = 0; pop_index < pop_count; pop_index++)
	{
		// We want to allow "carryover" of metadata from other sources such as msprime, so we do not want to require
		// that metadata is SLiM metadata.  We only prepare to receive individuals in subpopulations with SLiM metadata,
		// though; other subpopulations must not contain any extant individuals.  See issue #318.
		size_t metadata_length = pop_table.metadata_offset[pop_index + 1] - pop_table.metadata_offset[pop_index];
		char *metadata_char = pop_table.metadata + pop_table.metadata_offset[pop_index];
		slim_objectid_t subpop_id = CheckSLiMPopulationMetadata(metadata_char, metadata_length);
		
		// -1 indicates that the metadata does not represent an extant SLiM subpopulation
		if (subpop_id == -1)
			continue;
		
		// bounds-check the subpop id; if a slim_id is present, we require it to be well-behaved
		if ((subpop_id < 0) || (subpop_id > SLIM_MAX_ID_VALUE))
			EIDOS_TERMINATION << "ERROR (Species::__PrepareSubpopulationsFromTables): subpopulation id out of range (" << subpop_id << "); ids must be >= 0 and <= " << SLIM_MAX_ID_VALUE << "." << EidosTerminate();
		
		// create the ts_subpop_info record for this subpop_id
		if (p_subpopInfoMap.find(subpop_id) != p_subpopInfoMap.end())
			EIDOS_TERMINATION << "ERROR (Species::__PrepareSubpopulationsFromTables): subpopulation id (" << subpop_id << ") occurred twice in the subpopulation table." << EidosTerminate();
		if (subpop_id != (int)pop_index)
			EIDOS_TERMINATION << "ERROR (Species::__PrepareSubpopulationsFromTables): slim_id value " << subpop_id << " occurred at the wrong index in the subpopulation table; entries must be at their corresponding index.  This may result from simplification; if so, pass filter_populations=False to simplify()." << EidosTerminate();
		
		p_subpopInfoMap.emplace(subpop_id, ts_subpop_info());
	}
}

void Species::__TabulateSubpopulationsFromTreeSequence(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, tsk_treeseq_t *p_ts, TreeSeqInfo &p_treeseq, SLiMModelType p_file_model_type)
{
	tsk_table_collection_t &tables = p_treeseq.tables_;
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	Chromosome *chromosome = Chromosomes()[chromosome_index];
	ChromosomeType chromosomeType = chromosome->Type();
	size_t individual_count = p_ts->tables->individuals.num_rows;
	
	if (individual_count == 0)
		EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): loaded tree sequence files must contain a non-empty individuals table." << EidosTerminate();
	
	tsk_individual_t individual;
	int ret = 0;
	
	for (size_t individual_index = 0; individual_index < individual_count; individual_index++)
	{
		ret = tsk_treeseq_get_individual(p_ts, (tsk_id_t)individual_index, &individual);
		if (ret != 0) handle_error("__TabulateSubpopulationsFromTreeSequence tsk_treeseq_get_individual", ret);
		
		// tabulate only individuals marked as being alive; everybody else in the table is irrelevant to us during load
		if (!(individual.flags & SLIM_TSK_INDIVIDUAL_ALIVE))
			continue;
		
		// fetch the metadata for this individual
		if (individual.metadata_length != sizeof(IndividualMetadataRec))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): unexpected individual metadata length; this file cannot be read." << EidosTerminate();
		
		IndividualMetadataRec *metadata = (IndividualMetadataRec *)(individual.metadata);
		
		// find the ts_subpop_info rec for this individual's subpop, created by __PrepareSubpopulationsFromTables()
		slim_objectid_t subpop_id = metadata->subpopulation_id_;
		auto subpop_info_iter = p_subpopInfoMap.find(subpop_id);
		
		if (subpop_info_iter == p_subpopInfoMap.end())
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): individual has a subpopulation id (" << subpop_id << ") that is not described by the population table." << EidosTerminate();
		
		ts_subpop_info &subpop_info = subpop_info_iter->second;
		
		// check and tabulate sex within each subpop
		IndividualSex sex = (IndividualSex)metadata->sex_;			// IndividualSex, but int32_t in the record
		
		switch (sex)
		{
			case IndividualSex::kHermaphrodite:
				if (sex_enabled_)
					EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): hermaphrodites may not be loaded into a model in which sex is enabled." << EidosTerminate();
				subpop_info.countMH_++;
				break;
			case IndividualSex::kFemale:
				if (!sex_enabled_)
					EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): females may not be loaded into a model in which sex is not enabled." << EidosTerminate();
				subpop_info.countF_++;
				break;
			case IndividualSex::kMale:
				if (!sex_enabled_)
					EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): males may not be loaded into a model in which sex is not enabled." << EidosTerminate();
				subpop_info.countMH_++;
				break;
			default:
				EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): unrecognized individual sex value " << sex << "." << EidosTerminate();
		}
		
		subpop_info.sex_.emplace_back(sex);
		
		// check that the individual has exactly two nodes; we are always diploid in terms of nodes, regardless of the chromosome type
		if (individual.nodes_length != 2)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): unexpected node count; this file cannot be read." << EidosTerminate();
		
		subpop_info.nodes_.emplace_back(individual.nodes[0]);
		subpop_info.nodes_.emplace_back(individual.nodes[1]);
		
		// bounds-check and save off the pedigree ID, which we will use again; note that parent pedigree IDs are allowed to be -1
		if (metadata->pedigree_id_ < 0)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): individuals loaded must have pedigree IDs >= 0." << EidosTerminate();
		subpop_info.pedigreeID_.push_back(metadata->pedigree_id_);
		
		if ((metadata->pedigree_p1_ < -1) || (metadata->pedigree_p2_ < -1))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): individuals loaded must have parent pedigree IDs >= -1." << EidosTerminate();
		subpop_info.pedigreeP1_.push_back(metadata->pedigree_p1_);
		subpop_info.pedigreeP2_.push_back(metadata->pedigree_p2_);

		// save off the flags for later use
		subpop_info.flags_.push_back(metadata->flags_);
		
		// bounds-check ages; we cross-translate ages of 0 and -1 if the model type has been switched
		slim_age_t age = metadata->age_;
		
		if ((p_file_model_type == SLiMModelType::kModelTypeNonWF) && (model_type_ == SLiMModelType::kModelTypeWF) && (age == 0))
			age = -1;
		if ((p_file_model_type == SLiMModelType::kModelTypeWF) && (model_type_ == SLiMModelType::kModelTypeNonWF) && (age == -1))
			age = 0;
		
		if (((age < 0) || (age > SLIM_MAX_ID_VALUE)) && (model_type_ == SLiMModelType::kModelTypeNonWF))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): individuals loaded into a nonWF model must have age values >= 0 and <= " << SLIM_MAX_ID_VALUE << "." << EidosTerminate();
		if ((age != -1) && (model_type_ == SLiMModelType::kModelTypeWF))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): individuals loaded into a WF model must have age values == -1." << EidosTerminate();
		
		subpop_info.age_.emplace_back(age);
		
		// no bounds-checks for spatial position
		if (individual.location_length != 3)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): unexpected individual location length; this file cannot be read." << EidosTerminate();
		
		subpop_info.spatial_x_.emplace_back(individual.location[0]);
		subpop_info.spatial_y_.emplace_back(individual.location[1]);
		subpop_info.spatial_z_.emplace_back(individual.location[2]);
		
		// check the referenced nodes; right now this is not essential for re-creating the saved state, but is just a crosscheck
		// here we crosscheck the node information against expected values from other places in the tables or the model
		tsk_node_table_t &node_table = tables.nodes;
		tsk_id_t node0 = individual.nodes[0];
		tsk_id_t node1 = individual.nodes[1];
		
		if (((node_table.flags[node0] & TSK_NODE_IS_SAMPLE) == 0) || ((node_table.flags[node1] & TSK_NODE_IS_SAMPLE) == 0))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): nodes for individual are not in-sample; this file cannot be read." << EidosTerminate();
		if ((node_table.individual[node0] != individual.id) || (node_table.individual[node1] != individual.id))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): individual-node inconsistency; this file cannot be read." << EidosTerminate();
		
		size_t node0_metadata_length = node_table.metadata_offset[node0 + 1] - node_table.metadata_offset[node0];
		size_t node1_metadata_length = node_table.metadata_offset[node1 + 1] - node_table.metadata_offset[node1];
		
		int byte_index = chromosome_index / 8;
		int bit_shift = chromosome_index % 8;
		size_t expected_min_metadata_length = sizeof(HaplosomeMetadataRec) + byte_index;	// 1 byte already counted in HaplosomeMetadataRec
		
		// check that the metadata is long enough to contain the is_vacant bit we will look at; we don't check
		// that it is exactly the length we expect here, just that it works for our local purposes
		if ((node0_metadata_length < expected_min_metadata_length) || (node1_metadata_length < expected_min_metadata_length))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): unexpected node metadata length; this file cannot be read." << EidosTerminate();
		
		HaplosomeMetadataRec *node0_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node0]);
		HaplosomeMetadataRec *node1_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node1]);
		
		if ((node0_metadata->haplosome_id_ != metadata->pedigree_id_ * 2) || (node1_metadata->haplosome_id_ != metadata->pedigree_id_ * 2 + 1))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): haplosome id mismatch; this file cannot be read." << EidosTerminate();
		
		// check that the null-haplosome flags make sense with the chromosome type
		bool expected_is_vacant_0 = false, expected_is_vacant_1 = false;
		
		switch (chromosomeType)
		{
			case ChromosomeType::kA_DiploidAutosome:
				expected_is_vacant_0 = false;
				expected_is_vacant_1 = false;
				break;
			case ChromosomeType::kH_HaploidAutosome:
				expected_is_vacant_0 = false;
				expected_is_vacant_1 = true;	// unused
				break;
			case ChromosomeType::kX_XSexChromosome:
				expected_is_vacant_0 = false;
				expected_is_vacant_1 = (sex == IndividualSex::kMale) ? true : false;	// null in males
				break;
			case ChromosomeType::kY_YSexChromosome:
				expected_is_vacant_0 = (sex == IndividualSex::kMale) ? false : true;	// null in females
				expected_is_vacant_1 = true;	// unused
				break;
			case ChromosomeType::kZ_ZSexChromosome:
				expected_is_vacant_0 = (sex == IndividualSex::kMale) ? false : true;	// null in females
				expected_is_vacant_1 = false;
				break;
			case ChromosomeType::kW_WSexChromosome:
				expected_is_vacant_0 = (sex == IndividualSex::kMale) ? true : false;	// null in males
				expected_is_vacant_1 = true;	// unused
				break;
			case ChromosomeType::kHF_HaploidFemaleInherited:
				expected_is_vacant_0 = false;
				expected_is_vacant_1 = true;	// unused
				break;
			case ChromosomeType::kFL_HaploidFemaleLine:
				expected_is_vacant_0 = (sex == IndividualSex::kMale) ? true : false;	// null in males
				expected_is_vacant_1 = true;	// unused
				break;
			case ChromosomeType::kHM_HaploidMaleInherited:
				expected_is_vacant_0 = false;
				expected_is_vacant_1 = true;	// unused
				break;
			case ChromosomeType::kML_HaploidMaleLine:
				expected_is_vacant_0 = (sex == IndividualSex::kMale) ? false : true;	// null in females
				expected_is_vacant_1 = true;	// unused
				break;
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
				expected_is_vacant_0 = false;
				expected_is_vacant_1 = true;	// null
				break;
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
				expected_is_vacant_0 = true;	// null
				expected_is_vacant_1 = (sex == IndividualSex::kMale) ? false : true;	// null in females
				break;
		}
		
		// Null haplosomes are allowed to occur arbitrarily in nonWF models in chromosome types 'A' and 'H'
		bool node0_is_vacant = !!((node0_metadata->is_vacant_[byte_index] >> bit_shift) & 0x01);
		
		if (node0_is_vacant != expected_is_vacant_0)
		{
			if ((model_type_ == SLiMModelType::kModelTypeNonWF) &&
				((chromosomeType == ChromosomeType::kA_DiploidAutosome) || (chromosomeType == ChromosomeType::kH_HaploidAutosome)))
				;
			else
				EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): node is_vacant unexpected; this file cannot be read." << EidosTerminate();
		}
		
		// We do not check the second haplosome's null flag if the chromosome type is intrinsically haploid
		if (chromosome->IntrinsicPloidy() == 2)
		{
			bool node1_is_vacant = !!((node1_metadata->is_vacant_[byte_index] >> bit_shift) & 0x01);
			
			if (node1_is_vacant != expected_is_vacant_1)
			{
				if ((model_type_ == SLiMModelType::kModelTypeNonWF) &&
					((chromosomeType == ChromosomeType::kA_DiploidAutosome) || (chromosomeType == ChromosomeType::kH_HaploidAutosome)))
					;
				else
					EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): node is_vacant unexpected; this file cannot be read." << EidosTerminate();
			}
		}
	}
}

void Species::__CreateSubpopulationsFromTabulation(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Haplosome *> &p_nodeToHaplosomeMap, TreeSeqInfo &p_treeseq)
{
	tsk_table_collection_t &tables = p_treeseq.tables_;
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	Chromosome *chromosome = Chromosomes()[chromosome_index];
	ChromosomeType chromosomeType = chromosome->Type();
	int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
	int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
	
	// We will keep track of all pedigree IDs used, and check at the end that they do not collide; faster than checking as we go
	// This could be done with a hash table, but I imagine that would be slower until the number of individuals becomes very large
	// Also, I'm a bit nervous about putting a large number of consecutive integers into a hash table, re: edge-case performance
	std::vector<slim_pedigreeid_t> pedigree_id_check;
	std::vector<slim_haplosomeid_t> haplosome_id_check;
	
	gSLiM_next_pedigree_id = 0;
	
	for (auto subpop_info_iter : p_subpopInfoMap)
	{
		slim_objectid_t subpop_id = subpop_info_iter.first;
		ts_subpop_info &subpop_info = subpop_info_iter.second;
		slim_popsize_t subpop_size = sex_enabled_ ? (subpop_info.countMH_ + subpop_info.countF_) : subpop_info.countMH_;
		double sex_ratio = sex_enabled_ ? (subpop_info.countMH_ / (double)subpop_size) : 0.5;
		
		// Create the new subpopulation – without recording it in the tree-seq tables
		recording_tree_ = false;
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, sex_ratio, false);
		recording_tree_ = true;
		
		// define a new Eidos variable to refer to the new subpopulation
		EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
		
		if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): new subpopulation symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
		
		community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
		
		// connect up the individuals and haplosomes in the new subpop with the tree-seq table entries
		int sex_count = sex_enabled_ ? 2 : 1;
		
		for (int sex_index = 0; sex_index < sex_count; ++sex_index)
		{
			IndividualSex generating_sex = (sex_enabled_ ? (sex_index == 0 ? IndividualSex::kFemale : IndividualSex::kMale) : IndividualSex::kHermaphrodite);
			slim_popsize_t tabulation_size = (sex_enabled_ ? (sex_index == 0 ? subpop_info.countF_ : subpop_info.countMH_) : subpop_info.countMH_);
			slim_popsize_t start_index = (generating_sex == IndividualSex::kMale) ? new_subpop->parent_first_male_index_ : 0;
			slim_popsize_t last_index = (generating_sex == IndividualSex::kFemale) ? (new_subpop->parent_first_male_index_ - 1) : (new_subpop->parent_subpop_size_ - 1);
			slim_popsize_t sex_size = last_index - start_index + 1;
			
			if (tabulation_size != sex_size)
				EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): (internal error) mismatch between tabulation size and subpop size." << EidosTerminate();
			
			slim_popsize_t tabulation_index = -1;
			
			for (slim_popsize_t ind_index = start_index; ind_index <= last_index; ++ind_index)
			{
				// scan for the next tabulation entry of the expected sex
				do {
					tabulation_index++;
					
					if (tabulation_index >= subpop_size)
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): (internal error) ran out of tabulated individuals." << EidosTerminate();
				} while (subpop_info.sex_[tabulation_index] != generating_sex);
				
				Individual *individual = new_subpop->parent_individuals_[ind_index];
				
				if (individual->sex_ != generating_sex)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): (internal error) unexpected individual sex." << EidosTerminate();
				
				tsk_id_t node_id_0 = subpop_info.nodes_[(size_t)tabulation_index * 2];
				tsk_id_t node_id_1 = subpop_info.nodes_[(size_t)tabulation_index * 2 + 1];
				
				if (node_id_0 + 1 != node_id_1)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): (internal error) node ids for individual are not adjacent." << EidosTerminate();
				
				individual->SetTskitNodeIdBase(node_id_0);
				
				slim_pedigreeid_t pedigree_id = subpop_info.pedigreeID_[tabulation_index];
				individual->SetPedigreeID(pedigree_id);
				pedigree_id_check.emplace_back(pedigree_id);	// we will test for collisions below
				gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);

				individual->SetParentPedigreeID(subpop_info.pedigreeP1_[tabulation_index], subpop_info.pedigreeP2_[tabulation_index]);
				
				uint32_t flags = subpop_info.flags_[tabulation_index];
				if (flags & SLIM_INDIVIDUAL_METADATA_MIGRATED)
					individual->migrant_ = true;
				
				individual->age_ = subpop_info.age_[tabulation_index];
				individual->spatial_x_ = subpop_info.spatial_x_[tabulation_index];
				individual->spatial_y_ = subpop_info.spatial_y_[tabulation_index];
				individual->spatial_z_ = subpop_info.spatial_z_[tabulation_index];
				
				p_nodeToHaplosomeMap.emplace(node_id_0, individual->haplosomes_[first_haplosome_index]);
				slim_haplosomeid_t haplosome_id = pedigree_id * 2;
				individual->haplosomes_[first_haplosome_index]->haplosome_id_ = haplosome_id;
				haplosome_id_check.emplace_back(haplosome_id);	// we will test for collisions below
				
				if (last_haplosome_index != first_haplosome_index)
				{
					p_nodeToHaplosomeMap.emplace(node_id_1, individual->haplosomes_[last_haplosome_index]);
					haplosome_id = pedigree_id * 2 + 1;
					individual->haplosomes_[last_haplosome_index]->haplosome_id_ = haplosome_id;
					haplosome_id_check.emplace_back(haplosome_id);	// we will test for collisions below
				}
				
				// check the referenced nodes; right now this is not essential for re-creating the saved state, but is just a crosscheck
				// here we crosscheck the node information against the realized values in the haplosomes of the individual
				tsk_node_table_t &node_table = tables.nodes;
				size_t node0_metadata_length = node_table.metadata_offset[node_id_0 + 1] - node_table.metadata_offset[node_id_0];
				size_t node1_metadata_length = node_table.metadata_offset[node_id_1 + 1] - node_table.metadata_offset[node_id_1];
				
				int byte_index = chromosome_index / 8;
				int bit_shift = chromosome_index % 8;
				size_t expected_min_metadata_length = sizeof(HaplosomeMetadataRec) + byte_index;	// 1 byte already counted in HaplosomeMetadataRec
				
				// check that the metadata is long enough to contain the is_vacant bit we will look at; we don't check
				// that it is exactly the length we expect here, just that it works for our local purposes
				if ((node0_metadata_length < expected_min_metadata_length) || (node1_metadata_length < expected_min_metadata_length))
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): unexpected node metadata length; this file cannot be read." << EidosTerminate();
				
				HaplosomeMetadataRec *node0_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_0]);
				Haplosome *haplosome0 = individual->haplosomes_[first_haplosome_index];
				
				if (node0_metadata->haplosome_id_ != haplosome0->haplosome_id_)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome id mismatch; this file cannot be read." << EidosTerminate();
				
				// Null haplosomes are allowed to occur arbitrarily in nonWF models in chromosome types 'A' and 'H'
				bool node0_is_vacant = !!((node0_metadata->is_vacant_[byte_index] >> bit_shift) & 0x01);
				
				if (node0_is_vacant != haplosome0->IsNull())
				{
					if (node0_is_vacant && (model_type_ == SLiMModelType::kModelTypeNonWF) &&
						((chromosomeType == ChromosomeType::kA_DiploidAutosome) || (chromosomeType == ChromosomeType::kH_HaploidAutosome)))
					{
						haplosome0->MakeNull();
						new_subpop->has_null_haplosomes_ = true;
					}
					else
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome null mismatch; this file cannot be read." << EidosTerminate();
				}
				
				// We do not check the second haplosome's state if the chromosome type is intrinsically haploid
				if (last_haplosome_index != first_haplosome_index)
				{
					HaplosomeMetadataRec *node1_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_1]);
					Haplosome *haplosome1 = individual->haplosomes_[last_haplosome_index];
					
					if (node1_metadata->haplosome_id_ != haplosome1->haplosome_id_)
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome id mismatch; this file cannot be read." << EidosTerminate();
					
					bool node1_is_vacant = !!((node1_metadata->is_vacant_[byte_index] >> bit_shift) & 0x01);
					
					if (node1_is_vacant != haplosome1->IsNull())
					{
						if (node1_is_vacant && (model_type_ == SLiMModelType::kModelTypeNonWF) &&
							((chromosomeType == ChromosomeType::kA_DiploidAutosome) || (chromosomeType == ChromosomeType::kH_HaploidAutosome)))
						{
							haplosome1->MakeNull();
							new_subpop->has_null_haplosomes_ = true;
						}
						else
							EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome null mismatch; this file cannot be read." << EidosTerminate();
					}
				}
			}
		}
	}
	
	// Check for individual pedigree ID collisions by sorting and looking for duplicates
	std::sort(pedigree_id_check.begin(), pedigree_id_check.end());
	const auto duplicate = std::adjacent_find(pedigree_id_check.begin(), pedigree_id_check.end());
	
	if (duplicate != pedigree_id_check.end())
		EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): the individual pedigree ID value " << *duplicate << " was used more than once; individual pedigree IDs must be unique." << EidosTerminate();
}

void Species::__CreateSubpopulationsFromTabulation_SECONDARY(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, __attribute__((unused)) EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Haplosome *> &p_nodeToHaplosomeMap, TreeSeqInfo &p_treeseq)
{
	// NOTE: This version of __CreateSubpopulationsFromTabulation() validates subpopulations already created,
	// ensuring that they match those made by __CreateSubpopulationsFromTabulation() for the first chromosome
	// read.  BEWARE: These methods should be maintained in parallel!
	
	tsk_table_collection_t &tables = p_treeseq.tables_;
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	Chromosome *chromosome = Chromosomes()[chromosome_index];
	ChromosomeType chromosomeType = chromosome->Type();
	int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
	int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
	
	// We do not check pedigree ids in this secondary pass; __CreateSubpopulationsFromTabulation() set them up.
	
	for (auto subpop_info_iter : p_subpopInfoMap)
	{
		slim_objectid_t subpop_id = subpop_info_iter.first;
		ts_subpop_info &subpop_info = subpop_info_iter.second;
		slim_popsize_t subpop_size = sex_enabled_ ? (subpop_info.countMH_ + subpop_info.countF_) : subpop_info.countMH_;
		
		// Get the existing subpopulation and check that its size and sex ratio match expectations.
		Subpopulation *new_subpop = SubpopulationWithID(subpop_id);
		
		if (new_subpop->parent_subpop_size_ != subpop_size)
			EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): subpopulation size mismatch between chromosomes read." << EidosTerminate();
		
		if (sex_enabled_ && (new_subpop->parent_first_male_index_ != subpop_info.countF_))
			EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): sex ratio mismatch between chromosomes read." << EidosTerminate();
		
		// connect up the individuals and haplosomes in the new subpop with the tree-seq table entries
		int sex_count = sex_enabled_ ? 2 : 1;
		
		for (int sex_index = 0; sex_index < sex_count; ++sex_index)
		{
			IndividualSex generating_sex = (sex_enabled_ ? (sex_index == 0 ? IndividualSex::kFemale : IndividualSex::kMale) : IndividualSex::kHermaphrodite);
			slim_popsize_t tabulation_size = (sex_enabled_ ? (sex_index == 0 ? subpop_info.countF_ : subpop_info.countMH_) : subpop_info.countMH_);
			slim_popsize_t start_index = (generating_sex == IndividualSex::kMale) ? new_subpop->parent_first_male_index_ : 0;
			slim_popsize_t last_index = (generating_sex == IndividualSex::kFemale) ? (new_subpop->parent_first_male_index_ - 1) : (new_subpop->parent_subpop_size_ - 1);
			slim_popsize_t sex_size = last_index - start_index + 1;
			
			if (tabulation_size != sex_size)
				EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): (internal error) mismatch between tabulation size and subpop size." << EidosTerminate();
			
			slim_popsize_t tabulation_index = -1;
			
			for (slim_popsize_t ind_index = start_index; ind_index <= last_index; ++ind_index)
			{
				// scan for the next tabulation entry of the expected sex
				do {
					tabulation_index++;
					
					if (tabulation_index >= subpop_size)
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): (internal error) ran out of tabulated individuals." << EidosTerminate();
				} while (subpop_info.sex_[tabulation_index] != generating_sex);
				
				Individual *individual = new_subpop->parent_individuals_[ind_index];
				
				if (individual->sex_ != generating_sex)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): (internal error) unexpected individual sex." << EidosTerminate();
				
				tsk_id_t node_id_0 = subpop_info.nodes_[(size_t)tabulation_index * 2];
				tsk_id_t node_id_1 = subpop_info.nodes_[(size_t)tabulation_index * 2 + 1];
				
				if (node_id_0 + 1 != node_id_1)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): (internal error) node ids for individual are not adjacent." << EidosTerminate();
				
				if (individual->TskitNodeIdBase() != node_id_0)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): tskit node id mismatch between chromosomes read." << EidosTerminate();
				
				slim_pedigreeid_t pedigree_id = subpop_info.pedigreeID_[tabulation_index];
				
				if (individual->PedigreeID() != pedigree_id)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): pedigree id mismatch between chromosomes read." << EidosTerminate();
				if ((individual->Parent1PedigreeID() != subpop_info.pedigreeP1_[tabulation_index]) ||
					(individual->Parent2PedigreeID() != subpop_info.pedigreeP2_[tabulation_index]))
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): parent pedigree id mismatch between chromosomes read." << EidosTerminate();
				
				uint32_t flags = subpop_info.flags_[tabulation_index];
				if ((flags & SLIM_INDIVIDUAL_METADATA_MIGRATED) && !individual->migrant_)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): individual migrant flag mismatch between chromosomes read." << EidosTerminate();
				
				if (individual->age_ != subpop_info.age_[tabulation_index])
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): individual age mismatch between chromosomes read." << EidosTerminate();
				if ((individual->spatial_x_ != subpop_info.spatial_x_[tabulation_index]) ||
					(individual->spatial_y_ != subpop_info.spatial_y_[tabulation_index]) ||
					(individual->spatial_z_ != subpop_info.spatial_z_[tabulation_index]))
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): individual spatial position mismatch between chromosomes read." << EidosTerminate();
				
				// the haplosomes we're setting up are different from the haplosomes previously set up,
				// so unlike the above code, we actually do work here, not just checkbacks
				p_nodeToHaplosomeMap.emplace(node_id_0, individual->haplosomes_[first_haplosome_index]);
				individual->haplosomes_[first_haplosome_index]->haplosome_id_ = pedigree_id * 2;
				
				if (last_haplosome_index != first_haplosome_index)
				{
					p_nodeToHaplosomeMap.emplace(node_id_1, individual->haplosomes_[last_haplosome_index]);
					individual->haplosomes_[last_haplosome_index]->haplosome_id_ = pedigree_id * 2 + 1;
				}
				
				// check the referenced nodes; right now this is not essential for re-creating the saved state, but is just a crosscheck
				// here we crosscheck the node information against the realized values in the haplosomes of the individual
				tsk_node_table_t &node_table = tables.nodes;
				size_t node0_metadata_length = node_table.metadata_offset[node_id_0 + 1] - node_table.metadata_offset[node_id_0];
				size_t node1_metadata_length = node_table.metadata_offset[node_id_1 + 1] - node_table.metadata_offset[node_id_1];
				
				int byte_index = chromosome_index / 8;
				int bit_shift = chromosome_index % 8;
				size_t expected_min_metadata_length = sizeof(HaplosomeMetadataRec) + byte_index;	// 1 byte already counted in HaplosomeMetadataRec
				
				// check that the metadata is long enough to contain the is_vacant bit we will look at; we don't check
				// that it is exactly the length we expect here, just that it works for our local purposes
				if ((node0_metadata_length < expected_min_metadata_length) || (node1_metadata_length < expected_min_metadata_length))
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation_SECONDARY): unexpected node metadata length; this file cannot be read." << EidosTerminate();
				
				HaplosomeMetadataRec *node0_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_0]);
				Haplosome *haplosome0 = individual->haplosomes_[first_haplosome_index];
				
				if (node0_metadata->haplosome_id_ != haplosome0->haplosome_id_)
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome id mismatch; this file cannot be read." << EidosTerminate();
				
				// Null haplosomes are allowed to occur arbitrarily in nonWF models in chromosome types 'A' and 'H'
				bool node0_is_vacant = !!((node0_metadata->is_vacant_[byte_index] >> bit_shift) & 0x01);
				
				if (node0_is_vacant != haplosome0->IsNull())
				{
					if (node0_is_vacant && (model_type_ == SLiMModelType::kModelTypeNonWF) &&
						((chromosomeType == ChromosomeType::kA_DiploidAutosome) || (chromosomeType == ChromosomeType::kH_HaploidAutosome)))
					{
						haplosome0->MakeNull();
						new_subpop->has_null_haplosomes_ = true;
					}
					else
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome null mismatch; this file cannot be read." << EidosTerminate();
				}
				
				// We do not check the second haplosome's state if the chromosome type is intrinsically haploid
				if (last_haplosome_index != first_haplosome_index)
				{
					HaplosomeMetadataRec *node1_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_1]);
					Haplosome *haplosome1 = individual->haplosomes_[last_haplosome_index];
					
					if (node1_metadata->haplosome_id_ != haplosome1->haplosome_id_)
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome id mismatch; this file cannot be read." << EidosTerminate();
					
					bool node1_is_vacant = !!((node1_metadata->is_vacant_[byte_index] >> bit_shift) & 0x01);
					
					if (node1_is_vacant != haplosome1->IsNull())
					{
						if (node1_is_vacant && (model_type_ == SLiMModelType::kModelTypeNonWF) &&
							((chromosomeType == ChromosomeType::kA_DiploidAutosome) || (chromosomeType == ChromosomeType::kH_HaploidAutosome)))
						{
							haplosome1->MakeNull();
							new_subpop->has_null_haplosomes_ = true;
						}
						else
							EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome null mismatch; this file cannot be read." << EidosTerminate();
					}
				}
			}
		}
	}
}

void Species::__ConfigureSubpopulationsFromTables(EidosInterpreter *p_interpreter, TreeSeqInfo &p_treeseq)
{
	tsk_table_collection_t &tables = p_treeseq.tables_;
	tsk_population_table_t &pop_table = tables.populations;
	tsk_size_t pop_count = pop_table.num_rows;
	
	for (tsk_size_t pop_index = 0; pop_index < pop_count; pop_index++)
	{
		// validate and parse metadata; get metadata values or fall back to default values
		size_t metadata_length = pop_table.metadata_offset[pop_index + 1] - pop_table.metadata_offset[pop_index];
		char *metadata_char = pop_table.metadata + pop_table.metadata_offset[pop_index];
		slim_objectid_t subpop_id = CheckSLiMPopulationMetadata(metadata_char, metadata_length);
		
		// -1 indicates that the metadata does not represent an extant SLiM subpopulation, so we
		// skip it entirely; this logic mirrors that in __PrepareSubpopulationsFromTables(), which has
		// already created a ts_subpop_info record for every SLiM-compliant subpopulation
		if (subpop_id == -1)
			continue;
		
		// otherwise, the metadata is valid and we proceed; this design means we parse the JSON twice, but whatever
		std::string metadata_string(metadata_char, metadata_length);
		nlohmann::json subpop_metadata = nlohmann::json::parse(metadata_string);

		// Now we get to new work not done by __PrepareSubpopulationsFromTables()
		double metadata_selfing_fraction = 0.0;
		double metadata_female_clone_fraction = 0.0;
		double metadata_male_clone_fraction = 0.0;
		double metadata_sex_ratio = 0.5;
		double metadata_bounds_x0 = 0.0;
		double metadata_bounds_x1 = 1.0;
		double metadata_bounds_y0 = 0.0;
		double metadata_bounds_y1 = 1.0;
		double metadata_bounds_z0 = 0.0;
		double metadata_bounds_z1 = 1.0;
		
		if (subpop_metadata.contains("bounds_x0"))
		{
			nlohmann::json value = subpop_metadata["bounds_x0"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'bounds_x0' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_x0 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_x1"))
		{
			nlohmann::json value = subpop_metadata["bounds_x1"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'bounds_x1' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_x1 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_y0"))
		{
			nlohmann::json value = subpop_metadata["bounds_y0"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'bounds_y0' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_y0 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_y1"))
		{
			nlohmann::json value = subpop_metadata["bounds_y1"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'bounds_y1' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_y1 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_z0"))
		{
			nlohmann::json value = subpop_metadata["bounds_z0"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'bounds_z0' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_z0 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_z1"))
		{
			nlohmann::json value = subpop_metadata["bounds_z1"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'bounds_z1' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_z1 = value.get<double>();
		}
		if (subpop_metadata.contains("female_cloning_fraction"))
		{
			nlohmann::json value = subpop_metadata["female_cloning_fraction"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'female_cloning_fraction' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_female_clone_fraction = value.get<double>();
		}
		if (subpop_metadata.contains("male_cloning_fraction"))
		{
			nlohmann::json value = subpop_metadata["male_cloning_fraction"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'male_cloning_fraction' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_male_clone_fraction = value.get<double>();
		}
		if (subpop_metadata.contains("selfing_fraction"))
		{
			nlohmann::json value = subpop_metadata["selfing_fraction"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'selfing_fraction' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_selfing_fraction = value.get<double>();
		}
		if (subpop_metadata.contains("sex_ratio"))
		{
			nlohmann::json value = subpop_metadata["sex_ratio"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'sex_ratio' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_sex_ratio = value.get<double>();
		}
		
		std::string metadata_name = SLiMEidosScript::IDStringWithPrefix('p', subpop_id);
		std::string metadata_description;
		
		if (subpop_metadata.contains("name"))
		{
			nlohmann::json value = subpop_metadata["name"];
			if (!value.is_string())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'name' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_name = value.get<std::string>();
		}
		if (subpop_metadata.contains("description"))
		{
			nlohmann::json value = subpop_metadata["description"];
			if (!value.is_string())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'description' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_description = value.get<std::string>();
		}
		
		size_t migration_rec_count = 0;
		nlohmann::json migration_records;
		
		if (subpop_metadata.contains("migration_records"))
		{
			nlohmann::json value = subpop_metadata["migration_records"];
			if (!value.is_array())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata key 'migration_records' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			migration_records = value;
			migration_rec_count = migration_records.size();
		}
		
		// construct the subpopulation from the metadata values and other information we have decoded
		Subpopulation *subpop = SubpopulationWithID(subpop_id);
		
		if (!subpop)
		{
			// in a WF model it is an error to have a referenced subpop that is empty, so skip this subpop
			// we want to allow the population table to contain unreferenced empty subpops (for ancestral stuff)
			if (model_type_ == SLiMModelType::kModelTypeWF)
				continue;
			
			// In a nonWF model an empty subpop is legal, so create it without recording
			recording_tree_ = false;
			subpop = population_.AddSubpopulation(subpop_id, 0, 0.5, false);
			recording_tree_ = true;
			
			// define a new Eidos variable to refer to the new subpopulation
			EidosSymbolTableEntry &symbol_entry = subpop->SymbolTableEntry();
			
			if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): new subpopulation symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here; this file cannot be read." << EidosTerminate();
			
			community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
		}
		
		subpop->SetName(metadata_name);
		subpop->description_ = metadata_description;
		
		if (model_type_ == SLiMModelType::kModelTypeWF)
		{
			subpop->selfing_fraction_ = metadata_selfing_fraction;
			subpop->female_clone_fraction_ = metadata_female_clone_fraction;
			subpop->male_clone_fraction_ = metadata_male_clone_fraction;
			subpop->child_sex_ratio_ = metadata_sex_ratio;
			
			if (!sex_enabled_ && (subpop->female_clone_fraction_ != subpop->male_clone_fraction_))
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): cloning rate mismatch for non-sexual model; this file cannot be read." << EidosTerminate();
			if (sex_enabled_ && (subpop->selfing_fraction_ != 0.0))
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): selfing rate may be non-zero only for hermaphoditic models; this file cannot be read." << EidosTerminate();
			if ((subpop->female_clone_fraction_ < 0.0) || (subpop->female_clone_fraction_ > 1.0) ||
				(subpop->male_clone_fraction_ < 0.0) || (subpop->male_clone_fraction_ > 1.0) ||
				(subpop->selfing_fraction_ < 0.0) || (subpop->selfing_fraction_ > 1.0))
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): out-of-range value for cloning rate or selfing rate; this file cannot be read." << EidosTerminate();
			if (sex_enabled_ && ((subpop->child_sex_ratio_ < 0.0) || (subpop->child_sex_ratio_ > 1.0)))
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): out-of-range value for sex ratio; this file cannot be read." << EidosTerminate();
		}
		
		subpop->bounds_x0_ = metadata_bounds_x0;
		subpop->bounds_x1_ = metadata_bounds_x1;
		subpop->bounds_y0_ = metadata_bounds_y0;
		subpop->bounds_y1_ = metadata_bounds_y1;
		subpop->bounds_z0_ = metadata_bounds_z0;
		subpop->bounds_z1_ = metadata_bounds_z1;
		
		if (((spatial_dimensionality_ >= 1) && (subpop->bounds_x0_ >= subpop->bounds_x1_)) ||
			((spatial_dimensionality_ >= 2) && (subpop->bounds_y0_ >= subpop->bounds_y1_)) ||
			((spatial_dimensionality_ >= 3) && (subpop->bounds_z0_ >= subpop->bounds_z1_)))
			EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): unsorted spatial bounds; this file cannot be read." << EidosTerminate();
		if (((spatial_dimensionality_ >= 1) && periodic_x_ && (subpop->bounds_x0_ != 0.0)) ||
			((spatial_dimensionality_ >= 2) && periodic_y_ && (subpop->bounds_y0_ != 0.0)) ||
			((spatial_dimensionality_ >= 3) && periodic_z_ && (subpop->bounds_z0_ != 0.0)))
			EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): periodic bounds must have a minimum coordinate of 0.0; this file cannot be read." << EidosTerminate();
		
		if ((model_type_ == SLiMModelType::kModelTypeNonWF) && (migration_rec_count > 0))
			EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): migration rates cannot be provided in a nonWF model; this file cannot be read." << EidosTerminate();
		
		for (size_t migration_index = 0; migration_index < migration_rec_count; ++migration_index)
		{
			nlohmann::json migration_rec = migration_records[migration_index];
			
			if (!migration_rec.is_object() || !migration_rec.contains("migration_rate") || !migration_rec["migration_rate"].is_number() || !migration_rec.contains("source_subpop") || !migration_rec["source_subpop"].is_number_integer())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): population metadata migration record does not obey the metadata schema; this file cannot be read." << EidosTerminate(nullptr);
			
			slim_objectid_t source_id = migration_rec["source_subpop"].get<slim_objectid_t>();
			double rate = migration_rec["migration_rate"].get<double>();
			
			if (source_id == subpop_id)
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): self-referential migration record; this file cannot be read." << EidosTerminate();
			if (subpop->migrant_fractions_.find(source_id) != subpop->migrant_fractions_.end())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): duplicate migration record; this file cannot be read." << EidosTerminate();
			if ((rate < 0.0) || (rate > 1.0))
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables): out-of-range migration rate; this file cannot be read." << EidosTerminate();
			
			subpop->migrant_fractions_.emplace(source_id, rate);
		}
	}
}

void Species::__ConfigureSubpopulationsFromTables_SECONDARY(__attribute__((unused)) EidosInterpreter *p_interpreter, TreeSeqInfo &p_treeseq)
{
	// NOTE: This version of __ConfigureSubpopulationsFromTables() validates the configuration already set up,
	// ensuring that it matches those made by __ConfigureSubpopulationsFromTables() for the first chromosome
	// read.  BEWARE: These methods should be maintained in parallel!
	
	tsk_table_collection_t &tables = p_treeseq.tables_;
	tsk_population_table_t &pop_table = tables.populations;
	tsk_size_t pop_count = pop_table.num_rows;
	
	for (tsk_size_t pop_index = 0; pop_index < pop_count; pop_index++)
	{
		// validate and parse metadata; get metadata values or fall back to default values
		size_t metadata_length = pop_table.metadata_offset[pop_index + 1] - pop_table.metadata_offset[pop_index];
		char *metadata_char = pop_table.metadata + pop_table.metadata_offset[pop_index];
		slim_objectid_t subpop_id = CheckSLiMPopulationMetadata(metadata_char, metadata_length);
		
		// -1 indicates that the metadata does not represent an extant SLiM subpopulation, so we
		// skip it entirely; this logic mirrors that in __PrepareSubpopulationsFromTables(), which has
		// already created a ts_subpop_info record for every SLiM-compliant subpopulation
		if (subpop_id == -1)
			continue;
		
		// otherwise, the metadata is valid and we proceed; this design means we parse the JSON twice, but whatever
		std::string metadata_string(metadata_char, metadata_length);
		nlohmann::json subpop_metadata = nlohmann::json::parse(metadata_string);

		// Now we get to new work not done by __PrepareSubpopulationsFromTables()
		double metadata_selfing_fraction = 0.0;
		double metadata_female_clone_fraction = 0.0;
		double metadata_male_clone_fraction = 0.0;
		double metadata_sex_ratio = 0.5;
		double metadata_bounds_x0 = 0.0;
		double metadata_bounds_x1 = 1.0;
		double metadata_bounds_y0 = 0.0;
		double metadata_bounds_y1 = 1.0;
		double metadata_bounds_z0 = 0.0;
		double metadata_bounds_z1 = 1.0;
		
		if (subpop_metadata.contains("bounds_x0"))
		{
			nlohmann::json value = subpop_metadata["bounds_x0"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'bounds_x0' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_x0 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_x1"))
		{
			nlohmann::json value = subpop_metadata["bounds_x1"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'bounds_x1' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_x1 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_y0"))
		{
			nlohmann::json value = subpop_metadata["bounds_y0"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'bounds_y0' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_y0 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_y1"))
		{
			nlohmann::json value = subpop_metadata["bounds_y1"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'bounds_y1' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_y1 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_z0"))
		{
			nlohmann::json value = subpop_metadata["bounds_z0"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'bounds_z0' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_z0 = value.get<double>();
		}
		if (subpop_metadata.contains("bounds_z1"))
		{
			nlohmann::json value = subpop_metadata["bounds_z1"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'bounds_z1' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_bounds_z1 = value.get<double>();
		}
		if (subpop_metadata.contains("female_cloning_fraction"))
		{
			nlohmann::json value = subpop_metadata["female_cloning_fraction"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'female_cloning_fraction' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_female_clone_fraction = value.get<double>();
		}
		if (subpop_metadata.contains("male_cloning_fraction"))
		{
			nlohmann::json value = subpop_metadata["male_cloning_fraction"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'male_cloning_fraction' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_male_clone_fraction = value.get<double>();
		}
		if (subpop_metadata.contains("selfing_fraction"))
		{
			nlohmann::json value = subpop_metadata["selfing_fraction"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'selfing_fraction' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_selfing_fraction = value.get<double>();
		}
		if (subpop_metadata.contains("sex_ratio"))
		{
			nlohmann::json value = subpop_metadata["sex_ratio"];
			if (!value.is_number())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'sex_ratio' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_sex_ratio = value.get<double>();
		}
		
		std::string metadata_name = SLiMEidosScript::IDStringWithPrefix('p', subpop_id);
		
		if (subpop_metadata.contains("name"))
		{
			nlohmann::json value = subpop_metadata["name"];
			if (!value.is_string())
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): population metadata key 'name' is not the expected type; this file cannot be read." << EidosTerminate(nullptr);
			metadata_name = value.get<std::string>();
		}
		
		// we skip the description; it does not get validated across chromosomes
		
		// migration does not get validated across chromosomes either, too annoying and marginal
		
		// validate the subpopulation from the metadata values and other information we have decoded
		Subpopulation *subpop = SubpopulationWithID(subpop_id);
		
		if (!subpop)
			EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): a subpopulation (id " << subpop_id << ") was not defined by the first .trees file read, but was referenced by a later .trees file; subpopulation structure must match exactly across chromosomes." << EidosTerminate();
		
		if (subpop->name_ != metadata_name)
			EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): subpopulation name mismatch between chromosomes read." << EidosTerminate();
		
		if (model_type_ == SLiMModelType::kModelTypeWF)
		{
			if (subpop->selfing_fraction_ != metadata_selfing_fraction)
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): selfing fraction mismatch between chromosomes read." << EidosTerminate();
			if (subpop->female_clone_fraction_ != metadata_female_clone_fraction)
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): female cloning fraction mismatch between chromosomes read." << EidosTerminate();
			if (subpop->male_clone_fraction_ != metadata_male_clone_fraction)
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): male cloning fraction mismatch between chromosomes read." << EidosTerminate();
			if (subpop->child_sex_ratio_ != metadata_sex_ratio)
				EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): sex ratio mismatch between chromosomes read." << EidosTerminate();
		}
		
		if ((subpop->bounds_x0_ != metadata_bounds_x0) ||
			(subpop->bounds_x1_ != metadata_bounds_x1) ||
			(subpop->bounds_y0_ != metadata_bounds_y0) ||
			(subpop->bounds_y1_ != metadata_bounds_y1) ||
			(subpop->bounds_z0_ != metadata_bounds_z0) ||
			(subpop->bounds_z1_ != metadata_bounds_z1))
			EIDOS_TERMINATION << "ERROR (Species::__ConfigureSubpopulationsFromTables_SECONDARY): spatial bounds mismatch between chromosomes read." << EidosTerminate();
	}
}

typedef struct ts_mut_info {
	slim_position_t position;
	MutationMetadataRec metadata;
	slim_refcount_t ref_count;
} ts_mut_info;

void Species::__TabulateMutationsFromTables(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, TreeSeqInfo &p_treeseq, __attribute__ ((unused)) int p_file_version)
{
	tsk_table_collection_t &tables = p_treeseq.tables_;
	std::size_t metadata_rec_size = sizeof(MutationMetadataRec);
	tsk_mutation_table_t &mut_table = tables.mutations;
	tsk_size_t mut_count = mut_table.num_rows;
	
	if ((mut_count > 0) && !recording_mutations_)
		EIDOS_TERMINATION << "ERROR (Species::__TabulateMutationsFromTables): cannot load mutations when mutation recording is disabled." << EidosTerminate();
	
	for (tsk_size_t mut_index = 0; mut_index < mut_count; ++mut_index)
	{
		const char *derived_state_bytes = mut_table.derived_state + mut_table.derived_state_offset[mut_index];
		tsk_size_t derived_state_length = mut_table.derived_state_offset[mut_index + 1] - mut_table.derived_state_offset[mut_index];
		const char *metadata_bytes = mut_table.metadata + mut_table.metadata_offset[mut_index];
		tsk_size_t metadata_length = mut_table.metadata_offset[mut_index + 1] - mut_table.metadata_offset[mut_index];
		
		if (derived_state_length % sizeof(slim_mutationid_t) != 0)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateMutationsFromTables): unexpected mutation derived state length; this file cannot be read." << EidosTerminate();
		if (metadata_length % metadata_rec_size != 0)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateMutationsFromTables): unexpected mutation metadata length; this file cannot be read." << EidosTerminate();
		if (derived_state_length / sizeof(slim_mutationid_t) != metadata_length / metadata_rec_size)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateMutationsFromTables): (internal error) mutation metadata length does not match derived state length." << EidosTerminate();
		
		int stack_count = (int)(derived_state_length / sizeof(slim_mutationid_t));
		slim_mutationid_t *derived_state_vec = (slim_mutationid_t *)derived_state_bytes;
		const void *metadata_vec = metadata_bytes;	// either const MutationMetadataRec* or const MutationMetadataRec_PRENUC*
		tsk_id_t site_id = mut_table.site[mut_index];
		double position_double = tables.sites.position[site_id];
		double position_double_round = round(position_double);
		
		if (position_double_round != position_double)
			EIDOS_TERMINATION << "ERROR (Species::__TabulateMutationsFromTables): mutation positions must be whole numbers for importation into SLiM; fractional positions are not allowed." << EidosTerminate();
		
		slim_position_t position = (slim_position_t)position_double_round;
		
		// tabulate the mutations referenced by this entry, overwriting previous tabulations (last state wins)
		for (int stack_index = 0; stack_index < stack_count; ++stack_index)
		{
			slim_mutationid_t mut_id = derived_state_vec[stack_index];
			
			auto mut_info_find = p_mutMap.find(mut_id);
			ts_mut_info *mut_info;
			
			if (mut_info_find == p_mutMap.end())
			{
				// no entry already present; create one
				auto mut_info_insert = p_mutMap.emplace(mut_id, ts_mut_info());
				mut_info = &((mut_info_insert.first)->second);
				
				mut_info->position = position;
			}
			else
			{
				// entry already present; check that it refers to the same mutation, using its position (see https://github.com/MesserLab/SLiM/issues/179)
				mut_info = &(mut_info_find->second);
				
				if (mut_info->position != position)
					EIDOS_TERMINATION << "ERROR (Species::__TabulateMutationsFromTables): inconsistent mutation position observed reading tree sequence data; this may indicate that mutation IDs are not unique." << EidosTerminate();
			}
			
			MutationMetadataRec *metadata = (MutationMetadataRec *)metadata_vec + stack_index;
			
			mut_info->metadata = *metadata;
		}
	}
}

void Species::__TallyMutationReferencesWithTreeSequence(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, std::unordered_map<tsk_id_t, Haplosome *> p_nodeToHaplosomeMap, tsk_treeseq_t *p_ts)
{
	// allocate and set up the tsk_variant object we'll use to walk through sites
	tsk_variant_t *variant;
	variant = (tsk_variant_t *)malloc(sizeof(tsk_variant_t));
	if (!variant)
		EIDOS_TERMINATION << "ERROR (Species::__TallyMutationReferencesWithTreeSequence): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	int ret = tsk_variant_init(
			variant, p_ts, NULL, 0, NULL, TSK_ISOLATED_NOT_MISSING);
	if (ret != 0) handle_error("__TallyMutationReferencesWithTreeSequence tsk_variant_init()", ret);
	
	// set up a map from sample indices in the variant to Haplosome objects; the sample
	// may contain nodes that are ancestral and need to be excluded
	std::vector<Haplosome *> indexToHaplosomeMap;
	size_t sample_count = variant->num_samples;
	
	for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
	{
		tsk_id_t sample_node_id = variant->samples[sample_index];
		auto sample_nodeToHaplosome_iter = p_nodeToHaplosomeMap.find(sample_node_id);
		
		if (sample_nodeToHaplosome_iter != p_nodeToHaplosomeMap.end())
		{
			indexToHaplosomeMap.emplace_back(sample_nodeToHaplosome_iter->second);
		}
		else
		{
			// this sample is presumably not extant; no corresponding haplosome, so record nullptr
			indexToHaplosomeMap.emplace_back(nullptr);
		}
	}
	
	// add mutations to haplosomes by looping through variants
	for (tsk_size_t i = 0; i < p_ts->tables->sites.num_rows; i++)
	{
		ret = tsk_variant_decode(variant, (tsk_id_t)i, 0);
		if (ret < 0) handle_error("__TallyMutationReferencesWithTreeSequence tsk_variant_decode()", ret);
		
		// We have a new variant; set it into SLiM.  A variant represents a site at which a tracked mutation exists.
		// The tsk_variant_t will tell us all the allelic states involved at that site, what the alleles are, and which haplosomes
		// in the sample are using them.  We want to find any mutations that are shared across all non-null haplosomes.
		for (tsk_size_t allele_index = 0; allele_index < variant->num_alleles; ++allele_index)
		{
			tsk_size_t allele_length = variant->allele_lengths[allele_index];
			
			if (allele_length > 0)
			{
				// Calculate the number of extant haplosomes that reference this allele
				int32_t allele_refs = 0;
				
				for (size_t sample_index = 0; sample_index < sample_count; sample_index++)
					if ((variant->genotypes[sample_index] == (int32_t)allele_index) && (indexToHaplosomeMap[sample_index] != nullptr))
						allele_refs++;
				
				// If that count is greater than zero (might be zero if only non-extant nodes reference the allele), tally it
				if (allele_refs)
				{
					if (allele_length % sizeof(slim_mutationid_t) != 0)
						EIDOS_TERMINATION << "ERROR (Species::__TallyMutationReferencesWithTreeSequence): (internal error) variant allele had length that was not a multiple of sizeof(slim_mutationid_t)." << EidosTerminate();
					allele_length /= sizeof(slim_mutationid_t);
					
					slim_mutationid_t *allele = (slim_mutationid_t *)variant->alleles[allele_index];
					
					for (tsk_size_t mutid_index = 0; mutid_index < allele_length; ++mutid_index)
					{
						slim_mutationid_t mut_id = allele[mutid_index];
						auto mut_info_iter = p_mutMap.find(mut_id);
						
						if (mut_info_iter == p_mutMap.end())
							EIDOS_TERMINATION << "ERROR (Species::__TallyMutationReferencesWithTreeSequence): mutation id " << mut_id << " was referenced but does not exist." << EidosTerminate();
						
						// Add allele_refs to the refcount for this mutation
						ts_mut_info &mut_info = mut_info_iter->second;
						
						mut_info.ref_count += allele_refs;
					}
				}
			}
		}
	}
	
	// free
	ret = tsk_variant_free(variant);
	if (ret != 0) handle_error("__TallyMutationReferencesWithTreeSequence tsk_variant_free()", ret);
	free(variant);
}

void Species::__CreateMutationsFromTabulation(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutInfoMap, std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, TreeSeqInfo &p_treeseq)
{
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	int first_haplosome_index = FirstHaplosomeIndices()[chromosome_index];
	int last_haplosome_index = LastHaplosomeIndices()[chromosome_index];
	
	// count the number of non-null haplosomes there are for the focal chromosome; this is the count that would represent fixation
	slim_refcount_t fixation_count = 0;
	
	for (auto pop_iter : population_.subpops_)
	{
		Subpopulation *subpop = pop_iter.second;
		
		for (Individual *ind : subpop->parent_individuals_)
		{
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				if (!haplosome->IsNull())
					fixation_count++;
			}
		}
	}
	
	// instantiate mutations
	Mutation *mut_block_ptr = mutation_block_->mutation_buffer_;
	
	for (auto mut_info_iter : p_mutInfoMap)
	{
		slim_mutationid_t mutation_id = mut_info_iter.first;
		ts_mut_info &mut_info = mut_info_iter.second;
		MutationMetadataRec *metadata_ptr = &mut_info.metadata;
		MutationMetadataRec metadata;
		slim_position_t position = mut_info.position;
		
		// BCH 4 Feb 2020: bump the next mutation ID counter as needed here, so that this happens in all cases – even if
		// the mutation in the mutation table is fixed (so we will create a Substitution) or absent (so we will create
		// nothing).  Even in those cases, we have to ensure that we do not re-use the previously used mutation ID.
		if (gSLiM_next_mutation_id <= mutation_id)
			gSLiM_next_mutation_id = mutation_id + 1;
		
		// a mutation might not be referenced by any extant haplosome; it might be present in an ancestral node,
		// but have been lost in all descendants, in which case we do not need to instantiate it
		if (mut_info.ref_count == 0)
			continue;
		
		// BCH 4/25/2019: copy the metadata with memcpy(), avoiding a misaligned pointer access; this is needed because
		// sizeof(MutationMetadataRec) is odd, according to Xcode.  Actually I think this might be a bug in Xcode's runtime
		// checking, because MutationMetadataRec is defined as packed so the compiler should not use aligned reads for it...?
		// Anyway, it's a safe fix and will probably get optimized away by the compiler, so whatever...
		memcpy(&metadata, metadata_ptr, sizeof(MutationMetadataRec));
		
		// look up the mutation type from its index
		MutationType *mutation_type_ptr = MutationTypeWithID(metadata.mutation_type_id_);
		
		if (!mutation_type_ptr) 
			EIDOS_TERMINATION << "ERROR (Species::__CreateMutationsFromTabulation): mutation type m" << metadata.mutation_type_id_ << " has not been defined for this species." << EidosTerminate();
		
		if ((mut_info.ref_count == fixation_count) && (mutation_type_ptr->convert_to_substitution_))
		{
			// this mutation is fixed, and the muttype wants substitutions, so make a substitution
			// FIXME MULTITRAIT for now I assume the dominance coeff from the mutation type; needs to be added to MutationMetadataRec; likewise hemizygous dominance
			// FIXME MULTITRAIT this code will also now need to handle the independent dominance case, for which NaN should be in the metadata
			Substitution *sub = new Substitution(mutation_id, mutation_type_ptr, chromosome_index, position, metadata.selection_coeff_, mutation_type_ptr->DefaultDominanceForTrait(0) /* metadata.dominance_coeff_ */, metadata.subpop_index_, metadata.origin_tick_, community_.Tick(), metadata.nucleotide_);	// FIXME MULTITRAIT
			
			population_.treeseq_substitutions_map_.emplace(position, sub);
			population_.substitutions_.emplace_back(sub);
			
			// add -1 to our local map, so we know there's an entry but we also know it's a substitution
			p_mutIndexMap[mutation_id] = -1;
		}
		else
		{
			// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
			MutationIndex new_mut_index = mutation_block_->NewMutationFromBlock();
			
			// FIXME MULTITRAIT for now I assume the dominance coeff from the mutation type; needs to be added to MutationMetadataRec; likewise hemizygous dominance
			// FIXME MULTITRAIT this code will also now need to handle the independent dominance case, for which NaN should be in the metadata
			Mutation *new_mut = new (mut_block_ptr + new_mut_index) Mutation(mutation_id, mutation_type_ptr, chromosome_index, position, metadata.selection_coeff_, mutation_type_ptr->DefaultDominanceForTrait(0) /* metadata.dominance_coeff_ */, metadata.subpop_index_, metadata.origin_tick_, metadata.nucleotide_);	// FIXME MULTITRAIT
			
			// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
			p_mutIndexMap[mutation_id] = new_mut_index;
			population_.MutationRegistryAdd(new_mut);
			
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
			if (population_.keeping_muttype_registries_)
				EIDOS_TERMINATION << "ERROR (Species::__CreateMutationsFromTabulation): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
		}
		
		// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_neutral_mutations_
		if (metadata.selection_coeff_ != (slim_effect_t)0.0)
		{
			pure_neutral_ = false;
			mutation_type_ptr->all_neutral_mutations_ = false;
		}
	}
}

void Species::__AddMutationsFromTreeSequenceToHaplosomes(std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, std::unordered_map<tsk_id_t, Haplosome *> p_nodeToHaplosomeMap, tsk_treeseq_t *p_ts, TreeSeqInfo &p_treeseq)
{
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	Chromosome *chromosome = Chromosomes()[chromosome_index];
	
	// This code is based on Species::CrosscheckTreeSeqIntegrity(), but it can be much simpler.
	// We also don't need to sort/deduplicate/simplify; the tables read in should be simplified already.
	if (!recording_mutations_)
		return;
	
	// allocate and set up the variant object we'll use to walk through sites
	tsk_variant_t *variant;

	variant = (tsk_variant_t *)malloc(sizeof(tsk_variant_t));
	if (!variant)
		EIDOS_TERMINATION << "ERROR (Species::__AddMutationsFromTreeSequenceToHaplosomes): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	int ret = tsk_variant_init(variant, p_ts, NULL, 0, NULL, TSK_ISOLATED_NOT_MISSING);
	if (ret != 0) handle_error("__AddMutationsFromTreeSequenceToHaplosomes tsk_variant_init()", ret);
	
	// set up a map from sample indices in the variant to Haplosome objects; the sample
	// may contain nodes that are ancestral and need to be excluded
	std::vector<Haplosome *> indexToHaplosomeMap;
	size_t sample_count = variant->num_samples;
	
	for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
	{
		tsk_id_t sample_node_id = variant->samples[sample_index];
		auto sample_nodeToHaplosome_iter = p_nodeToHaplosomeMap.find(sample_node_id);
		
		if (sample_nodeToHaplosome_iter != p_nodeToHaplosomeMap.end())
		{
			// we found a haplosome for this sample, so record it
			indexToHaplosomeMap.emplace_back(sample_nodeToHaplosome_iter->second);
		}
		else
		{
			// this sample is presumably not extant; no corresponding haplosome, so record nullptr
			indexToHaplosomeMap.emplace_back(nullptr);
		}
	}
	
	// add mutations to haplosomes by looping through variants
#ifndef _OPENMP
	MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
	
	for (tsk_size_t i = 0; i < p_ts->tables->sites.num_rows; i++)
	{
		ret = tsk_variant_decode(variant, (tsk_id_t)i, 0);
		if (ret < 0) handle_error("__AddMutationsFromTreeSequenceToHaplosomes tsk_variant_decode()", ret);
		
		// We have a new variant; set it into SLiM.  A variant represents a site at which a tracked mutation exists.
		// The tsk_variant_t will tell us all the allelic states involved at that site, what the alleles are, and which haplosomes
		// in the sample are using them.  We will then set all the haplosomes that the variant claims to involve to have
		// the allele the variant attributes to them.  The variants are returned in sorted order by position, so we can
		// always add new mutations to the ends of haplosomes.
		slim_position_t variant_pos_int = (slim_position_t)variant->site.position;
		
		for (size_t sample_index = 0; sample_index < sample_count; sample_index++)
		{
			Haplosome *haplosome = indexToHaplosomeMap[sample_index];
			
			if (haplosome)
			{
				int32_t haplosome_variant = variant->genotypes[sample_index];
				tsk_size_t haplosome_allele_length = variant->allele_lengths[haplosome_variant];
				
				if (haplosome_allele_length % sizeof(slim_mutationid_t) != 0)
					EIDOS_TERMINATION << "ERROR (Species::__AddMutationsFromTreeSequenceToHaplosomes): (internal error) variant allele had length that was not a multiple of sizeof(slim_mutationid_t)." << EidosTerminate();
				haplosome_allele_length /= sizeof(slim_mutationid_t);
				
				if (haplosome_allele_length > 0)
				{
					if (haplosome->IsNull())
						EIDOS_TERMINATION << "ERROR (Species::__AddMutationsFromTreeSequenceToHaplosomes): (internal error) null haplosome has non-zero treeseq allele length " << haplosome_allele_length << "." << EidosTerminate();
					
					slim_mutationid_t *haplosome_allele = (slim_mutationid_t *)variant->alleles[haplosome_variant];
					slim_mutrun_index_t run_index = (slim_mutrun_index_t)(variant_pos_int / haplosome->mutrun_length_);
					
#ifdef _OPENMP
					// When parallel, the MutationRunContext depends upon the position in the haplosome
					MutationRunContext &mutrun_context = ChromosomeMutationRunContextForMutationRunIndex(run_index);
#endif
					
					// We use WillModifyRun_UNSHARED() because we know that these runs are unshared (unless empty);
					// we created them empty, nobody has modified them but us, and we process each haplosome separately.
					MutationRun *mutrun = haplosome->WillModifyRun_UNSHARED(run_index, mutrun_context);
					
					for (tsk_size_t mutid_index = 0; mutid_index < haplosome_allele_length; ++mutid_index)
					{
						slim_mutationid_t mut_id = haplosome_allele[mutid_index];
						auto mut_index_iter = p_mutIndexMap.find(mut_id);
						
						if (mut_index_iter == p_mutIndexMap.end())
							EIDOS_TERMINATION << "ERROR (Species::__AddMutationsFromTreeSequenceToHaplosomes): mutation id " << mut_id << " was referenced but does not exist." << EidosTerminate();
						
						// Add the mutation to the haplosome unless it is fixed (mut_index == -1)
						MutationIndex mut_index = mut_index_iter->second;
						
						if (mut_index != -1)
							mutrun->emplace_back(mut_index);
					}
				}
			}
			else
			{
				// This sample has no corresponding haplosome.  This is generally because the individual it
				// belongs to it is not extant, I think.  It could maybe also be due to some kind of erroneous
				// bookkeeping.  I'm not sure exactly what that would look like, or how we'd differentiate the
				// error case from the ordinary case.  Punting on this until such time as it manifests in a bug.
			}
		}
	}
	
	// free
	ret = tsk_variant_free(variant);
	if (ret != 0) handle_error("__AddMutationsFromTreeSequenceToHaplosomes tsk_variant_free()", ret);
	free(variant);
}

void Species::__CheckNodePedigreeIDs(__attribute__((unused)) EidosInterpreter *p_interpreter, TreeSeqInfo &p_treeseq)
{
	tsk_table_collection_t &tables = p_treeseq.tables_;
	
	// Make sure our next pedigree ID is safe; right now it only accounts for pedigree IDs used by individuals, but maybe there
	// could be nodes in the node table with haplosome pedigree IDs greater than those in use by individuals, in nonWF models.
	// See https://github.com/MesserLab/SLiM/pull/420 for an example model that does this very easily.
	
	// Previously, we checked for duplicate haplosome IDs here as well, just in case.
    // __CreateSubpopulationsFromTabulation() does this in living individuals
    // already; however, it was found to be overly restrictive, in situations
    // involving merging of parallel simulations; see https://github.com/MesserLab/SLiM/issues/538
	tsk_node_table_t &node_table = tables.nodes;
	tsk_size_t node_count = node_table.num_rows;
	
	for (tsk_size_t j = 0; (size_t)j < node_count; j++)
	{
		tsk_size_t offset1 = node_table.metadata_offset[j];
		tsk_size_t offset2 = node_table.metadata_offset[j + 1];
		tsk_size_t length = (offset2 - offset1);
		
		// allow nodes with other types of metadata; but if the metadata length metches ours, we have to assume it's ours
		if (length == sizeof(HaplosomeMetadataRec))
		{
			// get the metadata record and check the haplosome pedigree ID
			HaplosomeMetadataRec *metadata_rec = (HaplosomeMetadataRec *)(node_table.metadata + offset1);
			slim_pedigreeid_t pedigree_id = metadata_rec->haplosome_id_ / 2;			// rounds down to integer
			
			if (pedigree_id >= gSLiM_next_pedigree_id)
			{
				// We tried issuing a warning here:
				//
				// p_interpreter->ErrorOutputStream() << "#WARNING (Species::__CheckNodePedigreeIDs): in reading the tree sequence, a node was encountered with a haplosome pedigree ID that was (after division by 2) greater than the largest individual pedigree ID in the tree sequence.  This is not necessarily an error, but it is highly unusual, and could indicate that the tree sequence file is corrupted.  It may happen due to external manipulations of a tree sequence, or perhaps if an unsimplified tree sequence produced by SLiM is being loaded." << std::endl;
				//
				// It proved not useful.  It got hit if you remembered an individual and then killed it and ended the sim,
				// such that that individual's nodes were still in the table, but it was not-alive and was more recently
				// born than any alive individual.  An uncommon thing to do, but not unreasonable.  On the other hand, this
				// warning did not flag any actually pathological cases for anyone; and it was a very confusing warning!
				
				gSLiM_next_pedigree_id = pedigree_id + 1;
			}
		}
	}
}

void Species::_ReadAncestralSequence(const char *p_file, Chromosome &p_chromosome)
{
	if (nucleotide_based_)
	{
		char *buffer;				// kastore needs to provide us with a memory location from which to read the data
		std::size_t buffer_length;	// kastore needs to provide us with the length, in bytes, of the buffer
		kastore_t store;
		
		int ret = kastore_open(&store, p_file, "r", 0);
		if (ret != 0) {
			kastore_close(&store);
			handle_error("kastore_open", ret);
		}
		
		ret = kastore_gets_uint8(&store, "reference_sequence/data", (uint8_t **)&buffer, &buffer_length);
		
		// SLiM 3.6 and earlier wrote out int8_t data, but now tskit writes uint8_t data; to be tolerant of the old type, if
		// we get a type mismatch, try again with int8_t.  Note that buffer points into kastore's data and need not be freed.
		if (ret == KAS_ERR_TYPE_MISMATCH)
			ret = kastore_gets_int8(&store, "reference_sequence/data", (int8_t **)&buffer, &buffer_length);
		
		if (ret != 0)
			buffer = NULL;
		
		if (!buffer)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitBinaryFile): this is a nucleotide-based model, but there is no reference nucleotide sequence." << EidosTerminate();
		if (buffer_length != p_chromosome.AncestralSequence()->size())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitBinaryFile): the reference nucleotide sequence length does not match the model." << EidosTerminate();
		
		p_chromosome.AncestralSequence()->ReadNucleotidesFromBuffer(buffer);
		
		// buffer is owned by kastore and is freed by closing the store
		kastore_close(&store);
	}
}

static int table_memcmp(const void *__s1, const void *__s2, size_t __n)
{
	// this is a helper function for comparing tskit table columns, because I'm not sure
	// whether all of the defined table columns are necessarily allocated or not...
	
	// first, return unequal if one pointer is nullptr and the other is not
	if ((__s1 && !__s2) || (!__s1 && __s2))
		return 1;
	
	// if both are nullptr, return equal
	if (!__s1 && !__s2)
		return 0;
	
	// finally, fall back to memcmp() with two valid pointers
	return memcmp(__s1, __s2, __n);
}

// Note that tskit has tsk_node_table_equals(), tsk_individual_table_equals(), and tsk_population_table_equals().
// However, those functions don't provide a *reason* for the difference, which I think it important to report
// back to the user, so I'm keeping my versions.  The tskit folks don't intend to change this.
// See https://github.com/tskit-dev/tskit/issues/3089#issuecomment-2687232143
static void _CompareNodeTables(tsk_node_table_t &nodes0, tsk_node_table_t &nodes1);
static void _CompareIndividualTables(tsk_individual_table_t &individuals0, tsk_individual_table_t &individuals1);
static void _ComparePopulationTables(tsk_population_table_t &population0, tsk_population_table_t &population1);

void _CompareNodeTables(tsk_node_table_t &nodes0, tsk_node_table_t &nodes1)
{
	// This delves into the internals of tskit rather heinously...
	if (nodes0.num_rows != nodes1.num_rows)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (number of rows differs)." << EidosTerminate();
	
	if (nodes0.metadata_length != nodes1.metadata_length)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (metadata length differs)." << EidosTerminate();
	
	if (nodes0.metadata_schema_length != nodes1.metadata_schema_length)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (metadata schema length differs)." << EidosTerminate();
	
	tsk_size_t num_rows = nodes0.num_rows;
	
	if (table_memcmp(nodes0.flags, nodes1.flags, num_rows * sizeof(tsk_flags_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (flags column differs)." << EidosTerminate();
	
	if (table_memcmp(nodes0.time, nodes1.time, num_rows * sizeof(double)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (time column differs)." << EidosTerminate();
	
	if (table_memcmp(nodes0.population, nodes1.population, num_rows * sizeof(tsk_id_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (population column differs)." << EidosTerminate();
	
	if (table_memcmp(nodes0.individual, nodes1.individual, num_rows * sizeof(tsk_id_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (individual column differs)." << EidosTerminate();
	
	if (table_memcmp(nodes0.metadata, nodes1.metadata, nodes0.metadata_length) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (metadata column differs)." << EidosTerminate();
	
	if (table_memcmp(nodes0.metadata_offset, nodes1.metadata_offset, num_rows * sizeof(tsk_size_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (metadata_offset column differs)." << EidosTerminate();
	
	if (table_memcmp(nodes0.metadata_schema, nodes1.metadata_schema, nodes0.metadata_schema_length) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareNodeTables): node table mismatch between loaded chromosomes (metadata_schema column differs)." << EidosTerminate();
}

void _CompareIndividualTables(tsk_individual_table_t &individuals0, tsk_individual_table_t &individuals1)
{
	// This delves into the internals of tskit rather heinously...
	if (individuals0.num_rows != individuals1.num_rows)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (number of rows differs)." << EidosTerminate();
	
	if (individuals0.location_length != individuals1.location_length)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (location length differs)." << EidosTerminate();
	
	if (individuals0.parents_length != individuals1.parents_length)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (parents length differs)." << EidosTerminate();
	
	if (individuals0.metadata_length != individuals1.metadata_length)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (metadata length differs)." << EidosTerminate();
	
	if (individuals0.metadata_schema_length != individuals1.metadata_schema_length)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (metadata schema length differs)." << EidosTerminate();
	
	tsk_size_t num_rows = individuals0.num_rows;
	
	if (table_memcmp(individuals0.flags, individuals1.flags, num_rows * sizeof(tsk_flags_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (flags column differs)." << EidosTerminate();
	
	if (table_memcmp(individuals0.location, individuals1.location, individuals0.location_length * sizeof(double)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (location column differs)." << EidosTerminate();
	
	if (table_memcmp(individuals0.location_offset, individuals1.location_offset, num_rows * sizeof(tsk_size_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (location_offset column differs)." << EidosTerminate();
	
	if (table_memcmp(individuals0.parents, individuals1.parents, individuals0.parents_length * sizeof(tsk_id_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (parents column differs)." << EidosTerminate();
	
	if (table_memcmp(individuals0.parents_offset, individuals1.parents_offset, num_rows * sizeof(tsk_size_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (parents_offset column differs)." << EidosTerminate();
	
	if (table_memcmp(individuals0.metadata, individuals1.metadata, individuals0.metadata_length) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (metadata column differs)." << EidosTerminate();
	
	if (table_memcmp(individuals0.metadata_offset, individuals1.metadata_offset, num_rows * sizeof(tsk_size_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (metadata_offset column differs)." << EidosTerminate();
	
	if (table_memcmp(individuals0.metadata_schema, individuals1.metadata_schema, individuals0.metadata_schema_length) != 0)
		EIDOS_TERMINATION << "ERROR (_CompareIndividualTables): individual table mismatch between loaded chromosomes (metadata_schema column differs)." << EidosTerminate();
}

void _ComparePopulationTables(tsk_population_table_t &population0, tsk_population_table_t &population1)
{
	// This delves into the internals of tskit rather heinously...
	if (population0.num_rows != population1.num_rows)
		EIDOS_TERMINATION << "ERROR (_ComparePopulationTables): population table mismatch between loaded chromosomes (number of rows differs)." << EidosTerminate();
	
	if (population0.metadata_length != population1.metadata_length)
		EIDOS_TERMINATION << "ERROR (_ComparePopulationTables): population table mismatch between loaded chromosomes (metadata length differs)." << EidosTerminate();
	
	if (population0.metadata_schema_length != population1.metadata_schema_length)
		EIDOS_TERMINATION << "ERROR (_ComparePopulationTables): population table mismatch between loaded chromosomes (metadata schema length differs)." << EidosTerminate();
	
	tsk_size_t num_rows = population0.num_rows;
	
	if (table_memcmp(population0.metadata, population1.metadata, population0.metadata_length) != 0)
		EIDOS_TERMINATION << "ERROR (_ComparePopulationTables): population table mismatch between loaded chromosomes (metadata column differs)." << EidosTerminate();
	
	if (table_memcmp(population0.metadata_offset, population1.metadata_offset, num_rows * sizeof(tsk_size_t)) != 0)
		EIDOS_TERMINATION << "ERROR (_ComparePopulationTables): population table mismatch between loaded chromosomes (metadata_offset column differs)." << EidosTerminate();
	
	if (table_memcmp(population0.metadata_schema, population1.metadata_schema, population0.metadata_schema_length) != 0)
		EIDOS_TERMINATION << "ERROR (_ComparePopulationTables): population table mismatch between loaded chromosomes (metadata_schema column differs)." << EidosTerminate();
}

void Species::_InstantiateSLiMObjectsFromTables(EidosInterpreter *p_interpreter, slim_tick_t p_metadata_tick, slim_tick_t p_metadata_cycle, SLiMModelType p_file_model_type, int p_file_version, SUBPOP_REMAP_HASH &p_subpop_map, TreeSeqInfo &p_treeseq)
{
	// NOTE: This method handles the first (or only) chromosome being read in.  A parallel method,
	// _InstantiateSLiMObjectsFromTables_SECONDARY(), handles the second and onward.  The code is
	// quite similar, and should be maintained in parallel!
	tsk_table_collection_t &tables = p_treeseq.tables_;
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	Chromosome *chromosome = Chromosomes()[chromosome_index];
	
	// check the sequence length against the chromosome length
	if (tables.sequence_length != chromosome->last_position_ + 1)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): chromosome length in loaded population (" << tables.sequence_length << ") does not match the configured chromosome length (" << (chromosome->last_position_ + 1) << ")." << EidosTerminate();
	
	// set the tick and cycle from the provenance data
	community_.SetTick(p_metadata_tick);
	SetCycle(p_metadata_cycle);
	
	// rebase the times in the nodes to be in SLiM-land; see WriteTreeSequence for the inverse operation
	// BCH 4/4/2019: switched to using tree_seq_tick_ to avoid a parent/child timestamp conflict
	// This makes sense; as far as tree-seq recording is concerned, tree_seq_tick_ is the time counter
	slim_tick_t time_adjustment = community_.tree_seq_tick_;
	
	for (size_t node_index = 0; node_index < tables.nodes.num_rows; ++node_index)
		tables.nodes.time[node_index] -= time_adjustment;

	for (size_t mut_index = 0; mut_index < tables.mutations.num_rows; ++mut_index)
		tables.mutations.time[mut_index] -= time_adjustment;
	
	// check/rewrite the incoming tree-seq information in various ways
	__CheckPopulationMetadata(p_treeseq);
	__RemapSubpopulationIDs(p_subpop_map, p_treeseq, p_file_version);
	
	// allocate and set up the tree_sequence object
	// note that this tree sequence is based upon whatever sample the file was saved with, and may contain in-sample individuals
	// that are not presently alive, so we have to tread carefully; the actually alive individuals are flagged with 
	// SLIM_TSK_INDIVIDUAL_ALIVE in the individuals table (there may also be remembered and retained individuals in there too)
	tsk_treeseq_t *ts;
	int ret = 0;
	
	ts = (tsk_treeseq_t *)malloc(sizeof(tsk_treeseq_t));
	if (!ts)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	ret = tsk_treeseq_init(ts, &tables, TSK_TS_INIT_BUILD_INDEXES);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_init()", ret);
	
	std::unordered_map<tsk_id_t, Haplosome *> nodeToHaplosomeMap;
	
	{
		std::unordered_map<slim_objectid_t, ts_subpop_info> subpopInfoMap;
		
		__PrepareSubpopulationsFromTables(subpopInfoMap, p_treeseq);
		__TabulateSubpopulationsFromTreeSequence(subpopInfoMap, ts, p_treeseq, p_file_model_type);
		__CreateSubpopulationsFromTabulation(subpopInfoMap, p_interpreter, nodeToHaplosomeMap, p_treeseq);
		__ConfigureSubpopulationsFromTables(p_interpreter, p_treeseq);
	}
	
	std::unordered_map<slim_mutationid_t, MutationIndex> mutIndexMap;
	
	{
		std::unordered_map<slim_mutationid_t, ts_mut_info> mutInfoMap;
		
		__TabulateMutationsFromTables(mutInfoMap, p_treeseq, p_file_version);
		__TallyMutationReferencesWithTreeSequence(mutInfoMap, nodeToHaplosomeMap, ts);
		__CreateMutationsFromTabulation(mutInfoMap, mutIndexMap, p_treeseq);
	}
	
	__AddMutationsFromTreeSequenceToHaplosomes(mutIndexMap, nodeToHaplosomeMap, ts, p_treeseq);
	
	ret = tsk_treeseq_free(ts);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_free()", ret);
	free(ts);
	
	// Reset our last coalescence state; we don't know whether we're coalesced now or not
	p_treeseq.last_coalescence_state_ = false;
}

void Species::_InstantiateSLiMObjectsFromTables_SECONDARY(EidosInterpreter *p_interpreter, slim_tick_t p_metadata_tick, slim_tick_t p_metadata_cycle, SLiMModelType p_file_model_type, int p_file_version, SUBPOP_REMAP_HASH &p_subpop_map, TreeSeqInfo &p_treeseq)
{
	// NOTE: _InstantiateSLiMObjectsFromTables() handles the first (or only) chromosome being read in.  This
	// method handles the second and onward.  The code is quite similar, and should be maintained in parallel!
	
	// NOTE: At this stage we have our own nodes/individuals/population tables!  We will remove them at the end.
	tsk_table_collection_t &tables = p_treeseq.tables_;
	slim_chromosome_index_t chromosome_index = p_treeseq.chromosome_index_;
	Chromosome *chromosome = Chromosomes()[chromosome_index];
	
	// check the sequence length against the chromosome length
	if (tables.sequence_length != chromosome->last_position_ + 1)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables_SECONDARY): chromosome length in loaded population (" << tables.sequence_length << ") does not match the configured chromosome length (" << (chromosome->last_position_ + 1) << ")." << EidosTerminate();
	
	// check the tick and cycle; this should already have been validated externally
	if ((community_.Tick() != p_metadata_tick) || (Cycle() != p_metadata_cycle))
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables_SECONDARY): (internal error) tick or cycle mismatch." << EidosTerminate();
	
	// rebase the times in the nodes to be in SLiM-land; see WriteTreeSequence for the inverse operation
	// BCH 4/4/2019: switched to using tree_seq_tick_ to avoid a parent/child timestamp conflict
	// This makes sense; as far as tree-seq recording is concerned, tree_seq_tick_ is the time counter
	slim_tick_t time_adjustment = community_.tree_seq_tick_;
	
	for (size_t node_index = 0; node_index < tables.nodes.num_rows; ++node_index)
		tables.nodes.time[node_index] -= time_adjustment;
	
	for (size_t mut_index = 0; mut_index < tables.mutations.num_rows; ++mut_index)
		tables.mutations.time[mut_index] -= time_adjustment;
	
	// check/rewrite the incoming tree-seq information in various ways
	__CheckPopulationMetadata(p_treeseq);
	__RemapSubpopulationIDs(p_subpop_map, p_treeseq, p_file_version);
	
	// allocate and set up the tree_sequence object
	// note that this tree sequence is based upon whatever sample the file was saved with, and may contain in-sample individuals
	// that are not presently alive, so we have to tread carefully; the actually alive individuals are flagged with 
	// SLIM_TSK_INDIVIDUAL_ALIVE in the individuals table (there may also be remembered and retained individuals in there too)
	tsk_treeseq_t *ts;
	int ret = 0;
	
	ts = (tsk_treeseq_t *)malloc(sizeof(tsk_treeseq_t));
	if (!ts)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	ret = tsk_treeseq_init(ts, &tables, TSK_TS_INIT_BUILD_INDEXES);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_init()", ret);
	
	std::unordered_map<tsk_id_t, Haplosome *> nodeToHaplosomeMap;
	
	{
		std::unordered_map<slim_objectid_t, ts_subpop_info> subpopInfoMap;
		
		__PrepareSubpopulationsFromTables(subpopInfoMap, p_treeseq);
		__TabulateSubpopulationsFromTreeSequence(subpopInfoMap, ts, p_treeseq, p_file_model_type);
		__CreateSubpopulationsFromTabulation_SECONDARY(subpopInfoMap, p_interpreter, nodeToHaplosomeMap, p_treeseq);
		__ConfigureSubpopulationsFromTables_SECONDARY(p_interpreter, p_treeseq);
	}
	
	std::unordered_map<slim_mutationid_t, MutationIndex> mutIndexMap;
	
	{
		std::unordered_map<slim_mutationid_t, ts_mut_info> mutInfoMap;
		
		__TabulateMutationsFromTables(mutInfoMap, p_treeseq, p_file_version);
		__TallyMutationReferencesWithTreeSequence(mutInfoMap, nodeToHaplosomeMap, ts);
		__CreateMutationsFromTabulation(mutInfoMap, mutIndexMap, p_treeseq);
	}
	
	__AddMutationsFromTreeSequenceToHaplosomes(mutIndexMap, nodeToHaplosomeMap, ts, p_treeseq);
	
	ret = tsk_treeseq_free(ts);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_free()", ret);
	free(ts);
	
	// At this point we have loaded in and processed tables that are normally shared, and that
	// *should* be identical to the tables that we share with the main table collection.  We
	// may have made some changes above, such as remapping subpopulation IDs and whatnot, but
	// those changes should be deterministic, so those tables should still be identical to the
	// main table collection.  We will compare, to validate.
	_CompareNodeTables(treeseq_[0].tables_.nodes, p_treeseq.tables_.nodes);
	_CompareIndividualTables(treeseq_[0].tables_.individuals, p_treeseq.tables_.individuals);
	_ComparePopulationTables(treeseq_[0].tables_.populations, p_treeseq.tables_.populations);
	
	// Now we can remove our table copies to free up the memory usage immediately.
	tsk_node_table_free(&p_treeseq.tables_.nodes);
	tsk_individual_table_free(&p_treeseq.tables_.individuals);
	tsk_population_table_free(&p_treeseq.tables_.populations);
	
	// Reset our last coalescence state; we don't know whether we're coalesced now or not
	p_treeseq.last_coalescence_state_ = false;
}

void Species::_PostInstantiationCleanup(EidosInterpreter *p_interpreter)
{
	// We have read in one or a set of chromosomes, instantiated corresponding SLiM objects, and now need to
	// clean up after ourselves.  The shared tables in the table collection should now be disconnected; only
	// the main table collection is now complete, and our final cleanup will operate on that.
	TreeSeqInfo &treeSeqInfo = treeseq_[0];
	tsk_table_collection_t &tables = treeSeqInfo.tables_;
	
	// Ensure that the next pedigree ID used will not cause a collision with any existing nodes in the node table,
	// and that there are no duplicate node pedigree IDs in the input file (whether in use or not).
	__CheckNodePedigreeIDs(p_interpreter, treeSeqInfo);
	
	// Set up the remembered haplosomes by looking though the list of nodes and their individuals
	if (remembered_nodes_.size() != 0)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): (internal error) remembered_nodes_ is not empty." << EidosTerminate();
	
	for (tsk_id_t j = 0; (size_t) j < tables.nodes.num_rows; j++)
	{
		tsk_id_t ind = tables.nodes.individual[j];
		if (ind >=0)
		{
			uint32_t flags = tables.individuals.flags[ind];
			if (flags & SLIM_TSK_INDIVIDUAL_REMEMBERED)
				remembered_nodes_.emplace_back(j);
		}
	}
	assert(remembered_nodes_.size() % 2 == 0);
	
	// Sort them to match the order of the individual table, so that they satisfy
	// the invariants asserted in Species::AddIndividualsToTable(); see the comments there
	tsk_id_t *nodes_table_individuals = tables.nodes.individual;
	
	std::sort(remembered_nodes_.begin(), remembered_nodes_.end(), [nodes_table_individuals](tsk_id_t l, tsk_id_t r) {
		tsk_id_t l_ind = nodes_table_individuals[l];
		tsk_id_t r_ind = nodes_table_individuals[r];
		if (l_ind != r_ind)
			return l_ind < r_ind;
		return l < r;
	});
	
	// Clear ALIVE flags
	FixAliveIndividuals(&tables);
	
	// Remove individuals that are not remembered or retained
	std::vector<tsk_id_t> individual_map;
	for (tsk_id_t j = 0; (size_t) j < tables.individuals.num_rows; j++)
	{
		uint32_t flags = tables.individuals.flags[j];
		if (flags & (SLIM_TSK_INDIVIDUAL_REMEMBERED | SLIM_TSK_INDIVIDUAL_RETAINED))
			individual_map.emplace_back(j);
	}
	ReorderIndividualTable(&tables, individual_map, false);
	BuildTabledIndividualsHash(&tables, &tabled_individuals_hash_);
	
	// Re-tally mutation references so we have accurate frequency counts for our new mutations
	population_.UniqueMutationRuns();
	population_.InvalidateMutationReferencesCache();	// force a retally
	population_.TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ false);
	
	// Do a crosscheck to ensure data integrity
	// BCH 10/16/2019: this crosscheck can take a significant amount of time; for a single load that is not a big deal,
	// but for models that reload many times (e.g., conditional on fixation), this overhead can add up to a substantial
	// fraction of total runtime.  That's crazy, especially since I've never seen this crosscheck fail except when
	// actively working on the tree-seq code.  So let's run it only the first load, and then assume loads are valid,
	// if we're running a Release build.  With a Debug build we still check on every load.
#if DEBUG
	CheckTreeSeqIntegrity();
	CrosscheckTreeSeqIntegrity();
#else
	{
		static bool been_here = false;
		
		if (!been_here) {
			been_here = true;
			CheckTreeSeqIntegrity();
			CrosscheckTreeSeqIntegrity();
		}
	}
#endif
	
	// Simplification has just been done, in effect (assuming the tree sequence we loaded is simplified; we assume that
	// here, but if that is not true, no harm done really except that it might be a while before we simplify again)
	simplify_elapsed_ = 0;
	
	// I'm not sure why we record the table position here; it is used only by RetractNewIndividual().
	RecordTablePosition();
	
	tables_initialized_ = true;
}

slim_tick_t Species::_InitializePopulationFromTskitBinaryFile(const char *p_file, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_map, Chromosome &p_chromosome)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::_InitializePopulationFromTskitBinaryFile(): SLiM global state read");
	
	// BEWARE: _InitializePopulationFromTskitDirectory() has a modified version of this code.  Maintain in parallel!
	
	// note that we now allow this to be called without tree-seq on, just to load haplosomes/mutations from the .trees file
	int ret;

	if (recording_tree_)
		FreeTreeSequence();
	
	// if tree-seq is not enabled, we set recording_mutations_ to true temporarily, so mutations get loaded without a raise
	// we remember the state of recording_tree_, because it gets forced to true as a side effect of loading
	bool was_recording_tree = recording_tree_;
	
	if (!was_recording_tree)
	{
		recording_tree_ = true;
		recording_mutations_ = true;
	}
	
	// make a new TreeSeqInfo record in treeseq_ and set it up
	treeseq_.resize(1);
	
	TreeSeqInfo &treeSeqInfo = treeseq_.back();
	
	treeSeqInfo.chromosome_index_ = p_chromosome.Index();
	treeSeqInfo.last_coalescence_state_ = false;
	
	ret = tsk_table_collection_load(&treeSeqInfo.tables_, p_file, TSK_LOAD_SKIP_REFERENCE_SEQUENCE);	// we load the ref seq ourselves; see below
	if (ret != 0) handle_error("tsk_table_collection_load", ret);
	
	// BCH 4/25/2019: if indexes are present on tables_ we want to drop them; they are synced up
	// with the edge table, but we plan to modify the edge table so they will become invalid anyway, and
	// then they may cause a crash because of their unsynced-ness; see tskit issue #179
	ret = tsk_table_collection_drop_index(&treeSeqInfo.tables_, 0);
	if (ret != 0) handle_error("tsk_table_collection_drop_index", ret);

	// read in the tree sequence metadata first so we have file version information and check for SLiM compliance and such
	slim_tick_t metadata_tick;
	slim_tick_t metadata_cycle;
	SLiMModelType file_model_type;
	int file_version;
	
	ReadTreeSequenceMetadata(treeSeqInfo, &metadata_tick, &metadata_cycle, &file_model_type, &file_version);
	
	// convert ASCII derived-state data, which is the required format on disk, back to our in-memory binary format
	DerivedStatesFromAscii(&treeSeqInfo.tables_);
	
	// in nucleotide-based models, read the ancestral sequence; we do this ourselves, directly from kastore, to avoid having
	// tskit make a full ASCII copy of the reference sequences from kastore into tables_; see tsk_table_collection_load() above
	_ReadAncestralSequence(p_file, p_chromosome);
	
	// make the corresponding SLiM objects
	_InstantiateSLiMObjectsFromTables(p_interpreter, metadata_tick, metadata_cycle, file_model_type, file_version, p_subpop_map, treeSeqInfo);
	
	// cleanup such as handling remembered haplosomes and the individuals table, mutation tallying, and integrity checks
	// this incorporates all of the post-load work that spans the whole set of chromosomes in the model
	_PostInstantiationCleanup(p_interpreter);
	
	// if tree-seq is not on, throw away the tree-seq data structures now that we're done loading SLiM state
	if (!was_recording_tree)
	{
		FreeTreeSequence();
		recording_tree_ = false;
		recording_mutations_ = false;
	}
	
	return metadata_tick;
}

slim_tick_t Species::_InitializePopulationFromTskitDirectory(std::string p_directory, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_remap)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::_InitializePopulationFromTskitDirectory(): SLiM global state read");
	
	// First, get the .trees files in the directory
	int directory_length = (int)p_directory.length();
	bool directory_ends_in_slash = (directory_length > 0) && (p_directory[directory_length-1] == '/');
	struct stat file_info;
	DIR *dp = opendir(p_directory.c_str());
	std::vector<std::string> trees_paths;
	
	if (dp != NULL)
	{
		while (true)
		{
			errno = 0;
			struct dirent *ep = readdir(dp);
			int error = errno;
			
			if (!ep)
			{
				if (error)
				{
					(void)closedir(dp);
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the directory at path " << p_directory << " could not be read." << EidosTerminate();
				}
				break;
			}
			
			std::string interior_filename_base = ep->d_name;
			
			if ((interior_filename_base == ".") || (interior_filename_base == ".."))
				continue;
			
			std::string interior_filename = p_directory + (directory_ends_in_slash ? "" : "/") + interior_filename_base;		// NOLINT(*-inefficient-string-concatenation) : this is fine
			
			bool file_exists = (stat(interior_filename.c_str(), &file_info) == 0);
			
			if (!file_exists)
			{
				(void)closedir(dp);
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the directory at path " << p_directory << " could not be read." << EidosTerminate();
			}
			
			bool is_directory = !!(file_info.st_mode & S_IFDIR);
			
			if (!is_directory && Eidos_string_hasSuffix(interior_filename, ".trees"))
				trees_paths.push_back(interior_filename);
		}
		
		(void)closedir(dp);
	}
	else
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the directory at path " << p_directory << " could not be read." << EidosTerminate();
	
	// Second, confirm that the count is correct and the symbols match
	const std::vector<Chromosome *> &chromosomes = Chromosomes();
	
	if (trees_paths.size() != chromosomes.size())
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the directory at path " << p_directory << " contains " << trees_paths.size() << " .trees files, but the focal species defines " << Chromosomes().size() << " chromosomes." << EidosTerminate();
	
	for (Chromosome *chromosome : chromosomes)
	{
		const std::string &symbol = chromosome->Symbol();
		std::string expected_filename = std::string("chromosome_") + symbol + ".trees";
		std::string expected_path = p_directory + (directory_ends_in_slash ? "" : "/") + expected_filename;
		
		if (std::find(trees_paths.begin(), trees_paths.end(), expected_path) == trees_paths.end())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the directory at path " << p_directory << " contains " << trees_paths.size() << " does not contain a chromosome file named " << expected_filename << ", which is expected based upon the chromosomes defined in the focal species." << EidosTerminate();
	}
	
	// OK, we appear to have a well-formed multichrom archive that we can load.  Now we will load the data for
	// each chromosome.  The code here follows the pattern of _InitializePopulationFromTskitBinaryFile(), but
	// has been modified accordingly.  See that method for comments.  BEWARE: Maintain in parallel!
	slim_tick_t metadata_tick = 0;
	slim_tick_t metadata_cycle = 0;
	SLiMModelType file_model_type = SLiMModelType::kModelTypeWF;
	int file_version = 0;
	
	if (recording_tree_)
		FreeTreeSequence();
	
	bool was_recording_tree = recording_tree_;
	
	if (!was_recording_tree)
	{
		recording_tree_ = true;
		recording_mutations_ = true;
	}
	
	for (Chromosome *chromosome : chromosomes)
	{
		bool is_first_chromosome = (chromosome == chromosomes[0]);
		const std::string &symbol = chromosome->Symbol();
		std::string expected_filename = std::string("chromosome_") + symbol + ".trees";
		std::string expected_path = p_directory + (directory_ends_in_slash ? "" : "/") + expected_filename;
		
		treeseq_.resize(treeseq_.size() + 1);
		TreeSeqInfo &treeSeqInfo = treeseq_.back();
		treeSeqInfo.chromosome_index_ = chromosome->Index();
		treeSeqInfo.last_coalescence_state_ = false;
		
		int ret = tsk_table_collection_load(&treeSeqInfo.tables_, expected_path.c_str(), TSK_LOAD_SKIP_REFERENCE_SEQUENCE);
		if (ret != 0) handle_error("tsk_table_collection_load", ret);
		
		ret = tsk_table_collection_drop_index(&treeSeqInfo.tables_, 0);
		if (ret != 0) handle_error("tsk_table_collection_drop_index", ret);
		
		slim_tick_t this_metadata_tick;
		slim_tick_t this_metadata_cycle;
		SLiMModelType this_file_model_type;
		int this_file_version;
		
		ReadTreeSequenceMetadata(treeSeqInfo, &this_metadata_tick, &this_metadata_cycle, &this_file_model_type, &this_file_version);
		
		if (is_first_chromosome)
		{
			metadata_tick = this_metadata_tick;
			metadata_cycle = this_metadata_cycle;
			file_model_type = this_file_model_type;
			file_version = this_file_version;
		}
		else
		{
			if (this_metadata_tick != metadata_tick)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the .trees files for chromosomes were saved in different ticks (" << metadata_tick << " versus " << this_metadata_tick << ").  This must be consistent across all files." << EidosTerminate();
			if (this_metadata_cycle != metadata_cycle)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the .trees files for chromosomes were saved in different ticks (" << metadata_cycle << " versus " << this_metadata_cycle << ").  This must be consistent across all files." << EidosTerminate();
			if (this_file_model_type != file_model_type)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the .trees files for chromosomes were saved from different model types (WF versus nonWF).  This must be consistent across all files." << EidosTerminate();
			if (this_file_version != file_version)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitDirectory): the .trees files for chromosomes have different file versions (" << file_version << " versus " << this_file_version << ").  This must be consistent across all files." << EidosTerminate();
		}
		
		DerivedStatesFromAscii(&treeSeqInfo.tables_);
		_ReadAncestralSequence(expected_path.c_str(), *chromosome);
		
		// The first chromosome uses _InstantiateSLiMObjectsFromTables() and creates the subpopulations, etc.,
		// as needed.  The remaining chromosomes use _InstantiateSLiMObjectsFromTables_SECONDARY(), which
		// checks against the population structure that was created; it should always match exactly.
		if (is_first_chromosome)
			_InstantiateSLiMObjectsFromTables(p_interpreter, metadata_tick, metadata_cycle, file_model_type, file_version, p_subpop_remap, treeSeqInfo);
		else
			_InstantiateSLiMObjectsFromTables_SECONDARY(p_interpreter, metadata_tick, metadata_cycle, file_model_type, file_version, p_subpop_remap, treeSeqInfo);
	}
	
	// cleanup such as handling remembered haplosomes and the individuals table, mutation tallying, and integrity checks
	// this incorporates all of the post-load work that spans the whole set of chromosomes in the model
	_PostInstantiationCleanup(p_interpreter);
	
	if (!was_recording_tree)
	{
		FreeTreeSequence();
		recording_tree_ = false;
		recording_mutations_ = false;
	}
	
	return metadata_tick;
}

size_t Species::MemoryUsageForTreeSeqInfo(TreeSeqInfo &p_tsinfo, bool p_count_shared_tables)
{
	tsk_table_collection_t &t = p_tsinfo.tables_;
	size_t usage = 0;
	
	// the individuals table, nodes table, and population table are shared
	if (p_count_shared_tables)
	{
		usage += sizeof(tsk_individual_table_t);
		
		if (t.individuals.flags)
			usage += t.individuals.max_rows * sizeof(uint32_t);
		if (t.individuals.location_offset)
			usage += t.individuals.max_rows * sizeof(tsk_size_t);
		if (t.individuals.parents_offset)
			usage += t.individuals.max_rows * sizeof(tsk_size_t);
		if (t.individuals.metadata_offset)
			usage += t.individuals.max_rows * sizeof(tsk_size_t);
		
		if (t.individuals.location)
			usage += t.individuals.max_location_length * sizeof(double);
		if (t.individuals.parents)
			usage += t.individuals.max_parents_length * sizeof(tsk_id_t);
		if (t.individuals.metadata)
			usage += t.individuals.max_metadata_length * sizeof(char);
		
		usage += sizeof(tsk_node_table_t);
		
		if (t.nodes.flags)
			usage += t.nodes.max_rows * sizeof(uint32_t);
		if (t.nodes.time)
			usage += t.nodes.max_rows * sizeof(double);
		if (t.nodes.population)
			usage += t.nodes.max_rows * sizeof(tsk_id_t);
		if (t.nodes.individual)
			usage += t.nodes.max_rows * sizeof(tsk_id_t);
		if (t.nodes.metadata_offset)
			usage += t.nodes.max_rows * sizeof(tsk_size_t);
		
		if (t.nodes.metadata)
			usage += t.nodes.max_metadata_length * sizeof(char);
		
		usage += sizeof(tsk_population_table_t);
		
		if (t.populations.metadata_offset)
			usage += t.populations.max_rows * sizeof(tsk_size_t);
		
		if (t.populations.metadata)
			usage += t.populations.max_metadata_length * sizeof(char);
	}
	
	usage += sizeof(tsk_edge_table_t);
	
	if (t.edges.left)
		usage += t.edges.max_rows * sizeof(double);
	if (t.edges.right)
		usage += t.edges.max_rows * sizeof(double);
	if (t.edges.parent)
		usage += t.edges.max_rows * sizeof(tsk_id_t);
	if (t.edges.child)
		usage += t.edges.max_rows * sizeof(tsk_id_t);
	
	usage += sizeof(tsk_migration_table_t);
	
	if (t.migrations.source)
		usage += t.migrations.max_rows * sizeof(tsk_id_t);
	if (t.migrations.dest)
		usage += t.migrations.max_rows * sizeof(tsk_id_t);
	if (t.migrations.node)
		usage += t.migrations.max_rows * sizeof(tsk_id_t);
	if (t.migrations.left)
		usage += t.migrations.max_rows * sizeof(double);
	if (t.migrations.right)
		usage += t.migrations.max_rows * sizeof(double);
	if (t.migrations.time)
		usage += t.migrations.max_rows * sizeof(double);
	
	usage += sizeof(tsk_site_table_t);
	
	if (t.sites.position)
		usage += t.sites.max_rows * sizeof(double);
	if (t.sites.ancestral_state_offset)
		usage += t.sites.max_rows * sizeof(tsk_size_t);
	if (t.sites.metadata_offset)
		usage += t.sites.max_rows * sizeof(tsk_size_t);
	
	if (t.sites.ancestral_state)
		usage += t.sites.max_ancestral_state_length * sizeof(char);
	if (t.sites.metadata)
		usage += t.sites.max_metadata_length * sizeof(char);
	
	usage += sizeof(tsk_mutation_table_t);
	
	if (t.mutations.node)
		usage += t.mutations.max_rows * sizeof(tsk_id_t);
	if (t.mutations.site)
		usage += t.mutations.max_rows * sizeof(tsk_id_t);
	if (t.mutations.parent)
		usage += t.mutations.max_rows * sizeof(tsk_id_t);
	if (t.mutations.derived_state_offset)
		usage += t.mutations.max_rows * sizeof(tsk_size_t);
	if (t.mutations.metadata_offset)
		usage += t.mutations.max_rows * sizeof(tsk_size_t);
	
	if (t.mutations.derived_state)
		usage += t.mutations.max_derived_state_length * sizeof(char);
	if (t.mutations.metadata)
		usage += t.mutations.max_metadata_length * sizeof(char);
	
	usage += sizeof(tsk_provenance_table_t);
	
	if (t.provenances.timestamp_offset)
		usage += t.provenances.max_rows * sizeof(tsk_size_t);
	if (t.provenances.record_offset)
		usage += t.provenances.max_rows * sizeof(tsk_size_t);
	
	if (t.provenances.timestamp)
		usage += t.provenances.max_timestamp_length * sizeof(char);
	if (t.provenances.record)
		usage += t.provenances.max_record_length * sizeof(char);
	
	usage += remembered_nodes_.size() * sizeof(tsk_id_t);
	
	return usage;
}

void Species::TSXC_Enable(void)
{
	// This is called by command-line slim if a -TSXC command-line option is supplied; the point of this is to allow
	// tree-sequence recording to be turned on, with mutation recording and runtime crosschecks, with a simple
	// command-line flag, so that my existing test suite can be crosschecked easily.  The -TSXC flag is not public.
	recording_tree_ = true;
	recording_mutations_ = true;
	simplification_ratio_ = 10.0;
	simplification_interval_ = -1;			// this means "use the ratio, not a fixed interval"
	simplify_interval_ = 20;				// this is the initial simplification interval
	running_coalescence_checks_ = false;
	running_treeseq_crosschecks_ = true;
	treeseq_crosschecks_interval_ = 50;		// check every 50th cycle, otherwise it is just too slow
	
	pedigrees_enabled_ = true;
	pedigrees_enabled_by_SLiM_ = true;
}

void Species::TSF_Enable(void)
{
	// This is called by command-line slim if a -TSF command-line option is supplied; the point of this is to allow
	// tree-sequence recording to be turned on, with mutation recording but without runtime crosschecks, with a simple
	// command-line flag, so that my existing test suite can be tested with tree-seq easily.  -TSF is not public.
	recording_tree_ = true;
	recording_mutations_ = true;
	simplification_ratio_ = 10.0;
	simplification_interval_ = -1;            // this means "use the ratio, not a fixed interval"
	simplify_interval_ = 20;                // this is the initial simplification interval
	running_coalescence_checks_ = false;
	running_treeseq_crosschecks_ = false;
	
	pedigrees_enabled_ = true;
	pedigrees_enabled_by_SLiM_ = true;
}





























































