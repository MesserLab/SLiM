//
//  species.cpp
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
//  Copyright (c) 2014-2024 Philipp Messer.  All rights reserved.
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
static const char *SLIM_TREES_FILE_VERSION = "0.9";				// SLiM 4.4 onward, for multichrom (haplosomes not genomes, and `chromosomes` key)

#pragma mark -
#pragma mark Species
#pragma mark -

Species::Species(Community &p_community, slim_objectid_t p_species_id, const std::string &p_name) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(p_name), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_Species_Class))),
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
}

void Species::_MakeHaplosomeMetadataRecords(void)
{
	// Set up our default metadata records for haplosomes, which are variable-length.  The default records
	// are used as the initial configuration of the nodes for new individuals; then, as haplosomes are
	// added to the new individual, the is_null_ bits get tweaked as needed in the recorded metadata, which
	// is a bit gross, but necessary; the node metadata is recorded before the haplosomes are created.
	// See HaplosomeMetadataRec for comments on this design.
	
	// First, calculate how many bytes we need
	size_t bits_needed_for_is_null = chromosomes_.size();					// each chromosome needs one bit per node table entry
	haplosome_metadata_size_ = sizeof(HaplosomeMetadataRec) - 1;			// -1 to subtract out the is_null_[1] in the record
	haplosome_metadata_isnull_bytes_ = ((bits_needed_for_is_null + 7) / 8);	// (x+7)/8 rounds up to the number of bytes
	haplosome_metadata_size_ += haplosome_metadata_isnull_bytes_;
	
	// Then allocate the buffers needed; the "male" versions are present only when sex is enabled
	hap_metadata_1F_ = (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1);
	hap_metadata_1M_ = (sex_enabled_ ? (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1) : nullptr);
	hap_metadata_2F_ = (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1);
	hap_metadata_2M_ = (sex_enabled_ ? (HaplosomeMetadataRec *)calloc(haplosome_metadata_size_, 1) : nullptr);
	
	// Then set the is_null_ bits for the default state for males and females; this is the state in which
	// all chromosomes that dictate the is_null_ state by sex have that dictated state, while all others
	// (types "A", "H", and "H-" only) are assumed to be non-null.  Any positions that are unused for a
	// given chromosome type (like the second position for type "Y") are left as 0 here, but that should
	// not be assumed by code that uses these flags for model behavior.  We go from least-significant bit
	// to most-significant bit, byte by byte, with each chromosome using two bits.  The less significant
	// of those two bits is is_null_ for haplosome 1 for that chromosome; the more significant of those
	// two bits is is_null_ for haplosome 2 for that chromosome.
	IndividualSex sex = IndividualSex::kFemale;
	HaplosomeMetadataRec *focal_metadata_1 = hap_metadata_1F_;
	HaplosomeMetadataRec *focal_metadata_2 = hap_metadata_2F_;
	
	while (true)
	{
		for (Chromosome *chromosome : chromosomes_)
		{
			slim_chromosome_index_t chromosome_index = chromosome->Index();
			bool haplosome_1_is_null_or_unused, haplosome_2_is_null_or_unused;
			
			switch (chromosome->Type())
			{
				case ChromosomeType::kA_DiploidAutosome:
					haplosome_1_is_null_or_unused = false;								// always present (by default)
					haplosome_2_is_null_or_unused = false;								// always present (by default)
					break;
					
				case ChromosomeType::kH_HaploidAutosome:
				case ChromosomeType::kHF_HaploidFemaleInherited:
				case ChromosomeType::kHM_HaploidMaleInherited:
				case ChromosomeType::kHNull_HaploidAutosomeWithNull:
					haplosome_1_is_null_or_unused = false;								// always present (by default)
					haplosome_2_is_null_or_unused = true;								// always null or unused
					break;
					
				case ChromosomeType::kX_XSexChromosome:
					haplosome_1_is_null_or_unused = false;								// always present
					haplosome_2_is_null_or_unused = (sex == IndividualSex::kMale);		// null in males
					break;
					
				case ChromosomeType::kY_YSexChromosome:
				case ChromosomeType::kML_HaploidMaleLine:
					haplosome_1_is_null_or_unused = (sex == IndividualSex::kFemale);	// null in females
					haplosome_2_is_null_or_unused = true;								// always unused
					break;
					
				case ChromosomeType::kZ_ZSexChromosome:
					haplosome_1_is_null_or_unused = (sex == IndividualSex::kFemale);	// null in females
					haplosome_2_is_null_or_unused = false;								// always present
					break;
					
				case ChromosomeType::kW_WSexChromosome:
				case ChromosomeType::kFL_HaploidFemaleLine:
					haplosome_1_is_null_or_unused = (sex == IndividualSex::kMale);		// null in males
					haplosome_2_is_null_or_unused = true;								// always unused
					break;
					
				case ChromosomeType::kNullY_YSexChromosomeWithNull:
					haplosome_1_is_null_or_unused = true;								// always unused
					haplosome_2_is_null_or_unused = (sex == IndividualSex::kFemale);	// null in females
					break;
			}
			
			// set the appropriate bits in the focal metadata, which we know was cleared to zero initially
			int byte_index = chromosome_index / 8;
			int bit_shift = chromosome_index % 8;
			
			if (haplosome_1_is_null_or_unused)
				focal_metadata_1->is_null_[byte_index] |= (0x01 << bit_shift);
			
			if (haplosome_2_is_null_or_unused)
				focal_metadata_2->is_null_[byte_index] |= (0x01 << bit_shift);
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
	
	// Create an implicit Chromosome object with a retain on it from EidosDictionaryRetained::EidosDictionaryRetained()
	Chromosome *chromosome = new Chromosome(*this, p_type, 1, "1", /* p_index */ 0, /* p_preferred_mutcount */ 0);
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
	ChromosomeType type = p_chromosome->Type();
	
	chromosomes_.push_back(p_chromosome);
	chromosome_from_id_.emplace(id, p_chromosome);
	chromosome_from_symbol_.emplace(symbol, p_chromosome);
	
	// keep track of our haplosome configuration
	switch (type)
	{
			// these chromosome types keep two haplosomes per individual
		case ChromosomeType::kA_DiploidAutosome:
		case ChromosomeType::kX_XSexChromosome:
		case ChromosomeType::kZ_ZSexChromosome:
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
			chromosome_for_haplosome_index_.push_back(p_chromosome);
			chromosome_for_haplosome_index_.push_back(p_chromosome);
			chromosome_subindex_for_haplosome_index_.push_back(0);
			chromosome_subindex_for_haplosome_index_.push_back(1);
			first_haplosome_index_.push_back(haplosome_count_per_individual_);
			last_haplosome_index_.push_back(haplosome_count_per_individual_ + 1);
			haplosome_count_per_individual_ += 2;
			break;
			
			// these chromosome types keep one haplosome per individual
		case ChromosomeType::kH_HaploidAutosome:
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kHF_HaploidFemaleInherited:
		case ChromosomeType::kFL_HaploidFemaleLine:
		case ChromosomeType::kHM_HaploidMaleInherited:
		case ChromosomeType::kML_HaploidMaleLine:
			chromosome_for_haplosome_index_.push_back(p_chromosome);
			chromosome_subindex_for_haplosome_index_.push_back(0);
			first_haplosome_index_.push_back(haplosome_count_per_individual_);
			last_haplosome_index_.push_back(haplosome_count_per_individual_);
			haplosome_count_per_individual_ += 1;
			break;
	}
	
	// keep track of whether we contain null haplosomes or not (for optimizations)
	switch (type)
	{
			// these chromosome types do not (normally) employ null haplosomes
			// if addRecombinant() etc. places a null haplosome, it will fix this flag
		case ChromosomeType::kA_DiploidAutosome:
		case ChromosomeType::kH_HaploidAutosome:
		case ChromosomeType::kHF_HaploidFemaleInherited:
		case ChromosomeType::kHM_HaploidMaleInherited:
			break;
			
			// these chromosome types employ null haplosomes
		case ChromosomeType::kX_XSexChromosome:
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kZ_ZSexChromosome:
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kFL_HaploidFemaleLine:
		case ChromosomeType::kML_HaploidMaleLine:
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
			chromosomes_use_null_haplosomes_ = true;
			break;
	}
}

void Species::GetChromosomeIndicesFromEidosValue(std::vector<slim_chromosome_index_t> &chromosome_indices, EidosValue *chromosomes_value)
{
	EidosValueType chromosomes_value_type = chromosomes_value->Type();
	int chromosomes_value_count = chromosomes_value->Count();
	
	switch (chromosomes_value_type)
	{
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
					EIDOS_TERMINATION << "ERROR (Species::GetChromosomeIndicesFromEidosValue): could not find a chromosome with the given id (" << id << ")." << EidosTerminate();
				
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
					EIDOS_TERMINATION << "ERROR (Species::GetChromosomeIndicesFromEidosValue): could not find a chromosome with the given symbol (" << symbol << ")." << EidosTerminate();
				
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
				
				chromosome_indices.push_back(chromosome->Index());
			}
			break;
		}
		default: break;
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
			// (i.e. non-binary) format, but we no longer support that.  In future, this code path will become
			// FIXME MULTICHROM the code path for reading in a multi-chromosome archive of tree sequences.
			return SLiMFileFormat::kFormatUnrecognized;
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
#if INTERIM_TREESEQ_DISABLE
		new_tick = _InitializePopulationFromTskitBinaryFile(file_cstr, p_interpreter, p_subpop_remap);
#else
		EIDOS_TERMINATION << "ERROR (Species::InitializePopulationFromFile): (internal error) loading tree-sequence archives is temporarily disabled." << EidosTerminate();
#endif // INTERIM_TREESEQ_DISABLE
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
#if EIDOS_ROBIN_HOOD_HASHING
	robin_hood::unordered_flat_map<slim_polymorphismid_t,MutationIndex> mutations;
#elif STD_UNORDERED_MAP_HASHING
	std::unordered_map<slim_polymorphismid_t,MutationIndex> mutations;
#endif
	std::string line, sub; 
	std::ifstream infile(p_file);
	int age_output_count = 0;
	bool has_individual_pedigree_IDs = false;
	
	if (!infile.is_open())
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): could not open initialization file." << EidosTerminate();
	
	// Parse the first line, to get the tick and cycle
	{
		GetInputLine(infile, line);
	
		std::istringstream iss(line);
		
		iss >> sub;		// #OUT:
		
		iss >> sub;		// tick
		int64_t tick_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		file_tick = SLiMCastToTickTypeOrRaise(tick_long);
		
		// Next is either the cycle, or "A"; we handle the addition of cycle in SLiM 4 without a version bump
		iss >> sub;
		if (sub == "A")
		{
			// If it is "A", we are reading a pre-4.0 file, and the tick and cycle are the same
			file_cycle = file_tick;
		}
		else
		{
			int64_t cycle_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
			file_cycle = SLiMCastToTickTypeOrRaise(cycle_long);
			
			// "A" follows but we don't bother reading it
		}
	}
	
	// As of SLiM 2.1, we change the generation as a side effect of loading; otherwise we can't correctly update our state here!
	// As of SLiM 3, we set the generation up here, before making any individuals, because we need it to be correct for the tree-seq recording code.
	// As of SLiM 4, we set both the tick and the cycle, which are both saved to the file for version 7 and after.
	community_.SetTick(file_tick);
	SetCycle(file_cycle);
	
	// Read and ignore initial stuff until we hit the Populations section
	int64_t file_version = 0;	// initially unknown; we will leave this as 0 for versions < 3, for now
	
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		// Starting in SLiM 3, we will handle a Version line if we see one in passing
		if (line.find("Version:") != std::string::npos)
		{
			std::istringstream iss(line);
			
			iss >> sub;		// Version:
			iss >> sub;		// version number
			
			file_version = (int64_t)EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
			
			// version 5/6 are the same as 3/4 but have individual pedigree IDs; added in SLiM 3.5
			if (file_version >= 5)
			{
				has_individual_pedigree_IDs = true;
				file_version -= 2;
			}
			
			// version 4 is the same as version 3 but with an age value for each individual
			if (file_version == 4)
			{
				age_output_count = 1;
				file_version = 3;
			}
			
			if ((file_version != 1) && (file_version != 2) && (file_version != 3))
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): unrecognized version (" << file_version << "." << EidosTerminate();
			
			continue;
		}
		
		if (line.find("Populations") != std::string::npos)
			break;
	}
	
	if (age_output_count && (model_type_ == SLiMModelType::kModelTypeWF))
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): age information is present but the simulation is using a WF model." << EidosTerminate();
	if (!age_output_count && (model_type_ == SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): age information is not present but the simulation is using a nonWF model; age information must be included." << EidosTerminate();
	
	// Now we are in the Populations section; read and instantiate each population until we hit the Mutations section
	while (!infile.eof())
	{ 
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Mutations") != std::string::npos)
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
	
	// Now we are in the Mutations section; read and instantiate all mutations and add them to our map and to the registry
	while (!infile.eof()) 
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Haplosomes") != std::string::npos)
			break;
		if (line.find("Individuals") != std::string::npos)	// SLiM 2.0 added this section
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
		double selection_coeff = EidosInterpreter::FloatForString(sub, nullptr);
		
		iss >> sub;		// dominance coefficient, which is given in the mutation type; we check below that the value read matches the mutation type
		double dominance_coeff = EidosInterpreter::FloatForString(sub, nullptr);
		
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
		
		if (!Eidos_ApproximatelyEqual(mutation_type_ptr->dominance_coeff_, dominance_coeff))	// a reasonable tolerance to allow for I/O roundoff
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " has dominance coefficient " << mutation_type_ptr->dominance_coeff_ << " that does not match the population file dominance coefficient of " << dominance_coeff << "." << EidosTerminate();
		
		// BCH 9/22/2021: Note that mutation_type_ptr->hemizygous_dominance_coeff_ is not saved, or checked here; too edge to be bothered...
		
		if ((nucleotide == -1) && mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " is nucleotide-based, but a nucleotide value for a mutation of this type was not supplied." << EidosTerminate();
		if ((nucleotide != -1) && !mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " is not nucleotide-based, but a nucleotide value for a mutation of this type was supplied." << EidosTerminate();
		
		// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
		MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
		
		Mutation *new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_id, mutation_type_ptr, TheChromosome().Index(), position, selection_coeff, subpop_index, tick, nucleotide);
		
		// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
		mutations.emplace(polymorphism_id, new_mut_index);
		population_.MutationRegistryAdd(new_mut);
		
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		if (population_.keeping_muttype_registries_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
		
		// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_pure_neutral_DFE_
		if (selection_coeff != 0.0)
		{
			pure_neutral_ = false;
			mutation_type_ptr->all_pure_neutral_DFE_ = false;
		}
	}
	
	population_.InvalidateMutationReferencesCache();
	
	// If there is an Individuals section (added in SLiM 2.0), we now need to parse it since it might contain spatial positions
	if (has_individual_pedigree_IDs)
		gSLiM_next_pedigree_id = 0;
	
	if (line.find("Individuals") != std::string::npos)
	{
		while (!infile.eof()) 
		{
			GetInputLine(infile, line);
			
			if (line.length() == 0)
				continue;
			if (line.find("Haplosomes") != std::string::npos)
				break;
			
			std::istringstream iss(line);
			
			iss >> sub;		// pX:iY – individual identifier
			int pos = static_cast<int>(sub.find_first_of(':'));
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
					individual.haplosomes_[0]->SetHaplosomeID(pedigree_id * 2);
					individual.haplosomes_[1]->SetHaplosomeID(pedigree_id * 2 + 1);
					gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);
				}
			}
			
			iss >> sub;			// individual sex identifier (F/M/H) – added in SLiM 2.1, so we need to be robust if it is missing
			
			if ((sub == "F") || (sub == "M") || (sub == "H"))
				iss >> sub;
			
			;					// pX:Y – haplosome 1 identifier, which we do not presently need to parse [already fetched]
			iss >> sub;			// pX:Y – haplosome 2 identifier, which we do not presently need to parse
			
			// Parse the optional fields at the end of each individual line.  This is a bit tricky.
			// First we read all of the fields in, then we decide how to use them.
			std::vector<std::string> opt_params;
			int opt_param_count;
			
			while (iss >> sub)
				opt_params.emplace_back(sub);
			
			opt_param_count = (int)opt_params.size();
			
			if (opt_param_count == 0)
			{
				// no optional info present, which might be an error; should never occur unless someone has hand-constructed a bad input file
				if (age_output_count)
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): output file format does not contain age information, which is required." << EidosTerminate();
			}
			else if (opt_param_count == age_output_count)
			{
				// only age information is present
				individual.age_ = (slim_age_t)EidosInterpreter::NonnegativeIntegerForString(opt_params[0], nullptr);			// age
			}
			else if (opt_param_count == spatial_dimensionality_ + age_output_count)
			{
				// age information is present, in addition to the correct number of spatial positions
				if (spatial_dimensionality_ >= 1)
					individual.spatial_x_ = EidosInterpreter::FloatForString(opt_params[0], nullptr);							// spatial position x
				if (spatial_dimensionality_ >= 2)
					individual.spatial_y_ = EidosInterpreter::FloatForString(opt_params[1], nullptr);							// spatial position y
				if (spatial_dimensionality_ >= 3)
					individual.spatial_z_ = EidosInterpreter::FloatForString(opt_params[2], nullptr);							// spatial position z
				
				if (age_output_count)
					individual.age_ = (slim_age_t)EidosInterpreter::NonnegativeIntegerForString(opt_params[spatial_dimensionality_], nullptr);		// age
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): output file format does not match that expected by the simulation (spatial dimension or age information is incorrect or missing)." << EidosTerminate();
			}
		}
	}
	
	// FIXME MULTICHROM need to figure out the file format for multiple chromosomes
	Chromosome &chromosome = TheChromosome();
	
	// Now we are in the Haplosomes section, which should take us to the end of the file unless there is an Ancestral Sequence section
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
#ifndef _OPENMP
	MutationRunContext &mutrun_context = chromosome.ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
	
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Ancestral sequence") != std::string::npos)
			break;
		
		std::istringstream iss(line);
		
		iss >> sub;
		int pos = static_cast<int>(sub.find_first_of(':'));
		std::string &&subpop_id_string = sub.substr(0, pos);
		slim_objectid_t subpop_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_string, 'p', nullptr);
		
		Subpopulation *subpop = SubpopulationWithID(subpop_id);
		
		if (!subpop)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): referenced subpopulation p" << subpop_id << " not defined." << EidosTerminate();
		
		sub.erase(0, pos + 1);	// remove the subpop_id and the colon
		int64_t haplosome_index_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		
		if ((haplosome_index_long < 0) || (haplosome_index_long > SLIM_MAX_SUBPOP_SIZE * 2))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): haplosome index out of permitted range." << EidosTerminate();
		slim_popsize_t haplosome_index = static_cast<slim_popsize_t>(haplosome_index_long);	// range-check is above since we need to check against SLIM_MAX_SUBPOP_SIZE * 2
		
		int64_t individual_index = haplosome_index >> 1;
		Individual *ind = subpop->parent_individuals_[individual_index];
		Haplosome &haplosome = *(ind->haplosomes_[haplosome_index & 0x01]);
		
		// Now we might have [A|X|Y] (SLiM 2.0), or we might have the first mutation id - or we might have nothing at all
		if (iss >> sub)
		{
			// check whether this token is a haplosome type
			if ((sub.compare(gStr_A) == 0) || (sub.compare(gStr_X) == 0) || (sub.compare(gStr_Y) == 0))
			{
				// Let's do a little error-checking against what has already been instantiated for us...
				if ((sub.compare(gStr_A) == 0) && haplosome.AssociatedChromosome()->Type() != ChromosomeType::kA_DiploidAutosome)
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): haplosome is specified as A (autosome), but the instantiated haplosome does not match." << EidosTerminate();
				if ((sub.compare(gStr_X) == 0) && haplosome.AssociatedChromosome()->Type() != ChromosomeType::kX_XSexChromosome)
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): haplosome is specified as X (X-chromosome), but the instantiated haplosome does not match." << EidosTerminate();
				if ((sub.compare(gStr_Y) == 0) && haplosome.AssociatedChromosome()->Type() != ChromosomeType::kNullY_YSexChromosomeWithNull)
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTextFile): haplosome is specified as Y (Y-chromosome), but the instantiated haplosome does not match." << EidosTerminate();
				
				if (iss >> sub)
				{
					// BCH 9/27/2021: We instantiate null haplosomes only in the case where we expect them: in sex-chromosome models,
					// for either the X or Y (whichever is not being simulated).  In nonWF autosomal models, any haplosome is now
					// allowed to be null, at the user's discretion, so we transform the instantiated haplosome to a null haplosome
					// if necessary.  AddSubpopulation() created the haplosomes above, before we knew which would be null.
					if (sub == "<null>")
					{
						if (!haplosome.IsNull())
						{
							if ((model_type_ == SLiMModelType::kModelTypeNonWF) && (haplosome.AssociatedChromosome()->Type() == ChromosomeType::kA_DiploidAutosome))
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
					continue;
			}
			
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
	}
	
	// Now we are in the Ancestral sequence section, which should take us to the end of the file
	// Conveniently, NucleotideArray supports operator>> to read nucleotides until the EOF
	if (line.find("Ancestral sequence") != std::string::npos)
	{
		infile >> *(TheChromosome().AncestralSequence());
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
			(muttype_iter.second)->subject_to_mutationEffect_callback_ = true;			// we're not doing RecalculateFitness()'s work, so play it safe
		
		SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
		community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosMutationEffectCallback;	// used for both mutationEffect() and fitnessEffect() for simplicity
		community_.executing_species_ = this;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> mutationEffect_callbacks = CallbackBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosMutationEffectCallback, -1, -1, subpop_id);
			std::vector<SLiMEidosBlock*> fitnessEffect_callbacks = CallbackBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosFitnessEffectCallback, -1, -1, subpop_id);
			
			subpop->UpdateFitness(mutationEffect_callbacks, fitnessEffect_callbacks);
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

#ifndef __clang_analyzer__
slim_tick_t Species::_InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::_InitializePopulationFromBinaryFile(): SLiM global state read");
	
	std::size_t file_size = 0;
	slim_tick_t file_tick, file_cycle;
	int32_t spatial_output_count;
	int age_output_count = 0;
	int pedigree_output_count = 0;
	bool has_nucleotides = false;
	
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
	// for us that otherwise both the UndefinedBehaviorSanitizer.  On platforms that don't care about
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
		
		// version 4 is the same as version 3 but with an age value for each individual
		if (version_tag == 4)
		{
			age_output_count = 1;
			version_tag = 3;
		}
		
		if ((version_tag != 1) && (version_tag != 2) && (version_tag != 3) && (version_tag != 5) && (version_tag != 6) && (version_tag != 7))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unrecognized version (" << version_tag << ")." << EidosTerminate();
		
		file_version = version_tag;
	}
	
	// Header section
	{
		int32_t double_size;
		double double_test;
		int32_t slim_tick_t_size, slim_position_t_size, slim_objectid_t_size, slim_popsize_t_size, slim_refcount_t_size, slim_selcoeff_t_size, slim_mutationid_t_size, slim_polymorphismid_t_size, slim_age_t_size, slim_pedigreeid_t_size, slim_haplosomeid_t_size;
		int64_t flags = 0;
		int header_length = sizeof(double_size) + sizeof(double_test) + sizeof(slim_tick_t_size) + sizeof(slim_position_t_size) + sizeof(slim_objectid_t_size) + sizeof(slim_popsize_t_size) + sizeof(slim_refcount_t_size) + sizeof(slim_selcoeff_t_size) + sizeof(file_tick) + sizeof(section_end_tag);
		
		if (file_version >= 2)
			header_length += sizeof(slim_mutationid_t_size) + sizeof(slim_polymorphismid_t_size);
		if (file_version >= 6)
			header_length += sizeof(slim_age_t_size) + sizeof(slim_pedigreeid_t_size) + sizeof(slim_haplosomeid_t);
		if (file_version >= 5)
			header_length += sizeof(flags);
		if (file_version >= 7)
			header_length += sizeof(file_cycle);
		
		if (p + header_length > buf_end)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF while reading header." << EidosTerminate();
		
		memcpy(&double_size, p, sizeof(double_size));
		p += sizeof(double_size);
		
		memcpy(&double_test, p, sizeof(double_test));
		p += sizeof(double_test);
		
		if (file_version >= 5)
		{
			memcpy(&flags, p, sizeof(flags));
			p += sizeof(flags);
			
			if (flags & 0x01) age_output_count = 1;
			if (flags & 0x02) has_nucleotides = true;
			
			if (file_version >= 6)
				if (flags & 0x04) pedigree_output_count = 1;
		}
		
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
		
		memcpy(&slim_selcoeff_t_size, p, sizeof(slim_selcoeff_t_size));
		p += sizeof(slim_selcoeff_t_size);
		
		if (file_version >= 2)
		{
			memcpy(&slim_mutationid_t_size, p, sizeof(slim_mutationid_t_size));
			p += sizeof(slim_mutationid_t_size);
			
			memcpy(&slim_polymorphismid_t_size, p, sizeof(slim_polymorphismid_t_size));
			p += sizeof(slim_polymorphismid_t_size);
		}
		else
		{
			// Version 1 file; backfill correct values
			slim_mutationid_t_size = sizeof(slim_mutationid_t);
			slim_polymorphismid_t_size = sizeof(slim_polymorphismid_t);
		}
		
		if (file_version >= 6)
		{
			memcpy(&slim_age_t_size, p, sizeof(slim_age_t_size));
			p += sizeof(slim_age_t_size);
			
			memcpy(&slim_pedigreeid_t_size, p, sizeof(slim_pedigreeid_t_size));
			p += sizeof(slim_pedigreeid_t_size);
			
			memcpy(&slim_haplosomeid_t_size, p, sizeof(slim_haplosomeid_t_size));
			p += sizeof(slim_haplosomeid_t_size);
		}
		else
		{
			// Version <= 5 file; backfill correct values
			slim_age_t_size = sizeof(slim_age_t);
			slim_pedigreeid_t_size = sizeof(slim_pedigreeid_t);
			slim_haplosomeid_t_size = sizeof(slim_haplosomeid_t);
		}
		
		memcpy(&file_tick, p, sizeof(file_tick));
		p += sizeof(file_tick);
		
		if (file_version >= 7)
		{
			memcpy(&file_cycle, p, sizeof(file_cycle));
			p += sizeof(file_cycle);
		}
		else
		{
			// we are reading a pre-4.0 file, so the cycle is the same as the tick
			file_cycle = file_tick;
		}
		
		if (file_version >= 3)
		{
			memcpy(&spatial_output_count, p, sizeof(spatial_output_count));
			p += sizeof(spatial_output_count);
		}
		else
		{
			// Version 1 or 2 file; backfill correct value
			spatial_output_count = 0;
		}
		
		memcpy(&section_end_tag, p, sizeof(section_end_tag));
		p += sizeof(section_end_tag);
		
		if (double_size != sizeof(double))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): sizeof(double) mismatch." << EidosTerminate();
		if (double_test != 1234567890.0987654321)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): double format mismatch." << EidosTerminate();
		if (has_nucleotides && !nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): the output was generated by a nucleotide-based model, but the current model is not nucleotide-based." << EidosTerminate();
		if (!has_nucleotides && nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): the output was generated by a non-nucleotide-based model, but the current model is nucleotide-based." << EidosTerminate();
		if ((slim_tick_t_size != sizeof(slim_tick_t)) ||
			(slim_position_t_size != sizeof(slim_position_t)) ||
			(slim_objectid_t_size != sizeof(slim_objectid_t)) ||
			(slim_popsize_t_size != sizeof(slim_popsize_t)) ||
			(slim_refcount_t_size != sizeof(slim_refcount_t)) ||
			(slim_selcoeff_t_size != sizeof(slim_selcoeff_t)) ||
			(slim_mutationid_t_size != sizeof(slim_mutationid_t)) ||
			(slim_polymorphismid_t_size != sizeof(slim_polymorphismid_t)) ||
			(slim_age_t_size != sizeof(slim_age_t)) ||
			(slim_pedigreeid_t_size != sizeof(slim_pedigreeid_t)) ||
			(slim_haplosomeid_t_size != sizeof(slim_haplosomeid_t)))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): SLiM datatype size mismatch." << EidosTerminate();
		if ((spatial_output_count < 0) || (spatial_output_count > 3))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): spatial output count out of range." << EidosTerminate();
		if ((spatial_output_count > 0) && (spatial_output_count != spatial_dimensionality_))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): output spatial dimensionality does not match that of the simulation." << EidosTerminate();
		if (age_output_count && (model_type_ == SLiMModelType::kModelTypeWF))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): age information is present but the simulation is using a WF model." << EidosTerminate();
		if (!age_output_count && (model_type_ == SLiMModelType::kModelTypeNonWF))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): age information is not present but the simulation is using a nonWF model; age information must be included." << EidosTerminate();
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
		if (p + sizeof(subpop_start_tag) + sizeof(subpop_id) + sizeof(subpop_size) + sizeof(sex_flag) + sizeof(subpop_sex_ratio) > buf_end)
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
	
	// Read in the size of the mutation map, so we can allocate a vector rather than utilizing std::map
	int32_t mutation_map_size;
	
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
	
	if (!mutations)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): could not allocate mutations buffer." << EidosTerminate();
	
	while (true)
	{
		int32_t mutation_start_tag;
		slim_polymorphismid_t polymorphism_id;
		slim_mutationid_t mutation_id;					// Added in version 2
		slim_objectid_t mutation_type_id;
		slim_position_t position;
		slim_selcoeff_t selection_coeff;
		slim_selcoeff_t dominance_coeff;
		slim_objectid_t subpop_index;
		slim_tick_t tick;
		slim_refcount_t prevalence;
		int8_t nucleotide = -1;
		
		// If there isn't enough buffer left to read a full mutation record, we assume we are done with this section
		int record_size = sizeof(mutation_start_tag) + sizeof(polymorphism_id) + sizeof(mutation_type_id) + sizeof(position) + sizeof(selection_coeff) + sizeof(dominance_coeff) + sizeof(subpop_index) + sizeof(tick) + sizeof(prevalence);
		
		if (file_version >= 2)
			record_size += sizeof(mutation_id);
		if (has_nucleotides)
			record_size += sizeof(nucleotide);
		
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
		
		if (file_version >= 2)
		{
			memcpy(&mutation_id, p, sizeof(mutation_id));
			p += sizeof(mutation_id);
		}
		else
		{
			mutation_id = polymorphism_id;		// when parsing version 1 output, we use the polymorphism id as the mutation id
		}
		
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
		
		if (mutation_type_ptr->dominance_coeff_ != dominance_coeff)		// no tolerance, unlike _InitializePopulationFromTextFile(); should match exactly here since we used binary
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " has dominance coefficient " << mutation_type_ptr->dominance_coeff_ << " that does not match the population file dominance coefficient of " << dominance_coeff << "." << EidosTerminate();
		
		// BCH 9/22/2021: Note that mutation_type_ptr->hemizygous_dominance_coeff_ is not saved, or checked here; too edge to be bothered...
		
		if ((nucleotide == -1) && mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m"<< mutation_type_id << " is nucleotide-based, but a nucleotide value for a mutation of this type was not supplied." << EidosTerminate();
		if ((nucleotide != -1) && !mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): mutation type m"<< mutation_type_id << " is not nucleotide-based, but a nucleotide value for a mutation of this type was supplied." << EidosTerminate();
		
		// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
		MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
		
		Mutation *new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_id, mutation_type_ptr, TheChromosome().Index(), position, selection_coeff, subpop_index, tick, nucleotide);
		
		// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
		mutations[polymorphism_id] = new_mut_index;
		population_.MutationRegistryAdd(new_mut);
		
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		if (population_.keeping_muttype_registries_)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
		
		// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_pure_neutral_DFE_
		if (selection_coeff != 0.0)
		{
			pure_neutral_ = false;
			mutation_type_ptr->all_pure_neutral_DFE_ = false;
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
	
	// FIXME MULTICHROM need to figure out the file format for multiple chromosomes
	Chromosome &chromosome = TheChromosome();
	
	// Haplosomes section
	if (pedigree_output_count)
		gSLiM_next_pedigree_id = 0;
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	bool use_16_bit = (mutation_map_size <= UINT16_MAX - 1);	// 0xFFFF is reserved as the start of our various tags
	std::unique_ptr<MutationIndex[]> raii_haplosomebuf(new MutationIndex[mutation_map_size]);	// allowing us to use emplace_back_bulk() for speed
	MutationIndex *haplosomebuf = raii_haplosomebuf.get();
#ifndef _OPENMP
	MutationRunContext &mutrun_context = chromosome.ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
	
	while (true)
	{
		slim_objectid_t subpop_id;
		slim_popsize_t haplosome_index;
		int32_t haplosome_type;
		int32_t total_mutations;
		
		// If there isn't enough buffer left to read a full haplosome record, we assume we are done with this section
		if (p + sizeof(haplosome_type) + sizeof(subpop_id) + sizeof(haplosome_index) + sizeof(total_mutations) > buf_end)
			break;
		
		// First check the first 32 bits to see if it is a section end tag
		memcpy(&haplosome_type, p, sizeof(haplosome_type));
		
		if (haplosome_type == (int32_t)0xFFFF0000)
			break;
		
		// If not, proceed with reading the haplosome entry
		p += sizeof(haplosome_type);
		
		memcpy(&subpop_id, p, sizeof(subpop_id));
		p += sizeof(subpop_id);
		
		memcpy(&haplosome_index, p, sizeof(haplosome_index));
		p += sizeof(haplosome_index);
		
		// Look up the subpopulation
		Subpopulation *subpop = SubpopulationWithID(subpop_id);
		
		if (!subpop)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): referenced subpopulation p" << subpop_id << " not defined." << EidosTerminate();
		
		// Read in individual spatial position information.  Added in version 3.
		if (spatial_output_count && ((haplosome_index % 2) == 0))
		{
			// do another buffer length check
			if (p + spatial_output_count * sizeof(double) + sizeof(total_mutations) > buf_end)
				break;
			
			int individual_index = haplosome_index / 2;
			Individual &individual = *subpop->parent_individuals_[individual_index];
			
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
		
		// Read in individual pedigree ID information.  Added in version 6.
		if (pedigree_output_count  && ((haplosome_index % 2) == 0))
		{
			// do another buffer length check
			if (p + sizeof(slim_pedigreeid_t) + sizeof(total_mutations) > buf_end)
				break;
			
			if (PedigreesEnabled())
			{
				int individual_index = haplosome_index / 2;
				Individual &individual = *subpop->parent_individuals_[individual_index];
				slim_pedigreeid_t pedigree_id;
				
				memcpy(&pedigree_id, p, sizeof(pedigree_id));
				
				individual.SetPedigreeID(pedigree_id);
				individual.haplosomes_[0]->SetHaplosomeID(pedigree_id * 2);
				individual.haplosomes_[1]->SetHaplosomeID(pedigree_id * 2 + 1);
				gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);
			}
			
			p += sizeof(slim_pedigreeid_t);
		}
		
		// Read in individual age information.  Added in version 4.
		if (age_output_count && ((haplosome_index % 2) == 0))
		{
			// do another buffer length check
			if (p + sizeof(slim_age_t) + sizeof(total_mutations) > buf_end)
				break;
			
			int individual_index = haplosome_index / 2;
			Individual &individual = *subpop->parent_individuals_[individual_index];
			
			memcpy(&individual.age_, p, sizeof(individual.age_));
			p += sizeof(slim_age_t);
		}
		
		memcpy(&total_mutations, p, sizeof(total_mutations));
		p += sizeof(total_mutations);
		
		// Look up the haplosome
		if ((haplosome_index < 0) || (haplosome_index > SLIM_MAX_SUBPOP_SIZE * 2))
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): haplosome index out of permitted range." << EidosTerminate();
		
		int64_t individual_index = haplosome_index >> 1;
		Individual *ind = subpop->parent_individuals_[individual_index];
		Haplosome &haplosome = *(ind->haplosomes_[haplosome_index & 0x01]);
		
		// Error-check the haplosome type
		if (haplosome_type != (int32_t)haplosome.AssociatedChromosome()->Type())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): haplosome type does not match the instantiated haplosome." << EidosTerminate();
		
		// Check the null haplosome state
		// BCH 9/27/2021: We instantiate null haplosomes only in the case where we expect them: in sex-chromosome models,
		// for either the X or Y (whichever is not being simulated).  In nonWF autosomal models, any haplosome is now
		// allowed to be null, at the user's discretion, so we transform the instantiated haplosome to a null haplosome
		// if necessary.  AddSubpopulation() created the haplosomes above, before we knew which would be null.
		if (total_mutations == (int32_t)0xFFFF1000)
		{
			if (!haplosome.IsNull())
			{
				if ((model_type_ == SLiMModelType::kModelTypeNonWF) && (haplosome.AssociatedChromosome()->Type() == ChromosomeType::kA_DiploidAutosome))
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
	
	if (p + sizeof(section_end_tag) > buf_end)
		EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after haplosomes." << EidosTerminate();
	else
	{
		memcpy(&section_end_tag, p, sizeof(section_end_tag));
		p += sizeof(section_end_tag);
		(void)p;	// dead store above is deliberate
		
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after haplosomes." << EidosTerminate();
	}
	
	// Ancestral sequence section, for nucleotide-based models
	if (has_nucleotides)
	{
		if (p + sizeof(int64_t) > buf_end)
		{
			// The ancestral sequence can be suppressed at save time, to decrease file size etc.  If it is missing,
			// we do not consider that an error at present.  This is a little weird – it's more useful to suppress
			// the ancestral sequence when writing text – but maybe the user really doesn't want it.  So do nothing.
		}
		else
		{
			TheChromosome().AncestralSequence()->ReadCompressedNucleotides(&p, buf_end);
			
			if (p + sizeof(section_end_tag) > buf_end)
				EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): unexpected EOF after ancestral sequence." << EidosTerminate();
			else
			{
				memcpy(&section_end_tag, p, sizeof(section_end_tag));
				p += sizeof(section_end_tag);
				(void)p;	// dead store above is deliberate
				
				if (section_end_tag != (int32_t)0xFFFF0000)
					EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromBinaryFile): missing section end after ancestral sequence." << EidosTerminate();
			}
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
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> mutationEffect_callbacks = CallbackBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosMutationEffectCallback, -1, -1, subpop_id);
			std::vector<SLiMEidosBlock*> fitnessEffect_callbacks = CallbackBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosFitnessEffectCallback, -1, -1, subpop_id);
			
			subpop->UpdateFitness(mutationEffect_callbacks, fitnessEffect_callbacks);
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

std::vector<SLiMEidosBlock*> Species::CallbackBlocksMatching(slim_tick_t p_tick, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id)
{
	// Callbacks are species-specific; this method calls up to the community, which manages script blocks,
	// but does a species-specific search.
	return community_.ScriptBlocksMatching(p_tick, p_event_type, p_mutation_type_id, p_interaction_type_id, p_subpopulation_id, this);
}

void Species::RunInitializeCallbacks(void)
{
	// zero out the initialization check counts
	num_species_inits_ = 0;
	num_slimoptions_inits_ = 0;
	num_mutation_type_inits_ = 0;
	num_ge_type_inits_ = 0;
	num_sex_inits_ = 0;
	num_treeseq_inits_ = 0;
	num_chromosome_inits_ = 0;
	
	num_mutrate_inits_ = 0;
	num_recrate_inits_ = 0;
	num_genomic_element_inits_ = 0;
	num_gene_conv_inits_ = 0;
	num_ancseq_inits_ = 0;
	num_hotmap_inits_ = 0;
	
	has_implicit_chromosome_ = false;
	
	// execute initialize() callbacks, which should always have a tick of 0 set
	std::vector<SLiMEidosBlock*> init_blocks = CallbackBlocksMatching(0, SLiMEidosBlockType::SLiMEidosInitializeCallback, -1, -1, -1);
	
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
	
	if (nucleotide_based_)
	{
		if (num_ancseq_inits_ == 0)
			EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): Nucleotide-based models must provide an ancestral nucleotide sequence with initializeAncestralNucleotides()." << EidosTerminate();
		
		for (Chromosome *chromosome : chromosomes_)
			if (!chromosome->ancestral_seq_buffer_)
				EIDOS_TERMINATION << "ERROR (Species::RunInitializeCallbacks): (internal error) No ancestral sequence!" << EidosTerminate();
	}
	
	CheckMutationStackPolicy();
	
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
					if ((muttype->dfe_type_ == DFEType::kFixed) && (muttype->dfe_parameters_.size() == 1) && (muttype->dfe_parameters_[0] == 0.0))
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

void Species::RecalculateFitness(void)
{
	population_.RecalculateFitness(cycle_);	// used to be cycle_ + 1 in the WF cycle; removing that 18 Feb 2016 BCH
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
	
	graveyard_.clear();
}

void Species::WF_GenerateOffspring(void)
{
	slim_tick_t tick = community_.Tick();
	std::vector<SLiMEidosBlock*> mate_choice_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosMateChoiceCallback, -1, -1, -1);
	std::vector<SLiMEidosBlock*> modify_child_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosModifyChildCallback, -1, -1, -1);
	std::vector<SLiMEidosBlock*> recombination_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, -1);
	std::vector<SLiMEidosBlock*> mutation_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosMutationCallback, -1, -1, -1);
	bool mate_choice_callbacks_present = mate_choice_callbacks.size();
	bool modify_child_callbacks_present = modify_child_callbacks.size();
	bool recombination_callbacks_present = recombination_callbacks.size();
	bool mutation_callbacks_present = mutation_callbacks.size();
	bool no_active_callbacks = true;
	
	// a type 's' DFE needs to count as an active callback; it could activate other callbacks,
	// and in any case we need EvolveSubpopulation() to take the non-parallel code path
	if (type_s_dfes_present_)
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
			subpop->registered_mate_choice_callbacks_.clear();
			
			for (SLiMEidosBlock *callback : mate_choice_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_mate_choice_callbacks_.emplace_back(callback);
			}
			
			// Get modifyChild() callbacks that apply to this subpopulation
			subpop->registered_modify_child_callbacks_.clear();
			
			for (SLiMEidosBlock *callback : modify_child_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_modify_child_callbacks_.emplace_back(callback);
			}
			
			// Get recombination() callbacks that apply to this subpopulation
			subpop->registered_recombination_callbacks_.clear();
			
			for (SLiMEidosBlock *callback : recombination_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_recombination_callbacks_.emplace_back(callback);
			}
			
			// Get mutation() callbacks that apply to this subpopulation
			subpop->registered_mutation_callbacks_.clear();
			
			for (SLiMEidosBlock *callback : mutation_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop->registered_mutation_callbacks_.emplace_back(callback);
			}
		}
		
		// then evolve each subpop
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			population_.EvolveSubpopulation(*subpop_pair.second, mate_choice_callbacks_present, modify_child_callbacks_present, recombination_callbacks_present, mutation_callbacks_present, type_s_dfes_present_);
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
	std::vector<SLiMEidosBlock*> reproduction_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosReproductionCallback, -1, -1, -1);
	std::vector<SLiMEidosBlock*> modify_child_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosModifyChildCallback, -1, -1, -1);
	std::vector<SLiMEidosBlock*> recombination_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, -1);
	std::vector<SLiMEidosBlock*> mutation_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosMutationCallback, -1, -1, -1);
	
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
		subpop->registered_reproduction_callbacks_.clear();
		
		for (SLiMEidosBlock *callback : reproduction_callbacks)
		{
			slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
			
			if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
				subpop->registered_reproduction_callbacks_.emplace_back(callback);
		}
		
		// Get modifyChild() callbacks that apply to this subpopulation
		subpop->registered_modify_child_callbacks_.clear();
		
		for (SLiMEidosBlock *callback : modify_child_callbacks)
		{
			slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
			
			if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
				subpop->registered_modify_child_callbacks_.emplace_back(callback);
		}
		
		// Get recombination() callbacks that apply to this subpopulation
		subpop->registered_recombination_callbacks_.clear();
		
		for (SLiMEidosBlock *callback : recombination_callbacks)
		{
			slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
			
			if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
				subpop->registered_recombination_callbacks_.emplace_back(callback);
		}
		
		// Get mutation() callbacks that apply to this subpopulation
		subpop->registered_mutation_callbacks_.clear();
		
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
	// FIXME MULTICHROM deferred reproduction is disabled for now
	//population_.DoDeferredReproduction();
	
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
	std::vector<SLiMEidosBlock*> survival_callbacks = CallbackBlocksMatching(tick, SLiMEidosBlockType::SLiMEidosSurvivalCallback, -1, -1, -1);
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
			 (chromosome_type == ChromosomeType::kHF_HaploidFemaleInherited) ||
			 (chromosome_type == ChromosomeType::kFL_HaploidFemaleLine) ||
			 (chromosome_type == ChromosomeType::kHM_HaploidMaleInherited) ||
			 (chromosome_type == ChromosomeType::kML_HaploidMaleLine) ||
			 (chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull)))
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) chromosome type '" << chromosome_type << "' not allowed in non-sexual models." << EidosTerminate();
		
		// check haplosome indices
		int haplosome_count = 0;
		
		switch (chromosome_type)
		{
			case ChromosomeType::kA_DiploidAutosome:
			case ChromosomeType::kX_XSexChromosome:
			case ChromosomeType::kZ_ZSexChromosome:
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
				haplosome_count = 2;
				break;
			case ChromosomeType::kH_HaploidAutosome:
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kHF_HaploidFemaleInherited:
			case ChromosomeType::kFL_HaploidFemaleLine:
			case ChromosomeType::kHM_HaploidMaleInherited:
			case ChromosomeType::kML_HaploidMaleLine:
				haplosome_count = 1;
				break;
		}
		
		if (first_haplosome_index_[chromosome_index] != haplosome_index)
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) first_haplosome_index_ mismatch." << EidosTerminate();
		if (last_haplosome_index_[chromosome_index] != haplosome_index + haplosome_count - 1)
			EIDOS_TERMINATION << "ERROR (Species::Species_CheckIntegrity): (internal error) last_haplosome_index_ mismatch." << EidosTerminate();
		
		haplosome_index += haplosome_count;
		
		// check null haplosome optimization
		switch (chromosome_type)
		{
			case ChromosomeType::kX_XSexChromosome:
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kZ_ZSexChromosome:
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kFL_HaploidFemaleLine:
			case ChromosomeType::kML_HaploidMaleLine:
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
				null_haplosomes_used = true;
				break;
			case ChromosomeType::kA_DiploidAutosome:
			case ChromosomeType::kH_HaploidAutosome:
			case ChromosomeType::kHF_HaploidFemaleInherited:
			case ChromosomeType::kHM_HaploidMaleInherited:
				break;
		}
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
	
	haplosome_pool_usage = population_.species_haplosome_pool_.MemoryUsageForAllNodes();
	individual_pool_usage = population_.species_individual_pool_.MemoryUsageForAllNodes();
	
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
			if (subpop.cached_male_fitness_)
				p_usage->subpopulationFitnessCaches += subpop.cached_fitness_capacity_ * sizeof(double);
			
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
#endif
			}
		}
	}
	
	/*
	 Subpopulation:
	 
	gsl_ran_discrete_t *lookup_parent_ = nullptr;			// OWNED POINTER: lookup table for drawing a parent based upon fitness
	gsl_ran_discrete_t *lookup_female_parent_ = nullptr;	// OWNED POINTER: lookup table for drawing a female parent based upon fitness, SEX ONLY
	gsl_ran_discrete_t *lookup_male_parent_ = nullptr;		// OWNED POINTER: lookup table for drawing a male parent based upon fitness, SEX ONLY

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
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			Eidos_ran_shuffle(rng, buffer_contents, shuffle_buf_size_);
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
		if (!retain_coalescent_only_) flags |= TSK_SIMPLIFY_KEEP_UNARY;
		// FIXME Peter comments "I think this is supposed to be TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS; see #487."
		// That isn't related to multichrom at all, though, so I'm going to leave it for later, with #487.
		
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
			ret = tsk_table_collection_check_integrity(&chromosome_tables, 0);
			if (ret < 0) handle_error("SimplifyAllTreeSequences() tsk_table_collection_check_integrity after node remapping", ret);
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
			ret = tsk_table_collection_check_integrity(&main_tables, 0);
			if (ret < 0) handle_error("SimplifyAllTreeSequences() tsk_table_collection_check_integrity after individual remapping", ret);
#endif
		}
		
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
			CheckCoalescenceAfterSimplification(tsinfo);
	}
}

void Species::CheckCoalescenceAfterSimplification(TreeSeqInfo &tsinfo)
{
#if DEBUG
	if (!recording_tree_ || !running_coalescence_checks_)
		EIDOS_TERMINATION << "ERROR (Species::CheckCoalescenceAfterSimplification): (internal error) coalescence check called with recording or checking off." << EidosTerminate();
#endif
	
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
	
	for (TreeSeqInfo &tsinfo : treeseq_)
		tsk_table_collection_truncate(&tsinfo.tables_, &tsinfo.table_position_);
}

void Species::RecordNewHaplosome(std::vector<slim_position_t> *p_breakpoints, Haplosome *p_new_haplosome, 
		const Haplosome *p_initial_parental_haplosome, const Haplosome *p_second_parental_haplosome)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::RecordNewHaplosome): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	slim_chromosome_index_t index = p_new_haplosome->chromosome_index_;
	Chromosome &chromosome = *chromosomes_[index];
	TreeSeqInfo &tsinfo = treeseq_[index];
	
	// This records information about an individual in both the Node and Edge tables.
	// BCH 12/6/2024: Note that recording the new node table entries is now done by SetCurrentNewIndividual().  That method
	// determines the tskit node ids for the two haplosome positions of the individual, as tsk_node_id_base_ (+ 1).
	
	// Note that the breakpoints vector provided may (or may not) contain a breakpoint, as the final breakpoint in the vector, that is beyond
	// the end of the chromosome.  This is for bookkeeping in the crossover-mutation code and should be ignored, as the code below does.
	// The breakpoints vector may be nullptr (indicating no recombination), but if it exists it will be sorted in ascending order.

	// if there is no parent then no need to record edges
	if (!p_initial_parental_haplosome && !p_second_parental_haplosome)
		return;
	
	assert(p_initial_parental_haplosome);	// this cannot be nullptr if p_second_parental_haplosome is non-null, so now it is guaranteed non-null
	
	// get the TSK IDs for all the haplosomes involved; they are the tsk_node_id_base_ of the owning individual,
	// plus 0 or 1 depending on whether they are the first or second haplosome for their associated chromosome
	tsk_id_t offspringTSKID, haplosome1TSKID, haplosome2TSKID;
	
	offspringTSKID = p_new_haplosome->OwningIndividual()->TskitNodeIdBase() + p_new_haplosome->chromosome_subposition_;
	haplosome1TSKID = p_initial_parental_haplosome->OwningIndividual()->TskitNodeIdBase() + p_initial_parental_haplosome->chromosome_subposition_;
	if (!p_second_parental_haplosome)
		haplosome2TSKID = haplosome1TSKID;
	else
		haplosome2TSKID = p_second_parental_haplosome->OwningIndividual()->TskitNodeIdBase() + p_second_parental_haplosome->chromosome_subposition_;
	
	// fix possible excess past-the-end breakpoint
	size_t breakpoint_count = (p_breakpoints ? p_breakpoints->size() : 0);
	
	if (breakpoint_count && (p_breakpoints->back() > chromosome.last_position_))
		breakpoint_count--;
	
	// add an edge for each interval between breakpoints
	double left = 0.0;
	double right;
	bool polarity = true;
	
	for (size_t i = 0; i < breakpoint_count; i++)
	{
		right = (*p_breakpoints)[i];

		tsk_id_t parent = (tsk_id_t) (polarity ? haplosome1TSKID : haplosome2TSKID);
		int ret = tsk_edge_table_add_row(&tsinfo.tables_.edges, left, right, parent, offspringTSKID, NULL, 0);
		if (ret < 0) handle_error("tsk_edge_table_add_row", ret);
		
		polarity = !polarity;
		left = right;
	}
	
	right = (double)chromosome.last_position_+1;
	tsk_id_t parent = (tsk_id_t) (polarity ? haplosome1TSKID : haplosome2TSKID);
	int ret = tsk_edge_table_add_row(&tsinfo.tables_.edges, left, right, parent, offspringTSKID, NULL, 0);
	if (ret < 0) handle_error("tsk_edge_table_add_row", ret);
	
	// BCH 12/10/2024: With the new metadata scheme for haplosome, we also need to fix the is_null_metadata if
	// the new haplosome is a null haplosome *and* it belongs to a chromosome type where that is notable.  In
	// the present design, that can only be chromosome types "A" and "H"; the other chromosome types do not
	// allow deviation from the default null-haplosome configuration.
	if (p_new_haplosome->IsNull())
	{
		ChromosomeType chromosome_type = chromosome.Type();
		
		if ((chromosome_type == ChromosomeType::kA_DiploidAutosome) || (chromosome_type == ChromosomeType::kH_HaploidAutosome))
		{
			// it is null and that was unexpected; we need to flip the corresponding is_null_ bit
			// each chromosome has two node table entries; entry 1 is for haplosome 1, entry 2 is
			// for haplosome 2, so there is only one bit per chromosome in a given is_null_ vector
			slim_chromosome_index_t chromosome_index = chromosome.Index();
			tsk_node_table_t &shared_node_table = treeseq_[0].tables_.nodes;
			HaplosomeMetadataRec *metadata = (HaplosomeMetadataRec *)(shared_node_table.metadata + shared_node_table.metadata_offset[offspringTSKID]);
			uint8_t *metadata_is_null = metadata->is_null_;
			int byte_index = chromosome_index / 8;
			int bit_shift = chromosome_index % 8;
			
			metadata_is_null[byte_index] |= (0x01 << bit_shift);
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
	
	derived_mutation_ids.clear();
	mutation_metadata.clear();
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
	
	// add the mutation table row with the final derived state and metadata
	char *derived_muts_bytes = (char *)(derived_mutation_ids.data());
	size_t derived_state_length = derived_mutation_ids.size() * sizeof(slim_mutationid_t);
	char *mutation_metadata_bytes = (char *)(mutation_metadata.data());
	size_t mutation_metadata_length = mutation_metadata.size() * sizeof(MutationMetadataRec);

	double time = -(double) (community_.tree_seq_tick_ + community_.tree_seq_tick_offset_);	// see Population::AddSubpopulationSplit() regarding tree_seq_tick_offset_
	int ret = tsk_mutation_table_add_row(&tsinfo.tables_.mutations, site_id, haplosomeTSKID, TSK_NULL, 
					time,
					derived_muts_bytes, (tsk_size_t)derived_state_length,
					mutation_metadata_bytes, (tsk_size_t)mutation_metadata_length);
	if (ret < 0) handle_error("tsk_mutation_table_add_row", ret);
	
#if DEBUG
	if (time < tsinfo.tables_.nodes.time[haplosomeTSKID]) 
		std::cout << "Species::RecordNewDerivedState(): invalid derived state recorded in tick " << community_.Tick() << " haplosome " << haplosomeTSKID << " id " << p_haplosome->haplosome_id_ << " with time " << time << " >= " << tsinfo.tables_.nodes.time[haplosomeTSKID] << std::endl;
#endif
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
					got_metadata = true;
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
				got_metadata = true;
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
	
	std::string new_metadata_str = metadata.dump();

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
	size_t pos = tsk_node_metadata_schema.find("%d");
	std::string count_string = std::to_string(haplosome_metadata_isnull_bytes_);
	
	if (pos == std::string::npos)
		EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequenceMetadata): (internal error) `%d` substring missing from gSLiM_tsk_node_metadata_schema_FORMAT." << EidosTerminate();
	
	tsk_node_metadata_schema.replace(pos, 2, count_string);		// replace %d in the format string with the byte count
	
	ret = tsk_node_table_set_metadata_schema(&p_tables->nodes,
			tsk_node_metadata_schema.c_str(),
			(tsk_size_t)tsk_node_metadata_schema.length());
	if (ret != 0)
		handle_error("tsk_node_table_set_metadata_schema", ret);
}

void Species::WriteProvenanceTable(tsk_table_collection_t *p_tables, bool p_use_newlines, bool p_include_model)
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
	
#if 0
	// Old provenance writing code, making a JSON string by hand; this is file_version 0.1
	char *provenance_str;
	provenance_str = (char *)malloc(1024);
	sprintf(provenance_str, "{\"program\": \"SLiM\", \"version\": \"%s\", \"file_version\": \"%s\", \"model_type\": \"%s\", \"generation\": %d, \"remembered_node_count\": %ld}",
			SLIM_VERSION_STRING, SLIM_TREES_FILE_VERSION, (model_type_ == SLiMModelType::kModelTypeWF) ? "WF" : "nonWF", Cycle(), (long)remembered_nodes_.size());
	
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buffer, timestamp_size, "%Y-%m-%dT%H:%M:%S", tm_info);
	
	ret = tsk_provenance_table_add_row(&p_tables->provenances, buffer, strlen(buffer), provenance_str, strlen(provenance_str));
	free(provenance_str);
	if (ret < 0) handle_error("tsk_provenance_table_add_row", ret);
#else
	// New provenance writing code, using the JSON for Modern C++ library (json.hpp); this is file_version 0.2 (and up)
	nlohmann::json j;
	
	j["schema_version"] = "1.0.0";
	
	struct utsname name;
	ret = uname(&name);
	
	if (ret == -1)
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): (internal error) uname() failed." << EidosTerminate();
	
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
	
	std::string provenance_str;
	
	if (p_use_newlines)
		provenance_str = j.dump(4);
	else
		provenance_str = j.dump();
	
	//std::cout << "Provenance output: \n" << provenance_str << std::endl;
	
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buffer, timestamp_size, "%Y-%m-%dT%H:%M:%S", tm_info);
	
	ret = tsk_provenance_table_add_row(&p_tables->provenances, buffer, (tsk_size_t)strlen(buffer), provenance_str.c_str(), (tsk_size_t)provenance_str.length());
	if (ret < 0) handle_error("tsk_provenance_table_add_row", ret);
#endif
}

void Species::ReadTreeSequenceMetadata(tsk_table_collection_t *p_tables, slim_tick_t *p_tick, slim_tick_t *p_cycle, SLiMModelType *p_model_type, int *p_file_version)
{
#if 0
	// Old provenance reading code; this can handle file_version 0.1 only
	tsk_provenance_table_t &provenance_table = p_tables->provenances;
	tsk_size_t num_rows = provenance_table.num_rows;
	
	if (num_rows <= 0)
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): no provenance table entries; this file cannot be read." << EidosTerminate();
	
	// find the last record that is a SLiM provenance entry; we allow entries after ours, on the assumption that they have preserved SLiM-compliance
	int slim_record_index = num_rows - 1;
	
	for (slim_record_index = num_rows - 1; slim_record_index >= 0; --slim_record_index)
	{
		char *record = provenance_table.record + provenance_table.record_offset[slim_record_index];
		tsk_size_t record_len = provenance_table.record_offset[slim_record_index + 1] - provenance_table.record_offset[slim_record_index];
		
		// for an entry to be acceptable, it has to start with the SLiM program declaration
		if (record_len < strlen("{\"program\": \"SLiM\", "))
			continue;
		if (strncmp(record, "{\"program\": \"SLiM\", ", strlen("{\"program\": \"SLiM\", ")))
			continue;
		break;
	}
	
	if (slim_record_index == -1)
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): no SLiM provenance table entry found; this file cannot be read." << EidosTerminate();
	
	//char *slim_timestamp = provenance_table.timestamp + provenance_table.timestamp_offset[slim_record_index];
	char *slim_record = provenance_table.record + provenance_table.record_offset[slim_record_index];
	tsk_size_t slim_record_len = provenance_table.record_offset[slim_record_index + 1] - provenance_table.record_offset[slim_record_index];
	
	if (slim_record_len >= 1024)
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): SLiM provenance table entry is too long; this file cannot be read." << EidosTerminate();
	
	std::string last_record_str(slim_record, slim_record_len);
	
	static char *program = NULL, *version = NULL, *file_version = NULL, *model_type = NULL, *generation = NULL, *rem_count = NULL;
	
	if (!program)
	{
		program = (char *)malloc(101 * sizeof(char));
		version = (char *)malloc(101 * sizeof(char));
		file_version = (char *)malloc(101 * sizeof(char));
		model_type = (char *)malloc(101 * sizeof(char));
		generation = (char *)malloc(101 * sizeof(char));
		rem_count = (char *)malloc(101 * sizeof(char));
		
		if (!program || !version || !file_version || !model_type || !generation || !rem_count)
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
	}
	
	int end_pos;
	int conv = sscanf(last_record_str.c_str(), "{\"program\": \"%100[^\"]\", \"version\": \"%100[^\"]\", \"file_version\": \"%100[^\"]\", \"model_type\": \"%100[^\"]\", \"generation\": %100[0-9], \"remembered_node_count\": %100[0-9]}%n", program, version, file_version, model_type, generation, rem_count, &end_pos);
	
	if ((conv != 6) || (end_pos != (int)slim_record_len))
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): provenance table entry was malformed; this file cannot be read." << EidosTerminate();
	
	std::string program_str(program);
	std::string version_str(version);
	std::string file_version_str(file_version);
	std::string model_type_str(model_type);
	std::string cycle_str(generation);
	std::string rem_count_str(rem_count);
	
	if (program_str != "SLiM")
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): this .trees file was not generated with correct SLiM provenance information; this file cannot be read." << EidosTerminate();
	
	if (file_version_str != "0.1")
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): this .trees file was generated by an unrecognized version of SLiM or pyslim; this file cannot be read." << EidosTerminate();
	
	// check the model type; at the moment we do not require the model type to match what we are running, but we issue a warning on a mismatch
	if ((model_type_str != "WF") && (model_type_str != "nonWF"))
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): unrecognized model type; this file cannot be read." << EidosTerminate();
	
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
	
	// read the generation and bounds-check it
	long long gen_ll = 0;
	
	try {
		gen_ll = std::stoll(cycle_str);
	}
	catch (...)
	{
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): malformed generation value; this file cannot be read." << EidosTerminate();
	}
	
	if ((gen_ll < 1) || (gen_ll > SLIM_MAX_TICK))
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): generation value out of range; this file cannot be read." << EidosTerminate();
	
	*p_tick = (slim_tick_t)gen_ll;			// assumed to be the same as the generation, for this file format version
	*p_cycle = (slim_tick_t)gen_ll;
	
	// read the remembered haplosome count and bounds-check it
	long long rem_count_ll = 0;
	
	try {
		rem_count_ll = std::stoll(rem_count_str);
	}
	catch (...)
	{
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): malformed remembered node count; this file cannot be read." << EidosTerminate();
	}
	
	if (rem_count_ll < 0)
		EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): remembered node count value out of range; this file cannot be read." << EidosTerminate();
	
	*p_remembered_haplosome_count = (size_t)rem_count_ll;
#else
	// New provenance reading code, using the JSON for Modern C++ library (json.hpp); 
	
	std::string model_type_str;
	std::string cycle_stage_str;
	long long tick_ll, gen_ll;

	////////////
	// Format 0.5 and later: using top-level metadata
	try {
		// Note: we *could* parse the metadata schema (which is json),
		// but instead we'll just try parsing the metadata.
		// std::string metadata_schema_str(p_tables->metadata_schema, p_tables->metadata_schema_length);
		// nlohmann::json metadata_schema = nlohmann::json::parse(metadata_schema_str);

		std::string metadata_str(p_tables->metadata, p_tables->metadata_length);
		auto metadata = nlohmann::json::parse(metadata_str);
		
		// We test for some keys if they are new or optional, but assume that others must be there, such as "model_type".
		// If we fetch a key and it is missing, nhlohmann::json raises and we end up in the provenance fallback code below.
		model_type_str = metadata["SLiM"]["model_type"];
		
		if (metadata["SLiM"].contains("cycle"))
			gen_ll = metadata["SLiM"]["cycle"];
		else
			gen_ll = metadata["SLiM"]["generation"];
		
		if (metadata["SLiM"].contains("tick"))
			tick_ll = metadata["SLiM"]["tick"];
		else
			tick_ll = gen_ll;		// when tick is missing, assume it is equal to cycle
		
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
		
		auto file_version_03 = metadata["SLiM"]["file_version"];
		if (file_version_03 == SLIM_TREES_FILE_VERSION_META)
			*p_file_version = 5;
		else if (file_version_03 == SLIM_TREES_FILE_VERSION_PREPARENT)
			*p_file_version = 6;
		else if (file_version_03 == SLIM_TREES_FILE_VERSION_PRESPECIES)
			*p_file_version = 7;
		else if (file_version_03 == SLIM_TREES_FILE_VERSION)
			*p_file_version = 8;
		else
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): this .trees file was generated by an unrecognized version of SLiM or pyslim (internal file version " << file_version_03 << "); this file cannot be read." << EidosTerminate();
	} catch (...) {
	///////////////////////
	// Previous formats: everything is in provenance

		tsk_provenance_table_t &provenance_table = p_tables->provenances;
		tsk_size_t num_rows = provenance_table.num_rows;
		
		if (num_rows <= 0)
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): invalid/incomplete/outdated SLiM metadata and no provenance table entry found; this file cannot be read." << EidosTerminate();
		
		// find the last record that is a SLiM provenance entry; we allow entries after ours, on the assumption that they have preserved SLiM-compliance
		int slim_record_index = (int)(num_rows - 1);
		
		for (; slim_record_index >= 0; --slim_record_index)
		{
			char *record = provenance_table.record + provenance_table.record_offset[slim_record_index];
			tsk_size_t record_len = provenance_table.record_offset[slim_record_index + 1] - provenance_table.record_offset[slim_record_index];
			std::string record_str(record, record_len);
			
			try {
				auto j = nlohmann::json::parse(record_str);
				
				// for an entry to be acceptable, it has to have a "program": "SLiM" entry (file_version 0.1) or a "software"/"name": "SLiM" entry (file_version 0.2)
				if ((j["program"] == "SLiM") || (j["software"]["name"] == "SLiM"))
					break;
				continue;
			} catch (...) {
				continue;
			}
		}
		
		if (slim_record_index == -1)
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): invalid/incomplete/outdated SLiM metadata and no SLiM provenance table entry found; this file cannot be read." << EidosTerminate();
		
		//char *slim_timestamp = provenance_table.timestamp + provenance_table.timestamp_offset[slim_record_index];
		char *slim_record = provenance_table.record + provenance_table.record_offset[slim_record_index];
		tsk_size_t slim_record_len = provenance_table.record_offset[slim_record_index + 1] - provenance_table.record_offset[slim_record_index];
		std::string slim_record_str(slim_record, slim_record_len);
		auto j = nlohmann::json::parse(slim_record_str);	// no need for try/catch; this record parsed successfully above
		
		//std::cout << "Read provenance:\n" << slim_record_str << std::endl;
		
		auto file_version_01 = j["file_version"];
		auto file_version_02 = j["slim"]["file_version"];
		
		if (file_version_01.is_string() && (file_version_01 == "0.1"))
		{
			// We actually don't have any chance of being able to read SLiM 3.0 .trees files in, I guess;
			// all the new individuals table stuff, the addition of the population table, etc., mean that
			// it would just be a huge headache to try to do this.  So let's throw an error up front.
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): file_version is 0.1 in .trees file; this file cannot be read.  SLiM 3.1 and later cannot read saved .trees files from prior versions of SLiM; sorry." << EidosTerminate();
			
			/*
			try {
				model_type_str = j["model_type"];
				gen_ll = j["generation"];
				//rem_count_ll = j["remembered_node_count"];	// no longer using this key
			}
			catch (...)
			{
				EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): error reading provenance value (file_version 0.1); this file cannot be read." << EidosTerminate();
			}
			 */
		}
		else if (file_version_02.is_string())
		{
			// File version 0.2 was before the addition of nucleotides, so MutationMetadataRec_PRENUC will need to be used
			// File version 0.3 supports nucleotides, using MutationMetadataRec
			// File version 0.4 adds the model_hash key in provenance
			// File version >= 0.5 uses top-level metadata, but we don't write that out when writing text, so we end up here and get the info from provenance
			// when reading binary files, version >= 0.5 should not hit this code path, but it's harmless to test for those versions below.  Note the warning
			// emitted below for that case, added in SLiM 4 since we really shouldn't be hitting this code path any more except for very old legacy files.
			if (file_version_02 == SLIM_TREES_FILE_VERSION_PRENUC)
				*p_file_version = 2;
			else if (file_version_02 == SLIM_TREES_FILE_VERSION_POSTNUC)
				*p_file_version = 3;
			else if (file_version_02 == SLIM_TREES_FILE_VERSION_HASH)
				*p_file_version = 4;
			else if (file_version_02 == SLIM_TREES_FILE_VERSION_META)
				*p_file_version = 5;
			else if (file_version_02 == SLIM_TREES_FILE_VERSION_PREPARENT)
				*p_file_version = 6;
			else if (file_version_02 == SLIM_TREES_FILE_VERSION_PRESPECIES)
				*p_file_version = 7;
			else if (file_version_02 == SLIM_TREES_FILE_VERSION)
				*p_file_version = 8;
			else
				EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): this .trees file was generated by an unrecognized version of SLiM or pyslim (internal file version " << file_version_02 << "); this file cannot be read." << EidosTerminate();
			
			try {
				model_type_str = j["parameters"]["model_type"];
				
				if (j["slim"].contains("cycle"))
					gen_ll = j["slim"]["cycle"];
				else
					gen_ll = j["slim"]["generation"];
				
				tick_ll = j["slim"]["cycle"];			// assumed to be the same as the cycle, for this file format version
				
				if (j["parameters"].contains("stage"))
					cycle_stage_str = j["parameters"]["stage"];
				
				//rem_count_ll = j["slim"]["remembered_node_count"];	// no longer using this key
			}
			catch (...)
			{
				EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): error reading provenance value (internal file version " << file_version_02 << "); this file cannot be read." << EidosTerminate();
			}
			
			if (*p_file_version >= 5)
				std::cout << "WARNING (Species::ReadTreeSequenceMetadata): this .trees file may be readable by falling back on information in the provenance table entry, but its SLiM metadata appears to be invalid/incomplete/outdated.  Proceed with caution." << std::endl;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Species::ReadTreeSequenceMetadata): missing or corrupted file version; this file cannot be read." << EidosTerminate();
		}
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
#endif
}

void Species::WriteTreeSequence(std::string &p_recording_tree_path, bool p_simplify, bool p_include_model, EidosDictionaryUnretained *p_metadata_dict)
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
		std::string error_string;
		bool success = Eidos_CreateDirectory(resolved_user_path, &error_string);
		
		// Fatal error if we can't create the directory
		if (error_string.length())
			EIDOS_TERMINATION << error_string << EidosTerminate();
		else if (!success)
			EIDOS_TERMINATION << "ERROR (Species::WriteTreeSequence): directory could not be created at path " << resolved_user_path << ", for unknown reasons." << EidosTerminate();
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
		ret = tsk_table_collection_compute_mutation_parents(&output_tables, 0);
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
		WriteProvenanceTable(&output_tables, /* p_use_newlines */ true, p_include_model);
		
		// Add top-level metadata and metadata schema
		WriteTreeSequenceMetadata(&output_tables, p_metadata_dict, /* p_chromosome_index */ 0);
		
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
	p_metadata->selection_coeff_ = p_mutation->selection_coeff_;
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
	p_metadata->selection_coeff_ = p_substitution->selection_coeff_;
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
		int ret = tsk_table_collection_check_integrity(&tsinfo.tables_, TSK_NO_CHECK_POPULATION_REFS);
		if (ret < 0) handle_error("tsk_table_collection_check_integrity()", ret);
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
		haplosomes.clear();
		
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
				flags = TSK_SIMPLIFY_FILTER_SITES | TSK_SIMPLIFY_FILTER_INDIVIDUALS | TSK_SIMPLIFY_KEEP_INPUT_ROOTS;
				if (!retain_coalescent_only_) flags |= TSK_SIMPLIFY_KEEP_UNARY;
				ret = tsk_table_collection_simplify(tables_copy, samples.data(), (tsk_size_t)samples.size(), flags, NULL);
				if (ret != 0) handle_error("tsk_table_collection_simplify", ret);
				
				// must build indexes before compute mutation parents
				ret = tsk_table_collection_build_index(tables_copy, 0);
				if (ret < 0) handle_error("tsk_table_collection_build_index", ret);
				
				ret = tsk_table_collection_compute_mutation_parents(tables_copy, 0);
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
				
				fixed_mutids.clear();
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
						allele_mutids.clear();
						haplosome_mutids.clear();
						
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

#if INTERIM_TREESEQ_DISABLE
void Species::__RewriteOldIndividualsMetadata(int p_file_version)
{
	// rewrite individuals table metadata if it is in the old (pre-parent-pedigree-id) format; after this,
	// the format is current, so all downstream code can just assume the current metadata format
	if (p_file_version < 7)
	{
		size_t row_count = tables_.individuals.num_rows;

		if (row_count > 0)
		{
			size_t old_metadata_rec_size = sizeof(IndividualMetadataRec_PREPARENT);
			size_t new_metadata_rec_size = sizeof(IndividualMetadataRec);

			if (row_count * old_metadata_rec_size != tables_.individuals.metadata_length)
				EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): unexpected individuals table metadata length when translating metadata from pre-parent format." << EidosTerminate();

			size_t new_metadata_length = row_count * new_metadata_rec_size;
			IndividualMetadataRec *new_metadata_buffer = (IndividualMetadataRec *)malloc(new_metadata_length);

			if (!new_metadata_buffer)
				EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);

			for (size_t row_index = 0; row_index < row_count; ++row_index)
			{
				IndividualMetadataRec_PREPARENT *old_metadata = ((IndividualMetadataRec_PREPARENT *)tables_.individuals.metadata) + row_index;
				IndividualMetadataRec *new_metadata = new_metadata_buffer + row_index;

				new_metadata->pedigree_id_ = old_metadata->pedigree_id_;
				new_metadata->pedigree_p1_ = -1;
				new_metadata->pedigree_p2_ = -1;
				new_metadata->age_ = old_metadata->age_;
				new_metadata->subpopulation_id_ = old_metadata->subpopulation_id_;
				new_metadata->sex_ = old_metadata->sex_;
				new_metadata->flags_ = old_metadata->flags_;
			}

			for (size_t row_index = 0; row_index <= row_count; ++row_index)
				tables_.individuals.metadata_offset[row_index] = row_index * new_metadata_rec_size;

			free(tables_.individuals.metadata);
			tables_.individuals.metadata = (char *)new_metadata_buffer;
			tables_.individuals.metadata_length = new_metadata_length;
			tables_.individuals.max_metadata_length = new_metadata_length;
		}

		// replace the metadata schema; note that we don't check that the old schema is what we expect it to be
		int ret = tsk_individual_table_set_metadata_schema(&tables_.individuals,
				gSLiM_tsk_individual_metadata_schema.c_str(),
				(tsk_size_t)gSLiM_tsk_individual_metadata_schema.length());
		if (ret != 0)
			handle_error("tsk_individual_table_set_metadata_schema", ret);
	}
}

void Species::__RewriteOrCheckPopulationMetadata(void)
{
	// check population table metadata
	char *pop_schema_ptr = tables_.populations.metadata_schema;
	tsk_size_t pop_schema_len = tables_.populations.metadata_schema_length;
	std::string pop_schema(pop_schema_ptr, pop_schema_len);
	
	if (pop_schema == gSLiM_tsk_population_metadata_schema_PREJSON)
	{
		// The population table metadata is in the old (pre-JSON) format; rewrite it.  After this munging,
		// the format is current, so all downstream code can just assume the current metadata format.
		size_t row_count = tables_.populations.num_rows;
		
		if (row_count > 0)
		{
			std::string new_metadata;
			tsk_size_t *new_metadata_offsets = (tsk_size_t *)malloc((tables_.populations.max_rows + 1) * sizeof(tsk_size_t));
			
			if (!new_metadata_offsets)
				EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			new_metadata_offsets[0] = 0;
			
			for (size_t row_index = 0; row_index < row_count; ++row_index)
			{
				SubpopulationMetadataRec_PREJSON *old_metadata = ((SubpopulationMetadataRec_PREJSON *)tables_.populations.metadata) + row_index;
				tsk_size_t old_metadata_length = tables_.populations.metadata_offset[row_index + 1] - tables_.populations.metadata_offset[row_index];
				
				if (old_metadata_length)
				{
					if (old_metadata_length < sizeof(SubpopulationMetadataRec_PREJSON))
						EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): binary population metadata is not the expected length." << EidosTerminate(nullptr);
					
					tsk_size_t old_metadata_expected_length = sizeof(SubpopulationMetadataRec_PREJSON) + old_metadata->migration_rec_count_ * sizeof(SubpopulationMigrationMetadataRec_PREJSON);
					
					if (old_metadata_length != old_metadata_expected_length)
						EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): binary population metadata is not the expected length." << EidosTerminate(nullptr);
					
					nlohmann::json new_metadata_json = nlohmann::json::object();
					
					double bounds_x0 = old_metadata->bounds_x0_;		// need to use temporaries because some compilers don't like taking a reference inside a packed struct
					double bounds_x1 = old_metadata->bounds_x1_;
					double bounds_y0 = old_metadata->bounds_y0_;
					double bounds_y1 = old_metadata->bounds_y1_;
					double bounds_z0 = old_metadata->bounds_z0_;
					double bounds_z1 = old_metadata->bounds_z1_;
					slim_objectid_t subpopulation_id = old_metadata->subpopulation_id_;
					double selfing_fraction = old_metadata->selfing_fraction_;
					double female_clone_fraction = old_metadata->female_clone_fraction_;
					double male_clone_fraction = old_metadata->male_clone_fraction_;
					double sex_ratio = old_metadata->sex_ratio_;
					
					new_metadata_json["bounds_x0"] = bounds_x0;
					new_metadata_json["bounds_x1"] = bounds_x1;
					new_metadata_json["bounds_y0"] = bounds_y0;
					new_metadata_json["bounds_y1"] = bounds_y1;
					new_metadata_json["bounds_z0"] = bounds_z0;
					new_metadata_json["bounds_z1"] = bounds_z1;
					new_metadata_json["female_cloning_fraction"] = female_clone_fraction;
					new_metadata_json["male_cloning_fraction"] = male_clone_fraction;
					new_metadata_json["selfing_fraction"] = selfing_fraction;
					new_metadata_json["sex_ratio"] = sex_ratio;
					new_metadata_json["slim_id"] = subpopulation_id;
					
					if (old_metadata->migration_rec_count_ > 0)
					{
						SubpopulationMigrationMetadataRec_PREJSON *migration_base_ptr = (SubpopulationMigrationMetadataRec_PREJSON *)(old_metadata + 1);
						nlohmann::json migration_records = nlohmann::json::array();
						
						for (size_t migration_index = 0; migration_index < old_metadata->migration_rec_count_; ++migration_index)
						{
							nlohmann::json migration_record = nlohmann::json::object();
							
							double migration_rate = migration_base_ptr[migration_index].migration_rate_;				// avoid compiler issues with packed structs
							slim_objectid_t source_subpop_id = migration_base_ptr[migration_index].source_subpop_id_;
							
							migration_record["migration_rate"] = migration_rate;
							migration_record["source_subpop"] = source_subpop_id;
							
							migration_records.emplace_back(std::move(migration_record));
						}
						
						new_metadata_json["migration_records"] = std::move(migration_records);
					}
					
					new_metadata_json["name"] = SLiMEidosScript::IDStringWithPrefix('p', old_metadata->subpopulation_id_);
					
					std::string new_metadata_record = new_metadata_json.dump();
					
					new_metadata.append(new_metadata_record);
					new_metadata_offsets[row_index + 1] = new_metadata_offsets[row_index] + new_metadata_record.length();
				}
				else
				{
					// The tskit JSON metadata parser expects a 4-byte "null" value for empty metadata; it can't just be empty.
					// See also WritePopulationTable(), __ConfigureSubpopulationsFromTables() for interacting code.
					new_metadata.append("null");
					new_metadata_offsets[row_index + 1] = new_metadata_offsets[row_index] + 4;
				}
			}
			
			size_t new_metadata_length = new_metadata.length();
			char *new_metadata_buffer = (char *)malloc(new_metadata_length);
			
			if (!new_metadata_buffer)
				EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			memcpy(new_metadata_buffer, new_metadata.c_str(), new_metadata_length);
			
			free(tables_.populations.metadata);
			free(tables_.populations.metadata_offset);
			tables_.populations.metadata = new_metadata_buffer;
			tables_.populations.metadata_length = new_metadata_length;
			tables_.populations.max_metadata_length = new_metadata_length;
			tables_.populations.metadata_offset = new_metadata_offsets;
		}
		
		// replace the metadata schema
		int ret = tsk_population_table_set_metadata_schema(&tables_.populations,
				gSLiM_tsk_population_metadata_schema.c_str(),
				(tsk_size_t)gSLiM_tsk_population_metadata_schema.length());
		if (ret != 0)
			handle_error("tsk_population_table_set_metadata_schema", ret);
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
			EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): the population metadata schema must be JSON, or must match the exact binary schema used by SLiM prior to 3.7." << EidosTerminate(nullptr);
	}
}

// We need a reverse hash to construct the remapped population table
#if EIDOS_ROBIN_HOOD_HASHING
typedef robin_hood::unordered_flat_map<slim_objectid_t, int64_t> SUBPOP_REMAP_REVERSE_HASH;
#elif STD_UNORDERED_MAP_HASHING
typedef std::unordered_map<slim_objectid_t, int64_t> SUBPOP_REMAP_REVERSE_HASH;
#endif

void Species::__RemapSubpopulationIDs(SUBPOP_REMAP_HASH &p_subpop_map, int p_file_version)
{
	// If we have been given a remapping table, this method munges all of the data
	// and metadata in the treeseq tables to accomplish that remapping.  It is gross
	// to have to do this on the raw table data, but we need that data to be corrected
	// so that we can simulate forward from it.  Every subpop id referenced in the
	// tables must be remapped; if a map is given, it must remap everything.  We have
	// to check all metadata carefully, since this remap happens before other checks.
	// Note that __RewriteOrCheckPopulationMetadata() has already fixed pre-JSON metadata.
	// We handle both SLiM metadata and non-SLiM metadata correctly here if we can.
	if (p_subpop_map.size() > 0)
	{
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
			tsk_population_table_t &pop_table = tables_.populations;
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
			ret = tsk_population_table_copy(&tables_.populations, population_table_copy, 0);
			if (ret != 0) handle_error("__RemapSubpopulationIDs tsk_population_table_copy()", ret);
			ret = tsk_population_table_clear(&tables_.populations);
			if (ret != 0) handle_error("__RemapSubpopulationIDs tsk_population_table_clear()", ret);
			
			for (slim_objectid_t remapped_row_index = 0; remapped_row_index < remapped_row_count; ++remapped_row_index)
			{
				auto reverse_iter = subpop_reverse_hash.find(remapped_row_index);
				
				if (reverse_iter == subpop_reverse_hash.end())
				{
					// No remap hash entry for this row index, so it must be an empty row
					tsk_population_id = tsk_population_table_add_row(&tables_.populations, "null", 4);
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
						tsk_population_id = tsk_population_table_add_row(&tables_.populations, "null", 4);
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
							tsk_population_id = tsk_population_table_add_row(&tables_.populations, (char *)metadata_string.c_str(), (uint32_t)metadata_string.length());
						}
						else
						{
							tsk_population_id = tsk_population_table_add_row(&tables_.populations, metadata_char, metadata_length);
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
						tsk_population_id = tsk_population_table_add_row(&tables_.populations, (char *)metadata_string.c_str(), (uint32_t)metadata_string.length());
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
			std::size_t metadata_rec_size = ((p_file_version < 3) ? sizeof(MutationMetadataRec_PRENUC) : sizeof(MutationMetadataRec));
			tsk_mutation_table_t &mut_table = tables_.mutations;
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
					// Here we have to deal with the metadata format, which could be old
					if (p_file_version < 3)
					{
						MutationMetadataRec_PRENUC *prenuc_metadata = (MutationMetadataRec_PRENUC *)metadata_bytes + stack_index;
						slim_objectid_t old_subpop = prenuc_metadata->subpop_index_;
						auto remap_iter = subpop_map.find(old_subpop);
						
						if (remap_iter == subpop_map.end())
							EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_subpop << ") used by the tree sequence data (mutation metadata) was not remapped." << EidosTerminate();
						
						prenuc_metadata->subpop_index_ = remap_iter->second;
					}
					else
					{
						MutationMetadataRec *metadata = (MutationMetadataRec *)metadata_bytes + stack_index;
						slim_objectid_t old_subpop = metadata->subpop_index_;
						auto remap_iter = subpop_map.find(old_subpop);
						
						if (remap_iter == subpop_map.end())
							EIDOS_TERMINATION << "ERROR (Species::__RemapSubpopulationIDs): a subpopulation index (" << old_subpop << ") used by the tree sequence data (mutation metadata) was not remapped." << EidosTerminate();
						
						metadata->subpop_index_ = remap_iter->second;
					}
				}
			}
		}
		
		// Next we remap subpopulation_id_ in the individual metadata, in place
		// Note that __RewriteOldIndividualsMetadata() has already been called,
		// so we only need to worry about the current metadata format
		{
			tsk_individual_table_t &ind_table = tables_.individuals;
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
			tsk_node_table_t &node_table = tables_.nodes;
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
			tsk_migration_table_t &migration_table = tables_.migrations;
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

void Species::__PrepareSubpopulationsFromTables(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap)
{
	// This reads the subpopulation table and creates ts_subpop_info records for the non-empty subpopulations
	// Doing this first allows us to check that individuals are going into subpopulations that we understand
	// The code here is duplicated to some extent in __ConfigureSubpopulationsFromTables(), which finalizes things
	tsk_population_table_t &pop_table = tables_.populations;
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

void Species::__TabulateSubpopulationsFromTreeSequence(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, tsk_treeseq_t *p_ts, SLiMModelType p_file_model_type)
{
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
		
		// check that the individual has exactly two nodes; we are always diploid
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
		tsk_node_table_t &node_table = tables_.nodes;
		tsk_id_t node0 = individual.nodes[0];
		tsk_id_t node1 = individual.nodes[1];
		
		if (((node_table.flags[node0] & TSK_NODE_IS_SAMPLE) == 0) || ((node_table.flags[node1] & TSK_NODE_IS_SAMPLE) == 0))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): nodes for individual are not in-sample; this file cannot be read." << EidosTerminate();
		if ((node_table.individual[node0] != individual.id) || (node_table.individual[node1] != individual.id))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): individual-node inconsistency; this file cannot be read." << EidosTerminate();
		
		size_t node0_metadata_length = node_table.metadata_offset[node0 + 1] - node_table.metadata_offset[node0];
		size_t node1_metadata_length = node_table.metadata_offset[node1 + 1] - node_table.metadata_offset[node1];
		
		if ((node0_metadata_length != sizeof(HaplosomeMetadataRec)) || (node1_metadata_length != sizeof(HaplosomeMetadataRec)))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): unexpected node metadata length; this file cannot be read." << EidosTerminate();
		
		HaplosomeMetadataRec *node0_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node0]);
		HaplosomeMetadataRec *node1_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node1]);
		
		if ((node0_metadata->haplosome_id_ != metadata->pedigree_id_ * 2) || (node1_metadata->haplosome_id_ != metadata->pedigree_id_ * 2 + 1))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): haplosome id mismatch; this file cannot be read." << EidosTerminate();
		
		bool expected_is_null_0 = false, expected_is_null_1 = false;
		ChromosomeType expected_chromosome_type_0 = ChromosomeType::kA_DiploidAutosome;
		ChromosomeType expected_chromosome_type_1 = ChromosomeType::kA_DiploidAutosome;
		
		if (sex_enabled_)
		{
			// NOLINTBEGIN(*-branch-clone) : ok, this is a little weird, but it makes it explicit what happens for males versus females
			// FIXME MULTICHROM here we would be reading in a tree sequence for one chromosome in the model
			// FIXME MULTICHROM firstChromosomeType is a temporary hack
			ChromosomeType firstChromosomeType = Chromosomes()[0]->Type();
			
			if (firstChromosomeType == ChromosomeType::kX_XSexChromosome)
			{
				expected_is_null_0 = (sex == IndividualSex::kMale) ? false : false;
				expected_is_null_1 = (sex == IndividualSex::kMale) ? true : false;
				expected_chromosome_type_0 = (sex == IndividualSex::kMale) ? ChromosomeType::kX_XSexChromosome : ChromosomeType::kX_XSexChromosome;
				expected_chromosome_type_1 = (sex == IndividualSex::kMale) ? ChromosomeType::kNullY_YSexChromosomeWithNull : ChromosomeType::kX_XSexChromosome;
			}
			else if (firstChromosomeType == ChromosomeType::kNullY_YSexChromosomeWithNull)
			{
				expected_is_null_0 = (sex == IndividualSex::kMale) ? true : true;
				expected_is_null_1 = (sex == IndividualSex::kMale) ? false : true;
				expected_chromosome_type_0 = (sex == IndividualSex::kMale) ? ChromosomeType::kX_XSexChromosome : ChromosomeType::kX_XSexChromosome;
				expected_chromosome_type_1 = (sex == IndividualSex::kMale) ? ChromosomeType::kNullY_YSexChromosomeWithNull : ChromosomeType::kX_XSexChromosome;
			}
			// NOLINTEND(*-branch-clone)
		}
		
		// BCH 9/27/2021: Null haplosomes are now allowed to occur arbitrarily in nonWF models, as long as they aren't sex-chromosome models
		if (node0_metadata->is_null_ != expected_is_null_0)
		{
			if ((model_type_ == SLiMModelType::kModelTypeNonWF) && (expected_chromosome_type_0 == ChromosomeType::kA_DiploidAutosome))
				;
			else
				EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): node is_null unexpected; this file cannot be read." << EidosTerminate();
		}
		if (node1_metadata->is_null_ != expected_is_null_1)
		{
			if ((model_type_ == SLiMModelType::kModelTypeNonWF) && (expected_chromosome_type_1 == ChromosomeType::kA_DiploidAutosome))
				;
			else
				EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): node is_null unexpected; this file cannot be read." << EidosTerminate();
		}
		if ((node0_metadata->type_ != expected_chromosome_type_0) || (node1_metadata->type_ != expected_chromosome_type_1))
			EIDOS_TERMINATION << "ERROR (Species::__TabulateSubpopulationsFromTreeSequence): node type unexpected; this file cannot be read." << EidosTerminate();
	}
}

void Species::__CreateSubpopulationsFromTabulation(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Haplosome *> &p_nodeToHaplosomeMap)
{
	// We will keep track of all pedigree IDs used, and check at the end that they do not collide; faster than checking as we go
	// This could be done with a hash table, but I imagine that would be slower until the number of individuals becomes very large
	// Also, I'm a bit nervous about putting a large number of consecutive integers into a hash table, re: edge-case performance
	std::vector<slim_pedigreeid_t> pedigree_id_check;
	
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
				
				individual->haplosomes_[0]->tsk_node_id_ = node_id_0;
				individual->haplosomes_[1]->tsk_node_id_ = node_id_1;
				
				p_nodeToHaplosomeMap.emplace(node_id_0, individual->haplosomes_[0]);
				p_nodeToHaplosomeMap.emplace(node_id_1, individual->haplosomes_[1]);
				
				slim_pedigreeid_t pedigree_id = subpop_info.pedigreeID_[tabulation_index];
				individual->SetPedigreeID(pedigree_id);
				pedigree_id_check.emplace_back(pedigree_id);	// we will test for collisions below
				gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);

				individual->SetParentPedigreeID(subpop_info.pedigreeP1_[tabulation_index], subpop_info.pedigreeP2_[tabulation_index]);
				
				uint32_t flags = subpop_info.flags_[tabulation_index];
				if (flags & SLIM_INDIVIDUAL_METADATA_MIGRATED)
					individual->migrant_ = true;
				
				individual->haplosomes_[0]->haplosome_id_ = pedigree_id * 2;
				individual->haplosomes_[1]->haplosome_id_ = pedigree_id * 2 + 1;
				
				individual->age_ = subpop_info.age_[tabulation_index];
				individual->spatial_x_ = subpop_info.spatial_x_[tabulation_index];
				individual->spatial_y_ = subpop_info.spatial_y_[tabulation_index];
				individual->spatial_z_ = subpop_info.spatial_z_[tabulation_index];
				
				// check the referenced nodes; right now this is not essential for re-creating the saved state, but is just a crosscheck
				// here we crosscheck the node information against the realized values in the haplosomes of the individual
				tsk_node_table_t &node_table = tables_.nodes;
				size_t node0_metadata_length = node_table.metadata_offset[node_id_0 + 1] - node_table.metadata_offset[node_id_0];
				size_t node1_metadata_length = node_table.metadata_offset[node_id_1 + 1] - node_table.metadata_offset[node_id_1];
				
				if ((node0_metadata_length != sizeof(HaplosomeMetadataRec)) || (node1_metadata_length != sizeof(HaplosomeMetadataRec)))
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): unexpected node metadata length; this file cannot be read." << EidosTerminate();
				
				HaplosomeMetadataRec *node0_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_0]);
				HaplosomeMetadataRec *node1_metadata = (HaplosomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_1]);
				Haplosome *haplosome0 = individual->haplosomes_[0], *haplosome1 = individual->haplosomes_[1];
				
				if ((node0_metadata->haplosome_id_ != haplosome0->haplosome_id_) || (node1_metadata->haplosome_id_ != haplosome1->haplosome_id_))
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome id mismatch; this file cannot be read." << EidosTerminate();
				// BCH 9/27/2021: Null haplosomes are now allowed to occur arbitrarily in nonWF models, as long as they aren't sex-chromosome models
				if (node0_metadata->is_null_ != haplosome0->IsNull())
				{
					if (node0_metadata->is_null_ && (model_type_ == SLiMModelType::kModelTypeNonWF) && (node0_metadata->type_ == ChromosomeType::kA_DiploidAutosome))
					{
						haplosome0->MakeNull();
						new_subpop->has_null_haplosomes_ = true;
					}
					else
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome null mismatch; this file cannot be read." << EidosTerminate();
				}
				if (node1_metadata->is_null_ != haplosome1->IsNull())
				{
					if (node1_metadata->is_null_ && (model_type_ == SLiMModelType::kModelTypeNonWF) && (node1_metadata->type_ == ChromosomeType::kA_DiploidAutosome))
					{
						haplosome1->MakeNull();
						new_subpop->has_null_haplosomes_ = true;
					}
					else
						EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome null mismatch; this file cannot be read." << EidosTerminate();
				}
				if ((node0_metadata->type_ != haplosome0->AssociatedChromosome()->Type()) ||
					(node1_metadata->type_ != haplosome1->AssociatedChromosome()->Type()))	// FIXME MULTICHROM this should test index, not type
					EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): node-haplosome type mismatch; this file cannot be read." << EidosTerminate();
			}
		}
	}
	
	// Check for individual pedigree ID collisions by sorting and looking for duplicates
	std::sort(pedigree_id_check.begin(), pedigree_id_check.end());
	const auto duplicate = std::adjacent_find(pedigree_id_check.begin(), pedigree_id_check.end());
	
	if (duplicate != pedigree_id_check.end())
		EIDOS_TERMINATION << "ERROR (Species::__CreateSubpopulationsFromTabulation): the individual pedigree ID value " << *duplicate << " was used more than once; individual pedigree IDs must be unique." << EidosTerminate();
}

void Species::__ConfigureSubpopulationsFromTables(EidosInterpreter *p_interpreter)
{
	tsk_population_table_t &pop_table = tables_.populations;
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
			
			// If a nonWF model an empty subpop is legal, so create it without recording
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

typedef struct ts_mut_info {
	slim_position_t position;
	MutationMetadataRec metadata;
	slim_refcount_t ref_count;
} ts_mut_info;

void Species::__TabulateMutationsFromTables(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, int p_file_version)
{
	std::size_t metadata_rec_size = ((p_file_version < 3) ? sizeof(MutationMetadataRec_PRENUC) : sizeof(MutationMetadataRec));
	tsk_mutation_table_t &mut_table = tables_.mutations;
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
		double position_double = tables_.sites.position[site_id];
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
			
			// This method handles the fact that a file version of 2 or below will not contain a nucleotide field for its mutation metadata.
			// We hide this fact from the rest of the initialization code; ts_mut_info uses MutationMetadataRec, and we fill in a value of
			// -1 for the nucleotide_ field if we are using MutationMetadataRec_PRENUC due to the file version.
			if (p_file_version < 3)
			{
				MutationMetadataRec_PRENUC *prenuc_metadata = (MutationMetadataRec_PRENUC *)metadata_vec + stack_index;
				
				mut_info->metadata.mutation_type_id_ = prenuc_metadata->mutation_type_id_;
				mut_info->metadata.selection_coeff_ = prenuc_metadata->selection_coeff_;
				mut_info->metadata.subpop_index_ = prenuc_metadata->subpop_index_;
				mut_info->metadata.origin_tick_ = prenuc_metadata->origin_tick_;
				mut_info->metadata.nucleotide_ = -1;
			}
			else
			{
				MutationMetadataRec *metadata = (MutationMetadataRec *)metadata_vec + stack_index;
				
				mut_info->metadata = *metadata;
			}
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
			indexToHaplosomeMap.emplace_back(sample_nodeToHaplosome_iter->second);
		else
			indexToHaplosomeMap.emplace_back(nullptr);	// this sample is not extant; no corresponding haplosome
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

void Species::__CreateMutationsFromTabulation(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutInfoMap, std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap)
{
	// count the number of non-null haplosomes there are; this is the count that would represent fixation
	int haplosome_count_per_individual = HaplosomeCountPerIndividual();
	slim_refcount_t fixation_count = 0;
	
	for (auto pop_iter : population_.subpops_)
	{
		Subpopulation *subpop = pop_iter.second;
		
		for (Individual *ind : subpop->parent_individuals_)
		{
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				if (!haplosome->IsNull())
					fixation_count++;
			}
		}
	}
	
	// instantiate mutations
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
		
		// a mutation might not be refered by any extant haplosome; it might be present in an ancestral node,
		// but have been lost in all descendants, in which we do not need to instantiate it
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
			// FIXME MULTICHROM need a chromosome index from somewhere
			Substitution *sub = new Substitution(mutation_id, mutation_type_ptr, /* chromosome index */ 0, position, metadata.selection_coeff_, metadata.subpop_index_, metadata.origin_tick_, community_.Tick(), metadata.nucleotide_);
			
			population_.treeseq_substitutions_map_.emplace(position, sub);
			population_.substitutions_.emplace_back(sub);
			
			// add -1 to our local map, so we know there's an entry but we also know it's a substitution
			p_mutIndexMap[mutation_id] = -1;
		}
		else
		{
			// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
			MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
			
			Mutation *new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_id, mutation_type_ptr, TheChromosome().Index(), position, metadata.selection_coeff_, metadata.subpop_index_, metadata.origin_tick_, metadata.nucleotide_);
			
			// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
			p_mutIndexMap[mutation_id] = new_mut_index;
			population_.MutationRegistryAdd(new_mut);
			
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
			if (population_.keeping_muttype_registries_)
				EIDOS_TERMINATION << "ERROR (Species::__CreateMutationsFromTabulation): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
		}
		
		// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_pure_neutral_DFE_
		if (metadata.selection_coeff_ != 0.0)
		{
			pure_neutral_ = false;
			mutation_type_ptr->all_pure_neutral_DFE_ = false;
		}
	}
}

void Species::__AddMutationsFromTreeSequenceToHaplosomes(std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, std::unordered_map<tsk_id_t, Haplosome *> p_nodeToHaplosomeMap, tsk_treeseq_t *p_ts)
{
	Chromosome &chromosome = TheChromosome();
	
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
			// this sample is not extant; no corresponding haplosome, so record nullptr
			indexToHaplosomeMap.emplace_back(nullptr);
		}
	}
	
	// add mutations to haplosomes by looping through variants
#ifndef _OPENMP
	MutationRunContext &mutrun_context = chromosome.ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
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
		}
	}
	
	// free
	ret = tsk_variant_free(variant);
	if (ret != 0) handle_error("__AddMutationsFromTreeSequenceToHaplosomes tsk_variant_free()", ret);
	free(variant);
}

void Species::__CheckNodePedigreeIDs(EidosInterpreter *p_interpreter)
{
	// Make sure our next pedigree ID is safe; right now it only accounts for pedigree IDs used by individuals, but maybe there
	// could be nodes in the node table with haplosome pedigree IDs greater than those in use by individuals, in nonWF models.
	// See https://github.com/MesserLab/SLiM/pull/420 for an example model that does this very easily.
	
	// Also, check for duplicate pedigree IDs, just in case.  __CreateSubpopulationsFromTabulation() does this for individual
	// pedigree IDs; we do it for node pedigree IDs.  I decided to use a vector with std::sort() to check even though it is
	// O(n log n), rather than a hash table for O(n), because I'm nervous about hitting a bad edge case with the hash table
	// due to the nature of the values being inserted.  Shouldn't be a big deal in the grand scheme of things.
	tsk_node_table_t &node_table = tables_.nodes;
	tsk_size_t node_count = node_table.num_rows;
	std::vector<slim_haplosomeid_t> haplosome_id_check;
	
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
			slim_haplosomeid_t haplosome_id = metadata_rec->haplosome_id_;
			
			haplosome_id_check.emplace_back(haplosome_id);	// we will test for collisions below
			
			slim_pedigreeid_t pedigree_id = haplosome_id / 2;			// rounds down to integer
			
			if (pedigree_id >= gSLiM_next_pedigree_id)
			{
				static bool been_here = false;
				
				if (!been_here)
				{
					// decided to keep this as a warning; this circumstance is not necessarily pathological, but it probably usually is...
					p_interpreter->ErrorOutputStream() << "#WARNING (Species::__CheckNodePedigreeIDs): in reading the tree sequence, a node was encountered with a haplosome pedigree ID that was (after division by 2) greater than the largest individual pedigree ID in the tree sequence.  This is not necessarily an error, but it is highly unusual, and could indicate that the tree sequence file is corrupted.  It may happen due to external manipulations of a tree sequence, or perhaps if an unsimplified tree sequence produced by SLiM is being loaded." << std::endl;
					been_here = true;
				}
				
				gSLiM_next_pedigree_id = pedigree_id + 1;
			}
		}
	}
	
	// Check for haplosome pedigree ID collisions by sorting and looking for duplicates
	std::sort(haplosome_id_check.begin(), haplosome_id_check.end());
	const auto duplicate = std::adjacent_find(haplosome_id_check.begin(), haplosome_id_check.end());
	
	if (duplicate != haplosome_id_check.end())
		EIDOS_TERMINATION << "ERROR (Species::__CheckNodePedigreeIDs): the haplosome pedigree ID value " << *duplicate << " was used more than once; haplosome pedigree IDs must be unique." << EidosTerminate();
}

void Species::_InstantiateSLiMObjectsFromTables(EidosInterpreter *p_interpreter, slim_tick_t p_metadata_tick, slim_tick_t p_metadata_cycle, SLiMModelType p_file_model_type, int p_file_version, SUBPOP_REMAP_HASH &p_subpop_map)
{
	Chromosome &chromosome = TheChromosome();
	
	// set the tick and cycle from the provenance data
	if (tables_.sequence_length != chromosome.last_position_ + 1)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): chromosome length in loaded population (" << tables_.sequence_length << ") does not match the configured chromosome length (" << (chromosome.last_position_ + 1) << ")." << EidosTerminate();
	
	community_.SetTick(p_metadata_tick);
	SetCycle(p_metadata_cycle);
	
	// rebase the times in the nodes to be in SLiM-land; see WriteTreeSequence for the inverse operation
	// BCH 4/4/2019: switched to using tree_seq_tick_ to avoid a parent/child timestamp conflict
	// This makes sense; as far as tree-seq recording is concerned, tree_seq_tick_ is the time counter
	slim_tick_t time_adjustment = community_.tree_seq_tick_;
	
	for (size_t node_index = 0; node_index < tables_.nodes.num_rows; ++node_index)
		tables_.nodes.time[node_index] -= time_adjustment;

	for (size_t mut_index = 0; mut_index < tables_.mutations.num_rows; ++mut_index)
		tables_.mutations.time[mut_index] -= time_adjustment;
	
	// rewrite the incoming tree-seq information in various ways
	__RewriteOldIndividualsMetadata(p_file_version);
	__RewriteOrCheckPopulationMetadata();
	__RemapSubpopulationIDs(p_subpop_map, p_file_version);
	
	// allocate and set up the tree_sequence object
	// note that this tree sequence is based upon whatever sample the file was saved with, and may contain in-sample individuals
	// that are not presently alive, so we have to tread carefully; the actually alive individuals are flagged with 
	// SLIM_TSK_INDIVIDUAL_ALIVE in the individuals table (there may also be remembered and retained individuals in there too)
	tsk_treeseq_t *ts;
	int ret = 0;
	
	ts = (tsk_treeseq_t *)malloc(sizeof(tsk_treeseq_t));
	if (!ts)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	ret = tsk_treeseq_init(ts, &tables_, TSK_TS_INIT_BUILD_INDEXES);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_init()", ret);
	
	std::unordered_map<tsk_id_t, Haplosome *> nodeToHaplosomeMap;
	
	{
		std::unordered_map<slim_objectid_t, ts_subpop_info> subpopInfoMap;
		
		__PrepareSubpopulationsFromTables(subpopInfoMap);
		__TabulateSubpopulationsFromTreeSequence(subpopInfoMap, ts, p_file_model_type);
		__CreateSubpopulationsFromTabulation(subpopInfoMap, p_interpreter, nodeToHaplosomeMap);
		__ConfigureSubpopulationsFromTables(p_interpreter);
	}
	
	std::unordered_map<slim_mutationid_t, MutationIndex> mutIndexMap;
	
	{
		std::unordered_map<slim_mutationid_t, ts_mut_info> mutInfoMap;
		
		__TabulateMutationsFromTables(mutInfoMap, p_file_version);
		__TallyMutationReferencesWithTreeSequence(mutInfoMap, nodeToHaplosomeMap, ts);
		__CreateMutationsFromTabulation(mutInfoMap, mutIndexMap);
	}
	
	__AddMutationsFromTreeSequenceToHaplosomes(mutIndexMap, nodeToHaplosomeMap, ts);
	
	ret = tsk_treeseq_free(ts);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_free()", ret);
	free(ts);
	
	// Ensure that the next pedigree ID used will not cause a collision with any existing nodes in the node table,
	// and that there are no duplicate node pedigree IDs in the input file (whether in use or not).
	__CheckNodePedigreeIDs(p_interpreter);
	
	// Set up the remembered haplosomes by looking though the list of nodes and their individuals
	// FIXME MULTICHROM this doubtless needs work
	if (remembered_nodes_.size() != 0)
		EIDOS_TERMINATION << "ERROR (Species::_InstantiateSLiMObjectsFromTables): (internal error) remembered_nodes_ is not empty." << EidosTerminate();
	
	for (tsk_id_t j = 0; (size_t) j < tables_.nodes.num_rows; j++)
	{
		tsk_id_t ind = tables_.nodes.individual[j];
		if (ind >=0)
		{
			uint32_t flags = tables_.individuals.flags[ind];
			if (flags & SLIM_TSK_INDIVIDUAL_REMEMBERED)
				remembered_nodes_.emplace_back(j);
		}
	}
	assert(remembered_nodes_.size() % 2 == 0);

	// Sort them to match the order of the individual table, so that they satisfy
	// the invariants asserted in Species::AddIndividualsToTable(); see the comments there
	std::sort(remembered_nodes_.begin(), remembered_nodes_.end(), [this](tsk_id_t l, tsk_id_t r) {
		tsk_id_t l_ind = tables_.nodes.individual[l];
		tsk_id_t r_ind = tables_.nodes.individual[r];
		if (l_ind != r_ind)
			return l_ind < r_ind;
		return l < r;
	});
	
	// Clear ALIVE flags
	FixAliveIndividuals(&tables_);
	
	// Remove individuals that are not remembered or retained
	std::vector<tsk_id_t> individual_map;
	for (tsk_id_t j = 0; (size_t) j < tables_.individuals.num_rows; j++)
	{
		uint32_t flags = tables_.individuals.flags[j];
		if (flags & (SLIM_TSK_INDIVIDUAL_REMEMBERED | SLIM_TSK_INDIVIDUAL_RETAINED))
			individual_map.emplace_back(j);
	}
	ReorderIndividualTable(&tables_, individual_map, false);
	BuildTabledIndividualsHash(&tables_, &tabled_individuals_hash_);
	
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
	
	// Reset our last coalescence state; we don't know whether we're coalesced now or not
	last_coalescence_state_ = false;
}

slim_tick_t Species::_InitializePopulationFromTskitBinaryFile(const char *p_file, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_map)
{
	Chromosome &chromosome = TheChromosome();
	
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::_InitializePopulationFromTskitBinaryFile(): SLiM global state read");
	
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
	
	ret = tsk_table_collection_load(&tables_, p_file, TSK_LOAD_SKIP_REFERENCE_SEQUENCE);	// we load the ref seq ourselves; see below
	if (ret != 0) handle_error("tsk_table_collection_load", ret);
	
	tables_initialized_ = true;
	
	// BCH 4/25/2019: if indexes are present on tables_ we want to drop them; they are synced up
	// with the edge table, but we plan to modify the edge table so they will become invalid anyway, and
	// then they may cause a crash because of their unsynced-ness; see tskit issue #179
	ret = tsk_table_collection_drop_index(&tables_, 0);
	if (ret != 0) handle_error("tsk_table_collection_drop_index", ret);

	RecordTablePosition();
	
	// read in the tree sequence metadata first so we have file version information and check for SLiM compliance and such
	slim_tick_t metadata_tick;
	slim_tick_t metadata_cycle;
	SLiMModelType file_model_type;
	int file_version;
	
	ReadTreeSequenceMetadata(&tables_, &metadata_tick, &metadata_cycle, &file_model_type, &file_version);
	
	// convert ASCII derived-state data, which is the required format on disk, back to our in-memory binary format
	DerivedStatesFromAscii(&tables_);
	
	// in nucleotide-based models, read the ancestral sequence; we do this ourselves, directly from kastore, to avoid having
	// tskit make a full ASCII copy of the reference sequences from kastore into tables_; see tsk_table_collection_load() above
	if (nucleotide_based_)
	{
		char *buffer;				// kastore needs to provide us with a memory location from which to read the data
		std::size_t buffer_length;	// kastore needs to provide us with the length, in bytes, of the buffer
		kastore_t store;

		ret = kastore_open(&store, p_file, "r", 0);
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
		if (buffer_length != chromosome.AncestralSequence()->size())
			EIDOS_TERMINATION << "ERROR (Species::_InitializePopulationFromTskitBinaryFile): the reference nucleotide sequence length does not match the model." << EidosTerminate();
		
		chromosome.AncestralSequence()->ReadNucleotidesFromBuffer(buffer);
		
		// buffer is owned by kastore and is freed by closing the store
		kastore_close(&store);
	}

	// make the corresponding SLiM objects
	_InstantiateSLiMObjectsFromTables(p_interpreter, metadata_tick, metadata_cycle, file_model_type, file_version, p_subpop_map);
	
	// if tree-seq is not on, throw away the tree-seq data structures now that we're done loading SLiM state
	if (!was_recording_tree)
	{
		FreeTreeSequence();
		recording_tree_ = false;
		recording_mutations_ = false;
	}
	
	return metadata_tick;
}
#endif	// INTERIM_TREESEQ_DISABLE

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





























































