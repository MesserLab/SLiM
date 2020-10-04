//
//  slim_sim.cpp
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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


#include "slim_sim.h"
#include "slim_functions.h"
#include "eidos_test.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"
#include "individual.h"
#include "polymorphism.h"
#include "subpopulation.h"

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
static const char *SLIM_TREES_FILE_VERSION_HASH = "0.4";        // SLiM 3.4.x, with the new model_hash key in provenance
static const char *SLIM_TREES_FILE_VERSION = "0.5";				// SLiM 3.5.x onward, with information in metadata instead of provenance

#pragma mark -
#pragma mark SLiMSim
#pragma mark -

SLiMSim::SLiMSim(std::istream &p_infile) : chromosome_(this), population_(*this), self_symbol_(gID_sim, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMSim_Class))), x_experiments_enabled_(false)
{
	// set up the symbol table we will use for all of our constants; we use the external hash table design
	// BCH 1/18/2018: I looked into telling this table to use the external unordered_map from the start, but testing indicates
	// that that is actually a bit slower.  I suspect it crosses over for models with more SLiM symbols; but EidosSymbolTable
	// crosses over to the external table anyway when more symbols are used, so it shouldn't be a big deal.
	simulation_constants_ = new EidosSymbolTable(EidosSymbolTableType::kContextConstantsTable, gEidosConstantsSymbolTable);
	
	// set up the function map with the base Eidos functions plus zero-gen functions, since we're in an initial state
	simulation_functions_ = *EidosInterpreter::BuiltInFunctionMap();
	AddZeroGenerationFunctionsToMap(simulation_functions_);
	AddSLiMFunctionsToMap(simulation_functions_);
	
	// read all configuration information from the input file
	p_infile.clear();
	p_infile.seekg(0, std::fstream::beg);
	
	try {
		InitializeFromFile(p_infile);
	}
	catch (...) {
		// try to clean up what we've allocated so far
		delete simulation_constants_;
		simulation_constants_ = nullptr;
		
		simulation_functions_.clear();
		
		throw;
	}
}

SLiMSim::~SLiMSim(void)
{
	//EIDOS_ERRSTREAM << "SLiMSim::~SLiMSim" << std::endl;
	
	population_.RemoveAllSubpopulationInfo();
	
	delete simulation_constants_;
	simulation_constants_ = nullptr;
	
	simulation_functions_.clear();
	
	for (auto mutation_type : mutation_types_)
		delete mutation_type.second;
	mutation_types_.clear();
	
	for (auto genomic_element_type : genomic_element_types_)
		delete genomic_element_type.second;
	genomic_element_types_.clear();
	
	for (auto interaction_type : interaction_types_)
		delete interaction_type.second;
	interaction_types_.clear();
	
	for (auto script_block : script_blocks_)
		delete script_block;
	script_blocks_.clear();
	
	// All the script blocks that refer to the script are now gone
	delete script_;
	script_ = nullptr;
	
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
	
	// TREE SEQUENCE RECORDING
	if (RecordingTreeSequence())
		FreeTreeSequence();
}

void SLiMSim::InitializeRNGFromSeed(unsigned long int *p_override_seed_ptr)
{
	// track the random number seed given, if there is one
	unsigned long int rng_seed = (p_override_seed_ptr ? *p_override_seed_ptr : Eidos_GenerateSeedFromPIDAndTime());
	
	Eidos_InitializeRNG();
	Eidos_SetRNGSeed(rng_seed);
	
	if (SLiM_verbosity_level >= 1)
		SLIM_OUTSTREAM << "// Initial random seed:\n" << rng_seed << "\n" << std::endl;
	
	// remember the original seed for .trees provenance
	original_seed_ = rng_seed;
}

void SLiMSim::InitializeFromFile(std::istream &p_infile)
{
	// Reset error position indicators used by SLiMgui
	EidosScript::ClearErrorPosition();
	
	// Read in the file; going through stringstream is fast...
	std::stringstream buffer;
	
	buffer << p_infile.rdbuf();
	
	// Tokenize and parse
	// BCH 5/1/2019: Note that this script_ variable may leak if tokenization/parsing raises below, because this method
	// is called while the SLiMSim constructor is still in progress, so the destructor is not called to clean up.  But
	// we can't actually clean up this variable, because it is used by SLiMAssertScriptRaise() to diagnose where the raise
	// occurred in the user's script; we'd have to redesign that code to fix this leak.  So be it.  It's not a large leak.
	script_ = new SLiMEidosScript(buffer.str());
	
	// Set up top-level error-reporting info
	gEidosCurrentScript = script_;
	gEidosExecutingRuntimeScript = false;
	
	script_->Tokenize();
	script_->ParseSLiMFileToAST();
	
	// Extract SLiMEidosBlocks from the parse tree
	const EidosASTNode *root_node = script_->AST();
	
	for (EidosASTNode *script_block_node : root_node->children_)
	{
		SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_block_node);
		
		AddScriptBlock(new_script_block, nullptr, new_script_block->root_node_->children_[0]->token_);
	}
	
	// Reset error position indicators used by SLiMgui
	EidosScript::ClearErrorPosition();
	
	// Zero out error-reporting info so raises elsewhere don't get attributed to this script
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

// get one line of input, sanitizing by removing comments and whitespace; used only by SLiMSim::InitializePopulationFromTextFile
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

SLiMFileFormat SLiMSim::FormatOfPopulationFile(const std::string &p_file_string)
{
	if (p_file_string.length())
	{
		// p_file should have had its trailing slash stripped already, and a leading ~ should have been resolved
		// we will check those assumptions here for safety...
		if (p_file_string[0] == '~')
			EIDOS_TERMINATION << "ERROR (SLiMSim::FormatOfPopulationFile): (internal error) leading ~ in path was not resolved." << EidosTerminate();
		if (p_file_string.back() == '/')
			EIDOS_TERMINATION << "ERROR (SLiMSim::FormatOfPopulationFile): (internal error) trailing / in path was not stripped." << EidosTerminate();
		
		// First determine if the path is for a file or a directory
		const char *file_cstr = p_file_string.c_str();
		struct stat statbuf;
		
		if (stat(file_cstr, &statbuf) != 0)
			return SLiMFileFormat::kFileNotFound;
		
		if (S_ISDIR(statbuf.st_mode))
		{
			// The path is for a whole directory.  The only file format we recognize that is directory-based is the
			// tskit text (i.e. non-binary) format, which requires files with specific names inside; let's check.
			// The files should be named EdgeTable.txt, NodeTable.txt, SiteTable.txt, etc.; we require all files to
			// be present, for simplicity.
			std::string NodeFileName = p_file_string + "/NodeTable.txt";
			std::string EdgeFileName = p_file_string + "/EdgeTable.txt";
			std::string SiteFileName = p_file_string + "/SiteTable.txt";
			std::string MutationFileName = p_file_string + "/MutationTable.txt";
			std::string IndividualFileName = p_file_string + "/IndividualTable.txt";
			std::string PopulationFileName = p_file_string + "/PopulationTable.txt";
			std::string ProvenanceFileName = p_file_string + "/ProvenanceTable.txt";
			
			if (stat(NodeFileName.c_str(), &statbuf) != 0)			return SLiMFileFormat::kFormatUnrecognized;
			if (!S_ISREG(statbuf.st_mode))							return SLiMFileFormat::kFormatUnrecognized;
			
			if (stat(EdgeFileName.c_str(), &statbuf) != 0)			return SLiMFileFormat::kFormatUnrecognized;
			if (!S_ISREG(statbuf.st_mode))							return SLiMFileFormat::kFormatUnrecognized;
			
			if (stat(SiteFileName.c_str(), &statbuf) != 0)			return SLiMFileFormat::kFormatUnrecognized;
			if (!S_ISREG(statbuf.st_mode))							return SLiMFileFormat::kFormatUnrecognized;
			
			if (stat(MutationFileName.c_str(), &statbuf) != 0)		return SLiMFileFormat::kFormatUnrecognized;
			if (!S_ISREG(statbuf.st_mode))							return SLiMFileFormat::kFormatUnrecognized;
			
			if (stat(IndividualFileName.c_str(), &statbuf) != 0)	return SLiMFileFormat::kFormatUnrecognized;
			if (!S_ISREG(statbuf.st_mode))							return SLiMFileFormat::kFormatUnrecognized;
			
			if (stat(PopulationFileName.c_str(), &statbuf) != 0)	return SLiMFileFormat::kFormatUnrecognized;
			if (!S_ISREG(statbuf.st_mode))							return SLiMFileFormat::kFormatUnrecognized;
			
			if (stat(ProvenanceFileName.c_str(), &statbuf) != 0)	return SLiMFileFormat::kFormatUnrecognized;
			if (!S_ISREG(statbuf.st_mode))							return SLiMFileFormat::kFormatUnrecognized;
			
			return SLiMFileFormat::kFormatTskitText;
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

slim_generation_t SLiMSim::InitializePopulationFromFile(const std::string &p_file_string, EidosInterpreter *p_interpreter)
{
	SLiMFileFormat file_format = FormatOfPopulationFile(p_file_string);
	
	if (file_format == SLiMFileFormat::kFileNotFound)
		EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): initialization file does not exist or is empty." << EidosTerminate();
	if (file_format == SLiMFileFormat::kFormatUnrecognized)
		EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): initialization file is invalid." << EidosTerminate();
	
	// first we clear out all variables of type Subpopulation etc. from the symbol table; they will all be invalid momentarily
	// note that we do this not only in our constants table, but in the user's variables as well; we can leave no stone unturned
	// FIXME: Note that we presently have no way of clearing out EidosScribe/SLiMgui references (the variable browser, in particular),
	// and so EidosConsoleWindowController has to do an ugly and only partly effective hack to work around this issue.
	if (p_interpreter)
	{
		EidosSymbolTable &symbols = p_interpreter->SymbolTable();
		std::vector<std::string> all_symbols = symbols.AllSymbols();
		std::vector<EidosGlobalStringID> symbols_to_remove;
		
		for (std::string symbol_name : all_symbols)
		{
			EidosGlobalStringID symbol_ID = Eidos_GlobalStringIDForString(symbol_name);
			EidosValue_SP symbol_value = symbols.GetValueOrRaiseForSymbol(symbol_ID);
			
			if (symbol_value->Type() == EidosValueType::kValueObject)
			{
				const EidosObjectClass *symbol_class = static_pointer_cast<EidosValue_Object>(symbol_value)->Class();
				
				if ((symbol_class == gSLiM_Subpopulation_Class) || (symbol_class == gSLiM_Genome_Class) || (symbol_class == gSLiM_Individual_Class) || (symbol_class == gSLiM_Mutation_Class) || (symbol_class == gSLiM_Substitution_Class))
					symbols_to_remove.emplace_back(symbol_ID);
			}
		}
		
		for (EidosGlobalStringID symbol_ID : symbols_to_remove)
			symbols.RemoveConstantForSymbol(symbol_ID);
	}
	
	// invalidate interactions, since any cached interaction data depends on the subpopulations and individuals
    for (auto iter : interaction_types_)
		iter.second->Invalidate();
	
	// then we dispose of all existing subpopulations, mutations, etc.
	population_.RemoveAllSubpopulationInfo();
	
	const char *file_cstr = p_file_string.c_str();
	slim_generation_t new_generation = 0;
	
	// Read in the file.  The SLiM file-reading methods are not tree-sequence-aware, so we bracket them
	// with calls that fix the tree sequence recording state around them.  The treeSeq output methods
	// are of course treeSeq-aware, so we don't need to do that for them.
	if ((file_format == SLiMFileFormat::kFormatSLiMText) || (file_format == SLiMFileFormat::kFormatSLiMBinary))
	{
		// TREE SEQUENCE RECORDING
		if (RecordingTreeSequence())
		{
			FreeTreeSequence();
			AllocateTreeSequenceTables();
		}
		
		if (file_format == SLiMFileFormat::kFormatSLiMText)
			new_generation = _InitializePopulationFromTextFile(file_cstr, p_interpreter);
		else if (file_format == SLiMFileFormat::kFormatSLiMBinary)
			new_generation = _InitializePopulationFromBinaryFile(file_cstr, p_interpreter);
		
		// TREE SEQUENCE RECORDING
		if (RecordingTreeSequence())
		{
			// set up all of the mutations we just read in with the tree-seq recording code
			RecordAllDerivedStatesFromSLiM();
			
			// reset our tree-seq auto-simplification interval so we don't simplify immediately
			simplify_elapsed_ = 0;
			
			// reset our last coalescence state; we don't know whether we're coalesced now or not
			last_coalescence_state_ = false;
		}
	}
	else if (file_format == SLiMFileFormat::kFormatTskitText)
		new_generation = _InitializePopulationFromTskitTextFile(file_cstr, p_interpreter);
	else if (file_format == SLiMFileFormat::kFormatTskitBinary_kastore)
		new_generation = _InitializePopulationFromTskitBinaryFile(file_cstr, p_interpreter);
	else if (file_format == SLiMFileFormat::kFormatTskitBinary_HDF5)
		EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): msprime HDF5 binary files are not supported; that file format has been superseded by kastore." << EidosTerminate();
	else
		EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): unrecognized format code." << EidosTerminate();
	
	return new_generation;
}

slim_generation_t SLiMSim::_InitializePopulationFromTextFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	slim_generation_t file_generation;
	std::map<slim_polymorphismid_t,MutationIndex> mutations;
	std::string line, sub; 
	std::ifstream infile(p_file);
	int age_output_count = 0;
	bool has_individual_pedigree_IDs = false;
	
	if (!infile.is_open())
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): could not open initialization file." << EidosTerminate();
	
	// Parse the first line, to get the generation
	{
		GetInputLine(infile, line);
	
		std::istringstream iss(line);
		
		iss >> sub;		// #OUT:
		
		iss >> sub;		// generation
		int64_t generation_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		file_generation = SLiMCastToGenerationTypeOrRaise(generation_long);
	}
	
	// As of SLiM 2.1, we change the generation as a side effect of loading; otherwise we can't correctly update our state here!
	// As of SLiM 3, we set the generation up here, before making any individuals, because we need it to be correct for the tree-seq recording code.
	SetGeneration(file_generation);
	
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
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): unrecognized version." << EidosTerminate();
			
			continue;
		}
		
		if (line.find("Populations") != std::string::npos)
			break;
	}
	
	if (age_output_count && (ModelType() == SLiMModelType::kModelTypeWF))
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): age information is present but the simulation is using a WF model." << EidosTerminate();
	if (!age_output_count && (ModelType() == SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): age information is not present but the simulation is using a nonWF model; age information must be included." << EidosTerminate();
	
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
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_index, subpop_size, sex_ratio);
		
		// define a new Eidos variable to refer to the new subpopulation
		EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
		
		if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): new subpopulation symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
		
		simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	}
	
	// Now we are in the Mutations section; read and instantiate all mutations and add them to our map and to the registry
	while (!infile.eof()) 
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Genomes") != std::string::npos)
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
		int64_t generation_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		slim_generation_t generation = SLiMCastToGenerationTypeOrRaise(generation_long);
		
		iss >> sub;		// prevalence, which we discard
		
		int8_t nucleotide = -1;
		if (iss && !iss.eof())
		{
			// fetch the nucleotide field if it is present
			iss >> sub;
			if (sub == "A") nucleotide = 0;
			else if (sub == "C") nucleotide = 1;
			else if (sub == "G") nucleotide = 2;
			else if (sub == "T") nucleotide = 3;
			else EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): unrecognized value '"<< sub << "' in nucleotide field." << EidosTerminate();
		}
		
		// look up the mutation type from its index
        MutationType *mutation_type_ptr = MutationTypeWithID(mutation_type_id);
		
		if (!mutation_type_ptr) 
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " has not been defined." << EidosTerminate();
		
		if (fabs(mutation_type_ptr->dominance_coeff_ - dominance_coeff) > 0.001)	// a reasonable tolerance to allow for I/O roundoff
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " has dominance coefficient " << mutation_type_ptr->dominance_coeff_ << " that does not match the population file dominance coefficient of " << dominance_coeff << "." << EidosTerminate();
		
		if ((nucleotide == -1) && mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " is nucleotide-based, but a nucleotide value for a mutation of this type was not supplied." << EidosTerminate();
		if ((nucleotide != -1) && !mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): mutation type m"<< mutation_type_id << " is not nucleotide-based, but a nucleotide value for a mutation of this type was supplied." << EidosTerminate();
		
		// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
		MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
		
		Mutation *new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_id, mutation_type_ptr, position, selection_coeff, subpop_index, generation, nucleotide);
		
		// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
		mutations.insert(std::pair<slim_polymorphismid_t,MutationIndex>(polymorphism_id, new_mut_index));
		population_.MutationRegistryAdd(new_mut);
		
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		if (population_.keeping_muttype_registries_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
		
		// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_pure_neutral_DFE_
		if (selection_coeff != 0.0)
		{
			pure_neutral_ = false;
			mutation_type_ptr->all_pure_neutral_DFE_ = false;
		}
	}
	
	population_.cached_tally_genome_count_ = 0;
	
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
			if (line.find("Genomes") != std::string::npos)
				break;
			
			std::istringstream iss(line);
			
			iss >> sub;		// pX:iY – individual identifier
			int pos = static_cast<int>(sub.find_first_of(":"));
			std::string &&subpop_id_string = sub.substr(0, pos);
			
			slim_objectid_t subpop_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_string, 'p', nullptr);
			std::string &&individual_index_string = sub.substr(pos + 1, std::string::npos);
			
			if (individual_index_string[0] != 'i')
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): reference to individual is malformed." << EidosTerminate();
			
			int64_t individual_index = EidosInterpreter::NonnegativeIntegerForString(individual_index_string.c_str() + 1, nullptr);
			
			Subpopulation *subpop = SubpopulationWithID(subpop_id);
			
			if (!subpop)
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): referenced subpopulation p" << subpop_id << " not defined." << EidosTerminate();
			
			if (individual_index >= subpop->parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): referenced individual i" << individual_index << " is out of range." << EidosTerminate();
			
			Individual &individual = *subpop->parent_individuals_[individual_index];
			
			if (has_individual_pedigree_IDs)
			{
				// If pedigree IDs are present use them; if not, we'll get whatever the default IDs are from the subpop construction
				iss >> sub;
				int64_t pedigree_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
				slim_pedigreeid_t pedigree_id = SLiMCastToPedigreeIDOrRaise(pedigree_long);
				
				individual.SetPedigreeID(pedigree_id);
				individual.genome1_->SetGenomeID(pedigree_id * 2);
				individual.genome1_->SetGenomeID(pedigree_id * 2 + 1);
				gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);
			}
			
			iss >> sub;			// individual sex identifier (F/M/H) – added in SLiM 2.1, so we need to be robust if it is missing
			
			if ((sub == "F") || (sub == "M") || (sub == "H"))
				iss >> sub;
			
			;					// pX:Y – genome 1 identifier, which we do not presently need to parse [already fetched]
			iss >> sub;			// pX:Y – genome 2 identifier, which we do not presently need to parse
			
			// Parse the optional fields at the end of each individual line.  This is a bit tricky.
			// First we read all of the fields in, then we decide how to use them.
			std::vector<std::string> opt_params;
			int opt_param_count;
			
			while (iss >> sub)
				opt_params.push_back(sub);
			
			opt_param_count = (int)opt_params.size();
			
			if (opt_param_count == 0)
			{
				// no optional info present, which might be an error; should never occur unless someone has hand-constructed a bad input file
				if (age_output_count)
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): output file format does not contain age information, which is required." << EidosTerminate();
			}
#ifdef SLIM_NONWF_ONLY
			else if (opt_param_count == age_output_count)
			{
				// only age information is present
				individual.age_ = (slim_age_t)EidosInterpreter::NonnegativeIntegerForString(opt_params[0], nullptr);			// age
			}
#endif  // SLIM_NONWF_ONLY
			else if (opt_param_count == spatial_dimensionality_ + age_output_count)
			{
				// age information is present, in addition to the correct number of spatial positions
				if (spatial_dimensionality_ >= 1)
					individual.spatial_x_ = EidosInterpreter::FloatForString(opt_params[0], nullptr);							// spatial position x
				if (spatial_dimensionality_ >= 2)
					individual.spatial_y_ = EidosInterpreter::FloatForString(opt_params[1], nullptr);							// spatial position y
				if (spatial_dimensionality_ >= 3)
					individual.spatial_z_ = EidosInterpreter::FloatForString(opt_params[2], nullptr);							// spatial position z
				
#ifdef SLIM_NONWF_ONLY
				if (age_output_count)
					individual.age_ = (slim_age_t)EidosInterpreter::NonnegativeIntegerForString(opt_params[spatial_dimensionality_], nullptr);		// age
#endif  // SLIM_NONWF_ONLY
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): output file format does not match that expected by the simulation (spatial dimension or age information is incorrect or missing)." << EidosTerminate();
			}
		}
	}
	
	// Now we are in the Genomes section, which should take us to the end of the file unless there is an Ancestral Sequence section
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Ancestral sequence") != std::string::npos)
			break;
		
		std::istringstream iss(line);
		
		iss >> sub;
		int pos = static_cast<int>(sub.find_first_of(":"));
		std::string &&subpop_id_string = sub.substr(0, pos);
		slim_objectid_t subpop_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_string, 'p', nullptr);
		
        Subpopulation *subpop = SubpopulationWithID(subpop_id);
		
		if (!subpop)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): referenced subpopulation p" << subpop_id << " not defined." << EidosTerminate();
		
		sub.erase(0, pos + 1);	// remove the subpop_id and the colon
		int64_t genome_index_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
		
		if ((genome_index_long < 0) || (genome_index_long > SLIM_MAX_SUBPOP_SIZE * 2))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): genome index out of permitted range." << EidosTerminate();
		slim_popsize_t genome_index = static_cast<slim_popsize_t>(genome_index_long);	// range-check is above since we need to check against SLIM_MAX_SUBPOP_SIZE * 2
		
		Genome &genome = *subpop->parent_genomes_[genome_index];
		
		// Now we might have [A|X|Y] (SLiM 2.0), or we might have the first mutation id - or we might have nothing at all
		if (iss >> sub)
		{
			// check whether this token is a genome type
			if ((sub.compare(gStr_A) == 0) || (sub.compare(gStr_X) == 0) || (sub.compare(gStr_Y) == 0))
			{
				// Let's do a little error-checking against what has already been instantiated for us...
				if ((sub.compare(gStr_A) == 0) && genome.Type() != GenomeType::kAutosome)
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): genome is specified as A (autosome), but the instantiated genome does not match." << EidosTerminate();
				if ((sub.compare(gStr_X) == 0) && genome.Type() != GenomeType::kXChromosome)
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): genome is specified as X (X-chromosome), but the instantiated genome does not match." << EidosTerminate();
				if ((sub.compare(gStr_Y) == 0) && genome.Type() != GenomeType::kYChromosome)
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): genome is specified as Y (Y-chromosome), but the instantiated genome does not match." << EidosTerminate();
				
				if (iss >> sub)
				{
					if (sub == "<null>")
					{
						if (!genome.IsNull())
							EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): genome is specified as null, but the instantiated genome is non-null." << EidosTerminate();
						
						continue;	// this line is over
					}
					else
					{
						if (genome.IsNull())
							EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): genome is specified as non-null, but the instantiated genome is null." << EidosTerminate();
						
						// drop through, and sub will be interpreted as a mutation id below
					}
				}
				else
					continue;
			}
			
			slim_position_t mutrun_length_ = genome.mutrun_length_;
			slim_mutrun_index_t current_mutrun_index = -1;
			MutationRun *current_mutrun = nullptr;
			
			do
			{
				int64_t polymorphismid_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
				slim_polymorphismid_t polymorphism_id = SLiMCastToPolymorphismidTypeOrRaise(polymorphismid_long);
				
				auto found_mut_pair = mutations.find(polymorphism_id);
				
				if (found_mut_pair == mutations.end()) 
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTextFile): polymorphism " << polymorphism_id << " has not been defined." << EidosTerminate();
				
				MutationIndex mutation = found_mut_pair->second;
				slim_mutrun_index_t mutrun_index = (slim_mutrun_index_t)((mut_block_ptr + mutation)->position_ / mutrun_length_);
				
				if (mutrun_index != current_mutrun_index)
				{
					current_mutrun_index = mutrun_index;
					genome.WillModifyRun(current_mutrun_index);
					
					current_mutrun = genome.mutruns_[mutrun_index].get();
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
		infile >> *(chromosome_.AncestralSequence());
	}
	
	// It's a little unclear how we ought to clean up after ourselves, and this is a continuing source of bugs.  We could be loading
	// a new population in an early() event, in a late() event, or in between generations in SLiMgui using its Import Population command.
	// The safest avenue seems to be to just do all the bookkeeping we can think of: tally frequencies, calculate fitnesses, and
	// survey the population for SLiMgui.  This will lead to some of these actions being done at an unusual time in the generation cycle,
	// though, and will cause some things to be done unnecessarily (because they are not normally up-to-date at the current generation
	// cycle stage anyway) or done twice (which could be particularly problematic for fitness() callbacks).  Nevertheless, this seems
	// like the best policy, at least until shown otherwise...  BCH 11 June 2016
	
	// BCH 5 April 2017: Well, it has been shown otherwise.  Now that interactions have been added, fitness() callbacks often depend on
	// them, which means the interactions need to be evaluated, which means we can't evaluate fitness values yet; we need to give the
	// user's script a chance to evaluate the interactions.  This was always a problem, really; fitness() callbacks might have needed
	// some external state to be set up that would be on the population state.  But now it is a glaring problem, and forces us to revise
	// our policy.  For backward compatibility, we will keep the old behavior if reading a file that is version 2 or earlier; a bit
	// weird, but probably nobody will ever even notice...
	
	// Re-tally mutation references so we have accurate frequency counts for our new mutations
	population_.UniqueMutationRuns();
	population_.TallyMutationReferences(nullptr, true);
	
	if (file_version <= 2)
	{
		// Now that we have the info on everybody, update fitnesses so that we're ready to run the next generation
		// used to be generation + 1; removing that 18 Feb 2016 BCH
		
		nonneutral_change_counter_++;			// trigger unconditional nonneutral mutation caching inside UpdateFitness()
		last_nonneutral_regime_ = 3;			// this means "unpredictable callbacks", will trigger a recache next generation
		
		for (auto muttype_iter : mutation_types_)
			(muttype_iter.second)->subject_to_fitness_callback_ = true;			// we're not doing RecalculateFitness()'s work, so play it safe
		
		SLiMEidosBlockType old_executing_block_type = executing_block_type_;
		executing_block_type_ = SLiMEidosBlockType::SLiMEidosFitnessCallback;	// used for both fitness() and fitness(NULL) for simplicity
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> fitness_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosFitnessCallback, -1, -1, subpop_id);
			std::vector<SLiMEidosBlock*> global_fitness_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback, -2, -1, subpop_id);
			
			subpop->UpdateFitness(fitness_callbacks, global_fitness_callbacks);
		}
		
		executing_block_type_ = old_executing_block_type;
		
#ifdef SLIMGUI
		// Let SLiMgui survey the population for mean fitness and such, if it is our target
		population_.SurveyPopulation();
#endif
	}
	
	return file_generation;
}

#ifndef __clang_analyzer__
slim_generation_t SLiMSim::_InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	std::size_t file_size = 0;
	slim_generation_t file_generation;
	int32_t spatial_output_count;
	int age_output_count = 0;
	int pedigree_output_count = 0;
	bool has_nucleotides = false;
	
	// Read file into buf
	std::ifstream infile(p_file, std::ios::in | std::ios::binary);
	
	if (!infile.is_open() || infile.eof())
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): could not open initialization file." << EidosTerminate();
	
	// Determine the file length
	infile.seekg(0, std::ios_base::end);
	file_size = infile.tellg();
	
	// Read in the entire file; we assume we have enough memory, for now
	std::unique_ptr<char[]> raii_buf(new char[file_size]);
	char *buf = raii_buf.get();
	
	if (!buf)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): could not allocate input buffer." << EidosTerminate();
	
	char *buf_end = buf + file_size;
	char *p = buf;
	
	infile.seekg(0, std::ios_base::beg);
	infile.read(buf, file_size);
	
	// Close the file; we will work only with our buffer from here on
	infile.close();
	
	int32_t section_end_tag;
	int32_t file_version;
	
	// Header beginning, to check endianness and determine file version
	{
		int32_t endianness_tag, version_tag;
		
		if (p + sizeof(endianness_tag) + sizeof(version_tag) > buf_end)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF while reading header." << EidosTerminate();
		
		endianness_tag = *(int32_t *)p;
		p += sizeof(endianness_tag);
		
		version_tag = *(int32_t *)p;
		p += sizeof(version_tag);
		
		if (endianness_tag != 0x12345678)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): endianness mismatch." << EidosTerminate();
		
		// version 4 is the same as version 3 but with an age value for each individual
		if (version_tag == 4)
		{
			age_output_count = 1;
			version_tag = 3;
		}
		
		if ((version_tag != 1) && (version_tag != 2) && (version_tag != 3) && (version_tag != 5) && (version_tag != 6))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unrecognized version." << EidosTerminate();
		
		file_version = version_tag;
	}
	
	// Header section
	{
		int32_t double_size;
		double double_test;
		int32_t slim_generation_t_size, slim_position_t_size, slim_objectid_t_size, slim_popsize_t_size, slim_refcount_t_size, slim_selcoeff_t_size, slim_mutationid_t_size, slim_polymorphismid_t_size, slim_age_t_size, slim_pedigreeid_t_size, slim_genomeid_t_size;
		int64_t flags = 0;
		int header_length = sizeof(double_size) + sizeof(double_test) + sizeof(slim_generation_t_size) + sizeof(slim_position_t_size) + sizeof(slim_objectid_t_size) + sizeof(slim_popsize_t_size) + sizeof(slim_refcount_t_size) + sizeof(slim_selcoeff_t_size) + sizeof(file_generation) + sizeof(section_end_tag);
		
		if (file_version >= 2)
			header_length += sizeof(slim_mutationid_t_size) + sizeof(slim_polymorphismid_t_size);
		if (file_version >= 6)
			header_length += sizeof(slim_age_t_size) + sizeof(slim_pedigreeid_t_size) + sizeof(slim_genomeid_t);
		if (file_version >= 5)
			header_length += sizeof(flags);
		
		if (p + header_length > buf_end)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF while reading header." << EidosTerminate();
		
		double_size = *(int32_t *)p;
		p += sizeof(double_size);
		
		double_test = *(double *)p;
		p += sizeof(double_test);
		
		if (file_version >= 5)
		{
			flags = *(int64_t *)p;
			p += sizeof(flags);
			
			if (flags & 0x01) age_output_count = 1;
			if (flags & 0x02) has_nucleotides = true;
			
			if (file_version >= 6)
				if (flags & 0x04) pedigree_output_count = 1;
		}
		
		slim_generation_t_size = *(int32_t *)p;
		p += sizeof(slim_generation_t_size);
		
		slim_position_t_size = *(int32_t *)p;
		p += sizeof(slim_position_t_size);
		
		slim_objectid_t_size = *(int32_t *)p;
		p += sizeof(slim_objectid_t_size);
		
		slim_popsize_t_size = *(int32_t *)p;
		p += sizeof(slim_popsize_t_size);
		
		slim_refcount_t_size = *(int32_t *)p;
		p += sizeof(slim_refcount_t_size);
		
		slim_selcoeff_t_size = *(int32_t *)p;
		p += sizeof(slim_selcoeff_t_size);
		
		if (file_version >= 2)
		{
			slim_mutationid_t_size = *(int32_t *)p;
			p += sizeof(slim_mutationid_t_size);
			
			slim_polymorphismid_t_size = *(int32_t *)p;
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
			slim_age_t_size = *(int32_t *)p;
			p += sizeof(slim_age_t_size);
			
			slim_pedigreeid_t_size = *(int32_t *)p;
			p += sizeof(slim_pedigreeid_t_size);
			
			slim_genomeid_t_size = *(int32_t *)p;
			p += sizeof(slim_genomeid_t_size);
		}
		else
		{
			// Version <= 5 file; backfill correct values
			slim_age_t_size = sizeof(slim_age_t);
			slim_pedigreeid_t_size = sizeof(slim_pedigreeid_t);
			slim_genomeid_t_size = sizeof(slim_genomeid_t);
		}
		
		file_generation = *(slim_generation_t *)p;
		p += sizeof(file_generation);
		
		if (file_version >= 3)
		{
			spatial_output_count = *(int32_t *)p;
			p += sizeof(spatial_output_count);
		}
		else
		{
			// Version 1 or 2 file; backfill correct value
			spatial_output_count = 0;
		}
		
		section_end_tag = *(int32_t *)p;
		p += sizeof(section_end_tag);
		
		if (double_size != sizeof(double))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): sizeof(double) mismatch." << EidosTerminate();
		if (double_test != 1234567890.0987654321)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): double format mismatch." << EidosTerminate();
		if (has_nucleotides && !nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): the output was generated by a nucleotide-based model, but the current model is not nucleotide-based." << EidosTerminate();
		if (!has_nucleotides && nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): the output was generated by a non-nucleotide-based model, but the current model is nucleotide-based." << EidosTerminate();
		if ((slim_generation_t_size != sizeof(slim_generation_t)) ||
			(slim_position_t_size != sizeof(slim_position_t)) ||
			(slim_objectid_t_size != sizeof(slim_objectid_t)) ||
			(slim_popsize_t_size != sizeof(slim_popsize_t)) ||
			(slim_refcount_t_size != sizeof(slim_refcount_t)) ||
			(slim_selcoeff_t_size != sizeof(slim_selcoeff_t)) ||
			(slim_mutationid_t_size != sizeof(slim_mutationid_t)) ||
			(slim_polymorphismid_t_size != sizeof(slim_polymorphismid_t)) ||
			(slim_age_t_size != sizeof(slim_age_t)) ||
			(slim_pedigreeid_t_size != sizeof(slim_pedigreeid_t)) ||
			(slim_genomeid_t_size != sizeof(slim_genomeid_t)))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): SLiM datatype size mismatch." << EidosTerminate();
		if ((spatial_output_count < 0) || (spatial_output_count > 3))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): spatial output count out of range." << EidosTerminate();
		if ((spatial_output_count > 0) && (spatial_output_count != spatial_dimensionality_))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): output spatial dimensionality does not match that of the simulation." << EidosTerminate();
		if (age_output_count && (ModelType() == SLiMModelType::kModelTypeWF))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): age information is present but the simulation is using a WF model." << EidosTerminate();
		if (!age_output_count && (ModelType() == SLiMModelType::kModelTypeNonWF))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): age information is not present but the simulation is using a nonWF model; age information must be included." << EidosTerminate();
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): missing section end after header." << EidosTerminate();
	}
	
	// As of SLiM 2.1, we change the generation as a side effect of loading; otherwise we can't correctly update our state here!
	// As of SLiM 3, we set the generation up here, before making any individuals, because we need it to be correct for the tree-seq recording code.
	SetGeneration(file_generation);
	
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
		subpop_start_tag = *(int32_t *)p;
		if (subpop_start_tag != (int32_t)0xFFFF0001)
			break;
		
		// Otherwise, we have a subpop record; read in the rest of it
		p += sizeof(subpop_start_tag);
		
		subpop_id = *(slim_objectid_t *)p;
		p += sizeof(subpop_id);
		
		subpop_size = *(slim_popsize_t *)p;
		p += sizeof(subpop_size);
		
		sex_flag = *(int32_t *)p;
		p += sizeof(sex_flag);
		
		if (sex_flag != population_.sim_.sex_enabled_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): sex vs. hermaphroditism mismatch between file and simulation." << EidosTerminate();
		
		subpop_sex_ratio = *(double *)p;
		p += sizeof(subpop_sex_ratio);
		
		// Create the population population
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, subpop_sex_ratio);
		
		// define a new Eidos variable to refer to the new subpopulation
		EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
		
		if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): new subpopulation symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
		
		simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	}
	
	if (p + sizeof(section_end_tag) > buf_end)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF after subpopulations." << EidosTerminate();
	else
	{
		section_end_tag = *(int32_t *)p;
		p += sizeof(section_end_tag);
		
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): missing section end after subpopulations." << EidosTerminate();
	}
	
	// Read in the size of the mutation map, so we can allocate a vector rather than utilizing std::map
	int32_t mutation_map_size;
	
	if (p + sizeof(mutation_map_size) > buf_end)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF at mutation map size." << EidosTerminate();
	else
	{
		mutation_map_size = *(int32_t *)p;
		p += sizeof(mutation_map_size);
	}
	
	// Mutations section
	std::unique_ptr<MutationIndex[]> raii_mutations(new MutationIndex[mutation_map_size]);
	MutationIndex *mutations = raii_mutations.get();
	
	if (!mutations)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): could not allocate mutations buffer." << EidosTerminate();
	
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
		slim_generation_t generation;
		slim_refcount_t prevalence;
		int8_t nucleotide = -1;
		
		// If there isn't enough buffer left to read a full mutation record, we assume we are done with this section
		int record_size = sizeof(mutation_start_tag) + sizeof(polymorphism_id) + sizeof(mutation_type_id) + sizeof(position) + sizeof(selection_coeff) + sizeof(dominance_coeff) + sizeof(subpop_index) + sizeof(generation) + sizeof(prevalence);
		
		if (file_version >= 2)
			record_size += sizeof(mutation_id);
		if (has_nucleotides)
			record_size += sizeof(nucleotide);
		
		if (p + record_size > buf_end)
			break;
		
		// If the first int32_t is not a mutation start tag, then we are done with this section
		mutation_start_tag = *(int32_t *)p;
		if (mutation_start_tag != (int32_t)0xFFFF0002)
			break;
		
		// Otherwise, we have a mutation record; read in the rest of it
		p += sizeof(mutation_start_tag);
		
		polymorphism_id = *(slim_polymorphismid_t *)p;
		p += sizeof(polymorphism_id);
		
		if (file_version >= 2)
		{
			mutation_id = *(slim_mutationid_t *)p;
			p += sizeof(mutation_id);
		}
		else
		{
			mutation_id = polymorphism_id;		// when parsing version 1 output, we use the polymorphism id as the mutation id
		}
		
		mutation_type_id = *(slim_objectid_t *)p;
		p += sizeof(mutation_type_id);
		
		position = *(slim_position_t *)p;
		p += sizeof(position);
		
		selection_coeff = *(slim_selcoeff_t *)p;
		p += sizeof(selection_coeff);
		
		dominance_coeff = *(slim_selcoeff_t *)p;
		p += sizeof(dominance_coeff);
		
		subpop_index = *(slim_objectid_t *)p;
		p += sizeof(subpop_index);
		
		generation = *(slim_generation_t *)p;
		p += sizeof(generation);
		
		prevalence = *(slim_refcount_t *)p;
		(void)prevalence;	// we don't use the frequency when reading the pop data back in; let the static analyzer know that's OK
		p += sizeof(prevalence);
		
		if (has_nucleotides)
		{
			nucleotide = *(int8_t *)p;
			p += sizeof(nucleotide);
		}
		
		// look up the mutation type from its index
        MutationType *mutation_type_ptr = MutationTypeWithID(mutation_type_id);
		
		if (!mutation_type_ptr) 
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " has not been defined." << EidosTerminate();
		if (mutation_type_ptr->dominance_coeff_ != dominance_coeff)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): mutation type m" << mutation_type_id << " has dominance coefficient " << mutation_type_ptr->dominance_coeff_ << " that does not match the population file dominance coefficient of " << dominance_coeff << "." << EidosTerminate();
		
		if ((nucleotide == -1) && mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): mutation type m"<< mutation_type_id << " is nucleotide-based, but a nucleotide value for a mutation of this type was not supplied." << EidosTerminate();
		if ((nucleotide != -1) && !mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): mutation type m"<< mutation_type_id << " is not nucleotide-based, but a nucleotide value for a mutation of this type was supplied." << EidosTerminate();
		
		// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
		MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
		
		Mutation *new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_id, mutation_type_ptr, position, selection_coeff, subpop_index, generation, nucleotide);
		
		// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
		mutations[polymorphism_id] = new_mut_index;
		population_.MutationRegistryAdd(new_mut);
		
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		if (population_.keeping_muttype_registries_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
#endif
		
		// all mutations seen here will be added to the simulation somewhere, so check and set pure_neutral_ and all_pure_neutral_DFE_
		if (selection_coeff != 0.0)
		{
			pure_neutral_ = false;
			mutation_type_ptr->all_pure_neutral_DFE_ = false;
		}
	}
	
	population_.cached_tally_genome_count_ = 0;
	
	if (p + sizeof(section_end_tag) > buf_end)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF after mutations." << EidosTerminate();
	else
	{
		section_end_tag = *(int32_t *)p;
		p += sizeof(section_end_tag);
		
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): missing section end after mutations." << EidosTerminate();
	}
	
	// Genomes section
	if (pedigree_output_count)
		gSLiM_next_pedigree_id = 0;
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	bool use_16_bit = (mutation_map_size <= UINT16_MAX - 1);	// 0xFFFF is reserved as the start of our various tags
	std::unique_ptr<MutationIndex[]> raii_genomebuf(new MutationIndex[mutation_map_size]);	// allowing us to use emplace_back_bulk() for speed
	MutationIndex *genomebuf = raii_genomebuf.get();
	
	while (true)
	{
		slim_objectid_t subpop_id;
		slim_popsize_t genome_index;
		int32_t genome_type;
		int32_t total_mutations;
		
		// If there isn't enough buffer left to read a full genome record, we assume we are done with this section
		if (p + sizeof(genome_type) + sizeof(subpop_id) + sizeof(genome_index) + sizeof(total_mutations) > buf_end)
			break;
		
		// First check the first 32 bits to see if it is a section end tag
		genome_type = *(int32_t *)p;
		
		if (genome_type == (int32_t)0xFFFF0000)
			break;
		
		// If not, proceed with reading the genome entry
		p += sizeof(genome_type);
		
		subpop_id = *(slim_objectid_t *)p;
		p += sizeof(subpop_id);
		
		genome_index = *(slim_popsize_t *)p;
		p += sizeof(genome_index);
		
		// Look up the subpopulation
        Subpopulation *subpop = SubpopulationWithID(subpop_id);
		
		if (!subpop)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): referenced subpopulation p" << subpop_id << " not defined." << EidosTerminate();
		
		// Read in individual spatial position information.  Added in version 3.
		if (spatial_output_count && ((genome_index % 2) == 0))
		{
			// do another buffer length check
			if (p + spatial_output_count * sizeof(double) + sizeof(total_mutations) > buf_end)
				break;
			
			int individual_index = genome_index / 2;
			Individual &individual = *subpop->parent_individuals_[individual_index];
			
			if (spatial_output_count >= 1)
			{
				individual.spatial_x_ = *(double *)p;
				p += sizeof(double);
			}
			if (spatial_output_count >= 2)
			{
				individual.spatial_y_ = *(double *)p;
				p += sizeof(double);
			}
			if (spatial_output_count >= 3)
			{
				individual.spatial_z_ = *(double *)p;
				p += sizeof(double);
			}
		}
		
		// Read in individual pedigree ID information.  Added in version 6.
		if (pedigree_output_count  && ((genome_index % 2) == 0))
		{
			// do another buffer length check
			if (p + sizeof(slim_pedigreeid_t) + sizeof(total_mutations) > buf_end)
				break;
			
			int individual_index = genome_index / 2;
			Individual &individual = *subpop->parent_individuals_[individual_index];
			slim_pedigreeid_t pedigree_id = *(slim_pedigreeid_t *)p;
			
			individual.SetPedigreeID(pedigree_id);
			individual.genome1_->SetGenomeID(pedigree_id * 2);
			individual.genome1_->SetGenomeID(pedigree_id * 2 + 1);
			gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);
			p += sizeof(slim_pedigreeid_t);
		}
		
#ifdef SLIM_NONWF_ONLY
		// Read in individual age information.  Added in version 4.
		if (age_output_count && ((genome_index % 2) == 0))
		{
			// do another buffer length check
			if (p + sizeof(slim_age_t) + sizeof(total_mutations) > buf_end)
				break;
			
			int individual_index = genome_index / 2;
			Individual &individual = *subpop->parent_individuals_[individual_index];
			
			individual.age_ = *(slim_age_t *)p;
			p += sizeof(slim_age_t);
		}
#endif  // SLIM_NONWF_ONLY
		
		total_mutations = *(int32_t *)p;
		p += sizeof(total_mutations);
		
		// Look up the genome
		if ((genome_index < 0) || (genome_index > SLIM_MAX_SUBPOP_SIZE * 2))
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): genome index out of permitted range." << EidosTerminate();
		
		Genome &genome = *subpop->parent_genomes_[genome_index];
		
		// Error-check the genome type
		if (genome_type != (int32_t)genome.Type())
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): genome type does not match the instantiated genome." << EidosTerminate();
		
		// Check the null genome state
		if (total_mutations == (int32_t)0xFFFF1000)
		{
			if (!genome.IsNull())
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): genome is specified as null, but the instantiated genome is non-null." << EidosTerminate();
		}
		else
		{
			if (genome.IsNull())
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): genome is specified as non-null, but the instantiated genome is null." << EidosTerminate();
			
			// Read in the mutation list
			int32_t mutcount = 0;
			
			if (use_16_bit)
			{
				// reading 16-bit mutation tags
				uint16_t mutation_id;
				
				if (p + sizeof(mutation_id) * total_mutations > buf_end)
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF while reading genome." << EidosTerminate();
				
				for (; mutcount < total_mutations; ++mutcount)
				{
					mutation_id = *(uint16_t *)p;
					p += sizeof(mutation_id);
					
					// Add mutation to genome
					if (/*(mutation_id < 0) ||*/ (mutation_id >= mutation_map_size)) 
						EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): mutation " << mutation_id << " has not been defined." << EidosTerminate();
					
					genomebuf[mutcount] = mutations[mutation_id];
				}
			}
			else
			{
				// reading 32-bit mutation tags
				int32_t mutation_id;
				
				if (p + sizeof(mutation_id) * total_mutations > buf_end)
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF while reading genome." << EidosTerminate();
				
				for (; mutcount < total_mutations; ++mutcount)
				{
					mutation_id = *(int32_t *)p;
					p += sizeof(mutation_id);
					
					// Add mutation to genome
					if ((mutation_id < 0) || (mutation_id >= mutation_map_size)) 
						EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): mutation " << mutation_id << " has not been defined." << EidosTerminate();
					
					genomebuf[mutcount] = mutations[mutation_id];
				}
			}
			
			slim_position_t mutrun_length_ = genome.mutrun_length_;
			slim_mutrun_index_t current_mutrun_index = -1;
			MutationRun *current_mutrun = nullptr;
			
			for (int mut_index = 0; mut_index < mutcount; ++mut_index)
			{
				MutationIndex mutation = genomebuf[mut_index];
				slim_mutrun_index_t mutrun_index = (slim_mutrun_index_t)((mut_block_ptr + mutation)->position_ / mutrun_length_);
				
				if (mutrun_index != current_mutrun_index)
				{
					current_mutrun_index = mutrun_index;
					genome.WillModifyRun(current_mutrun_index);
					
					current_mutrun = genome.mutruns_[mutrun_index].get();
				}
				
				current_mutrun->emplace_back(mutation);
			}
		}
	}
	
	if (p + sizeof(section_end_tag) > buf_end)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF after genomes." << EidosTerminate();
	else
	{
		section_end_tag = *(int32_t *)p;
		p += sizeof(section_end_tag);
		(void)p;	// dead store above is deliberate
		
		if (section_end_tag != (int32_t)0xFFFF0000)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): missing section end after genomes." << EidosTerminate();
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
			chromosome_.AncestralSequence()->ReadCompressedNucleotides(&p, buf_end);
			
			if (p + sizeof(section_end_tag) > buf_end)
				EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): unexpected EOF after ancestral sequence." << EidosTerminate();
			else
			{
				section_end_tag = *(int32_t *)p;
				p += sizeof(section_end_tag);
				(void)p;	// dead store above is deliberate
				
				if (section_end_tag != (int32_t)0xFFFF0000)
					EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromBinaryFile): missing section end after ancestral sequence." << EidosTerminate();
			}
		}
	}
	
	// It's a little unclear how we ought to clean up after ourselves, and this is a continuing source of bugs.  We could be loading
	// a new population in an early() event, in a late() event, or in between generations in SLiMgui using its Import Population command.
	// The safest avenue seems to be to just do all the bookkeeping we can think of: tally frequencies, calculate fitnesses, and
	// survey the population for SLiMgui.  This will lead to some of these actions being done at an unusual time in the generation cycle,
	// though, and will cause some things to be done unnecessarily (because they are not normally up-to-date at the current generation
	// cycle stage anyway) or done twice (which could be particularly problematic for fitness() callbacks).  Nevertheless, this seems
	// like the best policy, at least until shown otherwise...  BCH 11 June 2016
	
	// BCH 5 April 2017: Well, it has been shown otherwise.  Now that interactions have been added, fitness() callbacks often depend on
	// them, which means the interactions need to be evaluated, which means we can't evaluate fitness values yet; we need to give the
	// user's script a chance to evaluate the interactions.  This was always a problem, really; fitness() callbacks might have needed
	// some external state to be set up that would be on the population state.  But now it is a glaring problem, and forces us to revise
	// our policy.  For backward compatibility, we will keep the old behavior if reading a file that is version 2 or earlier; a bit
	// weird, but probably nobody will ever even notice...
	
	// Re-tally mutation references so we have accurate frequency counts for our new mutations
	population_.UniqueMutationRuns();
	population_.TallyMutationReferences(nullptr, true);
	
	if (file_version <= 2)
	{
		// Now that we have the info on everybody, update fitnesses so that we're ready to run the next generation
		// used to be generation + 1; removing that 18 Feb 2016 BCH
		
		nonneutral_change_counter_++;			// trigger unconditional nonneutral mutation caching inside UpdateFitness()
		last_nonneutral_regime_ = 3;			// this means "unpredictable callbacks", will trigger a recache next generation
		
		for (auto muttype_iter : mutation_types_)
			(muttype_iter.second)->subject_to_fitness_callback_ = true;	// we're not doing RecalculateFitness()'s work, so play it safe
		
		SLiMEidosBlockType old_executing_block_type = executing_block_type_;
		executing_block_type_ = SLiMEidosBlockType::SLiMEidosFitnessCallback;	// used for both fitness() and fitness(NULL) for simplicity
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> fitness_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosFitnessCallback, -1, -1, subpop_id);
			std::vector<SLiMEidosBlock*> global_fitness_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback, -2, -1, subpop_id);
			
			subpop->UpdateFitness(fitness_callbacks, global_fitness_callbacks);
		}
		
		executing_block_type_ = old_executing_block_type;
		
#ifdef SLIMGUI
		// Let SLiMgui survey the population for mean fitness and such, if it is our target
		population_.SurveyPopulation();
#endif
	}
	
	return file_generation;
}
#else
// the static analyzer has a lot of trouble understanding this method
slim_generation_t SLiMSim::_InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	return 0;
}
#endif

void SLiMSim::ValidateScriptBlockCaches(void)
{
#if DEBUG_BLOCK_REG_DEREG
	std::cout << "Generation " << generation_ << ": ValidateScriptBlockCaches() called..." << std::endl;
#endif
	
	if (!script_block_types_cached_)
	{
		cached_early_events_.clear();
		cached_late_events_.clear();
		cached_initialize_callbacks_.clear();
		cached_fitness_callbacks_.clear();
		cached_fitnessglobal_callbacks_onegen_.clear();
		cached_fitnessglobal_callbacks_multigen_.clear();
		cached_interaction_callbacks_.clear();
		cached_matechoice_callbacks_.clear();
		cached_modifychild_callbacks_.clear();
		cached_recombination_callbacks_.clear();
		cached_mutation_callbacks_.clear();
		cached_reproduction_callbacks_.clear();
		cached_userdef_functions_.clear();
		
		std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
		
#if DEBUG_BLOCK_REG_DEREG
		std::cout << "   ValidateScriptBlockCaches() recaching, AllScriptBlocks() is:" << std::endl;
		for (SLiMEidosBlock *script_block : script_blocks)
		{
			std::cout << "      ";
			script_block->Print(std::cout);
			std::cout << std::endl;
		}
#endif
		
		for (auto script_block : script_blocks)
		{
			switch (script_block->type_)
			{
				case SLiMEidosBlockType::SLiMEidosEventEarly:				cached_early_events_.push_back(script_block);				break;
				case SLiMEidosBlockType::SLiMEidosEventLate:				cached_late_events_.push_back(script_block);				break;
				case SLiMEidosBlockType::SLiMEidosInitializeCallback:		cached_initialize_callbacks_.push_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosFitnessCallback:			cached_fitness_callbacks_.push_back(script_block);			break;
				case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:
				{
					// Global fitness callbacks are not order-dependent, so we don't have to preserve their order
					// of declaration the way we do with other types of callbacks.  This allows us to be very efficient
					// in how we look them up, which is good since sometimes we have a very large number of them.
					// We put those that are registered for just a single generation in a separate vector, which
					// we sort by generation, allowing us to look
					slim_generation_t start = script_block->start_generation_;
					slim_generation_t end = script_block->end_generation_;
					
					if (start == end)
					{
						cached_fitnessglobal_callbacks_onegen_.insert(std::pair<slim_generation_t, SLiMEidosBlock*>(start, script_block));
					}
					else
					{
						cached_fitnessglobal_callbacks_multigen_.push_back(script_block);
					}
					break;
				}
				case SLiMEidosBlockType::SLiMEidosInteractionCallback:		cached_interaction_callbacks_.push_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		cached_matechoice_callbacks_.push_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		cached_modifychild_callbacks_.push_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	cached_recombination_callbacks_.push_back(script_block);	break;
				case SLiMEidosBlockType::SLiMEidosMutationCallback:			cached_mutation_callbacks_.push_back(script_block);			break;
				case SLiMEidosBlockType::SLiMEidosReproductionCallback:		cached_reproduction_callbacks_.push_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		cached_userdef_functions_.push_back(script_block);			break;
				case SLiMEidosBlockType::SLiMEidosNoBlockType:				break;	// never hit
			}
		}
		
		script_block_types_cached_ = true;
		
#if DEBUG_BLOCK_REG_DEREG
		std::cout << "   ValidateScriptBlockCaches() recached, late() events cached are:" << std::endl;
		for (SLiMEidosBlock *script_block : cached_late_events_)
		{
			std::cout << "      ";
			script_block->Print(std::cout);
			std::cout << std::endl;
		}
#endif
	}
}

std::vector<SLiMEidosBlock*> SLiMSim::ScriptBlocksMatching(slim_generation_t p_generation, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id)
{
	if (!script_block_types_cached_)
		ValidateScriptBlockCaches();
	
	std::vector<SLiMEidosBlock*> *block_list = nullptr;
	
	switch (p_event_type)
	{
		case SLiMEidosBlockType::SLiMEidosEventEarly:				block_list = &cached_early_events_;						break;
		case SLiMEidosBlockType::SLiMEidosEventLate:				block_list = &cached_late_events_;						break;
		case SLiMEidosBlockType::SLiMEidosInitializeCallback:		block_list = &cached_initialize_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosFitnessCallback:			block_list = &cached_fitness_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:	block_list = &cached_fitnessglobal_callbacks_multigen_;	break;
		case SLiMEidosBlockType::SLiMEidosInteractionCallback:		block_list = &cached_interaction_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		block_list = &cached_matechoice_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		block_list = &cached_modifychild_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	block_list = &cached_recombination_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosMutationCallback:			block_list = &cached_mutation_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosReproductionCallback:		block_list = &cached_reproduction_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		block_list = &cached_userdef_functions_;				break;
		case SLiMEidosBlockType::SLiMEidosNoBlockType:				break;	// never hit
	}
	
	std::vector<SLiMEidosBlock*> matches;
	
	for (SLiMEidosBlock *script_block : *block_list)
	{
		// check that the generation is in range
		if ((script_block->start_generation_ > p_generation) || (script_block->end_generation_ < p_generation))
			continue;
		
		// check that the script type matches (event, callback, etc.) - now guaranteed by the caching mechanism
		//if (script_block->type_ != p_event_type)
		//	continue;
		
		// check that the mutation type id matches, if requested
		// this is now a bit tricky, with the NULL mut-type option, indicated by -2.  The rules now are:
		//
		//    if -2 is requested, -2 callbacks are all you get
		//    if anything other than -2 is requested (including -1), -2 callbacks will not be returned
		//
		// so -2 callbacks are treated in a completely separate manner; they are never returned with other callbacks
		slim_objectid_t mutation_type_id = script_block->mutation_type_id_;
		
		if ((p_mutation_type_id == -2) && (mutation_type_id != -2))
			continue;
		if ((p_mutation_type_id != -2) && (mutation_type_id == -2))
			continue;
		
		if (p_mutation_type_id != -1)
		{
			if ((mutation_type_id != -1) && (p_mutation_type_id != mutation_type_id))
				continue;
		}
		
		// check that the interaction type id matches, if requested
		if (p_interaction_type_id != -1)
		{
			slim_objectid_t interaction_type_id = script_block->interaction_type_id_;
			
			if ((interaction_type_id != -1) && (p_interaction_type_id != interaction_type_id))
				continue;
		}
		
		// check that the subpopulation id matches, if requested
		if (p_subpopulation_id != -1)
		{
			slim_objectid_t subpopulation_id = script_block->subpopulation_id_;
			
			if ((subpopulation_id != -1) && (p_subpopulation_id != subpopulation_id))
				continue;
		}
		
		// OK, everything matches, so we want to return this script block
		matches.emplace_back(script_block);
	}
	
	// add in any single-generation global fitness callbacks
	if (p_event_type == SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)
	{
		auto find_range = cached_fitnessglobal_callbacks_onegen_.equal_range(p_generation);
		auto find_start = find_range.first;
		auto find_end = find_range.second;
		
		for (auto block_iter = find_start; block_iter != find_end; ++block_iter)
		{
			SLiMEidosBlock *script_block = block_iter->second;
			
			// check that the subpopulation id matches, if requested
			if (p_subpopulation_id != -1)
			{
				slim_objectid_t subpopulation_id = script_block->subpopulation_id_;
				
				if ((subpopulation_id != -1) && (p_subpopulation_id != subpopulation_id))
					continue;
			}
			
			// OK, everything matches, so we want to return this script block
			matches.emplace_back(script_block);
		}
	}
	
	return matches;
}

std::vector<SLiMEidosBlock*> &SLiMSim::AllScriptBlocks()
{
	return script_blocks_;
}

void SLiMSim::OptimizeScriptBlock(SLiMEidosBlock *p_script_block)
{
	// The goal here is to look for specific structures in callbacks that we are able to optimize by short-circuiting
	// the callback interpretation entirely and replacing it with equivalent C++ code.  This is extremely messy, so
	// we're not going to do this for very many cases, but sometimes it is worth it.
	if (!p_script_block->has_cached_optimization_)
	{
		if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)
		{
			const EidosASTNode *base_node = p_script_block->compound_statement_node_;
			
			if ((base_node->token_->token_type_ == EidosTokenType::kTokenLBrace) && (base_node->children_.size() == 1))
			{
				bool opt_dnorm1_candidate = true;
				const EidosASTNode *expr_node = base_node->children_[0];
				
				// we must have an intervening "return", which we jump down through
				if ((expr_node->token_->token_type_ == EidosTokenType::kTokenReturn) && (expr_node->children_.size() == 1))
				{
					expr_node = expr_node->children_[0];
					
					// parse an optional constant at the beginning, like 1.0 + ...
					double added_constant = NAN;
					
					if ((expr_node->token_->token_type_ == EidosTokenType::kTokenPlus) && (expr_node->children_.size() == 2))
					{
						const EidosASTNode *constant_node = expr_node->children_[0];
						const EidosASTNode *rhs_node = expr_node->children_[1];
						
						if (constant_node->HasCachedNumericValue())
						{
							added_constant = constant_node->CachedNumericValue();
							expr_node = rhs_node;
						}
						else
							opt_dnorm1_candidate = false;
					}
					else
					{
						added_constant = 0.0;
					}
					
					// parse an optional divisor at the end, ... / div
					double denominator = NAN;
					
					if ((expr_node->token_->token_type_ == EidosTokenType::kTokenDiv) && (expr_node->children_.size() == 2))
					{
						const EidosASTNode *numerator_node = expr_node->children_[0];
						const EidosASTNode *denominator_node = expr_node->children_[1];
						
						if (denominator_node->HasCachedNumericValue())
						{
							denominator = denominator_node->CachedNumericValue();
							expr_node = numerator_node;
						}
						else
							opt_dnorm1_candidate = false;
					}
					else
					{
						denominator = 1.0;
					}
					
					// parse the dnorm() function call
					if (opt_dnorm1_candidate && (expr_node->token_->token_type_ == EidosTokenType::kTokenLParen) && (expr_node->children_.size() >= 2))
					{
						const EidosASTNode *call_node = expr_node->children_[0];
						
						if ((call_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (call_node->token_->token_string_ == "dnorm"))
						{
							int child_count = (int)expr_node->children_.size();
							const EidosASTNode *x_node = expr_node->children_[1];
							const EidosASTNode *mean_node = (child_count >= 3) ? expr_node->children_[2] : nullptr;
							const EidosASTNode *sd_node = (child_count >= 4) ? expr_node->children_[3] : nullptr;
							double mean_value = 0.0, sd_value = 1.0;
							
							// resolve named arguments
							if ((x_node->token_->token_type_ == EidosTokenType::kTokenAssign) && (x_node->children_.size() == 2))
							{
								const EidosASTNode *name_node = x_node->children_[0];
								const EidosASTNode *value_node = x_node->children_[1];
								
								if ((name_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (name_node->token_->token_string_ == "x"))
									x_node = value_node;
								else
									opt_dnorm1_candidate = false;
							}
							if (mean_node && (mean_node->token_->token_type_ == EidosTokenType::kTokenAssign) && (mean_node->children_.size() == 2))
							{
								const EidosASTNode *name_node = mean_node->children_[0];
								const EidosASTNode *value_node = mean_node->children_[1];
								
								if ((name_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (name_node->token_->token_string_ == "mean"))
									mean_node = value_node;
								else
									opt_dnorm1_candidate = false;
							}
							if (sd_node && (sd_node->token_->token_type_ == EidosTokenType::kTokenAssign) && (sd_node->children_.size() == 2))
							{
								const EidosASTNode *name_node = sd_node->children_[0];
								const EidosASTNode *value_node = sd_node->children_[1];
								
								if ((name_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (name_node->token_->token_string_ == "sd"))
									sd_node = value_node;
								else
									opt_dnorm1_candidate = false;
							}
							
							// the mean and sd parameters of dnorm can be omitted in the below calls, but if they are given, get their values
							if (mean_node)
							{
								if (mean_node->HasCachedNumericValue())
									mean_value = mean_node->CachedNumericValue();
								else
									opt_dnorm1_candidate = false;
							}
							
							if (sd_node)
							{
								if (sd_node->HasCachedNumericValue())
									sd_value = sd_node->CachedNumericValue();
								else
									opt_dnorm1_candidate = false;
							}
							
							// parse the x argument to dnorm, which can take several different forms
							if (opt_dnorm1_candidate)
							{
								if ((x_node->token_->token_type_ == EidosTokenType::kTokenMinus) && (x_node->children_.size() == 2) && (mean_value == 0.0))
								{
									const EidosASTNode *lhs_node = x_node->children_[0];
									const EidosASTNode *rhs_node = x_node->children_[1];
									const EidosASTNode *dot_node = nullptr, *constant_node = nullptr;
									
									if (lhs_node->token_->token_type_ == EidosTokenType::kTokenDot)
									{
										dot_node = lhs_node;
										constant_node = rhs_node;
									}
									else if (rhs_node->token_->token_type_ == EidosTokenType::kTokenDot)
									{
										dot_node = rhs_node;
										constant_node = lhs_node;
									}
									
									if (dot_node && constant_node && (dot_node->children_.size() == 2) && constant_node->HasCachedNumericValue())
									{
										const EidosASTNode *var_node = dot_node->children_[0];
										const EidosASTNode *prop_node = dot_node->children_[1];
										
										mean_value = constant_node->CachedNumericValue();
										
										if ((var_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (var_node->token_->token_string_ == "individual")
											&& (prop_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (prop_node->token_->token_string_ == "tagF"))
										{
											// callback of the form { return D + dnorm(individual.tagF - A, 0.0, B) / C; }
											// callback of the form { return D + dnorm(individual.tagF - A, 0.0, B); }
											// callback of the form { return D + dnorm(A - individual.tagF, 0.0, B) / C; }
											// callback of the form { return D + dnorm(A - individual.tagF, 0.0, B); }
											p_script_block->has_cached_optimization_ = true;
											p_script_block->has_cached_opt_dnorm1_ = true;
											p_script_block->cached_opt_A_ = mean_value;
											p_script_block->cached_opt_B_ = sd_value;
											p_script_block->cached_opt_C_ = denominator;
											p_script_block->cached_opt_D_ = added_constant;
										}
									}
								}
								else if ((x_node->token_->token_type_ == EidosTokenType::kTokenDot) && (x_node->children_.size() == 2))
								{
									const EidosASTNode *var_node = x_node->children_[0];
									const EidosASTNode *prop_node = x_node->children_[1];
									
									if ((var_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (var_node->token_->token_string_ == "individual")
										&& (prop_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (prop_node->token_->token_string_ == "tagF"))
									{
										// callback of the form { return D + dnorm(individual.tagF, A, B) / C; }
										// callback of the form { return D + dnorm(individual.tagF, A, B); }
										p_script_block->has_cached_optimization_ = true;
										p_script_block->has_cached_opt_dnorm1_ = true;
										p_script_block->cached_opt_A_ = mean_value;
										p_script_block->cached_opt_B_ = sd_value;
										p_script_block->cached_opt_C_ = denominator;
										p_script_block->cached_opt_D_ = added_constant;
									}
								}
							}
						}
					}
				}
			}
			
//			if (p_script_block->has_cached_optimization_)
//				std::cout << "optimized:" << std::endl << "   " << base_node->token_->token_string_ << std::endl;
//			else
//				std::cout << "NOT OPTIMIZED:" << std::endl << "   " << base_node->token_->token_string_ << std::endl;
		}
		else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosFitnessCallback)
		{
			const EidosASTNode *base_node = p_script_block->compound_statement_node_;
			
			if ((base_node->token_->token_type_ == EidosTokenType::kTokenLBrace) && (base_node->children_.size() == 1))
			{
				const EidosASTNode *expr_node = base_node->children_[0];
				
				// we must have an intervening "return", which we jump down through
				if ((expr_node->token_->token_type_ == EidosTokenType::kTokenReturn) && (expr_node->children_.size() == 1))
				{
					expr_node = expr_node->children_[0];
					
					if ((expr_node->token_->token_type_ == EidosTokenType::kTokenDiv) && (expr_node->children_.size() == 2))
					{
						const EidosASTNode *numerator_node = expr_node->children_[0];
						const EidosASTNode *denominator_node = expr_node->children_[1];
						
						if (numerator_node->HasCachedNumericValue())
						{
							double numerator = numerator_node->CachedNumericValue();
							
							if ((denominator_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (denominator_node->token_->token_string_ == "relFitness"))
							{
								// callback of the form { return A/relFitness; }
								p_script_block->has_cached_optimization_ = true;
								p_script_block->has_cached_opt_reciprocal = true;
								p_script_block->cached_opt_A_ = numerator;
							}
						}
					}
				}
			}
			
//			if (p_script_block->has_cached_optimization_)
//				std::cout << "optimized:" << std::endl << "   " << base_node->token_->token_string_ << std::endl;
//			else
//				std::cout << "NOT OPTIMIZED:" << std::endl << "   " << base_node->token_->token_string_ << std::endl;
		}
	}
}

void SLiMSim::AddScriptBlock(SLiMEidosBlock *p_script_block, EidosInterpreter *p_interpreter, const EidosToken *p_error_token)
{
	script_blocks_.emplace_back(p_script_block);
	
	p_script_block->TokenizeAndParse();	// can raise
	
	// The script block passed tokenization and parsing, so it is reasonably well-formed.  Now we check for cases we optimize.
	OptimizeScriptBlock(p_script_block);
	
	// Define the symbol for the script block, if any
	if (p_script_block->block_id_ != -1)
	{
		EidosSymbolTableEntry &symbol_entry = p_script_block->ScriptBlockSymbolTableEntry();
		EidosGlobalStringID symbol_id = symbol_entry.first;
		
		if ((simulation_constants_->ContainsSymbol(symbol_id)) || (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_id)))
			EIDOS_TERMINATION << "ERROR (SLiMSim::AddScriptBlock): script block symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate(p_error_token);
		
		simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	}
	
	// Notify the various interested parties that the script blocks have changed
	last_script_block_gen_cached_ = false;
	script_block_types_cached_ = false;
	scripts_changed_ = true;
	
#if DEBUG_BLOCK_REG_DEREG
	std::cout << "Generation " << generation_ << ": AddScriptBlock() just added a block, script_blocks_ is:" << std::endl;
	for (SLiMEidosBlock *script_block : script_blocks_)
	{
		std::cout << "      ";
		script_block->Print(std::cout);
		std::cout << std::endl;
	}
#endif
}

void SLiMSim::DeregisterScheduledScriptBlocks(void)
{
	// If we have blocks scheduled for deregistration, we sweep through and deregister them at the end of each stage of each generation.
	// This happens at a time when script blocks are not executing, so that we're guaranteed not to leave hanging pointers that could
	// cause a crash; it also guarantees that script blocks are applied consistently across each generation stage.  A single block
	// might be scheduled for deregistration more than once, but should only occur in script_blocks_ once, so we have to be careful
	// with our deallocations here; we deallocate a block only when we find it in script_blocks_.
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_deregistrations_.size())
	{
		std::cout << "Generation " << generation_ << ": DeregisterScheduledScriptBlocks() planning to remove:" << std::endl;
		for (SLiMEidosBlock *script_block : scheduled_deregistrations_)
		{
			std::cout << "      ";
			script_block->Print(std::cout);
			std::cout << std::endl;
		}
	}
#endif
	
	for (SLiMEidosBlock *block_to_dereg : scheduled_deregistrations_)
	{
		auto script_block_position = std::find(script_blocks_.begin(), script_blocks_.end(), block_to_dereg);
		
		if (script_block_position != script_blocks_.end())
		{
#if DEBUG_BLOCK_REG_DEREG
			std::cout << "Generation " << generation_ << ": DeregisterScheduledScriptBlocks() removing block:" << std::endl;
			std::cout << "   ";
			block_to_dereg->Print(std::cout);
			std::cout << std::endl;
#endif
			
			// Remove the symbol for it first
			if (block_to_dereg->block_id_ != -1)
				simulation_constants_->RemoveConstantForSymbol(block_to_dereg->ScriptBlockSymbolTableEntry().first);
			
			// Then remove it from our script block list and deallocate it
			script_blocks_.erase(script_block_position);
			last_script_block_gen_cached_ = false;
			script_block_types_cached_ = false;
			scripts_changed_ = true;
			delete block_to_dereg;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (SLiMSim::DeregisterScheduledScriptBlocks): (internal error) couldn't find block for deregistration." << EidosTerminate();
		}
	}
	
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_deregistrations_.size())
	{
		std::cout << "Generation " << generation_ << ": DeregisterScheduledScriptBlocks() after removal:" << std::endl;
		for (SLiMEidosBlock *script_block : script_blocks_)
		{
			std::cout << "      ";
			script_block->Print(std::cout);
			std::cout << std::endl;
		}
	}
#endif
	
	scheduled_deregistrations_.clear();
}

void SLiMSim::DeregisterScheduledInteractionBlocks(void)
{
	// Identical to DeregisterScheduledScriptBlocks() above, but for the interaction() dereg list; see deregisterScriptBlock()
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_interaction_deregs_.size())
	{
		std::cout << "Generation " << generation_ << ": DeregisterScheduledInteractionBlocks() planning to remove:" << std::endl;
		for (SLiMEidosBlock *script_block : scheduled_interaction_deregs_)
		{
			std::cout << "      ";
			script_block->Print(std::cout);
			std::cout << std::endl;
		}
	}
#endif
	
	for (SLiMEidosBlock *block_to_dereg : scheduled_interaction_deregs_)
	{
		auto script_block_position = std::find(script_blocks_.begin(), script_blocks_.end(), block_to_dereg);
		
		if (script_block_position != script_blocks_.end())
		{
#if DEBUG_BLOCK_REG_DEREG
			std::cout << "Generation " << generation_ << ": DeregisterScheduledInteractionBlocks() removing block:" << std::endl;
			std::cout << "   ";
			block_to_dereg->Print(std::cout);
			std::cout << std::endl;
#endif
			
			// Remove the symbol for it first
			if (block_to_dereg->block_id_ != -1)
				simulation_constants_->RemoveConstantForSymbol(block_to_dereg->ScriptBlockSymbolTableEntry().first);
			
			// Then remove it from our script block list and deallocate it
			script_blocks_.erase(script_block_position);
			last_script_block_gen_cached_ = false;
			script_block_types_cached_ = false;
			scripts_changed_ = true;
			delete block_to_dereg;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (SLiMSim::DeregisterScheduledInteractionBlocks): (internal error) couldn't find block for deregistration." << EidosTerminate();
		}
	}
	
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_interaction_deregs_.size())
	{
		std::cout << "Generation " << generation_ << ": DeregisterScheduledInteractionBlocks() after removal:" << std::endl;
		for (SLiMEidosBlock *script_block : script_blocks_)
		{
			std::cout << "      ";
			script_block->Print(std::cout);
			std::cout << std::endl;
		}
	}
#endif
	
	scheduled_interaction_deregs_.clear();
}

void SLiMSim::ExecuteFunctionDefinitionBlock(SLiMEidosBlock *p_script_block)
{
	EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &SymbolTable());
	EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
	
	EidosInterpreter interpreter(p_script_block->root_node_->children_[0], client_symbols, simulation_functions_, this);
	
	try
	{
		// Interpret the script; the result from the interpretation is not used for anything
		EidosValue_SP result = interpreter.EvaluateInternalBlock(p_script_block->script_);
		
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

void SLiMSim::RunInitializeCallbacks(void)
{
	// zero out the initialization check counts
	num_interaction_types_ = 0;
	num_mutation_types_ = 0;
	num_mutation_rates_ = 0;
	num_genomic_element_types_ = 0;
	num_genomic_elements_ = 0;
	num_recombination_rates_ = 0;
	num_gene_conversions_ = 0;
	num_sex_declarations_ = 0;
	num_options_declarations_ = 0;
	num_treeseq_declarations_ = 0;
	num_modeltype_declarations_ = 0;
	num_ancseq_declarations_ = 0;
	num_hotspot_maps_ = 0;
	
	if (SLiM_verbosity_level >= 1)
		SLIM_OUTSTREAM << "// RunInitializeCallbacks():" << std::endl;
	
	// execute user-defined function blocks first; no need to profile this, it's just the definitions not the executions
	std::vector<SLiMEidosBlock*> function_blocks = ScriptBlocksMatching(-1, SLiMEidosBlockType::SLiMEidosUserDefinedFunction, -1, -1, -1);
	
	for (auto script_block : function_blocks)
		ExecuteFunctionDefinitionBlock(script_block);
	
	// execute initialize() callbacks, which should always have a generation of 0 set
	std::vector<SLiMEidosBlock*> init_blocks = ScriptBlocksMatching(0, SLiMEidosBlockType::SLiMEidosInitializeCallback, -1, -1, -1);
	
	SLiMEidosBlockType old_executing_block_type = executing_block_type_;
	executing_block_type_ = SLiMEidosBlockType::SLiMEidosInitializeCallback;
	
	for (auto script_block : init_blocks)
	{
		if (script_block->active_)
		{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_START();
#endif
			
			population_.ExecuteScript(script_block, generation_, chromosome_);
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END(profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInitializeCallback)]);
#endif
		}
	}
	
	executing_block_type_ = old_executing_block_type;
	
	DeregisterScheduledScriptBlocks();
	
	// we're done with the initialization generation, so remove the zero-gen functions
	RemoveZeroGenerationFunctionsFromMap(simulation_functions_);
	
	// check for complete initialization
	if (!nucleotide_based_ && (num_mutation_rates_ == 0))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one mutation rate interval must be defined in an initialize() callback with initializeMutationRate()." << EidosTerminate();
	if (nucleotide_based_ && (num_mutation_rates_ > 0))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): initializeMutationRate() may not be called in nucleotide-based models (use initializeHotspotMap() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	if (num_mutation_types_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one mutation type must be defined in an initialize() callback with initializeMutationType() (or initializeMutationTypeNuc(), in nucleotide-based models)." << EidosTerminate();
	
	if (num_genomic_element_types_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one genomic element type must be defined in an initialize() callback with initializeGenomicElementType()." << EidosTerminate();
	
	if (num_genomic_elements_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one genomic element must be defined in an initialize() callback with initializeGenomicElement()." << EidosTerminate();
	
	if (num_recombination_rates_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one recombination rate interval must be defined in an initialize() callback with initializeRecombinationRate()." << EidosTerminate();
	
	
	if ((chromosome_.recombination_rates_H_.size() != 0) && ((chromosome_.recombination_rates_M_.size() != 0) || (chromosome_.recombination_rates_F_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): Cannot define both sex-specific and sex-nonspecific recombination rates." << EidosTerminate();
	
	if (((chromosome_.recombination_rates_M_.size() == 0) && (chromosome_.recombination_rates_F_.size() != 0)) ||
		((chromosome_.recombination_rates_M_.size() != 0) && (chromosome_.recombination_rates_F_.size() == 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): Both sex-specific recombination rates must be defined, not just one (but one may be defined as zero)." << EidosTerminate();
	
	
	if ((chromosome_.mutation_rates_H_.size() != 0) && ((chromosome_.mutation_rates_M_.size() != 0) || (chromosome_.mutation_rates_F_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): Cannot define both sex-specific and sex-nonspecific mutation rates." << EidosTerminate();
	
	if (((chromosome_.mutation_rates_M_.size() == 0) && (chromosome_.mutation_rates_F_.size() != 0)) ||
		((chromosome_.mutation_rates_M_.size() != 0) && (chromosome_.mutation_rates_F_.size() == 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): Both sex-specific mutation rates must be defined, not just one (but one may be defined as zero)." << EidosTerminate();
	
	
	if ((chromosome_.hotspot_multipliers_H_.size() != 0) && ((chromosome_.hotspot_multipliers_M_.size() != 0) || (chromosome_.hotspot_multipliers_F_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): Cannot define both sex-specific and sex-nonspecific hotspot maps." << EidosTerminate();
	
	if (((chromosome_.hotspot_multipliers_M_.size() == 0) && (chromosome_.hotspot_multipliers_F_.size() != 0)) ||
		((chromosome_.hotspot_multipliers_M_.size() != 0) && (chromosome_.hotspot_multipliers_F_.size() == 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): Both sex-specific hotspot maps must be defined, not just one (but one may be defined as 1.0)." << EidosTerminate();
	
	
	if (ModelType() == SLiMModelType::kModelTypeNonWF)
	{
		std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
		
		for (auto script_block : script_blocks)
			if (script_block->type_ == SLiMEidosBlockType::SLiMEidosMateChoiceCallback)
				EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): mateChoice() callbacks may not be defined in nonWF models." << EidosTerminate(script_block->identifier_token_);
	}
	if (ModelType() == SLiMModelType::kModelTypeWF)
	{
		std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
		
		for (auto script_block : script_blocks)
			if (script_block->type_ == SLiMEidosBlockType::SLiMEidosReproductionCallback)
				EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): reproduction() callbacks may not be defined in WF models." << EidosTerminate(script_block->identifier_token_);
	}
	if (!sex_enabled_)
	{
		std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
		
		for (auto script_block : script_blocks)
			if ((script_block->type_ == SLiMEidosBlockType::SLiMEidosReproductionCallback) && (script_block->sex_specificity_ != IndividualSex::kUnspecified))
				EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): reproduction() callbacks may not be limited by sex in non-sexual models." << EidosTerminate(script_block->identifier_token_);
	}
	
	if (nucleotide_based_)
	{
		if (num_ancseq_declarations_ == 0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): Nucleotide-based models must provide an ancestral nucleotide sequence with initializeAncestralNucleotides()." << EidosTerminate();
		if (!chromosome_.ancestral_seq_buffer_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): (internal error) No ancestral sequence!" << EidosTerminate();
	}
	
	CheckMutationStackPolicy();
	
	// In nucleotide-based models, process the mutationMatrix parameters for genomic element types, and construct a mutation rate map
	if (nucleotide_based_)
	{
		CacheNucleotideMatrices();
		CreateNucleotideMutationRateMap();
	}
	
	// Defining a neutral mutation type when tree-recording is on (with mutation recording) and the mutation rate is non-zero is legal, but causes a warning
	// I'm not sure this is a good idea, but maybe it will help people avoid doing dumb things; added at the suggestion of Peter Ralph...
	// BCH 26 Jan. 2020; refining the test here so it only logs if the neutral mutation type is used by a genomic element type
	if (recording_tree_ && recording_mutations_)
	{
		bool mut_rate_zero = true;
		
		for (double rate : chromosome_.mutation_rates_H_)
			if (rate != 0.0) { mut_rate_zero = false; break; }
		if (mut_rate_zero)
			for (double rate : chromosome_.mutation_rates_M_)
				if (rate != 0.0) { mut_rate_zero = false; break; }
		if (mut_rate_zero)
			for (double rate : chromosome_.mutation_rates_F_)
				if (rate != 0.0) { mut_rate_zero = false; break; }
		
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
				SLIM_OUTSTREAM << "#WARNING (SLiMSim::RunInitializeCallbacks): with tree-sequence recording enabled and a non-zero mutation rate, a neutral mutation type was defined and used; this is legal, but usually undesirable, since neutral mutations can be overlaid later using the tree-sequence information." << std::endl;
		}
	}
	
	time_start_ = FirstGeneration();	// SLIM_MAX_GENERATION + 1 if it can't find a first block
	
	if (time_start_ == SLIM_MAX_GENERATION + 1)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): No Eidos event found to start the simulation." << EidosTerminate();
	
	// start at the beginning
	SetGeneration(time_start_);
	
	// set up the "sim" symbol now that initialization is complete
	simulation_constants_->InitializeConstantSymbolEntry(SymbolTableEntry());
	
	// initialize chromosome
	chromosome_.InitializeDraws();
	chromosome_.ChooseMutationRunLayout(preferred_mutrun_count_);
	
	// Ancestral sequence check; this has to wait until after the chromosome has been initialized
	if (nucleotide_based_)
	{
		if (chromosome_.ancestral_seq_buffer_->size() != (std::size_t)(chromosome_.last_position_ + 1))
			EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): The chromosome length (" << chromosome_.last_position_ + 1 << " base" << (chromosome_.last_position_ + 1 != 1 ? "s" : "") << ") does not match the ancestral sequence length (" << chromosome_.ancestral_seq_buffer_->size() << " base" << (chromosome_.ancestral_seq_buffer_->size() != 1 ? "s" : "") << ")." << EidosTerminate();
	}
	
	// kick off mutation run experiments, if needed
	InitiateMutationRunExperiments();
	
	// TREE SEQUENCE RECORDING
	if (RecordingTreeSequence())
		AllocateTreeSequenceTables();
	
	// emit our start log
	if (SLiM_verbosity_level >= 1)
		SLIM_OUTSTREAM << "\n// Starting run at generation <start>:\n" << time_start_ << " " << "\n" << std::endl;
}

void SLiMSim::InitiateMutationRunExperiments(void)
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
	if (chromosome_.mutrun_length_ <= SLIM_MUTRUN_MAXIMUM_COUNT)
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
	
	x_current_mutcount_ = chromosome_.mutrun_count_;
	x_current_runtimes_ = (double *)malloc(SLIM_MUTRUN_EXPERIMENT_LENGTH * sizeof(double));
	x_current_buflen_ = 0;
	
	x_previous_mutcount_ = 0;			// marks that no previous experiment has been done
	x_previous_runtimes_ = (double *)malloc(SLIM_MUTRUN_EXPERIMENT_LENGTH * sizeof(double));
	x_previous_buflen_ = 0;
	
	x_continuing_trend_ = false;
	
	x_stasis_limit_ = 5;				// once we reach stasis, we will conduct 5 stasis experiments before exploring again
	x_stasis_alpha_ = 0.01;				// initially, we use an alpha of 0.01 to break out of stasis due to a change in mean
	x_prev1_stasis_mutcount_ = 0;		// we have never reached stasis before, so we have no memory of it
	x_prev2_stasis_mutcount_ = 0;		// we have never reached stasis before, so we have no memory of it
	
	if (SLiM_verbosity_level >= 2)
	{
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "// Mutation run experiments started" << std::endl;
	}
}

void SLiMSim::TransitionToNewExperimentAgainstCurrentExperiment(int32_t p_new_mutrun_count)
{
	// Save off the old experiment
	x_previous_mutcount_ = x_current_mutcount_;
	std::swap(x_current_runtimes_, x_previous_runtimes_);
	x_previous_buflen_ = x_current_buflen_;
	
	// Set up the next experiment
	x_current_mutcount_ = p_new_mutrun_count;
	x_current_buflen_ = 0;
}

void SLiMSim::TransitionToNewExperimentAgainstPreviousExperiment(int32_t p_new_mutrun_count)
{
	// Set up the next experiment
	x_current_mutcount_ = p_new_mutrun_count;
	x_current_buflen_ = 0;
}

void SLiMSim::EnterStasisForMutationRunExperiments(void)
{
	if ((x_current_mutcount_ == x_prev1_stasis_mutcount_) || (x_current_mutcount_ == x_prev2_stasis_mutcount_))
	{
		// One of our recent trips to stasis was at the same count, so we broke stasis incorrectly; get stricter.
		// The purpose for keeping two previous counts is to detect when we are ping-ponging between two values
		// that produce virtually identical performance; we want to detect that and just settle on one of them.
		x_stasis_alpha_ *= 0.5;
		x_stasis_limit_ *= 2;
		
#if MUTRUN_EXPERIMENT_OUTPUT
		if (SLiM_verbose_output)
			SLIM_OUTSTREAM << "// Remembered previous stasis at " << x_current_mutcount_ << ", strengthening stasis criteria" << std::endl;
#endif
	}
	else
	{
		// Our previous trips to stasis were at a different number of mutation runs, so reset our stasis parameters
		x_stasis_limit_ = 5;
		x_stasis_alpha_ = 0.01;
		
#if MUTRUN_EXPERIMENT_OUTPUT
		if (SLiM_verbose_output)
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
	if (SLiM_verbose_output)
		SLIM_OUTSTREAM << "// ****** ENTERING STASIS AT " << x_current_mutcount_ << " : x_stasis_limit_ = " << x_stasis_limit_ << ", x_stasis_alpha_ = " << x_stasis_alpha_ << std::endl;
#endif
}

void SLiMSim::MaintainMutationRunExperiments(double p_last_gen_runtime)
{
	// Log the last generation time into our buffer
	if (x_current_buflen_ >= SLIM_MUTRUN_EXPERIMENT_LENGTH)
		EIDOS_TERMINATION << "ERROR (SLiMSim::MaintainMutationRunExperiments): Buffer overrun, failure to reset after completion of an experiment." << EidosTerminate();
	
	x_current_runtimes_[x_current_buflen_] = p_last_gen_runtime;
	
	// Remember the history of the mutation run count
	x_mutcount_history_.push_back(x_current_mutcount_);
	
	// If the current experiment is not over, continue running it
	++x_current_buflen_;
	
	double current_mean = 0.0, previous_mean = 0.0, p = 0.0;
	
	if ((x_current_buflen_ == 10) && (x_current_mutcount_ != x_previous_mutcount_) && (x_previous_mutcount_ != 0))
	{
		// We want to be able to cut an experiment short if it is clearly a disaster.  So if we're not in stasis, and
		// we've run for 10 generations, and the experiment mean is already different from the baseline at alpha 0.01,
		// and the experiment mean is worse than the baseline mean (if it is better, we want to continue collecting),
		// let's short-circuit the rest of the experiment and bail – like early termination of a medical trial.
		p = Eidos_TTest_TwoSampleWelch(x_current_runtimes_, x_current_buflen_, x_previous_runtimes_, x_previous_buflen_, &current_mean, &previous_mean);
		
		if ((p < 0.01) && (current_mean > previous_mean))
		{
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbose_output)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << generation_ << " : Early t-test yielded HIGHLY SIGNIFICANT p of " << p << " with negative results; terminating early." << std::endl;
			}
#endif
			
			goto early_ttest_passed;
		}
#if MUTRUN_EXPERIMENT_OUTPUT
		else if (SLiM_verbose_output)
		{
			if (p >= 0.01)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << generation_ << " : Early t-test yielded not highly significant p of " << p << "; continuing." << std::endl;
			}
			else if (current_mean > previous_mean)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << generation_ << " : Early t-test yielded highly significant p of " << p << " with positive results; continuing data collection." << std::endl;
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
		if (SLiM_verbose_output)
		{
			SLIM_OUTSTREAM << std::endl;
			SLIM_OUTSTREAM << "// ** " << generation_ << " : First mutation run experiment completed with mutrun count " << x_current_mutcount_ << "; will now try " << (x_current_mutcount_ * 2) << std::endl;
		}
#endif
		
		TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ * 2);
	}
	else
	{
		// If we've just finished the second stasis experiment, run another stasis experiment before trying to draw any
		// conclusions.  We often enter stasis with one generation's worth of data that was actually collected quite a
		// while ago, because we did exploration in both directions first.  This can lead to breaking out of stasis
		// immediately after entering, because we're comparing apples and oranges.  So we avoid doing that here.
		if ((x_stasis_counter_ <= 1) && (x_current_mutcount_ == x_previous_mutcount_))
		{
			TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_);
			++x_stasis_counter_;
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbose_output)
			{
				SLIM_OUTSTREAM << std::endl;
				SLIM_OUTSTREAM << "// " << generation_ << " : Mutation run experiment completed (second stasis generation, no tests conducted)" << std::endl;
			}
#endif
			
			return;
		}
		
		// Otherwise, get a result from a t-test and decide what to do
		p = Eidos_TTest_TwoSampleWelch(x_current_runtimes_, x_current_buflen_, x_previous_runtimes_, x_previous_buflen_, &current_mean, &previous_mean);
		
	early_ttest_passed:
		
#if MUTRUN_EXPERIMENT_OUTPUT
		if (SLiM_verbose_output)
		{
			SLIM_OUTSTREAM << std::endl;
			SLIM_OUTSTREAM << "// " << generation_ << " : Mutation run experiment completed:" << std::endl;
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
			if (SLiM_verbose_output)
				SLIM_OUTSTREAM << "//    p == " << p << " : " << (means_different_stasis ? "SIGNIFICANT DIFFERENCE" : "no significant difference") << " at stasis alpha " << x_stasis_alpha_ << std::endl;
#endif
			
			if (means_different_stasis)
			{
				// OK, it looks like something has changed about our scenario, so we should come out of stasis and re-test.
				// We don't have any information about the new state of affairs, so we have no directional preference.
				// Let's try a larger number of mutation runs first, since genomes tend to fill up, unless we're at the max.
				if (x_current_mutcount_ >= SLIM_MUTRUN_MAXIMUM_COUNT)
					TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ / 2);
				else
					TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ * 2);
				
#if MUTRUN_EXPERIMENT_OUTPUT
				if (SLiM_verbose_output)
					SLIM_OUTSTREAM << "// ** " << generation_ << " : Stasis mean changed, EXITING STASIS and trying new mutcount of " << x_current_mutcount_ << std::endl;
#endif
			}
			else
			{
				// We seem to be in a constant scenario.  Increment our stasis counter and see if we have reached our stasis limit
				if (++x_stasis_counter_ >= x_stasis_limit_)
				{
					// We reached the stasis limit, so we will try an experiment even though we don't seem to have changed;
					// as before, we try more mutation runs first, since increasing genetic complexity is typical
					if (x_current_mutcount_ >= SLIM_MUTRUN_MAXIMUM_COUNT)
						TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ / 2);
					else
						TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_ * 2);
					
#if MUTRUN_EXPERIMENT_OUTPUT
					if (SLiM_verbose_output)
						SLIM_OUTSTREAM << "// ** " << generation_ << " : Stasis limit reached, EXITING STASIS and trying new mutcount of " << x_current_mutcount_ << std::endl;
#endif
				}
				else
				{
					// We have not yet reached the stasis limit, so run another stasis experiment.
					// In this case we don't do a transition; we want to continue comparing against the original experiment
					// data so that if stasis slowly drift away from us, we eventually detect that as a change in stasis.
					x_current_buflen_ = 0;
					
#if MUTRUN_EXPERIMENT_OUTPUT
					if (SLiM_verbose_output)
						SLIM_OUTSTREAM << "//    " << generation_ << " : Stasis limit not reached (" << x_stasis_counter_ << " of " << x_stasis_limit_ << "), running another stasis experiment at " << x_current_mutcount_ << std::endl;
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
			if (SLiM_verbose_output)
				SLIM_OUTSTREAM << "//    p == " << p << " : " << (means_different_05 ? "SIGNIFICANT DIFFERENCE" : "no significant difference") << " at alpha " << alpha << std::endl;
#endif
			
			int32_t trend_next = (x_current_mutcount_ < x_previous_mutcount_) ? (x_current_mutcount_ / 2) : (x_current_mutcount_ * 2);
			int32_t trend_limit = (x_current_mutcount_ < x_previous_mutcount_) ? 1 : SLIM_MUTRUN_MAXIMUM_COUNT;
			
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
				if (x_current_mutcount_ == trend_limit)
				{
					if (current_mean < previous_mean)
					{
						// Can't go beyond the trend limit (1 or SLIM_MUTRUN_MAXIMUM_COUNT), so we're done; ****** ENTER STASIS
						// We keep the current experiment as the first stasis experiment.
						TransitionToNewExperimentAgainstCurrentExperiment(x_current_mutcount_);
						
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbose_output)
							SLIM_OUTSTREAM << "// ****** " << generation_ << " : Experiment " << (means_different_05 ? "successful" : "inconclusive but positive") << " at " << x_previous_mutcount_ << ", nowhere left to go; entering stasis at " << x_current_mutcount_ << "." << std::endl;
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
						if (SLiM_verbose_output)
							SLIM_OUTSTREAM << "// ****** " << generation_ << " : Experiment " << (means_different_05 ? "failed" : "inconclusive but negative") << " at " << x_previous_mutcount_ << ", nowhere left to go; entering stasis at " << x_current_mutcount_ << "." << std::endl;
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
						if (SLiM_verbose_output)
							SLIM_OUTSTREAM << "// ** " << generation_ << " : Experiment " << (means_different_05 ? "successful" : "inconclusive but positive") << " at " << x_current_mutcount_ << " (against " << x_previous_mutcount_ << "), continuing trend with " << trend_next << " (against " << x_current_mutcount_ << ")" << std::endl;
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
						if (SLiM_verbose_output)
							SLIM_OUTSTREAM << "// ** " << generation_ << " : Experiment inconclusive but negative at " << x_current_mutcount_ << " (against " << x_previous_mutcount_ << "), checking " << trend_next << " (against " << x_previous_mutcount_ << ")" << std::endl;
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
					if (SLiM_verbose_output)
						SLIM_OUTSTREAM << "// ****** " << generation_ << " : Experiment failed, already tried opposite side, so " << x_current_mutcount_ << " appears optimal; entering stasis at " << x_current_mutcount_ << "." << std::endl;
#endif
					
					EnterStasisForMutationRunExperiments();
				}
				else
				{
					// We have not tried a step on the opposite side of the old position; let's return to our old position,
					// which we know is better than the position we just ran an experiment at, and then advance onward to
					// run an experiment at the next position in that reversed trend direction.
					
					if ((x_previous_mutcount_ == 1) || (x_previous_mutcount_ == SLIM_MUTRUN_MAXIMUM_COUNT))
					{
						// can't jump over the previous mutcount, so we enter stasis at it
						TransitionToNewExperimentAgainstPreviousExperiment(x_previous_mutcount_);
						
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbose_output)
							SLIM_OUTSTREAM << "// ****** " << generation_ << " : Experiment failed, opposite side blocked so " << x_current_mutcount_ << " appears optimal; entering stasis at " << x_current_mutcount_ << "." << std::endl;
#endif
						
						EnterStasisForMutationRunExperiments();
					}
					else
					{
						int32_t new_mutcount = ((x_current_mutcount_ > x_previous_mutcount_) ? (x_previous_mutcount_ / 2) : (x_previous_mutcount_ * 2));
						
#if MUTRUN_EXPERIMENT_OUTPUT
						if (SLiM_verbose_output)
							SLIM_OUTSTREAM << "// ** " << generation_ << " : Experiment failed at " << x_current_mutcount_ << ", opposite side untried, reversing trend back to " << new_mutcount << " (against " << x_previous_mutcount_ << ")" << std::endl;
#endif
						
						TransitionToNewExperimentAgainstPreviousExperiment(new_mutcount);
						x_continuing_trend_ = true;
					}
				}
			}
		}
	}
	
	// Promulgate the new mutation run count
	if (x_current_mutcount_ != chromosome_.mutrun_count_)
	{
		// Fix all genomes.  We could do this by brute force, by making completely new mutation runs for every
		// existing genome and then calling Population::UniqueMutationRuns(), but that would be inefficient,
		// and would also cause a huge memory usage spike.  Instead, we want to preserve existing redundancy.
		
		while (x_current_mutcount_ > chromosome_.mutrun_count_)
		{
#if MUTRUN_EXPERIMENT_OUTPUT
			std::clock_t start_clock = std::clock();
#endif
			
			// We are splitting existing runs in two, so make a map from old mutrun index to new pair of
			// mutrun indices; every time we encounter the same old index we will substitute the same pair.
			population_.SplitMutationRuns(chromosome_.mutrun_count_ * 2);
			
			// Fix the chromosome values
			chromosome_.mutrun_count_ *= 2;
			chromosome_.mutrun_length_ /= 2;
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbose_output)
				SLIM_OUTSTREAM << "// ++ Splitting to achieve new mutation run count of " << chromosome_.mutrun_count_ << " took " << ((std::clock() - start_clock) / (double)CLOCKS_PER_SEC) << " seconds" << std::endl;
#endif
		}
		
		while (x_current_mutcount_ < chromosome_.mutrun_count_)
		{
#if MUTRUN_EXPERIMENT_OUTPUT
			std::clock_t start_clock = std::clock();
#endif
			
			// We are joining existing runs together, so make a map from old mutrun index pairs to a new
			// index; every time we encounter the same pair of indices we will substitute the same index.
			population_.JoinMutationRuns(chromosome_.mutrun_count_ / 2);
			
			// Fix the chromosome values
			chromosome_.mutrun_count_ /= 2;
			chromosome_.mutrun_length_ *= 2;
			
#if MUTRUN_EXPERIMENT_OUTPUT
			if (SLiM_verbose_output)
				SLIM_OUTSTREAM << "// ++ Joining to achieve new mutation run count of " << chromosome_.mutrun_count_ << " took " << ((std::clock() - start_clock) / (double)CLOCKS_PER_SEC) << " seconds" << std::endl;
#endif
		}
		
		if (chromosome_.mutrun_count_ != x_current_mutcount_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::MaintainMutationRunExperiments): Failed to transition to new mutation run count" << x_current_mutcount_ << "." << EidosTerminate();
	}
}

#if defined(SLIMGUI) && (SLIMPROFILING == 1)
// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
void SLiMSim::CollectSLiMguiMutationProfileInfo(void)
{
	// maintain our history of the number of mutruns per genome and the nonneutral regime
	profile_mutcount_history_.push_back(chromosome_.mutrun_count_);
	profile_nonneutral_regime_history_.push_back(last_nonneutral_regime_);
	
	// track the maximum number of mutations in existence at one time
	int registry_size;
	
	population_.MutationRegistry(&registry_size);
	profile_max_mutation_index_ = std::max(profile_max_mutation_index_, (int64_t)registry_size);
	
	// tally up the number of mutation runs, mutation usage metrics, etc.
	int64_t operation_id = ++gSLiM_MutationRun_OperationID;
	
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		std::vector<Genome *> &subpop_genomes = subpop->parent_genomes_;
		
		for (Genome *genome : subpop_genomes)
		{
			MutationRun_SP *mutruns = genome->mutruns_;
			int32_t mutrun_count = genome->mutrun_count_;
			
			profile_mutrun_total_usage_ += mutrun_count;
			
			for (int32_t mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				MutationRun *mutrun = mutruns[mutrun_index].get();
				
				if (mutrun)
				{
					if (mutrun->operation_id_ != operation_id)
					{
						mutrun->operation_id_ = operation_id;
						profile_unique_mutrun_total_++;
					}
					
					// tally the total and nonneutral mutations
					mutrun->tally_nonneutral_mutations(&profile_mutation_total_usage_, &profile_nonneutral_mutation_total_, &profile_mutrun_nonneutral_recache_total_);
				}
			}
		}
	}
}
#endif
#endif

slim_generation_t SLiMSim::FirstGeneration(void)
{
	slim_generation_t first_gen = SLIM_MAX_GENERATION + 1;
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	
	// Figure out our first generation; it is the earliest generation in which an Eidos event is set up to run,
	// since an Eidos event that adds a subpopulation is necessary to get things started
	for (auto script_block : script_blocks)
		if (((script_block->type_ == SLiMEidosBlockType::SLiMEidosEventEarly) || (script_block->type_ == SLiMEidosBlockType::SLiMEidosEventLate))
			&& (script_block->start_generation_ < first_gen) && (script_block->start_generation_ > 0))
			first_gen = script_block->start_generation_;
	
	return first_gen;
}

slim_generation_t SLiMSim::EstimatedLastGeneration(void)
{
	// return our cached value if we have one
	if (last_script_block_gen_cached_)
		return last_script_block_gen_;
	
	// otherwise, fill the cache
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	slim_generation_t last_gen = 1;
	
	// The estimate is derived from the last generation in which an Eidos block is registered.
	// Any block type works, since the simulation could plausibly be stopped within a callback.
	// However, blocks that do not specify an end generation don't count.
	for (auto script_block : script_blocks)
		if ((script_block->end_generation_ > last_gen) && (script_block->end_generation_ != SLIM_MAX_GENERATION + 1))
			last_gen = script_block->end_generation_;
	
	last_script_block_gen_ = last_gen;
	last_script_block_gen_cached_ = true;
	
	return last_script_block_gen_;
}

void SLiMSim::SetGeneration(slim_generation_t p_new_generation)
{
	generation_ = p_new_generation;
	cached_value_generation_.reset();
	
	// The tree sequence generation increments when offspring generation occurs, not at the ends of generations as delineated by SLiM.
	// This prevents the tree sequence code from seeing two "generations" with the same value for the generation counter.
	if ((ModelType() == SLiMModelType::kModelTypeWF) && (GenerationStage() < SLiMGenerationStage::kWFStage2GenerateOffspring))
		tree_seq_generation_ = generation_ - 1;
	else
		tree_seq_generation_ = generation_;
	
	tree_seq_generation_offset_ = 0;
}

// This function is called only by the SLiM self-testing machinery.  It has no exception handling; raises will
// blow through to the catch block in the test harness so that they can be handled there.
bool SLiMSim::_RunOneGeneration(void)
{
	// ******************************************************************
	//
	// Stage 0: Pre-generation bookkeeping
	//
	generation_stage_ = SLiMGenerationStage::kStage0PreGeneration;
	
	// Define the current script around each generation execution, for error reporting
	gEidosCurrentScript = script_;
	gEidosExecutingRuntimeScript = false;
	
	// Activate all registered script blocks at the beginning of the generation
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
		script_block->active_ = -1;
	
	if (generation_ == 0)
	{
		// The zero generation is handled here by shared code, since it is the same for WF and nonWF models
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		RunInitializeCallbacks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[0]);
#endif
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		if (gEidosProfilingClientCount)
			CollectSLiMguiMemoryUsageProfileInfo();
#endif
		
		return true;
	}
	else
	{
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		// Optimization; see mutation_type.h for an explanation of what this counter is used for
		if (population_.any_muttype_call_count_used_)
		{
			for (auto muttype_iter : mutation_types_)
				(muttype_iter.second)->muttype_registry_call_count_ = 0;
			
			population_.any_muttype_call_count_used_ = false;
		}
#endif
		
		// Non-zero generations are handled by separate functions for WF and nonWF models
		
#if defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY)
		if (model_type_ == SLiMModelType::kModelTypeWF)
			return _RunOneGenerationWF();
		else
			return _RunOneGenerationNonWF();
#elif defined(SLIM_WF_ONLY)
		return _RunOneGenerationWF();
#elif defined(SLIM_NONWF_ONLY)
		return _RunOneGenerationNonWF();
#endif
	}
}

#ifdef SLIM_WF_ONLY
//
//		_RunOneGenerationWF() : runs all the stages for one generation of a WF model
//
bool SLiMSim::_RunOneGenerationWF(void)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	if (gEidosProfilingClientCount)
		CollectSLiMguiMutationProfileInfo();
#endif
#endif
	
	// make a clock if we're running experiments
	std::clock_t x_clock0 = (x_experiments_enabled_ ? std::clock() : 0);
	
	
	// ******************************************************************
	//
	// Stage 1: Execute early() script events for the current generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		//std::cout << "WF early() events, generation_ == " << generation_ << ", tree_seq_generation_ == " << tree_seq_generation_ << std::endl;
		
		generation_stage_ = SLiMGenerationStage::kWFStage1ExecuteEarlyScripts;
		
		std::vector<SLiMEidosBlock*> early_blocks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosEventEarly, -1, -1, -1);
		
		SLiMEidosBlockType old_executing_block_type = executing_block_type_;
		executing_block_type_ = SLiMEidosBlockType::SLiMEidosEventEarly;
		
		for (auto script_block : early_blocks)
		{
			if (script_block->active_)
			{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_START_NESTED();
#endif
				
				population_.ExecuteScript(script_block, generation_, chromosome_);
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_END_NESTED(profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventEarly)]);
#endif
			}
		}
		
		executing_block_type_ = old_executing_block_type;
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[1]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 2: Generate offspring: evolve all subpopulations
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		CheckMutationStackPolicy();
		
		generation_stage_ = SLiMGenerationStage::kWFStage2GenerateOffspring;
		
		// increment the tree-sequence generation immediately, since we are now going to make a new generation of individuals
		tree_seq_generation_++;
		tree_seq_generation_offset_ = 0;
		// note that generation_ is incremented later!
		
		std::vector<SLiMEidosBlock*> mate_choice_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosMateChoiceCallback, -1, -1, -1);
		std::vector<SLiMEidosBlock*> modify_child_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosModifyChildCallback, -1, -1, -1);
		std::vector<SLiMEidosBlock*> recombination_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, -1);
		std::vector<SLiMEidosBlock*> mutation_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosMutationCallback, -1, -1, -1);
		bool mate_choice_callbacks_present = mate_choice_callbacks.size();
		bool modify_child_callbacks_present = modify_child_callbacks.size();
		bool recombination_callbacks_present = recombination_callbacks.size();
		bool mutation_callbacks_present = mutation_callbacks.size();
		bool no_active_callbacks = true;
		
		// if there are no active callbacks of any type, we can pretend there are no callbacks at all
		// if there is a callback of any type, however, then inactive callbacks could become active
		if (mate_choice_callbacks_present || modify_child_callbacks_present || recombination_callbacks_present || mutation_callbacks_present)
		{
			for (SLiMEidosBlock *callback : mate_choice_callbacks)
				if (callback->active_)
				{
					no_active_callbacks = false;
					break;
				}
			
			if (no_active_callbacks)
				for (SLiMEidosBlock *callback : modify_child_callbacks)
					if (callback->active_)
					{
						no_active_callbacks = false;
						break;
					}
			
			if (no_active_callbacks)
				for (SLiMEidosBlock *callback : recombination_callbacks)
					if (callback->active_)
					{
						no_active_callbacks = false;
						break;
					}
			
			if (no_active_callbacks)
				for (SLiMEidosBlock *callback : mutation_callbacks)
					if (callback->active_)
					{
						no_active_callbacks = false;
						break;
					}
		}
		
		if (no_active_callbacks)
		{
			for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
				population_.EvolveSubpopulation(*subpop_pair.second, false, false, false, false);
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
				population_.EvolveSubpopulation(*subpop_pair.second, mate_choice_callbacks_present, modify_child_callbacks_present, recombination_callbacks_present, mutation_callbacks_present);
		}
		
		// then switch to the child generation; we don't want to do this until all callbacks have executed for all subpops
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			subpop_pair.second->child_generation_valid_ = true;
		
		population_.child_generation_valid_ = true;
		
		// added 30 November 2016 so MutationRun refcounts reflect their usage count in the simulation
		// moved up to SLiMGenerationStage::kWFStage2GenerateOffspring, 9 January 2018, so that the
		// population is in a standard state for CheckIndividualIntegrity() at the end of this stage
		population_.ClearParentalGenomes();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[2]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 3: Remove fixed mutations and associated tasks
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kWFStage3RemoveFixedMutations;
		
		population_.MaintainRegistry();
		
		// Every hundredth generation we unique mutation runs to optimize memory usage and efficiency.  The number 100 was
		// picked out of a hat – often enough to perhaps be useful in keeping SLiM slim, but infrequent enough that if it
		// is a time sink it won't impact the simulation too much.  This call is really quite fast, though – on the order
		// of 0.015 seconds for a pop of 10000 with a 1e5 chromosome and lots of mutations.  So although doing this every
		// generation would seem like overkill – very few duplicates would be found per call – every 100 should be fine.
		// Anyway, if we start seeing this call in performance analysis, we should probably revisit this; the benefit is
		// likely to be pretty small for most simulations, so if the cost is significant then it may be a lose.
		if (generation_ % 100 == 0)
			population_.UniqueMutationRuns();
		
		// Invalidate interactions, now that the generation they were valid for is disappearing
        for (auto iter : interaction_types_)
			iter.second->Invalidate();
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[3]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 4: Swap generations
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kWFStage4SwapGenerations;
		
		population_.SwapGenerations();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[4]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 5: Execute late() script events for the current generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kWFStage5ExecuteLateScripts;
		
		//std::cout << "WF late() events, generation_ == " << generation_ << ", tree_seq_generation_ == " << tree_seq_generation_ << std::endl;
		
		std::vector<SLiMEidosBlock*> late_blocks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosEventLate, -1, -1, -1);
		
		SLiMEidosBlockType old_executing_block_type = executing_block_type_;
		executing_block_type_ = SLiMEidosBlockType::SLiMEidosEventLate;
		
		for (auto script_block : late_blocks)
		{
			if (script_block->active_)
			{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_START_NESTED();
#endif
				
				population_.ExecuteScript(script_block, generation_, chromosome_);
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_END_NESTED(profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventLate)]);
#endif
			}
		}
		
		executing_block_type_ = old_executing_block_type;
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[5]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 6: Calculate fitness values for the new parental generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kWFStage6CalculateFitness;
		
		population_.RecalculateFitness(generation_);	// used to be generation_ + 1; removing that 18 Feb 2016 BCH
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Maintain our mutation run experiments; we want this overhead to appear within the stage 6 profile
		if (x_experiments_enabled_)
			MaintainMutationRunExperiments((std::clock() - x_clock0) / (double)CLOCKS_PER_SEC);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[6]);
#endif
		
#ifdef SLIMGUI
		// Let SLiMgui survey the population for mean fitness and such, if it is our target
		// We do this outside of profiling and mutation run experiments, since SLiMgui overhead should not affect those
		population_.SurveyPopulation();
#endif
	}
	
	
	// ******************************************************************
	//
	// Stage 7: Advance the generation counter and do end-generation tasks
	//
	{
		generation_stage_ = SLiMGenerationStage::kWFStage7AdvanceGenerationCounter;
		
#ifdef SLIMGUI
		// re-tally for SLiMgui; this should avoid doing any new work if no mutations have been added or removed since the last tally
		// it is needed, though, so that if the user added/removed mutations in a late() event SLiMgui displays correctly
		// NOTE that this means tallies may be different in SLiMgui than in slim!  I *think* this will never be visible to the
		// user's model, because if they ask for mutation counts/frequences a call to TallyMutationReferences() will be made at that
		// point anyway to synchronize; but in slim's code itself, not in Eidos, the tallies can definitely differ!  Beware!
		population_.TallyMutationReferences(nullptr, false);
#endif
	
		// TREE SEQUENCE RECORDING
		if (recording_tree_)
		{
#if DEBUG
			// check the integrity of the tree sequence in every generation in Debug mode only
			CheckTreeSeqIntegrity();
#endif
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_START();
#endif
			
			CheckAutoSimplification();
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END(profile_stage_totals_[7]);
#endif
			
			// note that this causes simplification, so it will confuse the auto-simplification code
			if (running_treeseq_crosschecks_ && (generation_ % treeseq_crosschecks_interval_ == 0))
				CrosscheckTreeSeqIntegrity();
		}
		
		cached_value_generation_.reset();
		generation_++;
		// note that tree_seq_generation_ was incremented earlier!
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		if (gEidosProfilingClientCount)
			CollectSLiMguiMemoryUsageProfileInfo();
#endif
		
		// Decide whether the simulation is over.  We need to call EstimatedLastGeneration() every time; we can't
		// cache it, because it can change based upon changes in script registration / deregistration.
		bool result;
		
		if (sim_declared_finished_)
			result = false;
		else
			result = (generation_ <= EstimatedLastGeneration());
		
		if (!result)
			SimulationFinished();
		
		return result;
	}
}
#endif	// SLIM_WF_ONLY

#ifdef SLIM_NONWF_ONLY
//
//		_RunOneGenerationNonWF() : runs all the stages for one generation of a nonWF model
//
bool SLiMSim::_RunOneGenerationNonWF(void)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	if (gEidosProfilingClientCount)
		CollectSLiMguiMutationProfileInfo();
#endif
#endif
	
	// make a clock if we're running experiments
	std::clock_t x_clock0 = (x_experiments_enabled_ ? std::clock() : 0);
	
	
	// ******************************************************************
	//
	// Stage 1: Generate offspring: call reproduce() callbacks
	//
	{
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
		// zero out offspring counts used for SLiMgui's display
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			subpop->gui_offspring_cloned_M_ = 0;
			subpop->gui_offspring_cloned_F_ = 0;
			subpop->gui_offspring_selfed_ = 0;
			subpop->gui_offspring_crossed_ = 0;
			subpop->gui_offspring_empty_ = 0;
		}
		
		// zero out migration counts used for SLiMgui's display
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			subpop->gui_premigration_size_ = subpop->parent_subpop_size_;
			subpop->gui_migrants_.clear();
		}
#endif
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		CheckMutationStackPolicy();
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage1GenerateOffspring;
		
		std::vector<SLiMEidosBlock*> reproduction_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosReproductionCallback, -1, -1, -1);
		std::vector<SLiMEidosBlock*> modify_child_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosModifyChildCallback, -1, -1, -1);
		std::vector<SLiMEidosBlock*> recombination_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, -1);
		std::vector<SLiMEidosBlock*> mutation_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosMutationCallback, -1, -1, -1);
		
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
		SLiMEidosBlockType old_executing_block_type = executing_block_type_;
		executing_block_type_ = SLiMEidosBlockType::SLiMEidosReproductionCallback;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			subpop_pair.second->ReproduceSubpopulation();
		
		executing_block_type_ = old_executing_block_type;
		
		// Invalidate interactions, now that the generation they were valid for is disappearing
        for (auto iter : interaction_types_)
			iter.second->Invalidate();
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
		// then merge in the generated offspring; we don't want to do this until all callbacks have executed for all subpops
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			subpop_pair.second->MergeReproductionOffspring();
		
		// clear the "migrant" property on all individuals
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			for (Individual *individual : subpop_pair.second->parent_individuals_)
				individual->migrant_ = false;
		
		// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
		population_.cached_tally_genome_count_ = 0;
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[1]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 2: Execute early() script events for the current generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts;
		
		//std::cout << "nonWF early() events, generation_ == " << generation_ << ", tree_seq_generation_ == " << tree_seq_generation_ << std::endl;
		
		std::vector<SLiMEidosBlock*> early_blocks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosEventEarly, -1, -1, -1);
		
		SLiMEidosBlockType old_executing_block_type = executing_block_type_;
		executing_block_type_ = SLiMEidosBlockType::SLiMEidosEventEarly;
		
		for (auto script_block : early_blocks)
		{
			if (script_block->active_)
			{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_START_NESTED();
#endif
				
				population_.ExecuteScript(script_block, generation_, chromosome_);
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_END_NESTED(profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventEarly)]);
#endif
			}
		}
		
		executing_block_type_ = old_executing_block_type;
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[2]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 3: Calculate fitness values for the new generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage3CalculateFitness;
		
		population_.RecalculateFitness(generation_);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Invalidate interactions, now that the generation they were valid for is disappearing
        for (auto iter : interaction_types_)
			iter.second->Invalidate();
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[3]);
#endif
	}
	
	
	// ******************************************************************
	//
	// Stage 4: Viability/survival selection
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage4SurvivalSelection;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			subpop_pair.second->ViabilitySelection();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[4]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 5: Remove fixed mutations and associated tasks
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage5RemoveFixedMutations;
		
		population_.MaintainRegistry();
		
		// Every hundredth generation we unique mutation runs to optimize memory usage and efficiency.  The number 100 was
		// picked out of a hat – often enough to perhaps be useful in keeping SLiM slim, but infrequent enough that if it
		// is a time sink it won't impact the simulation too much.  This call is really quite fast, though – on the order
		// of 0.015 seconds for a pop of 10000 with a 1e5 chromosome and lots of mutations.  So although doing this every
		// generation would seem like overkill – very few duplicates would be found per call – every 100 should be fine.
		// Anyway, if we start seeing this call in performance analysis, we should probably revisit this; the benefit is
		// likely to be pretty small for most simulations, so if the cost is significant then it may be a lose.
		if (generation_ % 100 == 0)
			population_.UniqueMutationRuns();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[5]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 6: Execute late() script events for the current generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage6ExecuteLateScripts;
		
		//std::cout << "nonWF late() events, generation_ == " << generation_ << ", tree_seq_generation_ == " << tree_seq_generation_ << std::endl;
		
		std::vector<SLiMEidosBlock*> late_blocks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosEventLate, -1, -1, -1);
		
		SLiMEidosBlockType old_executing_block_type = executing_block_type_;
		executing_block_type_ = SLiMEidosBlockType::SLiMEidosEventLate;
		
		for (auto script_block : late_blocks)
		{
			if (script_block->active_)
			{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_START_NESTED();
#endif
				
				population_.ExecuteScript(script_block, generation_, chromosome_);
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_END_NESTED(profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventLate)]);
#endif
			}
		}
		
		executing_block_type_ = old_executing_block_type;
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Maintain our mutation run experiments; we want this overhead to appear within the stage 6 profile
		if (x_experiments_enabled_)
			MaintainMutationRunExperiments((std::clock() - x_clock0) / (double)CLOCKS_PER_SEC);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[6]);
#endif
	}
	
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		subpop_pair.second->CheckIndividualIntegrity();
#endif
	
	
	// ******************************************************************
	//
	// Stage 7: Advance the generation counter and do end-generation tasks
	//
	{
		generation_stage_ = SLiMGenerationStage::kNonWFStage7AdvanceGenerationCounter;
		
#ifdef SLIMGUI
		// Let SLiMgui survey the population for mean fitness and such, if it is our target
		// We do this outside of profiling and mutation run experiments, since SLiMgui overhead should not affect those
		population_.SurveyPopulation();
#endif
		
#ifdef SLIMGUI
		// re-tally for SLiMgui; this should avoid doing any new work if no mutations have been added or removed since the last tally
		// it is needed, though, so that if the user added/removed mutations in a late() event SLiMgui displays correctly
		// NOTE that this means tallies may be different in SLiMgui than in slim!  I *think* this will never be visible to the
		// user's model, because if they ask for mutation counts/frequences a call to TallyMutationReferences() will be made at that
		// point anyway to synchronize; but in slim's code itself, not in Eidos, the tallies can definitely differ!  Beware!
		population_.TallyMutationReferences(nullptr, false);
#endif
	
		// TREE SEQUENCE RECORDING
		if (recording_tree_)
		{
#if DEBUG
			// check the integrity of the tree sequence in every generation in Debug mode only
			CheckTreeSeqIntegrity();
#endif
									
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_START();
#endif
			
			CheckAutoSimplification();
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END(profile_stage_totals_[7]);
#endif
			
			// note that this causes simplification, so it will confuse the auto-simplification code
			if (running_treeseq_crosschecks_ && (generation_ % treeseq_crosschecks_interval_ == 0))
				CrosscheckTreeSeqIntegrity();
		}
		
		cached_value_generation_.reset();
		generation_++;
		tree_seq_generation_++;
		tree_seq_generation_offset_ = 0;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			subpop_pair.second->IncrementIndividualAges();
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		if (gEidosProfilingClientCount)
			CollectSLiMguiMemoryUsageProfileInfo();
#endif
		
		// Decide whether the simulation is over.  We need to call EstimatedLastGeneration() every time; we can't
		// cache it, because it can change based upon changes in script registration / deregistration.
		bool result;
		
		if (sim_declared_finished_)
			result = false;
		else
			result = (generation_ <= EstimatedLastGeneration());
		
		if (!result)
			SimulationFinished();
		
		return result;
	}
}
#endif  // SLIM_NONWF_ONLY

// This function is called by both SLiM and SLiMgui to run a generation.  In SLiM, it simply calls _RunOneGeneration(),
// with no exception handling; in that scenario exceptions should not be thrown, since EidosTerminate() will log an
// error and then call exit().  In SLiMgui, EidosTerminate() will raise an exception, and it will be caught right
// here and converted to an "invalid simulation" state (simulation_valid_ == false), which will be noticed by SLiMgui
// and will cause error reporting to occur based upon the error-tracking variables set.
bool SLiMSim::RunOneGeneration(void)
{
#ifdef SLIMGUI
	if (simulation_valid_)
	{
		try
		{
#endif
			return _RunOneGeneration();
#ifdef SLIMGUI
		}
		catch (...)
		{
			simulation_valid_ = false;
			
			// In the event of a raise, we clear gEidosCurrentScript, which is not normally part of the error-
			// reporting state, but is used only to inform EidosTerminate() about the current script at the point
			// when a raise occurs.  We don't want raises after RunOneGeneration() returns to be attributed to us,
			// so we clear the script pointer.  We do NOT clear any of the error-reporting state, since it will
			// be used by higher levels to select the error in the GUI.
			gEidosCurrentScript = nullptr;
			return false;
		}
	}
	
	gEidosCurrentScript = nullptr;
#endif
	
	return false;
}

void SLiMSim::SimulationFinished(void)
{
	// This is an opportunity for final calculation/output when a simulation finishes
	
#if MUTRUN_EXPERIMENT_OUTPUT
	// Print a full mutation run count history if MUTRUN_EXPERIMENT_OUTPUT is enabled
	if (SLiM_verbose_output && x_experiments_enabled_)
	{
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "// Mutrun count history:" << std::endl;
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
	if ((SLiM_verbosity_level >= 2) && x_experiments_enabled_)
	{
		int modal_index, modal_tally;
		int power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
		
		for (int i = 0; i < 20; ++i)
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
		
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "// Mutation run modal count: " << modal_count << " (" << (modal_fraction * 100) << "% of generations)" << std::endl;
		SLIM_OUTSTREAM << "//" << std::endl;
		SLIM_OUTSTREAM << "// It might (or might not) speed up your model to add a call to:" << std::endl;
		SLIM_OUTSTREAM << "//" << std::endl;
		SLIM_OUTSTREAM << "//    initializeSLiMOptions(mutationRuns=" << modal_count << ");" << std::endl;
		SLIM_OUTSTREAM << "//" << std::endl;
		SLIM_OUTSTREAM << "// to your initialize() callback.  The optimal value will change" << std::endl;
		SLIM_OUTSTREAM << "// if your model changes.  See the SLiM manual for more details." << std::endl;
		SLIM_OUTSTREAM << std::endl;
	}
}

void SLiMSim::_CheckMutationStackPolicy(void)
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
						EIDOS_TERMINATION << "ERROR (SLiMSim::_CheckMutationStackPolicy): inconsistent mutationStackPolicy values within one mutationStackGroup." << EidosTerminate();
				}
				
				checked_groups.push_back(stack_group);
			}
		}
	}
	
	// we're good until the next change
	mutation_stack_policy_changed_ = false;
}

void SLiMSim::CacheNucleotideMatrices(void)
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
			EidosValue_Float_vector *mm = ge_type->mutation_matrix_.get();
			double *mm_data = mm->data();
			
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
				EIDOS_TERMINATION << "ERROR (SLiMSim::CacheNucleotideMatrices): (internal error) unsupported mutation matrix size." << EidosTerminate();
		}
	}
	
	// Now go through the genomic element types again, and calculate normalized mutation rate
	// threshold values that will allow fast decisions on which derived nucleotide to create
	for (auto type_entry : genomic_element_types_)
	{
		GenomicElementType *ge_type = type_entry.second;
		
		if (ge_type->mutation_matrix_)
		{
			EidosValue_Float_vector *mm = ge_type->mutation_matrix_.get();
			double *mm_data = mm->data();
			
			if (mm->Count() == 16)
			{
				ge_type->mm_thresholds = (double *)malloc(16 * sizeof(double));
				
				for (int nuc = 0; nuc < 4; ++nuc)
				{
					double rateA = mm_data[nuc];
					double rateC = mm_data[nuc + 4];
					double rateG = mm_data[nuc + 8];
					double rateT = mm_data[nuc + 12];
					double total_rate = rateA + rateC + rateG + rateT;
					double fraction_of_max_rate = total_rate / max_nucleotide_mut_rate_;
					double *nuc_thresholds = ge_type->mm_thresholds + nuc * 4;
					
					nuc_thresholds[0] = (rateA / total_rate) * fraction_of_max_rate;
					nuc_thresholds[1] = ((rateA + rateC) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[2] = ((rateA + rateC + rateG) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[3] = fraction_of_max_rate;
				}
			}
			else if (mm->Count() == 256)
			{
				ge_type->mm_thresholds = (double *)malloc(256 * sizeof(double));
				
				for (int trinuc = 0; trinuc < 64; ++trinuc)
				{
					double rateA = mm_data[trinuc];
					double rateC = mm_data[trinuc + 64];
					double rateG = mm_data[trinuc + 128];
					double rateT = mm_data[trinuc + 192];
					double total_rate = rateA + rateC + rateG + rateT;
					double fraction_of_max_rate = total_rate / max_nucleotide_mut_rate_;
					double *nuc_thresholds = ge_type->mm_thresholds + trinuc * 4;
					
					nuc_thresholds[0] = (rateA / total_rate) * fraction_of_max_rate;
					nuc_thresholds[1] = ((rateA + rateC) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[2] = ((rateA + rateC + rateG) / total_rate) * fraction_of_max_rate;
					nuc_thresholds[3] = fraction_of_max_rate;
				}
			}
			else
				EIDOS_TERMINATION << "ERROR (SLiMSim::CacheNucleotideMatrices): (internal error) unsupported mutation matrix size." << EidosTerminate();
		}
	}
}

void SLiMSim::CreateNucleotideMutationRateMap(void)
{
	// In SLiMSim::CacheNucleotideMatrices() we find the maximum sequence-based mutation rate requested.  Absent a
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
	
	std::vector<slim_position_t> &hotspot_end_positions_H = chromosome_.hotspot_end_positions_H_;
	std::vector<slim_position_t> &hotspot_end_positions_M = chromosome_.hotspot_end_positions_M_;
	std::vector<slim_position_t> &hotspot_end_positions_F = chromosome_.hotspot_end_positions_F_;
	std::vector<double> &hotspot_multipliers_H = chromosome_.hotspot_multipliers_H_;
	std::vector<double> &hotspot_multipliers_M = chromosome_.hotspot_multipliers_M_;
	std::vector<double> &hotspot_multipliers_F = chromosome_.hotspot_multipliers_F_;
	
	std::vector<slim_position_t> &mut_positions_H = chromosome_.mutation_end_positions_H_;
	std::vector<slim_position_t> &mut_positions_M = chromosome_.mutation_end_positions_M_;
	std::vector<slim_position_t> &mut_positions_F = chromosome_.mutation_end_positions_F_;
	std::vector<double> &mut_rates_H = chromosome_.mutation_rates_H_;
	std::vector<double> &mut_rates_M = chromosome_.mutation_rates_M_;
	std::vector<double> &mut_rates_F = chromosome_.mutation_rates_F_;
	
	// clear the mutation map; there may be old cruft in there, if we're called by setHotspotMap() for example
	mut_positions_H.clear();
	mut_positions_M.clear();
	mut_positions_F.clear();
	mut_rates_H.clear();
	mut_rates_M.clear();
	mut_rates_F.clear();
	
	if ((hotspot_multipliers_M.size() > 0) && (hotspot_multipliers_F.size() > 0))
	{
		// two sex-specific hotspot maps
		for (size_t index = 0; index < hotspot_multipliers_M.size(); ++index)
		{
			double rate = max_nucleotide_mut_rate_ * hotspot_multipliers_M[index];
			
			if (rate > 1.0)
				EIDOS_TERMINATION << "ERROR (SLiMSim::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
			
			mut_rates_M.emplace_back(rate);
		}
		for (size_t index = 0; index < hotspot_multipliers_F.size(); ++index)
		{
			double rate = max_nucleotide_mut_rate_ * hotspot_multipliers_F[index];
			
			if (rate > 1.0)
				EIDOS_TERMINATION << "ERROR (SLiMSim::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
			
			mut_rates_F.emplace_back(rate);
		}
		
		mut_positions_M = hotspot_end_positions_M;
		mut_positions_F = hotspot_end_positions_F;
	}
	else if (hotspot_multipliers_H.size() > 0)
	{
		// one hotspot map
		for (size_t index = 0; index < hotspot_multipliers_H.size(); ++index)
		{
			double rate = max_nucleotide_mut_rate_ * hotspot_multipliers_H[index];
			
			if (rate > 1.0)
				EIDOS_TERMINATION << "ERROR (SLiMSim::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
			
			mut_rates_H.emplace_back(rate);
		}
		
		mut_positions_H = hotspot_end_positions_H;
	}
	else
	{
		// No hotspot map specified at all; use a rate of 1.0 across the chromosome with an inferred length
		if (max_nucleotide_mut_rate_ > 1.0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::CreateNucleotideMutationRateMap): the maximum mutation rate in nucleotide-based models is 1.0." << EidosTerminate();
		
		mut_rates_H.emplace_back(max_nucleotide_mut_rate_);
		//mut_positions_H.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	
	chromosome_changed_ = true;
}

void SLiMSim::TabulateMemoryUsage(SLiM_MemoryUsage *p_usage, EidosSymbolTable *p_current_symbols)
{
	// Gather genomes
	std::vector<Genome *> all_genomes_in_use, all_genomes_not_in_use;
	size_t genome_pool_usage = 0, individual_pool_usage = 0;
	
	for (auto iter : population_.subpops_)
	{
		Subpopulation &subpop = *iter.second;
		
		all_genomes_not_in_use.insert(all_genomes_not_in_use.end(), subpop.genome_junkyard_nonnull.begin(), subpop.genome_junkyard_nonnull.end());
		all_genomes_not_in_use.insert(all_genomes_not_in_use.end(), subpop.genome_junkyard_null.begin(), subpop.genome_junkyard_null.end());
		
		all_genomes_in_use.insert(all_genomes_in_use.end(), subpop.parent_genomes_.begin(), subpop.parent_genomes_.end());
		all_genomes_in_use.insert(all_genomes_in_use.end(), subpop.child_genomes_.begin(), subpop.child_genomes_.end());
		all_genomes_in_use.insert(all_genomes_in_use.end(), subpop.nonWF_offspring_genomes_.begin(), subpop.nonWF_offspring_genomes_.end());
		
		genome_pool_usage += subpop.genome_pool_->MemoryUsageForAllNodes();
		individual_pool_usage += subpop.individual_pool_->MemoryUsageForAllNodes();
	}
	
	// Chromosome
	{
		p_usage->chromosomeObjects_count = 1;
		
		p_usage->chromosomeObjects = sizeof(Chromosome) * p_usage->chromosomeObjects_count;
		
		p_usage->chromosomeMutationRateMaps = chromosome_.MemoryUsageForMutationMaps();
		
		p_usage->chromosomeRecombinationRateMaps = chromosome_.MemoryUsageForRecombinationMaps();
		
		p_usage->chromosomeAncestralSequence = chromosome_.MemoryUsageForAncestralSequence();
	}
	
	// Genome
	{
		p_usage->genomeObjects_count = (int64_t)all_genomes_in_use.size();
		
		p_usage->genomeObjects = sizeof(Genome) * p_usage->genomeObjects_count;
		
		p_usage->genomeExternalBuffers = 0;
		for (Genome *genome : all_genomes_in_use)
			p_usage->genomeExternalBuffers += genome->MemoryUsageForMutrunBuffers();
		
		p_usage->genomeUnusedPoolSpace = genome_pool_usage - p_usage->genomeObjects;	// includes junkyard objects and unused space
		
		p_usage->genomeUnusedPoolBuffers = 0;
		for (Genome *genome : all_genomes_not_in_use)
			p_usage->genomeUnusedPoolBuffers += genome->MemoryUsageForMutrunBuffers();
	}
	
	// GenomicElement
	{
		p_usage->genomicElementObjects_count = (int64_t)chromosome_.GenomicElementCount();
		
		p_usage->genomicElementObjects = sizeof(GenomicElement) * p_usage->genomicElementObjects_count;
	}
	
	// GenomicElementType
	{
		p_usage->genomicElementTypeObjects_count = (int64_t)genomic_element_types_.size();
		
		p_usage->genomicElementTypeObjects = sizeof(GenomicElementType) * p_usage->genomicElementTypeObjects_count;
	}
	
	// Individual
	{
		p_usage->individualObjects_count = 0;
		for (auto iter : population_.subpops_)
		{
			Subpopulation &subpop = *iter.second;
			
			p_usage->individualObjects_count += (subpop.parent_subpop_size_ + subpop.child_subpop_size_ + subpop.nonWF_offspring_individuals_.size());
		}
		
		p_usage->individualObjects = sizeof(Individual) * p_usage->individualObjects_count;
		
		p_usage->individualUnusedPoolSpace = individual_pool_usage - p_usage->individualObjects;
	}
	
	// InteractionType
	{
		p_usage->interactionTypeObjects_count = (int64_t)interaction_types_.size();
		
		p_usage->interactionTypeObjects = sizeof(InteractionType) * p_usage->interactionTypeObjects_count;
		
		p_usage->interactionTypeKDTrees = 0;
		for (auto iter : interaction_types_)
			p_usage->interactionTypeKDTrees += iter.second->MemoryUsageForKDTrees();
		
		p_usage->interactionTypePositionCaches = 0;
		for (auto iter : interaction_types_)
			p_usage->interactionTypePositionCaches += iter.second->MemoryUsageForPositions();
		
		p_usage->interactionTypeSparseArrays = 0;
		for (auto iter : interaction_types_)
			p_usage->interactionTypeSparseArrays += iter.second->MemoryUsageForSparseArrays();
	}
	
	// Mutation
	{
		int registry_size;
		
		population_.MutationRegistry(&registry_size);
		p_usage->mutationObjects_count = (int64_t)registry_size;
		
		p_usage->mutationObjects = sizeof(Mutation) * p_usage->mutationObjects_count;
		
		p_usage->mutationRefcountBuffer = SLiM_MemoryUsageForMutationRefcounts();
		
		p_usage->mutationUnusedPoolSpace = SLiM_MemoryUsageForMutationBlock() - p_usage->mutationObjects;
	}
	
	// MutationRun
	{
		int64_t operation_id = ++gSLiM_MutationRun_OperationID;
		
		p_usage->mutationRunObjects_count = 0;
		p_usage->mutationRunExternalBuffers = 0;
		p_usage->mutationRunNonneutralCaches = 0;
		for (Genome *genome : all_genomes_in_use)
		{
			MutationRun_SP *mutruns = genome->mutruns_;
			int32_t mutrun_count = genome->mutrun_count_;
			
			for (int32_t mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				MutationRun *mutrun = mutruns[mutrun_index].get();
				
				if (mutrun)
				{
					if (mutrun->operation_id_ != operation_id)
					{
						mutrun->operation_id_ = operation_id;
						p_usage->mutationRunObjects_count++;
						p_usage->mutationRunExternalBuffers += mutrun->MemoryUsageForMutationIndexBuffers();
						p_usage->mutationRunNonneutralCaches += mutrun->MemoryUsageForNonneutralCaches();
					}
				}
			}
		}
		
		p_usage->mutationRunObjects = sizeof(MutationRun) * p_usage->mutationRunObjects_count;
		
		p_usage->mutationRunUnusedPoolSpace = sizeof(MutationRun) * MutationRun::s_freed_mutation_runs_.size();
		
		p_usage->mutationRunUnusedPoolBuffers = 0;
		for (MutationRun *mutrun : MutationRun::s_freed_mutation_runs_)
		{
			p_usage->mutationRunUnusedPoolBuffers += mutrun->MemoryUsageForMutationIndexBuffers();
			p_usage->mutationRunUnusedPoolBuffers += mutrun->MemoryUsageForNonneutralCaches();
		}
	}
	
	// MutationType
	{
		p_usage->mutationTypeObjects_count = (int64_t)mutation_types_.size();
		
		p_usage->mutationTypeObjects = sizeof(MutationType) * p_usage->mutationTypeObjects_count;
	}
	
	// SLiMSim (including the Population object)
	{
		p_usage->slimsimObjects_count = 1;
		
		p_usage->slimsimObjects = (sizeof(SLiMSim) - sizeof(Chromosome)) * p_usage->slimsimObjects_count;	// Chromosome is handled separately above
		
		p_usage->slimsimTreeSeqTables = recording_tree_ ? MemoryUsageForTables(tables_) : 0;
	}
	
	// Subpopulation
	{
		p_usage->subpopulationObjects_count = (int64_t)population_.subpops_.size();
		
		p_usage->subpopulationObjects = sizeof(Subpopulation) * p_usage->subpopulationObjects_count;
		
		p_usage->subpopulationFitnessCaches = 0;
		for (auto iter : population_.subpops_)
		{
			Subpopulation &subpop = *iter.second;
			
			if (subpop.cached_parental_fitness_)
				p_usage->subpopulationFitnessCaches += subpop.cached_fitness_capacity_ * sizeof(double);
			if (subpop.cached_male_fitness_)
				p_usage->subpopulationFitnessCaches += subpop.cached_fitness_capacity_ * sizeof(double);
		}
		
		p_usage->subpopulationParentTables = 0;
		for (auto iter : population_.subpops_)
		{
			Subpopulation &subpop = *iter.second;
			
			p_usage->subpopulationParentTables += subpop.MemoryUsageForParentTables();
		}
		
		p_usage->subpopulationSpatialMaps = 0;
		p_usage->subpopulationSpatialMapsDisplay = 0;
		for (auto iter : population_.subpops_)
		{
			Subpopulation &subpop = *iter.second;
			
			for (auto iter_map : subpop.spatial_maps_)
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
				if (map.display_buffer_)
					p_usage->subpopulationSpatialMapsDisplay += map.buffer_width_ * map.buffer_height_ * sizeof(uint8_t) * 3;
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
		p_usage->substitutionObjects_count = (int64_t)population_.substitutions_.size();
		
		p_usage->substitutionObjects = sizeof(Substitution) * p_usage->substitutionObjects_count;
	}
	
	// Eidos usage
	{
		p_usage->eidosASTNodePool = gEidosASTNodePool->MemoryUsageForAllNodes();
		
		p_usage->eidosSymbolTablePool = MemoryUsageForSymbolTables(p_current_symbols);
		
		p_usage->eidosValuePool = gEidosValuePool->MemoryUsageForAllNodes();
	}
	
	// missing: EidosCallSignature, EidosPropertySignature, EidosScript, EidosToken, function map, global strings and ids and maps, std::strings in various objects
	// that sort of overhead should be fairly constant, though, and should be dwarfed by the overhead of the objects above in bigger models
	
	// total
	size_t total_usage = 0;
	
	total_usage += p_usage->chromosomeObjects;
	total_usage += p_usage->chromosomeMutationRateMaps;
	total_usage += p_usage->chromosomeRecombinationRateMaps;
	total_usage += p_usage->chromosomeAncestralSequence;
	
	total_usage += p_usage->genomeObjects;
	total_usage += p_usage->genomeExternalBuffers;
	total_usage += p_usage->genomeUnusedPoolSpace;
	total_usage += p_usage->genomeUnusedPoolBuffers;
	
	total_usage += p_usage->genomicElementObjects;
	
	total_usage += p_usage->genomicElementTypeObjects;
	
	total_usage += p_usage->individualObjects;
	total_usage += p_usage->individualUnusedPoolSpace;
	
	total_usage += p_usage->interactionTypeObjects;
	total_usage += p_usage->interactionTypeKDTrees;
	total_usage += p_usage->interactionTypePositionCaches;
	total_usage += p_usage->interactionTypeSparseArrays;
	
	total_usage += p_usage->mutationObjects;
	total_usage += p_usage->mutationRefcountBuffer;
	total_usage += p_usage->mutationUnusedPoolSpace;
	
	total_usage += p_usage->mutationRunObjects;
	total_usage += p_usage->mutationRunExternalBuffers;
	total_usage += p_usage->mutationRunNonneutralCaches;
	total_usage += p_usage->mutationRunUnusedPoolSpace;
	total_usage += p_usage->mutationRunUnusedPoolBuffers;
	
	total_usage += p_usage->mutationTypeObjects;
	
	total_usage += p_usage->slimsimObjects;
	total_usage += p_usage->slimsimTreeSeqTables;
	
	total_usage += p_usage->subpopulationObjects;
	total_usage += p_usage->subpopulationFitnessCaches;
	total_usage += p_usage->subpopulationParentTables;
	total_usage += p_usage->subpopulationSpatialMaps;
	total_usage += p_usage->subpopulationSpatialMapsDisplay;
	
	total_usage += p_usage->substitutionObjects;
	
	total_usage += p_usage->eidosASTNodePool;
	total_usage += p_usage->eidosSymbolTablePool;
	total_usage += p_usage->eidosValuePool;
	
	p_usage->totalMemoryUsage = total_usage;
}

#if defined(SLIMGUI) && (SLIMPROFILING == 1)
// PROFILING
void SLiMSim::CollectSLiMguiMemoryUsageProfileInfo(void)
{
	TabulateMemoryUsage(&profile_last_memory_usage_, nullptr);
	
	profile_total_memory_usage_.chromosomeObjects_count += profile_last_memory_usage_.chromosomeObjects_count;
	profile_total_memory_usage_.chromosomeObjects += profile_last_memory_usage_.chromosomeObjects;
	profile_total_memory_usage_.chromosomeMutationRateMaps += profile_last_memory_usage_.chromosomeMutationRateMaps;
	profile_total_memory_usage_.chromosomeRecombinationRateMaps += profile_last_memory_usage_.chromosomeRecombinationRateMaps;
	profile_total_memory_usage_.chromosomeAncestralSequence += profile_last_memory_usage_.chromosomeAncestralSequence;
	
	profile_total_memory_usage_.genomeObjects_count += profile_last_memory_usage_.genomeObjects_count;
	profile_total_memory_usage_.genomeObjects += profile_last_memory_usage_.genomeObjects;
	profile_total_memory_usage_.genomeExternalBuffers += profile_last_memory_usage_.genomeExternalBuffers;
	profile_total_memory_usage_.genomeUnusedPoolSpace += profile_last_memory_usage_.genomeUnusedPoolSpace;
	profile_total_memory_usage_.genomeUnusedPoolBuffers += profile_last_memory_usage_.genomeUnusedPoolBuffers;
	
	profile_total_memory_usage_.genomicElementObjects_count += profile_last_memory_usage_.genomicElementObjects_count;
	profile_total_memory_usage_.genomicElementObjects += profile_last_memory_usage_.genomicElementObjects;
	
	profile_total_memory_usage_.genomicElementTypeObjects_count += profile_last_memory_usage_.genomicElementTypeObjects_count;
	profile_total_memory_usage_.genomicElementTypeObjects += profile_last_memory_usage_.genomicElementTypeObjects;
	
	profile_total_memory_usage_.individualObjects_count += profile_last_memory_usage_.individualObjects_count;
	profile_total_memory_usage_.individualObjects += profile_last_memory_usage_.individualObjects;
	profile_total_memory_usage_.individualUnusedPoolSpace += profile_last_memory_usage_.individualUnusedPoolSpace;
	
	profile_total_memory_usage_.interactionTypeObjects_count += profile_last_memory_usage_.interactionTypeObjects_count;
	profile_total_memory_usage_.interactionTypeObjects += profile_last_memory_usage_.interactionTypeObjects;
	profile_total_memory_usage_.interactionTypeKDTrees += profile_last_memory_usage_.interactionTypeKDTrees;
	profile_total_memory_usage_.interactionTypePositionCaches += profile_last_memory_usage_.interactionTypePositionCaches;
	profile_total_memory_usage_.interactionTypeSparseArrays += profile_last_memory_usage_.interactionTypeSparseArrays;
	
	profile_total_memory_usage_.mutationObjects_count += profile_last_memory_usage_.mutationObjects_count;
	profile_total_memory_usage_.mutationObjects += profile_last_memory_usage_.mutationObjects;
	profile_total_memory_usage_.mutationRefcountBuffer += profile_last_memory_usage_.mutationRefcountBuffer;
	profile_total_memory_usage_.mutationUnusedPoolSpace += profile_last_memory_usage_.mutationUnusedPoolSpace;
	
	profile_total_memory_usage_.mutationRunObjects_count += profile_last_memory_usage_.mutationRunObjects_count;
	profile_total_memory_usage_.mutationRunObjects += profile_last_memory_usage_.mutationRunObjects;
	profile_total_memory_usage_.mutationRunExternalBuffers += profile_last_memory_usage_.mutationRunExternalBuffers;
	profile_total_memory_usage_.mutationRunNonneutralCaches += profile_last_memory_usage_.mutationRunNonneutralCaches;
	profile_total_memory_usage_.mutationRunUnusedPoolSpace += profile_last_memory_usage_.mutationRunUnusedPoolSpace;
	profile_total_memory_usage_.mutationRunUnusedPoolBuffers += profile_last_memory_usage_.mutationRunUnusedPoolBuffers;
	
	profile_total_memory_usage_.mutationTypeObjects_count += profile_last_memory_usage_.mutationTypeObjects_count;
	profile_total_memory_usage_.mutationTypeObjects += profile_last_memory_usage_.mutationTypeObjects;
	
	profile_total_memory_usage_.slimsimObjects_count += profile_last_memory_usage_.slimsimObjects_count;
	profile_total_memory_usage_.slimsimObjects += profile_last_memory_usage_.slimsimObjects;
	profile_total_memory_usage_.slimsimTreeSeqTables += profile_last_memory_usage_.slimsimTreeSeqTables;
	
	profile_total_memory_usage_.subpopulationObjects_count += profile_last_memory_usage_.subpopulationObjects_count;
	profile_total_memory_usage_.subpopulationObjects += profile_last_memory_usage_.subpopulationObjects;
	profile_total_memory_usage_.subpopulationFitnessCaches += profile_last_memory_usage_.subpopulationFitnessCaches;
	profile_total_memory_usage_.subpopulationParentTables += profile_last_memory_usage_.subpopulationParentTables;
	profile_total_memory_usage_.subpopulationSpatialMaps += profile_last_memory_usage_.subpopulationSpatialMaps;
	profile_total_memory_usage_.subpopulationSpatialMapsDisplay += profile_last_memory_usage_.subpopulationSpatialMapsDisplay;
	
	profile_total_memory_usage_.substitutionObjects_count += profile_last_memory_usage_.substitutionObjects_count;
	profile_total_memory_usage_.substitutionObjects += profile_last_memory_usage_.substitutionObjects;
	
	profile_total_memory_usage_.eidosASTNodePool += profile_last_memory_usage_.eidosASTNodePool;
	profile_total_memory_usage_.eidosSymbolTablePool += profile_last_memory_usage_.eidosSymbolTablePool;
	profile_total_memory_usage_.eidosValuePool += profile_last_memory_usage_.eidosValuePool;
	
	profile_total_memory_usage_.totalMemoryUsage += profile_last_memory_usage_.totalMemoryUsage;
	
	total_memory_tallies_++;
}
#endif


//
// TREE SEQUENCE RECORDING
//
#pragma mark -
#pragma mark Tree sequence recording
#pragma mark -

void SLiMSim::handle_error(std::string msg, int err)
{
	std::cout << "Error:" << msg << ": " << tsk_strerror(err) << std::endl;
	EIDOS_TERMINATION << msg << ": " << tsk_strerror(err) << EidosTerminate();
}

void SLiMSim::ReorderIndividualTable(tsk_table_collection_t *p_tables, std::vector<int> p_individual_map, bool p_keep_unmapped)
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
				p_individual_map.push_back(j);
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
		
		ret = tsk_individual_table_add_row(&p_tables->individuals, flags, location, (tsk_size_t)location_length, metadata, (tsk_size_t)metadata_length);
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

struct edge_plus_time {
	double time;
	tsk_id_t parent, child;
	double left, right;
};

static int
slim_sort_edges(tsk_table_sorter_t *sorter, tsk_size_t start)
{
	if (sorter->tables->edges.metadata_length != 0)
		throw std::invalid_argument("the sorter does not currently handle edge metadata");
	if (start != 0)
		throw std::invalid_argument("the sorter requires start==0");
	
	std::vector<edge_plus_time> temp;
	temp.reserve(static_cast<std::size_t>(sorter->tables->edges.num_rows));
	
	auto edges = &sorter->tables->edges;
	auto nodes = &sorter->tables->nodes;
	
	for (tsk_size_t i = 0; i < sorter->tables->edges.num_rows; ++i)
		temp.push_back(edge_plus_time{ nodes->time[edges->parent[i]], edges->parent[i], edges->child[i], edges->left[i], edges->right[i] });
	
	std::sort(begin(temp), end(temp),
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
	
	for (std::size_t i = 0; i < temp.size(); ++i)
	{
		edges->left[i] = temp[i].left;
		edges->right[i] = temp[i].right;
		edges->parent[i] = temp[i].parent;
		edges->child[i] = temp[i].child;
	}
	
	return 0;
}

void SLiMSim::SimplifyTreeSequence(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::SimplifyTreeSequence): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	if (tables_.nodes.num_rows == 0)
		return;
	
	std::vector<tsk_id_t> samples;
	
	// BCH 7/27/2019: We now build a std::unordered_map containing all of the entries of remembered_genomes_,
	// so that the find() operations in the loop below can be done in constant time instead of O(N) time.
	// We need to be able to find out the index of an entry, in remembered_genomes_, once we have found it;
	// that is what the mapped value provides, whereas the key value is the tsk_id_t we need to find below.
	// We do all this inside a block so the map gets deallocated as soon as possible, to minimize footprint.
	{
		std::unordered_map<tsk_id_t, uint32_t> remembered_genomes_lookup;
		
		// the remembered_genomes_ come first in the list of samples
		uint32_t index = 0;
		
		for (tsk_id_t sid : remembered_genomes_)
		{
			samples.push_back(sid);
			remembered_genomes_lookup.emplace(std::pair<tsk_id_t, uint32_t>(sid, index));
			index++;
		}
		
		// and then come all the genomes of the extant individuals
		tsk_id_t newValueInNodeTable = (tsk_id_t)remembered_genomes_.size();
		
		for (auto it : population_.subpops_)
		{
			std::vector<Genome *> &subpopulationGenomes = it.second->parent_genomes_;
			
			for (Genome *genome : subpopulationGenomes)
			{
				tsk_id_t M = genome->tsk_node_id_;
				
				// check if this sample is already being remembered, and assign the correct tsk_node_id_
				// if not remembered, it is currently alive, so we need to mark it as a sample so it persists through simplify()
				auto iter = remembered_genomes_lookup.find(M);
				
				if (iter == remembered_genomes_lookup.end())
				{
					samples.push_back(M);
					genome->tsk_node_id_ = newValueInNodeTable++;
				}
				else
				{
					genome->tsk_node_id_ = (tsk_id_t)(iter->second);
				}
			}
		}
	}
	
	// the tables need to have a population table to be able to sort it
	WritePopulationTable(&tables_);
	
	// sort the table collection
    int flags = TSK_NO_CHECK_INTEGRITY;
#if DEBUG
    flags = 0;
#endif
	
#if 0
	// sort the tables using tsk_table_collection_sort() to get the default behavior
	int ret = tsk_table_collection_sort(&tables_, /* edge_start */ NULL, /* flags */ flags);
	if (ret < 0) handle_error("tsk_table_collection_sort", ret);
#else
	// sort the tables using our own custom edge sorter, for additional speed through inlining of the comparison function
	// see https://github.com/tskit-dev/tskit/pull/627, https://github.com/tskit-dev/tskit/pull/711
	// FIXME for additional speed we could perhaps be smart about only sorting the portions of the edge table
	// that need it, but the tricky thing is that all the old stuff has to be at the bottom of the table, not the top...
	tsk_table_sorter_t sorter;
	int ret = tsk_table_sorter_init(&sorter, &tables_, /* flags */ flags);
	if (ret != 0) handle_error("tsk_table_sorter_init", ret);
	
	sorter.sort_edges = slim_sort_edges;
	
	try {
		ret = tsk_table_sorter_run(&sorter, NULL);
	} catch (std::exception &e) {
		EIDOS_TERMINATION << "ERROR (SLiMSim::SimplifyTreeSequence): (internal error) exception raised during tsk_table_sorter_run(): " << e.what() << "." << EidosTerminate();
	}
	if (ret != 0) handle_error("tsk_table_sorter_run", ret);
	
	tsk_table_sorter_free(&sorter);
	if (ret != 0) handle_error("tsk_table_sorter_free", ret);
#endif
	
	// remove redundant sites we added
	ret = tsk_table_collection_deduplicate_sites(&tables_, 0);
	if (ret < 0) handle_error("tsk_table_collection_deduplicate_sites", ret);
	
	// simplify
	ret = tsk_table_collection_simplify(&tables_, samples.data(), (tsk_size_t)samples.size(), TSK_FILTER_SITES | TSK_FILTER_INDIVIDUALS | TSK_KEEP_INPUT_ROOTS, NULL);
	if (ret != 0) handle_error("tsk_table_collection_simplify", ret);
	
	// update map of remembered_genomes_, which are now the first n entries in the node table
	for (tsk_id_t i = 0; i < (tsk_id_t)remembered_genomes_.size(); i++)
		remembered_genomes_[i] = i;
	
	// reset current position, used to rewind individuals that are rejected by modifyChild()
	RecordTablePosition();
	
	// and reset our elapsed time since last simplification, for auto-simplification
	simplify_elapsed_ = 0;
	
	// as a side effect of simplification, update a "model has coalesced" flag that the user can consult, if requested
	if (running_coalescence_checks_)
		CheckCoalescenceAfterSimplification();
}

void SLiMSim::CheckCoalescenceAfterSimplification(void)
{
#if DEBUG
	if (!recording_tree_ || !running_coalescence_checks_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::CheckCoalescenceAfterSimplification): (internal error) coalescence check called with recording or checking off." << EidosTerminate();
#endif
	
	// Copy the table collection; Jerome says this is unnecessary since tsk_table_collection_build_index()
	// does not modify the core information in the table collection, but just adds some separate indices.
	// However, we also need to add a population table, so really it is best to make a copy I think.
	tsk_table_collection_t tables_copy;
	int ret;
	
	ret = tsk_table_collection_copy(&tables_, &tables_copy, 0);
	if (ret < 0) handle_error("tsk_table_collection_copy", ret);
	
	// Our tables copy needs to have a population table now, since this is required to build a tree sequence
	WritePopulationTable(&tables_copy);
	
	ret = tsk_table_collection_build_index(&tables_copy, 0);
	if (ret < 0) handle_error("tsk_table_collection_build_index", ret);
	
	tsk_treeseq_t ts;
	
	ret = tsk_treeseq_init(&ts, &tables_copy, 0);
	if (ret < 0) handle_error("tsk_treeseq_init", ret);
	
	// Collect a vector of all extant genome node IDs
	std::vector<tsk_id_t> all_extant_nodes;
	
	for (auto subpop_iter : population_.subpops_)
	{
		Subpopulation *subpop = subpop_iter.second;
		std::vector<Genome *> &genomes = subpop->parent_genomes_;
		slim_popsize_t genome_count = subpop->parent_subpop_size_ * 2;
		Genome **genome_ptr = genomes.data();
		
		for (slim_popsize_t genome_index = 0; genome_index < genome_count; ++genome_index)
			all_extant_nodes.push_back(genome_ptr[genome_index]->tsk_node_id_);
	}
	
	int64_t extant_node_count = (int64_t)all_extant_nodes.size();
	
	// Iterate through the trees to check coalescence; this is a bit tricky because of retained first-gen ancestors
	// and other remembered individuals.  We use the sparse tree's "tracked samples" feature, tracking extant individuals
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
		// If we didn't retain FIRST_GEN ancestors, or remember genomes, >1 root would mean not coalesced
		if (t.right_sib[t.left_root] != TSK_NULL)
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
		for (tsk_id_t root = t.left_root; root != TSK_NULL; root = t.right_sib[root])
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
	
	if (&tables_copy != &tables_)
	{
		ret = tsk_table_collection_free(&tables_copy);
		if (ret < 0) handle_error("tsk_table_collection_free", ret);
	}
	
	//std::cout << generation_ << ": fully_coalesced == " << (fully_coalesced ? "TRUE" : "false") << std::endl;
	last_coalescence_state_ = fully_coalesced;
}

void SLiMSim::RecordTablePosition(void)
{
	// keep the current table position for rewinding if a proposed child is rejected
	tsk_table_collection_record_num_rows(&tables_, &table_position_);
}

void SLiMSim::AllocateTreeSequenceTables(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::AllocateTreeSequenceTables): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// Set up the table collection before loading a saved population or starting a simulation
	
	//INITIALIZE NODE AND EDGE TABLES.
	int ret = tsk_table_collection_init(&tables_, TSK_NO_EDGE_METADATA);
	if (ret != 0) handle_error("AllocateTreeSequenceTables()", ret);
	
	tables_.sequence_length = (double)chromosome_.last_position_ + 1;
	
	RecordTablePosition();
}

void SLiMSim::SetCurrentNewIndividual(__attribute__((unused))Individual *p_individual)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::SetCurrentNewIndividual): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This is called by code where new individuals are created
	
	// Remember the new individual being defined; we don't need this right now,
	// but it seems to keep coming back, so I've kept the code for it...
	//current_new_individual_ = p_individual;
	
	// Remember the current table position so we can return to it later in RetractNewIndividual()
    RecordTablePosition();
}

void SLiMSim::RetractNewIndividual()
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RetractNewIndividual): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This is called when a new child, introduced by SetCurrentNewIndividual(), gets rejected by a modifyChild()
	// callback.  We will have logged recombination breakpoints and new mutations into our tables, and now want
	// to back those changes out by re-setting the active row index for the tables.
	
	// We presently don't use current_new_individual_ any more, but I've kept
	// around the code since it seems to keep coming back...
	//current_new_individual_ = nullptr;
	
    tsk_table_collection_truncate(&tables_, &table_position_);
}

void SLiMSim::RecordNewGenome(std::vector<slim_position_t> *p_breakpoints, Genome *p_new_genome, 
        const Genome *p_initial_parental_genome, const Genome *p_second_parental_genome)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RecordNewGenome): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
    // This records information about an individual in both the Node and Edge tables.

	// Note that the breakpoints vector provided may (or may not) contain a breakpoint, as the final breakpoint in the vector, that is beyond
	// the end of the chromosome.  This is for bookkeeping in the crossover-mutation code and should be ignored, as the code below does.
	// The breakpoints vector may be nullptr (indicating no recombination), but if it exists it will be sorted in ascending order.

	// add genome node; we mark all nodes with TSK_NODE_IS_SAMPLE here because we have full genealogical information on all of them
	// (until simplify, which clears TSK_NODE_IS_SAMPLE from nodes that are not kept in the sample).
	double time = (double) -1 * (tree_seq_generation_ + tree_seq_generation_offset_);	// see Population::AddSubpopulationSplit() regarding tree_seq_generation_offset_
	uint32_t flags = TSK_NODE_IS_SAMPLE;
	GenomeMetadataRec metadata_rec;
	
	MetadataForGenome(p_new_genome, &metadata_rec);
	
	const char *metadata = (char *)&metadata_rec;
	size_t metadata_length = sizeof(GenomeMetadataRec)/sizeof(char);
	tsk_id_t offspringTSKID = tsk_node_table_add_row(&tables_.nodes, flags, time, (tsk_id_t)p_new_genome->subpop_->subpopulation_id_,
                                        TSK_NULL, metadata, (tsk_size_t)metadata_length);
	if (offspringTSKID < 0) handle_error("tsk_node_table_add_row", offspringTSKID);
	
	p_new_genome->tsk_node_id_ = offspringTSKID;
	
    // if there is no parent then no need to record edges
	if (!p_initial_parental_genome && !p_second_parental_genome)
		return;
	
	// map the Parental Genome SLiM Id's to TSK IDs.
	tsk_id_t genome1TSKID = p_initial_parental_genome->tsk_node_id_;
	tsk_id_t genome2TSKID = (!p_second_parental_genome) ? genome1TSKID : p_second_parental_genome->tsk_node_id_;
	
	// fix possible excess past-the-end breakpoint
	size_t breakpoint_count = (p_breakpoints ? p_breakpoints->size() : 0);
	
	if (breakpoint_count && (p_breakpoints->back() > chromosome_.last_position_))
		breakpoint_count--;
	
	// add an edge for each interval between breakpoints
	double left = 0.0;
	double right;
	bool polarity = true;
	
	for (size_t i = 0; i < breakpoint_count; i++)
	{
		right = (*p_breakpoints)[i];

		tsk_id_t parent = (tsk_id_t) (polarity ? genome1TSKID : genome2TSKID);
		int ret = tsk_edge_table_add_row(&tables_.edges, left, right, parent, offspringTSKID, NULL, 0);
		if (ret < 0) handle_error("tsk_edge_table_add_row", ret);
		
		polarity = !polarity;
		left = right;
	}
	
	right = (double)chromosome_.last_position_+1;
	tsk_id_t parent = (tsk_id_t) (polarity ? genome1TSKID : genome2TSKID);
	int ret = tsk_edge_table_add_row(&tables_.edges, left, right, parent, offspringTSKID, NULL, 0);
	if (ret < 0) handle_error("tsk_edge_table_add_row", ret);
}

void SLiMSim::RecordNewDerivedState(const Genome *p_genome, slim_position_t p_position, const std::vector<Mutation *> &p_derived_mutations)
{
#if DEBUG
	if (!recording_mutations_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RecordNewDerivedState): (internal error) tree sequence mutation recording method called with recording off." << EidosTerminate();
#endif
	
    // This records information in the Site and Mutation tables.
    // This is called whenever a new mutation is added to a genome.  Because
    // mutation stacking makes things complicated, this hook supplies not just
    // the new mutation, but the entire new derived state – all of the
    // mutations that exist at the given position in the given genome,
    // post-addition.  This derived state may involve the removal of some
    // ancestral mutations (or may not), in addition to the new mutation that
    // was added.  The new state is not even guaranteed to be different from
    // the ancestral state; because of the way new mutations are added in some
    // paths (with bulk operations) we may not know.  This method will also be
    // called when a mutation is removed from a given genome; if no mutations
    // remain at the given position, p_derived_mutations will be empty.  The
    // vector of mutations passed in here is reused internally, so this method
    // must not keep a pointer to it; any information that needs to be kept
    // from it must be copied out.  See treerec/implementation.md for more.
	
	// BCH 4/29/2018: Null genomes should never contain any mutations at all,
	// including fixed mutations; the simplest thing is to just disallow derived
	// states for them altogether.
	if (p_genome->IsNull())
		EIDOS_TERMINATION << "ERROR (SLiMSim::RecordNewDerivedState): new derived states cannot be recorded for null genomes." << EidosTerminate();
	
    tsk_id_t genomeTSKID = p_genome->tsk_node_id_;

    // Identify any previous mutations at this site in this genome, and add a new site.
	// This site may already exist, but we add it anyway, and deal with that in deduplicate_sites().
    double tsk_position = (double) p_position;

    tsk_id_t site_id = tsk_site_table_add_row(&tables_.sites, tsk_position, NULL, 0, NULL, 0);
	if (site_id < 0) handle_error("tsk_site_table_add_row", site_id);
	
    // form derived state
	static std::vector<slim_mutationid_t> derived_mutation_ids;
	static std::vector<MutationMetadataRec> mutation_metadata;
	MutationMetadataRec metadata_rec;
	
	derived_mutation_ids.clear();
    mutation_metadata.clear();
	for (Mutation *mutation : p_derived_mutations)
	{
		derived_mutation_ids.push_back(mutation->mutation_id_);
		MetadataForMutation(mutation, &metadata_rec);
        mutation_metadata.push_back(metadata_rec);
    }
	
	// find and incorporate any fixed mutations at this position, which exist in all new derived states but are not included by SLiM
	// BCH 5/14/2019: Note that this means that derived states will be recorded that look "stacked" even when those mutations would
	// not have stacked, by the stacking policy, had they occurred in the same genome at the same time.  So this is a bit weird.
	// For example, you can end up with a derived state that appears to show two nucleotides stacked at the same position; but one
	// fixed before the other one occurred, so they aren't stacked really, the new one just occurred on the ancestral background of
	// the old one.  Possibly we ought to do something different about this (and not record a stacked derived state), but that
	// would be a big change since it has implications for crosscheck, etc.  FIXME
	auto position_range_iter = population_.treeseq_substitutions_map_.equal_range(p_position);
	
	for (auto position_iter = position_range_iter.first; position_iter != position_range_iter.second; ++position_iter)
	{
		Substitution *substitution = position_iter->second;
		
		derived_mutation_ids.push_back(substitution->mutation_id_);
		MetadataForSubstitution(substitution, &metadata_rec);
		mutation_metadata.push_back(metadata_rec);
	}
	
	// add the mutation table row with the final derived state and metadata
    char *derived_muts_bytes = (char *)(derived_mutation_ids.data());
    size_t derived_state_length = derived_mutation_ids.size() * sizeof(slim_mutationid_t);
    char *mutation_metadata_bytes = (char *)(mutation_metadata.data());
    size_t mutation_metadata_length = mutation_metadata.size() * sizeof(MutationMetadataRec);

	double time = -(double) (tree_seq_generation_ + tree_seq_generation_offset_);	// see Population::AddSubpopulationSplit() regarding tree_seq_generation_offset_
    int ret = tsk_mutation_table_add_row(&tables_.mutations, site_id, genomeTSKID, TSK_NULL, 
                    time,
                    derived_muts_bytes, (tsk_size_t)derived_state_length,
                    mutation_metadata_bytes, (tsk_size_t)mutation_metadata_length);
	if (ret < 0) handle_error("tsk_mutation_table_add_row", ret);
}

void SLiMSim::CheckAutoSimplification(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::CheckAutoSimplification): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This is called at the end of each generation, at an appropriate time to simplify.  This method decides
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
			SimplifyTreeSequence();
		}
	}
	else if (!std::isinf(simplification_ratio_))
	{
		if (simplify_elapsed_ >= simplify_interval_)
		{
			// We could, in principle, calculate actual memory used based on number of rows * sizeof(column), etc.,
			// but that seems like overkill; adding together the number of rows in all the tables should be a
			// reasonable proxy, and this whole thing is just a heuristic that needs to be tailored anyway.
			uint64_t old_table_size = (uint64_t)tables_.nodes.num_rows;
            old_table_size += (uint64_t)tables_.edges.num_rows;
            old_table_size += (uint64_t)tables_.sites.num_rows;
            old_table_size += (uint64_t)tables_.mutations.num_rows;
			
			SimplifyTreeSequence();
			
			uint64_t new_table_size = (uint64_t)tables_.nodes.num_rows;
            new_table_size += (uint64_t)tables_.edges.num_rows;
            new_table_size += (uint64_t)tables_.sites.num_rows;
            new_table_size += (uint64_t)tables_.mutations.num_rows;
			double ratio = old_table_size / (double)new_table_size;
			
			//std::cout << "auto-simplified in generation " << generation_ << "; old size " << old_table_size << ", new size " << new_table_size;
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

void SLiMSim::TreeSequenceDataFromAscii(std::string NodeFileName,
										std::string EdgeFileName,
										std::string SiteFileName,
										std::string MutationFileName,
										std::string IndividualsFileName,
										std::string PopulationFileName,
										std::string ProvenanceFileName)
{
    FILE *MspTxtNodeTable = fopen(NodeFileName.c_str(), "r");
    FILE *MspTxtEdgeTable = fopen(EdgeFileName.c_str(), "r");
    FILE *MspTxtSiteTable = fopen(SiteFileName.c_str(), "r");
    FILE *MspTxtMutationTable = fopen(MutationFileName.c_str(), "r");
	FILE *MspTxtIndividualTable = fopen(IndividualsFileName.c_str(), "r");
	FILE *MspTxtPopulationTable = fopen(PopulationFileName.c_str(), "r");
    FILE *MspTxtProvenanceTable = fopen(ProvenanceFileName.c_str(), "r");
	
	int ret = tsk_table_collection_init(&tables_, TSK_NO_EDGE_METADATA);
	if (ret != 0) handle_error("TreeSequenceDataFromAscii()", ret);
	
	ret = table_collection_load_text(&tables_,
									 MspTxtNodeTable,
									 MspTxtEdgeTable,
									 MspTxtSiteTable,
									 MspTxtMutationTable,
									 NULL, // migrations
									 MspTxtIndividualTable,
									 MspTxtPopulationTable,
									 MspTxtProvenanceTable);
    if (ret < 0) handle_error("read_from_ascii :: table_collection_load_text", ret);
	
	// Parse the provenance info just to find out the file version, which we need for mutation metadata parsing
	slim_generation_t metadata_gen;
	SLiMModelType file_model_type;
	int file_version;
	
	ReadTreeSequenceMetadata(&tables_, &metadata_gen, &file_model_type, &file_version);
	
	// We will be replacing the columns of some of the tables in tables with de-ASCII-fied versions.  That can't be
	// done in place, so we make a copy of tables here to act as a source for the process of copying new information
	// back into tables.
	tsk_table_collection_t tables_copy;
	ret = tsk_table_collection_copy(&tables_, &tables_copy, 0);
	if (ret < 0) handle_error("read_from_ascii :: tsk_table_collection_copy", ret);
	
	// de-ASCII-fy the metadata and derived state information; this is the inverse of the work done by TreeSequenceDataToAscii()
	
	/***  De-ascii-ify Mutation Table ***/
	{
		static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec has changed size; this code probably needs to be updated");
		
		bool metadata_has_nucleotide = (file_version >= 3);		// at or after "0.3"
		
		// Mutation derived state
		const char *derived_state = tables_.mutations.derived_state;
		tsk_size_t *derived_state_offset = tables_.mutations.derived_state_offset;
		std::vector<slim_mutationid_t> binary_derived_state;
		std::vector<tsk_size_t> binary_derived_state_offset;
		size_t derived_state_total_part_count = 0;
		
		// Mutation metadata
		const char *mutation_metadata = tables_.mutations.metadata;
		tsk_size_t *mutation_metadata_offset = tables_.mutations.metadata_offset;
		std::vector<MutationMetadataRec> binary_mutation_metadata;
		std::vector<tsk_size_t> binary_mutation_metadata_offset;
		size_t mutation_metadata_total_part_count = 0;
		
		binary_derived_state_offset.push_back(0);
		binary_mutation_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < tables_.mutations.num_rows; j++)
		{
			// Mutation derived state
			std::string string_derived_state(derived_state + derived_state_offset[j], derived_state_offset[j+1] - derived_state_offset[j]);
			std::vector<std::string> derived_state_parts = Eidos_string_split(string_derived_state, ",");
			
			for (std::string &derived_state_part : derived_state_parts)
				binary_derived_state.emplace_back((slim_mutationid_t)std::stoll(derived_state_part));
			
			derived_state_total_part_count += derived_state_parts.size();
			binary_derived_state_offset.push_back((tsk_size_t)(derived_state_total_part_count * sizeof(slim_mutationid_t)));
			
			// Mutation metadata
			std::string string_mutation_metadata(mutation_metadata + mutation_metadata_offset[j], mutation_metadata_offset[j+1] - mutation_metadata_offset[j]);
			std::vector<std::string> mutation_metadata_parts = Eidos_string_split(string_mutation_metadata, ";");
			
			if (derived_state_parts.size() != mutation_metadata_parts.size())
				EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): derived state length != mutation metadata length; this file cannot be read." << EidosTerminate();
			
			for (std::string &mutation_metadata_part : mutation_metadata_parts)
			{
				std::vector<std::string> mutation_metadata_subparts = Eidos_string_split(mutation_metadata_part, ",");
				
				if (mutation_metadata_subparts.size() != (metadata_has_nucleotide ? 5 : 4))
					EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): unexpected mutation metadata length; this file cannot be read." << EidosTerminate();
				
				MutationMetadataRec metarec;
				metarec.mutation_type_id_ = (slim_objectid_t)std::stoll(mutation_metadata_subparts[0]);
				metarec.selection_coeff_ = (slim_selcoeff_t)std::stod(mutation_metadata_subparts[1]);
				metarec.subpop_index_ = (slim_objectid_t)std::stoll(mutation_metadata_subparts[2]);
				metarec.origin_generation_ = (slim_generation_t)std::stoll(mutation_metadata_subparts[3]);
				metarec.nucleotide_ = metadata_has_nucleotide ? (int8_t)std::stod(mutation_metadata_subparts[4]) : (int8_t)-1;
				binary_mutation_metadata.emplace_back(metarec);
			}
			
			mutation_metadata_total_part_count += mutation_metadata_parts.size();
			binary_mutation_metadata_offset.push_back((tsk_size_t)(mutation_metadata_total_part_count * sizeof(MutationMetadataRec)));
		}
		
		// if we have no rows, these vectors will be empty, and .data() will return NULL, which tskit doesn't like;
		// it wants to get a non-NULL pointer even when it knows that the pointer points to zero valid bytes.  So
		// we poke the vectors so they're non-zero-length and thus have non-NULL pointers; a harmless workaround.
		if (binary_derived_state.size() == 0)
			binary_derived_state.resize(1);
		if (binary_mutation_metadata.size() == 0)
			binary_mutation_metadata.resize(1);
		
		ret = tsk_mutation_table_set_columns(&tables_.mutations,
										 tables_copy.mutations.num_rows,
										 tables_copy.mutations.site,
										 tables_copy.mutations.node,
										 tables_copy.mutations.parent,
										 tables_copy.mutations.time,
										 (char *)binary_derived_state.data(),
										 binary_derived_state_offset.data(),
										 (char *)binary_mutation_metadata.data(),
										 binary_mutation_metadata_offset.data());
		if (ret < 0) handle_error("convert_from_ascii", ret);
	}
	
	/***** De-ascii-ify Node Table *****/
	{
		static_assert(sizeof(GenomeMetadataRec) == 10, "GenomeMetadataRec has changed size; this code probably needs to be updated");
		
		const char *metadata = tables_.nodes.metadata;
		tsk_size_t *metadata_offset = tables_.nodes.metadata_offset;
		std::vector<GenomeMetadataRec> binary_metadata;
		std::vector<tsk_size_t> binary_metadata_offset;
		size_t metadata_total_part_count = 0;
		
		binary_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < tables_.nodes.num_rows; j++)
		{
			std::string string_metadata(metadata + metadata_offset[j], metadata_offset[j+1] - metadata_offset[j]);
			std::vector<std::string> metadata_parts = Eidos_string_split(string_metadata, ",");
			
			if (metadata_parts.size() != 3)
				EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): unexpected node metadata length; this file cannot be read." << EidosTerminate();
			
			GenomeMetadataRec metarec;
			metarec.genome_id_ = (slim_genomeid_t)std::stoll(metadata_parts[0]);
			
			if (metadata_parts[1] == "T")		metarec.is_null_ = true;
			else if (metadata_parts[1] == "F")	metarec.is_null_ = false;
			else
				EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): unexpected node is_null value; this file cannot be read." << EidosTerminate();
			
			if (metadata_parts[2] == gStr_A)		metarec.type_ = GenomeType::kAutosome;
			else if (metadata_parts[2] == gStr_X)	metarec.type_ = GenomeType::kXChromosome;
			else if (metadata_parts[2] == gStr_Y)	metarec.type_ = GenomeType::kYChromosome;
			else
				EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): unexpected node type value; this file cannot be read." << EidosTerminate();
			
			binary_metadata.emplace_back(metarec);
			
			metadata_total_part_count++;
			binary_metadata_offset.push_back((tsk_size_t)(metadata_total_part_count * sizeof(GenomeMetadataRec)));
		}
		
		ret = tsk_node_table_set_columns(&tables_.nodes,
									 tables_copy.nodes.num_rows,
									 tables_copy.nodes.flags,
									 tables_copy.nodes.time,
									 tables_copy.nodes.population,
									 tables_copy.nodes.individual,
									 (char *)binary_metadata.data(),
									 binary_metadata_offset.data());
		if (ret < 0) handle_error("convert_from_ascii", ret);
	}
	
	/***** De-ascii-ify Individuals Table *****/
	{
		static_assert(sizeof(IndividualMetadataRec) == 24, "IndividualMetadataRec has changed size; this code probably needs to be updated");
		
		const char *metadata = tables_.individuals.metadata;
		tsk_size_t *metadata_offset = tables_.individuals.metadata_offset;
		std::vector<IndividualMetadataRec> binary_metadata;
		std::vector<tsk_size_t> binary_metadata_offset;
		size_t metadata_total_part_count = 0;
		
		binary_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < tables_.individuals.num_rows; j++)
		{
			std::string string_metadata(metadata + metadata_offset[j], metadata_offset[j+1] - metadata_offset[j]);
			std::vector<std::string> metadata_parts = Eidos_string_split(string_metadata, ",");
			
			if (metadata_parts.size() != 5)
				EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): unexpected individual metadata length; this file cannot be read." << EidosTerminate();
			
			IndividualMetadataRec metarec;
			metarec.pedigree_id_ = (slim_pedigreeid_t)std::stoll(metadata_parts[0]);
			metarec.age_ = (slim_age_t)std::stoll(metadata_parts[1]);
			metarec.subpopulation_id_ = (slim_objectid_t)std::stoll(metadata_parts[2]);
			metarec.sex_ = (IndividualSex)std::stoll(metadata_parts[3]);
			metarec.flags_ = (uint32_t)std::stoull(metadata_parts[4]);
			
			binary_metadata.emplace_back(metarec);
			
			metadata_total_part_count++;
			binary_metadata_offset.push_back((tsk_size_t)(metadata_total_part_count * sizeof(IndividualMetadataRec)));
		}
		
		ret = tsk_individual_table_set_columns(&tables_.individuals,
										   tables_copy.individuals.num_rows,
										   tables_copy.individuals.flags,
										   tables_copy.individuals.location,
										   tables_copy.individuals.location_offset,
										   (char *)binary_metadata.data(),
										   binary_metadata_offset.data());
		if (ret < 0) handle_error("convert_from_ascii", ret);
	}
	
	/***** De-ascii-ify Population Table *****/
	{
		static_assert(sizeof(SubpopulationMetadataRec) == 88, "SubpopulationMetadataRec has changed size; this code probably needs to be updated");
		static_assert(sizeof(SubpopulationMigrationMetadataRec) == 12, "SubpopulationMigrationMetadataRec has changed size; this code probably needs to be updated");
		
		const char *metadata = tables_.populations.metadata;
		tsk_size_t *metadata_offset = tables_.populations.metadata_offset;
		char *binary_metadata = NULL;
		std::vector<tsk_size_t> binary_metadata_offset;
		
		binary_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < tables_.populations.num_rows; j++)
		{
			tsk_size_t metadata_string_length = metadata_offset[j+1] - metadata_offset[j];
			
			if (metadata_string_length == 0)
			{
				// empty population table entries just get preserved verbatim; these are unused subpop IDs
				binary_metadata_offset.push_back(binary_metadata_offset[j]);
				continue;
			}
			
			std::string string_metadata(metadata + metadata_offset[j], metadata_string_length);
			std::vector<std::string> metadata_parts = Eidos_string_split(string_metadata, ",");
			
			if (metadata_parts.size() < 12)
				EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): unexpected population metadata length; this file cannot be read." << EidosTerminate();
			
			SubpopulationMetadataRec metarec;
			metarec.subpopulation_id_ = (slim_objectid_t)std::stoll(metadata_parts[0]);
			metarec.selfing_fraction_ = std::stod(metadata_parts[1]);
			metarec.female_clone_fraction_ = std::stod(metadata_parts[2]);
			metarec.male_clone_fraction_ = std::stod(metadata_parts[3]);
			metarec.sex_ratio_ = std::stod(metadata_parts[4]);
			metarec.bounds_x0_ = std::stod(metadata_parts[5]);
			metarec.bounds_x1_ = std::stod(metadata_parts[6]);
			metarec.bounds_y0_ = std::stod(metadata_parts[7]);
			metarec.bounds_y1_ = std::stod(metadata_parts[8]);
			metarec.bounds_z0_ = std::stod(metadata_parts[9]);
			metarec.bounds_z1_ = std::stod(metadata_parts[10]);
			metarec.migration_rec_count_ = (uint32_t)std::stoll(metadata_parts[11]);
			
			if (metadata_parts.size() != (12 + metarec.migration_rec_count_ * 2))
				EIDOS_TERMINATION << "ERROR (SLiMSim::TreeSequenceDataFromAscii): malformed population metadata record; this file cannot be read." << EidosTerminate();
			
			size_t metadata_length = sizeof(SubpopulationMetadataRec) + metarec.migration_rec_count_ * sizeof(SubpopulationMigrationMetadataRec);
			
			binary_metadata = (char *)realloc(binary_metadata, binary_metadata_offset[j] + metadata_length);
			
			SubpopulationMetadataRec *binary_metadata_subpop_rec = (SubpopulationMetadataRec *)(binary_metadata + binary_metadata_offset[j]);
			SubpopulationMigrationMetadataRec *binary_metadata_migrations = (SubpopulationMigrationMetadataRec *)(binary_metadata_subpop_rec + 1);
			
			*binary_metadata_subpop_rec = metarec;
			
			for (size_t migration_index = 0; migration_index < metarec.migration_rec_count_; ++migration_index)
			{
				binary_metadata_migrations[migration_index].source_subpop_id_ = (slim_objectid_t)std::stoll(metadata_parts[12 + migration_index * 2]);
				binary_metadata_migrations[migration_index].migration_rate_ = std::stod(metadata_parts[12 + migration_index * 2 + 1]);
			}
			
			binary_metadata_offset.push_back((tsk_size_t)(binary_metadata_offset[j] + metadata_length));
		}
		
		ret = tsk_population_table_set_columns(&tables_.populations,
										   tables_copy.populations.num_rows,
										   binary_metadata,
										   binary_metadata_offset.data());
		if (ret < 0) handle_error("convert_from_ascii", ret);
		
		if (binary_metadata)
		{
			free(binary_metadata);
			binary_metadata = NULL;
		}
	}

    // not sure if we need to do this here, but it doesn't hurt
	RecordTablePosition();
	
	// We are done with our private copy of the table collection
	tsk_table_collection_free(&tables_copy);
}

void SLiMSim::TreeSequenceDataToAscii(tsk_table_collection_t *p_tables)
{
	// This modifies p_tables in place, replacing the metadata and derived_state columns of p_tables with ASCII versions.
	// We make a copy of the table collection here, as a source for column data, because you can't pass existing
	// columns in to tsk_mutation_table_set_columns() and tsk_node_table_set_columns(); there is no way to just patch up the
	// columns in p_tables, we have to copy a new set of information into p_tables wholesale.
	tsk_table_collection_t tables_copy;
	int ret = tsk_table_collection_copy(p_tables, &tables_copy, 0);
	if (ret < 0) handle_error("convert_to_ascii", ret);
	
    /********************************************************
     * Make the data stored in the tables readable as ASCII.
     ********************************************************/
	
    /***  Notes: ancestral states are always zero-length, so we don't need to Ascii-ify Site Table ***/
	
	// this buffer is used for converting double values to strings
	static char *double_buf = NULL;
	
	if (!double_buf)
		double_buf = (char *)malloc(40 *sizeof(char));
	
    /***  Ascii-ify Mutation Table ***/
	{
		static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec has changed size; this code probably needs to be updated");
		
		// Mutation derived state
		const char *derived_state = p_tables->mutations.derived_state;
		tsk_size_t *derived_state_offset = p_tables->mutations.derived_state_offset;
		std::string text_derived_state;
		std::vector<tsk_size_t> text_derived_state_offset;
		
		// Mutation metadata
		const char *mutation_metadata = p_tables->mutations.metadata;
		tsk_size_t *mutation_metadata_offset = p_tables->mutations.metadata_offset;
		std::string text_mutation_metadata;
		std::vector<tsk_size_t> text_mutation_metadata_offset;
		
		text_derived_state_offset.push_back(0);
		text_mutation_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < p_tables->mutations.num_rows; j++)
		{
			// Mutation derived state
			slim_mutationid_t *int_derived_state = (slim_mutationid_t *)(derived_state + derived_state_offset[j]);
			size_t cur_derived_state_length = (derived_state_offset[j+1] - derived_state_offset[j])/sizeof(slim_mutationid_t);
			
			for (size_t i = 0; i < cur_derived_state_length; i++)
			{
				if (i != 0) text_derived_state.append(",");
				text_derived_state.append(std::to_string(int_derived_state[i]));
			}
			text_derived_state_offset.push_back((tsk_size_t)text_derived_state.size());
			
			// Mutation metadata
			MutationMetadataRec *struct_mutation_metadata = (MutationMetadataRec *)(mutation_metadata + mutation_metadata_offset[j]);
			size_t cur_mutation_metadata_length = (mutation_metadata_offset[j+1] - mutation_metadata_offset[j])/sizeof(MutationMetadataRec);
			
			assert(cur_mutation_metadata_length == cur_derived_state_length);
			
			for (size_t i = 0; i < cur_mutation_metadata_length; i++)
			{
				if (i > 0) text_mutation_metadata.append(";");
				text_mutation_metadata.append(std::to_string(struct_mutation_metadata->mutation_type_id_));
				text_mutation_metadata.append(",");
				
				static_assert(sizeof(slim_selcoeff_t) == 4, "use EIDOS_DBL_DIGS if slim_selcoeff_t is double");
				sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_mutation_metadata->selection_coeff_);		// necessary precision for non-lossiness
				text_mutation_metadata.append(double_buf);
				text_mutation_metadata.append(",");
				
				text_mutation_metadata.append(std::to_string(struct_mutation_metadata->subpop_index_));
				text_mutation_metadata.append(",");
				text_mutation_metadata.append(std::to_string(struct_mutation_metadata->origin_generation_));
				text_mutation_metadata.append(",");
				text_mutation_metadata.append(std::to_string(struct_mutation_metadata->nucleotide_));	// new in SLiM 3.3, file format 0.3 and later; -1 if no nucleotide
				struct_mutation_metadata++;
			}
			text_mutation_metadata_offset.push_back((tsk_size_t)text_mutation_metadata.size());
		}
		
		ret = tsk_mutation_table_set_columns(&p_tables->mutations,
										tables_copy.mutations.num_rows,
										tables_copy.mutations.site,
										tables_copy.mutations.node,
										tables_copy.mutations.parent,
										tables_copy.mutations.time,
										text_derived_state.c_str(),
										text_derived_state_offset.data(),
										text_mutation_metadata.c_str(),
										text_mutation_metadata_offset.data());
		if (ret < 0) handle_error("convert_to_ascii", ret);
	}
	
	/***** Ascii-ify Node Table *****/
	{
		static_assert(sizeof(GenomeMetadataRec) == 10, "GenomeMetadataRec has changed size; this code probably needs to be updated");
		
		const char *metadata = p_tables->nodes.metadata;
		tsk_size_t *metadata_offset = p_tables->nodes.metadata_offset;
		std::string text_metadata;
		std::vector<tsk_size_t> text_metadata_offset;
		
		text_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < p_tables->nodes.num_rows; j++)
		{
			GenomeMetadataRec *struct_genome_metadata = (GenomeMetadataRec *)(metadata + metadata_offset[j]);
			
			text_metadata.append(std::to_string(struct_genome_metadata->genome_id_));
			text_metadata.append(",");
			text_metadata.append(struct_genome_metadata->is_null_ ? "T" : "F");
			text_metadata.append(",");
			text_metadata.append(StringForGenomeType(struct_genome_metadata->type_));
			text_metadata_offset.push_back((tsk_size_t)text_metadata.size());
		}
		
		ret = tsk_node_table_set_columns(&p_tables->nodes,
									 tables_copy.nodes.num_rows,
									 tables_copy.nodes.flags,
									 tables_copy.nodes.time,
									 tables_copy.nodes.population,
									 tables_copy.nodes.individual,
									 text_metadata.c_str(),
									 text_metadata_offset.data());
		if (ret < 0) handle_error("convert_to_ascii", ret);
	}
	
	/***** Ascii-ify Individuals Table *****/
	{
		static_assert(sizeof(IndividualMetadataRec) == 24, "IndividualMetadataRec has changed size; this code probably needs to be updated");
		
		const char *metadata = p_tables->individuals.metadata;
		tsk_size_t *metadata_offset = p_tables->individuals.metadata_offset;
		std::string text_metadata;
		std::vector<tsk_size_t> text_metadata_offset;
		
		text_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < p_tables->individuals.num_rows; j++)
		{
			IndividualMetadataRec *struct_individual_metadata = (IndividualMetadataRec *)(metadata + metadata_offset[j]);
			
			text_metadata.append(std::to_string(struct_individual_metadata->pedigree_id_));
			text_metadata.append(",");
			text_metadata.append(std::to_string(struct_individual_metadata->age_));
			text_metadata.append(",");
			text_metadata.append(std::to_string(struct_individual_metadata->subpopulation_id_));
			text_metadata.append(",");
			text_metadata.append(std::to_string((int32_t)struct_individual_metadata->sex_));
			text_metadata.append(",");
			text_metadata.append(std::to_string(struct_individual_metadata->flags_));
			text_metadata_offset.push_back((tsk_size_t)text_metadata.size());
		}
		
		ret = tsk_individual_table_set_columns(&p_tables->individuals,
										   tables_copy.individuals.num_rows,
										   tables_copy.individuals.flags,
										   tables_copy.individuals.location,
										   tables_copy.individuals.location_offset,
										   text_metadata.c_str(),
										   text_metadata_offset.data());
		if (ret < 0) handle_error("convert_to_ascii", ret);
	}
	
	/***** Ascii-ify Population Table *****/
	{
		static_assert(sizeof(SubpopulationMetadataRec) == 88, "SubpopulationMetadataRec has changed size; this code probably needs to be updated");
		static_assert(sizeof(SubpopulationMigrationMetadataRec) == 12, "SubpopulationMigrationMetadataRec has changed size; this code probably needs to be updated");
		
		const char *metadata = p_tables->populations.metadata;
		tsk_size_t *metadata_offset = p_tables->populations.metadata_offset;
		std::string text_metadata;
		std::vector<tsk_size_t> text_metadata_offset;
		
		text_metadata_offset.push_back(0);
		
		for (size_t j = 0; j < p_tables->populations.num_rows; j++)
		{
			tsk_size_t metadata_binary_length = metadata_offset[j+1] - metadata_offset[j];
			
			if (metadata_binary_length == 0)
			{
				// empty population table entries just get preserved verbatim; these are unused subpop IDs
				text_metadata_offset.push_back((tsk_size_t)text_metadata.size());
				continue;
			}
			
			SubpopulationMetadataRec *struct_population_metadata = (SubpopulationMetadataRec *)(metadata + metadata_offset[j]);
			SubpopulationMigrationMetadataRec *struct_migration_metadata = (SubpopulationMigrationMetadataRec *)(struct_population_metadata + 1);
			
			text_metadata.append(std::to_string(struct_population_metadata->subpopulation_id_));
			text_metadata.append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->selfing_fraction_);
			text_metadata.append(double_buf);
			text_metadata.append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->female_clone_fraction_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->male_clone_fraction_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->sex_ratio_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->bounds_x0_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->bounds_x1_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->bounds_y0_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->bounds_y1_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->bounds_z0_);
			text_metadata.append(double_buf).append(",");
			
			sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_population_metadata->bounds_z1_);
			text_metadata.append(double_buf).append(",");
			
			text_metadata.append(std::to_string(struct_population_metadata->migration_rec_count_));
			
			for (size_t migration_index = 0; migration_index < struct_population_metadata->migration_rec_count_; ++migration_index)
			{
				text_metadata.append(",");
				
				text_metadata.append(std::to_string(struct_migration_metadata[migration_index].source_subpop_id_));
				text_metadata.append(",");
				
				sprintf(double_buf, "%.*g", EIDOS_FLT_DIGS, struct_migration_metadata[migration_index].migration_rate_);
				text_metadata.append(double_buf);
			}
			
			text_metadata_offset.push_back((tsk_size_t)text_metadata.size());
		}
		
		ret = tsk_population_table_set_columns(&p_tables->populations,
										   tables_copy.populations.num_rows,
										   text_metadata.c_str(),
										   text_metadata_offset.data());
		if (ret < 0) handle_error("convert_to_ascii", ret);
	}
	
	// We are done with our private copy of the table collection
	tsk_table_collection_free(&tables_copy);
}

void SLiMSim::DerivedStatesFromAscii(tsk_table_collection_t *p_tables)
{
	// This modifies p_tables in place, replacing the derived_state column of p_tables with a binary version.
	// See TreeSequenceDataFromAscii() for comments; this is basically just a pruned version of that method.
	tsk_mutation_table_t mutations_copy;
	int ret = tsk_mutation_table_copy(&p_tables->mutations, &mutations_copy, 0);
	if (ret < 0) handle_error("derived_to_ascii", ret);
	
	{
		const char *derived_state = p_tables->mutations.derived_state;
		tsk_size_t *derived_state_offset = p_tables->mutations.derived_state_offset;
		std::vector<slim_mutationid_t> binary_derived_state;
		std::vector<tsk_size_t> binary_derived_state_offset;
		size_t derived_state_total_part_count = 0;
		
		binary_derived_state_offset.push_back(0);
		
		for (size_t j = 0; j < p_tables->mutations.num_rows; j++)
		{
			std::string string_derived_state(derived_state + derived_state_offset[j], derived_state_offset[j+1] - derived_state_offset[j]);
			
			if (string_derived_state.size() == 0)
			{
				// nothing to do for an empty derived state
			}
			else if (string_derived_state.find(",") == std::string::npos)
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
			
			binary_derived_state_offset.push_back((tsk_size_t)(derived_state_total_part_count * sizeof(slim_mutationid_t)));
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
		if (ret < 0) handle_error("convert_from_ascii", ret);
	}
	
	tsk_mutation_table_free(&mutations_copy);
}

void SLiMSim::DerivedStatesToAscii(tsk_table_collection_t *p_tables)
{
	// This modifies p_tables in place, replacing the derived_state column of p_tables with an ASCII version.
	// See TreeSequenceDataToAscii() for comments; this is basically just a pruned version of that method.
	tsk_mutation_table_t mutations_copy;
	int ret = tsk_mutation_table_copy(&p_tables->mutations, &mutations_copy, 0);
	if (ret < 0) handle_error("derived_to_ascii", ret);
	
	{
		const char *derived_state = p_tables->mutations.derived_state;
		tsk_size_t *derived_state_offset = p_tables->mutations.derived_state_offset;
		std::string text_derived_state;
		std::vector<tsk_size_t> text_derived_state_offset;
		
		text_derived_state_offset.push_back(0);
		
		for (size_t j = 0; j < p_tables->mutations.num_rows; j++)
		{
			slim_mutationid_t *int_derived_state = (slim_mutationid_t *)(derived_state + derived_state_offset[j]);
			size_t cur_derived_state_length = (derived_state_offset[j+1] - derived_state_offset[j])/sizeof(slim_mutationid_t);
			
			for (size_t i = 0; i < cur_derived_state_length; i++)
			{
				if (i != 0) text_derived_state.append(",");
				text_derived_state.append(std::to_string(int_derived_state[i]));
			}
			text_derived_state_offset.push_back((tsk_size_t)text_derived_state.size());
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

void SLiMSim::AddIndividualsToTable(Individual * const *p_individual, size_t p_num_individuals, tsk_table_collection_t *p_tables, uint32_t p_flags)
{
    // We use currently use this function in two ways, depending on p_flags:
    //  1. (SLIM_TSK_INDIVIDUAL_REMEMBERED) to retain individuals to be forever remembered, or
    //  2. (SLIM_TSK_INDIVIDUAL_ALIVE) to retain the final generation in the tree sequence.
    // So, in case (1) we set the REMEMBERED flag,
    // and in case (2) we set the ALIVE flag.  Individuals who are permanently
    // remembered but still alive when the tree sequence is written out will
    // have this method called on them twice, first (1), then (2), so they get both flags set.

	// do this so that we can access the internal tables from outside, by passing in nullptr
	if (p_tables == nullptr)
		p_tables = &tables_;
	
	// construct the map of currently remembered individuals first; these are not really just those
	// that are "remembered", but all individuals that are currently in the tables
	// BCH 16 Nov. 2019: Making this into an unordered_map for faster lookup; this can end up
	// being accessed for a large number of individuals, making for an O(N*M) bottleneck.
	// The key is the pedigree ID, so we can look up remembered individuals quickly; the value
	// is the index of that pedigree ID in the list of remembered individuals, so we can
	// look up the metadata for the remembered individual and patch it with new information.
	// Also making it so we don't use this map at all if we're adding first-gen individuals,
	// since we know they are not already remembered.
	// BCH 28 Jan. 2020: My previous optimization turns out to be quite slow in one case: when
	// p_num_individuals == 1, because the user is adding just a single remembered individual
	// to the list.  When this is done many times (as in my #116 test case!), the overhead of
	// building a whole std::unordered_map for a single lookup proves to be a big problem --
	// a large percentage of total runtime, which it used to be negligible.  This seems like
	// a case that might well arise in real-world use, so I'm optimizing it by re-introducing
	// the old std::vector code, used only when p_num_individuals < 5 (a wild guess at a
	// heuristic).  This makes the code a bit messy, but it's simple really: we just use one
	// of two data structures, std::vector or std::unordered_map, to do our lookups based on
	// how many lookups we anticipate doing.
	bool using_std_vector = (p_num_individuals < 5);
    std::vector<slim_pedigreeid_t> remembered_individuals;									// used when using_std_vector==true
	std::unordered_map<slim_pedigreeid_t, slim_popsize_t> remembered_individuals_lookup;	// used when using_std_vector==false
	
	if (using_std_vector)
	{
		slim_pedigreeid_t last_added_id = -1;
		
		for (tsk_id_t nid : remembered_genomes_) 
		{
			tsk_id_t tsk_individual = p_tables->nodes.individual[nid];
			assert((tsk_individual >= 0) && ((tsk_size_t)tsk_individual < p_tables->individuals.num_rows));
			IndividualMetadataRec *metadata_rec = (IndividualMetadataRec *)(p_tables->individuals.metadata + p_tables->individuals.metadata_offset[tsk_individual]);
			slim_pedigreeid_t metadata_id = metadata_rec->pedigree_id_;
			
			// remembered_genomes_ has two entries per individual; we want to work with individuals, so we filter
			if (metadata_id != last_added_id)
			{
				remembered_individuals.push_back(metadata_rec->pedigree_id_);
				last_added_id = metadata_id;
			}
		}
	}
	else
	{
		slim_pedigreeid_t last_added_id = -1;
		slim_popsize_t added_count = 0;
		
		for (tsk_id_t nid : remembered_genomes_) 
		{
			tsk_id_t tsk_individual = p_tables->nodes.individual[nid];
			assert((tsk_individual >= 0) && ((tsk_size_t)tsk_individual < p_tables->individuals.num_rows));
			IndividualMetadataRec *metadata_rec = (IndividualMetadataRec *)(p_tables->individuals.metadata + p_tables->individuals.metadata_offset[tsk_individual]);
			slim_pedigreeid_t metadata_id = metadata_rec->pedigree_id_;
			
			// remembered_genomes_ has two entries per individual; we want to work with individuals, so we filter
			if (metadata_id != last_added_id)
			{
				remembered_individuals_lookup.emplace(std::pair<slim_pedigreeid_t, slim_popsize_t>(metadata_id, added_count));
				last_added_id = metadata_id;
				added_count++;
			}
		}
	}
	
	// loop over individuals and add entries to the individual table; if they are already
	// there, we just need to update their metadata, location, etc.
    for (size_t j = 0; j < p_num_individuals; j++)
    {
        Individual *ind = p_individual[j];
        slim_pedigreeid_t ped_id = ind->PedigreeID();

        std::vector<double> location;
        location.push_back(ind->spatial_x_);
        location.push_back(ind->spatial_y_);
        location.push_back(ind->spatial_z_);
        
        IndividualMetadataRec metadata_rec;
        MetadataForIndividual(ind, &metadata_rec);
		
		tsk_id_t tsk_individual;
		
        if (using_std_vector)
        {
            // this case has a slow lookup (linear search), but the vector is fast to build
            auto ind_pos = std::find(remembered_individuals.begin(), remembered_individuals.end(), ped_id);
            
            if (ind_pos == remembered_individuals.end())
                tsk_individual = TSK_NULL;	// not in the table already
            else
                tsk_individual = (tsk_id_t)std::distance(remembered_individuals.begin(), ind_pos);
        }
        else
        {
            // this case has a fast search (hash table), but the unordered_map is slow to build
            auto ind_pos = remembered_individuals_lookup.find(ped_id);
            
            if (ind_pos == remembered_individuals_lookup.end())
                tsk_individual = TSK_NULL;	// not in the table already
            else
                tsk_individual = ind_pos->second;
        }
		
        if (tsk_individual == TSK_NULL) {
            // This individual is not already in the tables.
            tsk_individual = tsk_individual_table_add_row(&p_tables->individuals,
                    p_flags, location.data(), (uint32_t)location.size(), 
                    (char *)&metadata_rec, (uint32_t)sizeof(IndividualMetadataRec));
            if (tsk_individual < 0) handle_error("tsk_individual_table_add_row", tsk_individual);
            
            // Update node table
            assert(ind->genome1_->tsk_node_id_ < (tsk_id_t) p_tables->nodes.num_rows
                   && ind->genome2_->tsk_node_id_ < (tsk_id_t) p_tables->nodes.num_rows);
            p_tables->nodes.individual[ind->genome1_->tsk_node_id_] = tsk_individual;
            p_tables->nodes.individual[ind->genome2_->tsk_node_id_] = tsk_individual;

            // update remembered genomes
            if (p_flags & SLIM_TSK_INDIVIDUAL_REMEMBERED)
            {
                remembered_genomes_.push_back(ind->genome1_->tsk_node_id_);
                remembered_genomes_.push_back(ind->genome2_->tsk_node_id_);
            }
        } else {
            // This individual is already there; we need to update the information.
            assert(((size_t)tsk_individual < p_tables->individuals.num_rows)
                   && (location.size()
                       == (p_tables->individuals.location_offset[tsk_individual + 1]
                           - p_tables->individuals.location_offset[tsk_individual]))
                   && (sizeof(IndividualMetadataRec)
                       == (p_tables->individuals.metadata_offset[tsk_individual + 1]
                           - p_tables->individuals.metadata_offset[tsk_individual])));
            
			// BCH 4/29/2019: This assert is, we think, not technically necessary – the code
			// would work even if it were violated.  But it's a nice invariant to guarantee,
			// and right now it is always true.
			assert((p_tables->nodes.individual[ind->genome1_->tsk_node_id_]
                     == tsk_individual)
                   && (p_tables->nodes.individual[ind->genome2_->tsk_node_id_]
                       == tsk_individual));
			
			memcpy(p_tables->individuals.location
				   + p_tables->individuals.location_offset[tsk_individual],
				   location.data(), location.size() * sizeof(double));
            memcpy(p_tables->individuals.metadata
                    + p_tables->individuals.metadata_offset[tsk_individual],
                    &metadata_rec, sizeof(IndividualMetadataRec));
			p_tables->individuals.flags[tsk_individual] |= p_flags;
			
            // Check node table
            assert(ind->genome1_->tsk_node_id_ < (tsk_id_t) p_tables->nodes.num_rows
                   && ind->genome2_->tsk_node_id_ < (tsk_id_t) p_tables->nodes.num_rows);
			
			// BCH 4/29/2019: These asserts are, we think, not technically necessary – the code
			// would work even if they were violated.  But they're a nice invariant to guarantee,
			// and right now they are always true.
            assert(p_tables->nodes.individual[ind->genome1_->tsk_node_id_] == (tsk_id_t)tsk_individual);
            assert(p_tables->nodes.individual[ind->genome2_->tsk_node_id_] == (tsk_id_t)tsk_individual);
        }
    }
}

void SLiMSim::AddCurrentGenerationToIndividuals(tsk_table_collection_t *p_tables)
{
	// add currently alive individuals to the individuals table, so they persist
	// through simplify and can be revived when loading saved state
	for (auto subpop_iter : population_.subpops_)
	{
        AddIndividualsToTable(subpop_iter.second->parent_individuals_.data(), subpop_iter.second->parent_individuals_.size(), p_tables, SLIM_TSK_INDIVIDUAL_ALIVE);
	}
}

void SLiMSim::FixAliveIndividuals(tsk_table_collection_t *p_tables)
{
	// This clears the alive flags of the remaining entries; our internal tables never say "alive",
	// since that changes from generation to generation, so after loading saved state we want to strip
	for (size_t j = 0; j < p_tables->individuals.num_rows; j++)
		p_tables->individuals.flags[j] &= (~SLIM_TSK_INDIVIDUAL_ALIVE);
}

void SLiMSim::WritePopulationTable(tsk_table_collection_t *p_tables)
{
	// This overwrites whatever might be previously in the population table
	tsk_population_table_clear(&p_tables->populations);
	
	// we will write out empty entries for all unused slots, up to largest_subpop_id_
	// this is because tskit doesn't like unused slots; slot indices must correspond to subpop ids
	slim_objectid_t last_subpop_id = -1; // population_.largest_subpop_id_;
	
	for (size_t j = 0; j < p_tables->nodes.num_rows; j++)
		last_subpop_id = std::max(last_subpop_id, p_tables->nodes.population[j]);
	
	// write out an entry for each subpop
	slim_objectid_t last_id_written = -1;
	
	for (auto subpop_iter : population_.subpops_)
	{
		Subpopulation *subpop = subpop_iter.second;
		slim_objectid_t subpop_id = subpop->subpopulation_id_;
		
		// first, write out empty entries for unused subpop ids before this one
		while (last_id_written < subpop_id - 1)
		{
			tsk_id_t tsk_population = tsk_population_table_add_row(&p_tables->populations, (char *)&last_subpop_id, (uint32_t)0);	// the address is unused
			if (tsk_population < 0) handle_error("tsk_population_table_add_row", tsk_population);
			last_id_written++;
		}
		
		// now we're at the slot for this subpopulation, so construct it and write it out
		size_t migration_rec_count = subpop->migrant_fractions_.size();
		size_t metadata_length = sizeof(SubpopulationMetadataRec) + migration_rec_count * sizeof(SubpopulationMigrationMetadataRec);
		SubpopulationMetadataRec *metadata_rec = (SubpopulationMetadataRec *)malloc(metadata_length);
		SubpopulationMigrationMetadataRec *migration_rec_base = (SubpopulationMigrationMetadataRec *)(metadata_rec + 1);
		
		metadata_rec->subpopulation_id_ = subpop->subpopulation_id_;
		metadata_rec->selfing_fraction_ = subpop->selfing_fraction_;
		metadata_rec->female_clone_fraction_ = subpop->female_clone_fraction_;
		metadata_rec->male_clone_fraction_ = subpop->male_clone_fraction_;
		metadata_rec->sex_ratio_ = subpop->parent_sex_ratio_;
		metadata_rec->bounds_x0_ = subpop->bounds_x0_;
		metadata_rec->bounds_x1_ = subpop->bounds_x1_;
		metadata_rec->bounds_y0_ = subpop->bounds_y0_;
		metadata_rec->bounds_y1_ = subpop->bounds_y1_;
		metadata_rec->bounds_z0_ = subpop->bounds_z0_;
		metadata_rec->bounds_z1_ = subpop->bounds_z1_;
		metadata_rec->migration_rec_count_ = (uint32_t)migration_rec_count;
		
		int migration_index = 0;
		
		for (std::pair<slim_objectid_t,double> migration_pair : subpop->migrant_fractions_)
		{
			migration_rec_base[migration_index].source_subpop_id_ = migration_pair.first;
			migration_rec_base[migration_index].migration_rate_ = migration_pair.second;
			migration_index++;
		}
		
		tsk_id_t tsk_population = tsk_population_table_add_row(&p_tables->populations, (char *)metadata_rec, (uint32_t)metadata_length);
		last_id_written++;
		
		free(metadata_rec);
		
		if (tsk_population < 0) handle_error("tsk_population_table_add_row", tsk_population);
	}
	
	// finally, write out empty entries for the rest of the table; empty entries are needed
	// up to largest_subpop_id_ because there could be ancestral nodes that reference them
	while (last_id_written < last_subpop_id)
	{
		tsk_id_t tsk_population = tsk_population_table_add_row(&p_tables->populations, (char *)&last_subpop_id, (uint32_t)0);	// the address is unused
		if (tsk_population < 0) handle_error("tsk_population_table_add_row", tsk_population);
		last_id_written++;
	}
}

void SLiMSim::WriteTreeSequenceMetadata(tsk_table_collection_t *p_tables)
{
    int ret = 0;

    //////
    // Top-level (tree sequence) metadata:
    // In the future, we might need to *add* to the metadata *and also* the schema,
    // leaving other keys that might already be there.
    // But that's being a headache, so we're skipping it.
    nlohmann::json metadata;
    if (ModelType() == SLiMModelType::kModelTypeWF) {
        metadata["SLiM"]["model_type"] = "WF";
        if (GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) {
            metadata["SLiM"]["stage"] = "early";
        } else {
            assert(GenerationStage() == SLiMGenerationStage::kWFStage5ExecuteLateScripts);
            metadata["SLiM"]["stage"] = "late";
        }
    } else {
        assert(ModelType() == SLiMModelType::kModelTypeNonWF);
        metadata["SLiM"]["model_type"] = "nonWF";
        if (GenerationStage() == SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) {
            metadata["SLiM"]["stage"] = "early";
        } else {
            assert(GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts);
            metadata["SLiM"]["stage"] = "late";
        }
    }
    metadata["SLiM"]["generation"] = Generation();
	metadata["SLiM"]["file_version"] = SLIM_TREES_FILE_VERSION;
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
    ret = tsk_node_table_set_metadata_schema(&p_tables->nodes,
            gSLiM_tsk_node_metadata_schema.c_str(),
            (tsk_size_t)gSLiM_tsk_node_metadata_schema.length());
    if (ret != 0)
        handle_error("tsk_node_table_set_metadata_schema", ret);
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
}

void SLiMSim::WriteProvenanceTable(tsk_table_collection_t *p_tables, bool p_use_newlines, bool p_include_model)
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
	// Old provenance generation code, making a JSON string by hand; this is file_version 0.1
	char *provenance_str;
	provenance_str = (char *)malloc(1024);
	sprintf(provenance_str, "{\"program\": \"SLiM\", \"version\": \"%s\", \"file_version\": \"%s\", \"model_type\": \"%s\", \"generation\": %d, \"remembered_node_count\": %ld}",
			SLIM_VERSION_STRING, SLIM_TREES_FILE_VERSION, (ModelType() == SLiMModelType::kModelTypeWF) ? "WF" : "nonWF", Generation(), (long)remembered_genomes_.size());
	
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buffer, timestamp_size, "%Y-%m-%dT%H:%M:%S", tm_info);
	
	ret = tsk_provenance_table_add_row(&p_tables->provenances, buffer, strlen(buffer), provenance_str, strlen(provenance_str));
	free(provenance_str);
	if (ret < 0) handle_error("tsk_provenance_table_add_row", ret);
#else
	// New provenance generation code, using the JSON for Modern C++ library (json.hpp); this is file_version 0.2 (and up)
	nlohmann::json j;
	
	j["schema_version"] = "1.0.0";
	
	struct utsname name;
	ret = uname(&name);
	j["environment"]["os"]["version"] = std::string(name.version);
	j["environment"]["os"]["node"] = std::string(name.nodename);
	j["environment"]["os"]["release"] = std::string(name.release);
	j["environment"]["os"]["system"] = std::string(name.sysname);
	j["environment"]["os"]["machine"] = std::string(name.machine);
	
	j["software"]["name"] = "SLiM";		// note this key was named "program" in provenance version 0.1
	j["software"]["version"] = SLIM_VERSION_STRING;
	
	j["slim"]["file_version"] = SLIM_TREES_FILE_VERSION;	// 0.4 for this provenance format, 0.3 for after nucleotides were added, 0.2 before nucleotides, 0.1 for the old format above
	j["slim"]["generation"] = Generation();
	//j["slim"]["remembered_node_count"] = (long)remembered_genomes_.size();	// no longer writing this key!
	
	// compute the SHA-256 hash of the script string
	const std::string &scriptString = script_->String();
	const char *script_chars = scriptString.c_str();
	uint8_t script_hash[32];
	char script_hash_string[65];
	
	Eidos_calc_sha_256(script_hash, script_chars, strlen(script_chars));
	Eidos_hash_to_string(script_hash_string, script_hash);
	
	std::string scriptHashString(script_hash_string);
	
	//std::cout << "script hash: " << scriptHashString << std::endl;
	
	j["parameters"]["command"] = cli_params_;

    // note high overlap with WriteTreeSequenceMetadata
    if (ModelType() == SLiMModelType::kModelTypeWF) {
        j["parameters"]["model_type"] = "WF";
        if (GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) {
            j["parameters"]["stage"] = "early";
        } else {
            assert(GenerationStage() == SLiMGenerationStage::kWFStage5ExecuteLateScripts);
            j["parameters"]["stage"] = "late";
        }
    } else {
        assert(ModelType() == SLiMModelType::kModelTypeNonWF);
        j["parameters"]["model_type"] = "nonWF";
        if (GenerationStage() == SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) {
            j["parameters"]["stage"] = "early";
        } else {
            assert(GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts);
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
	j["parameters"]["seed"] = original_seed_;
	
	j["metadata"]["individuals"]["flags"]["16"]["name"] = "SLIM_TSK_INDIVIDUAL_ALIVE";
	j["metadata"]["individuals"]["flags"]["16"]["description"] = "the individual was alive at the time the file was written";
	j["metadata"]["individuals"]["flags"]["17"]["name"] = "SLIM_TSK_INDIVIDUAL_REMEMBERED";
	j["metadata"]["individuals"]["flags"]["17"]["description"] = "the individual was requested by the user to be remembered";
	
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

void SLiMSim::ReadTreeSequenceMetadata(tsk_table_collection_t *p_tables, slim_generation_t *p_generation, SLiMModelType *p_model_type, int *p_file_version)
{
#if 0
	// Old provenance reading code; this can handle file_version 0.1 only
	tsk_provenance_table_t &provenance_table = p_tables->provenances;
	tsk_size_t num_rows = provenance_table.num_rows;
	
	if (num_rows <= 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): no provenance table entries; this file cannot be read." << EidosTerminate();
	
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
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): no SLiM provenance table entry found; this file cannot be read." << EidosTerminate();
	
	//char *slim_timestamp = provenance_table.timestamp + provenance_table.timestamp_offset[slim_record_index];
	char *slim_record = provenance_table.record + provenance_table.record_offset[slim_record_index];
	tsk_size_t slim_record_len = provenance_table.record_offset[slim_record_index + 1] - provenance_table.record_offset[slim_record_index];
	
	if (slim_record_len >= 1024)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): SLiM provenance table entry is too long; this file cannot be read." << EidosTerminate();
	
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
	}
	
	int end_pos;
	int conv = sscanf(last_record_str.c_str(), "{\"program\": \"%100[^\"]\", \"version\": \"%100[^\"]\", \"file_version\": \"%100[^\"]\", \"model_type\": \"%100[^\"]\", \"generation\": %100[0-9], \"remembered_node_count\": %100[0-9]}%n", program, version, file_version, model_type, generation, rem_count, &end_pos);
	
	if ((conv != 6) || (end_pos != (int)slim_record_len))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): provenance table entry was malformed; this file cannot be read." << EidosTerminate();
	
	std::string program_str(program);
	std::string version_str(version);
	std::string file_version_str(file_version);
	std::string model_type_str(model_type);
	std::string generation_str(generation);
	std::string rem_count_str(rem_count);
	
	if (program_str != "SLiM")
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): this .trees file was not generated with correct SLiM provenance information; this file cannot be read." << EidosTerminate();
	
	if (file_version_str != "0.1")
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): this .trees file was generated by an unrecognized version of SLiM or pyslim; this file cannot be read." << EidosTerminate();
	
	// check the model type; at the moment we do not require the model type to match what we are running, but we issue a warning on a mismatch
	if ((model_type_str != "WF") && (model_type_str != "nonWF"))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): unrecognized model type; this file cannot be read." << EidosTerminate();
	
	if (((model_type_str == "WF") && (ModelType() != SLiMModelType::kModelTypeWF)) ||
		((model_type_str == "nonWF") && (ModelType() != SLiMModelType::kModelTypeNonWF)))
	{
		if (!gEidosSuppressWarnings)
			SLIM_OUTSTREAM << "#WARNING (SLiMSim::ReadTreeSequenceMetadata): the model type of the .trees file does not match the current model type." << std::endl;
	}
	
	if (model_type_str == "WF")
		*p_model_type = SLiMModelType::kModelTypeWF;
	else if (model_type_str == "nonWF")
		*p_model_type = SLiMModelType::kModelTypeNonWF;
	
	// read the generation and bounds-check it
	long long gen_ll = 0;
	
	try {
		gen_ll = std::stoll(generation_str);
	}
	catch (...)
	{
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): malformed generation value; this file cannot be read." << EidosTerminate();
	}
	
	if ((gen_ll < 1) || (gen_ll > SLIM_MAX_GENERATION))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): generation value out of range; this file cannot be read." << EidosTerminate();
	
	*p_generation = (slim_generation_t)gen_ll;
	
	// read the remembered genome count and bounds-check it
	long long rem_count_ll = 0;
	
	try {
		rem_count_ll = std::stoll(rem_count_str);
	}
	catch (...)
	{
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): malformed remembered node count; this file cannot be read." << EidosTerminate();
	}
	
	if (rem_count_ll < 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): remembered node count value out of range; this file cannot be read." << EidosTerminate();
	
	*p_remembered_genome_count = (size_t)rem_count_ll;
#else
	// New provenance reading code, using the JSON for Modern C++ library (json.hpp); 
    
	std::string model_type_str;
	long long gen_ll;

    ////////////
    // Format 0.5: using top-level metadata
    try {
        // Note: we *could* parse the metadata schema (which is json),
        // but instead we'll just try parsing the metadata.
        // std::string metadata_schema_str(p_tables->metadata_schema, p_tables->metadata_schema_length);
        // nlohmann::json metadata_schema = nlohmann::json::parse(metadata_schema_str);

        std::string metadata_str(p_tables->metadata, p_tables->metadata_length);
        auto metadata = nlohmann::json::parse(metadata_str);
        model_type_str = metadata["SLiM"]["model_type"];
        gen_ll = metadata["SLiM"]["generation"];
        auto file_version_03 = metadata["SLiM"]["file_version"];
		if (file_version_03 == SLIM_TREES_FILE_VERSION)
			*p_file_version = 5;
        else
			EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): this .trees file was generated by an unrecognized version of SLiM or pyslim; this file cannot be read." << EidosTerminate();
    } catch (...) {
    ///////////////////////
    // Previous formats: everything is in provenance

        tsk_provenance_table_t &provenance_table = p_tables->provenances;
        tsk_size_t num_rows = provenance_table.num_rows;
        
        if (num_rows <= 0)
            EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): no SLiM metadata; this file cannot be read." << EidosTerminate();
        
        // find the last record that is a SLiM provenance entry; we allow entries after ours, on the assumption that they have preserved SLiM-compliance
        int slim_record_index = num_rows - 1;
        
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
            EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): no SLiM provenance table entry found; this file cannot be read." << EidosTerminate();
        
        //char *slim_timestamp = provenance_table.timestamp + provenance_table.timestamp_offset[slim_record_index];
        char *slim_record = provenance_table.record + provenance_table.record_offset[slim_record_index];
        tsk_size_t slim_record_len = provenance_table.record_offset[slim_record_index + 1] - provenance_table.record_offset[slim_record_index];
        std::string slim_record_str(slim_record, slim_record_len);
        auto j = nlohmann::json::parse(slim_record_str);
        
        //std::cout << "Read provenance:\n" << slim_record_str << std::endl;
        
        auto file_version_01 = j["file_version"];
        auto file_version_02 = j["slim"]["file_version"];
        
        if (file_version_01.is_string() && (file_version_01 == "0.1"))
        {
            // We actually don't have any chance of being able to read SLiM 3.0 .trees files in, I guess;
            // all the new individuals table stuff, the addition of the population table, etc., mean that
            // it would just be a huge headache to try to do this.  So let's throw an error up front.
            EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): file_version is 0.1 in .trees file; this file cannot be read.  SLiM 3.1 and later cannot read saved .trees files from prior versions of SLiM; sorry." << EidosTerminate();
            
            /*
            try {
                model_type_str = j["model_type"];
                gen_ll = j["generation"];
                //rem_count_ll = j["remembered_node_count"];	// no longer using this key
            }
            catch (...)
            {
                EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): error reading provenance value (file_version 0.1); this file cannot be read." << EidosTerminate();
            }
             */
        }
        else if (file_version_02.is_string())
        {
            // File version 0.2 was before the addition of nucleotides, so MutationMetadataRec_PRENUC will need to be used
            // File version 0.3 supports nucleotides, using MutationMetadataRec
            // File version 0.4 adds the model_hash key in provenance
            // File version >= 0.5 uses top-level metadata, but we don't write that out when writing text, so we end up here and get the info from provenance
            if (file_version_02 == SLIM_TREES_FILE_VERSION_PRENUC)
                *p_file_version = 2;
            else if (file_version_02 == SLIM_TREES_FILE_VERSION_POSTNUC)
                *p_file_version = 3;
            else if (file_version_02 == SLIM_TREES_FILE_VERSION_HASH)
                *p_file_version = 4;
            else if (file_version_02 == SLIM_TREES_FILE_VERSION)
                *p_file_version = 5;
			else
                EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): this .trees file was generated by an unrecognized version of SLiM or pyslim; this file cannot be read." << EidosTerminate();
            
            try {
                model_type_str = j["parameters"]["model_type"];
                gen_ll = j["slim"]["generation"];
                //rem_count_ll = j["slim"]["remembered_node_count"];	// no longer using this key
            }
            catch (...)
            {
                EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): error reading provenance value (file_version " << file_version_02 << "); this file cannot be read." << EidosTerminate();
            }
        }
        else
        {
            EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): missing or corrupted file version; this file cannot be read." << EidosTerminate();
        }
    }
	
	// check the model type; at the moment we do not require the model type to match what we are running, but we issue a warning on a mismatch
	if ((model_type_str != "WF") && (model_type_str != "nonWF"))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): unrecognized model type; this file cannot be read." << EidosTerminate();
	
	if (((model_type_str == "WF") && (ModelType() != SLiMModelType::kModelTypeWF)) ||
		((model_type_str == "nonWF") && (ModelType() != SLiMModelType::kModelTypeNonWF)))
	{
		if (!gEidosSuppressWarnings)
			SLIM_OUTSTREAM << "#WARNING (SLiMSim::ReadTreeSequenceMetadata): the model type of the .trees file does not match the current model type." << std::endl;
	}
	
	if (model_type_str == "WF")
		*p_model_type = SLiMModelType::kModelTypeWF;
	else if (model_type_str == "nonWF")
		*p_model_type = SLiMModelType::kModelTypeNonWF;
	
	// bounds-check the generation
	if ((gen_ll < 1) || (gen_ll > SLIM_MAX_GENERATION))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ReadTreeSequenceMetadata): generation value out of range; this file cannot be read." << EidosTerminate();
	
	*p_generation = (slim_generation_t)gen_ll;
#endif
}

void SLiMSim::WriteTreeSequence(std::string &p_recording_tree_path, bool p_binary, bool p_simplify, bool p_include_model)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::WriteTreeSequence): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
    // If p_binary, then write out to that path;
    // otherwise, create p_recording_tree_path as a directory,
    // and write out to text files in that directory
	int ret = 0;
	
	// Standardize the path, resolving a leading ~ and maybe other things
	std::string path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(p_recording_tree_path));
	
	// Add a population (i.e., subpopulation) table to the table collection; subpopulation information
	// comes from the time of output.  This needs to happen before simplify/sort.
	WritePopulationTable(&tables_);
	
	// First we simplify, on the original table collection; we considered doing this on the copy,
	// but then the copy takes longer and the simplify's work is lost, and there doesn't seem to
	// be a compelling case for leaving the original tables unsimplified.
    if (p_simplify)
	{
        SimplifyTreeSequence();
    }
	else
	{
        // this is done by SimplifyTreeSequence() but we need to do in any case
        int flags = TSK_NO_CHECK_INTEGRITY;
#if DEBUG
        flags = 0;
#endif
		ret = tsk_table_collection_sort(&tables_, /* edge_start */ NULL, /* flags */ flags);
        if (ret < 0) handle_error("tsk_table_collection_sort", ret);
		
        // Remove redundant sites we added
        ret = tsk_table_collection_deduplicate_sites(&tables_, 0);
        if (ret < 0) handle_error("tsk_table_collection_deduplicate_sites", ret);
    }
	
	// Copy the table collection so that modifications we do for writing don't affect the original tables
	tsk_table_collection_t output_tables;
	ret = tsk_table_collection_copy(&tables_, &output_tables, 0);
	if (ret < 0) handle_error("tsk_table_collection_copy", ret);
	
	// Add in the mutation.parent information; valid tree sequences need parents, but we don't keep them while running
	ret = tsk_table_collection_build_index(&output_tables, 0);
	if (ret < 0) handle_error("tsk_table_collection_build_index", ret);
	ret = tsk_table_collection_compute_mutation_parents(&output_tables, 0);
	if (ret < 0) handle_error("tsk_table_collection_compute_mutation_parents", ret);
	
	// Add information about the current generation to the individual table; 
	// this modifies "remembered" individuals, since information comes from the
	// time of output, not creation
	AddCurrentGenerationToIndividuals(&output_tables);

	// We need the individual table's order, for alive individuals, to match that of
	// SLiM so that when we read back in it doesn't cause a reordering as a side effect
	std::vector<int> individual_map;
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		
		for (Individual *individual : subpop->parent_individuals_)
		{
			tsk_id_t node_id = individual->genome1_->tsk_node_id_;
			tsk_id_t ind_id = output_tables.nodes.individual[node_id];
			
			individual_map.push_back(ind_id);
		}
	}

	// all other individuals in the table will be retained, at the end
	ReorderIndividualTable(&output_tables, individual_map, true);
	
	// Rebase the times in the nodes to be in tskit-land; see _InstantiateSLiMObjectsFromTables() for the inverse operation
	// BCH 4/4/2019: switched to using tree_seq_generation_ to avoid a parent/child timestamp conflict
	// This makes sense; as far as tree-seq recording is concerned, tree_seq_generation_ is the generation counter
	slim_generation_t time_adjustment = tree_seq_generation_;
	
	for (size_t node_index = 0; node_index < output_tables.nodes.num_rows; ++node_index)
		output_tables.nodes.time[node_index] += time_adjustment;

    for (size_t mut_index = 0; mut_index < output_tables.mutations.num_rows; ++mut_index)
        output_tables.mutations.time[mut_index] += time_adjustment;
	
	// Add a row to the Provenance table to record current state; text format does not allow newlines in the entry,
	// so we don't prettyprint the JSON when going to text, as a quick fix that avoids quoting the newlines etc.
    WriteProvenanceTable(&output_tables, /* p_use_newlines */ p_binary, p_include_model);

    // Add top-level metadata and metadata schema
    WriteTreeSequenceMetadata(&output_tables);
	
	// Write out the copied tables
    if (p_binary)
	{
		// derived state data must be in ASCII (or unicode) on disk, according to tskit policy
		DerivedStatesToAscii(&output_tables);
		
		tsk_table_collection_dump(&output_tables, path.c_str(), 0);
		
		// In nucleotide-based models, write out the ancestral sequence, re-opening the kastore to append
		if (nucleotide_based_)
		{
			std::size_t buflen = chromosome_.AncestralSequence()->size();
			char *buffer;	// kastore needs to provide us with a memory location to which to write the data
			kastore_t store;
			
			buffer = (char *)malloc(buflen);
			chromosome_.AncestralSequence()->WriteNucleotidesToBuffer(buffer);
			
			ret = kastore_open(&store, path.c_str(), "a", 0);
			if (ret < 0) handle_error("kastore_open", ret);
				
			kastore_oputs_int8(&store, "reference_sequence/data", (int8_t *)buffer, buflen, 0);
			if (ret < 0) handle_error("kastore_oputs_int8", ret);
			
			ret = kastore_close(&store);
			if (ret < 0) handle_error("kastore_close", ret);
			
			// kastore owns buffer now, so we do not free it
		}
    }
	else
	{
        std::string error_string;
        bool success = Eidos_CreateDirectory(path, &error_string);
		
		if (success)
		{
            // first translate the bytes we've put into mutation derived state into printable ascii
            TreeSequenceDataToAscii(&output_tables);
			
			std::string NodeFileName = path + "/NodeTable.txt";
			std::string EdgeFileName = path + "/EdgeTable.txt";
			std::string SiteFileName = path + "/SiteTable.txt";
			std::string MutationFileName = path + "/MutationTable.txt";
			std::string IndividualFileName = path + "/IndividualTable.txt";
			std::string PopulationFileName = path + "/PopulationTable.txt";
			std::string ProvenanceFileName = path + "/ProvenanceTable.txt";
			
			FILE *MspTxtNodeTable = fopen(NodeFileName.c_str(), "w");
			FILE *MspTxtEdgeTable = fopen(EdgeFileName.c_str(), "w");
			FILE *MspTxtSiteTable = fopen(SiteFileName.c_str(), "w");
			FILE *MspTxtMutationTable = fopen(MutationFileName.c_str(), "w");
			FILE *MspTxtIndividualTable = fopen(IndividualFileName.c_str(), "w");
			FILE *MspTxtPopulationTable = fopen(PopulationFileName.c_str(), "w");
			FILE *MspTxtProvenanceTable = fopen(ProvenanceFileName.c_str(), "w");
			
			tsk_node_table_dump_text(&output_tables.nodes, MspTxtNodeTable);
			tsk_edge_table_dump_text(&output_tables.edges, MspTxtEdgeTable);
			tsk_site_table_dump_text(&output_tables.sites, MspTxtSiteTable);
			tsk_mutation_table_dump_text(&output_tables.mutations, MspTxtMutationTable);
			tsk_individual_table_dump_text(&output_tables.individuals, MspTxtIndividualTable);
			tsk_population_table_dump_text(&output_tables.populations, MspTxtPopulationTable);
			tsk_provenance_table_dump_text(&output_tables.provenances, MspTxtProvenanceTable);
			
			fclose(MspTxtNodeTable);
			fclose(MspTxtEdgeTable);
			fclose(MspTxtSiteTable);
			fclose(MspTxtMutationTable);
			fclose(MspTxtIndividualTable);
			fclose(MspTxtPopulationTable);
			fclose(MspTxtProvenanceTable);
			
			// In nucleotide-based models, write out the ancestral sequence as a separate text file
			if (nucleotide_based_)
			{
				std::string RefSeqFileName = path + "/ReferenceSequence.txt";
				std::ofstream outfile;
				
				outfile.open(RefSeqFileName, std::ofstream::out);
				if (!outfile.is_open())
					EIDOS_TERMINATION << "ERROR (SLiMSim::WriteTreeSequence): treeSeqOutput() could not open "<< RefSeqFileName << "." << EidosTerminate();
				
				outfile << *(chromosome_.AncestralSequence());
				outfile.close();
			}
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (SLiMSim::WriteTreeSequence): unable to create output folder for treeSeqOutput() (" << error_string << ")" << EidosTerminate();
		}
    }
	
	// Done with our tables copy
	tsk_table_collection_free(&output_tables);
}	


void SLiMSim::FreeTreeSequence(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::FreeTreeSequence): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// Free any tree-sequence recording stuff that has been allocated; called when SLiMSim is getting deallocated,
	// and also when we're wiping the slate clean with something like readFromPopulationFile().
	tsk_table_collection_free(&tables_);
	
	remembered_genomes_.clear();
}

void SLiMSim::RecordAllDerivedStatesFromSLiM(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RecordAllDerivedStatesFromSLiM): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// This is called when new tree sequence tables need to be built to correspond to the current state of SLiM, such as
	// after handling a readFromPopulationFile() call.  It is guaranteed by the caller of this method that any old tree
	// sequence recording stuff has been freed with a call to FreeTreeSequence(), and then a new recording session has
	// been initiated with AllocateTreeSequenceTables(); it might be good for this method to do a sanity check that all
	// of the recording tables are indeed allocated but empty, I guess.  This method then records every extant individual
	// by making calls to SetCurrentNewIndividual(), and RecordNewGenome(). Note
	// that modifyChild() callbacks do not happen in this scenario, so new individuals will not get retracted.  Note
	// also that new mutations will not be added one at a time, when they are stacked; instead, each block of stacked
	// mutations in a genome will be added with a single derived state call here.
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		
		for (Individual *individual : subpop->parent_individuals_)
		{
			Genome *genome1 = individual->genome1_;
			Genome *genome2 = individual->genome2_;
			
			// This is done for us, at present, as a side effect of the readPopulationFile() code...
			//SetCurrentNewIndividual(individual);
			//RecordNewGenome(nullptr, genome1, nullptr, nullptr);
			//RecordNewGenome(nullptr, genome2, nullptr, nullptr);
			
			if (recording_mutations_)
			{
				if (!genome1->IsNull())
					genome1->record_derived_states(this);
				if (!genome2->IsNull())
					genome2->record_derived_states(this);
			}
		}
	}
}

void SLiMSim::MetadataForMutation(Mutation *p_mutation, MutationMetadataRec *p_metadata)
{
	static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec has changed size; this code probably needs to be updated");
	
	if (!p_mutation || !p_metadata)
		EIDOS_TERMINATION << "ERROR (SLiMSim::MetadataForMutation): (internal error) bad parameters to MetadataForMutation()." << EidosTerminate();
	
	p_metadata->mutation_type_id_ = p_mutation->mutation_type_ptr_->mutation_type_id_;
	p_metadata->selection_coeff_ = p_mutation->selection_coeff_;
	p_metadata->subpop_index_ = p_mutation->subpop_index_;
	p_metadata->origin_generation_ = p_mutation->origin_generation_;
	p_metadata->nucleotide_ = p_mutation->nucleotide_;
}

void SLiMSim::MetadataForSubstitution(Substitution *p_substitution, MutationMetadataRec *p_metadata)
{
	static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec has changed size; this code probably needs to be updated");
	
	if (!p_substitution || !p_metadata)
		EIDOS_TERMINATION << "ERROR (SLiMSim::MetadataForSubstitution): (internal error) bad parameters to MetadataForSubstitution()." << EidosTerminate();
	
	p_metadata->mutation_type_id_ = p_substitution->mutation_type_ptr_->mutation_type_id_;
	p_metadata->selection_coeff_ = p_substitution->selection_coeff_;
	p_metadata->subpop_index_ = p_substitution->subpop_index_;
	p_metadata->origin_generation_ = p_substitution->origin_generation_;
	p_metadata->nucleotide_ = p_substitution->nucleotide_;
}

void SLiMSim::MetadataForGenome(Genome *p_genome, GenomeMetadataRec *p_metadata)
{
	static_assert(sizeof(GenomeMetadataRec) == 10, "GenomeMetadataRec has changed size; this code probably needs to be updated");
	
	if (!p_genome || !p_metadata)
		EIDOS_TERMINATION << "ERROR (SLiMSim::MetadataForGenome): (internal error) bad parameters to MetadataForGenome()." << EidosTerminate();
	
	p_metadata->genome_id_ = p_genome->genome_id_;
	p_metadata->is_null_ = p_genome->IsNull();
	p_metadata->type_ = p_genome->genome_type_;
}

void SLiMSim::MetadataForIndividual(Individual *p_individual, IndividualMetadataRec *p_metadata)
{
	static_assert(sizeof(IndividualMetadataRec) == 24, "IndividualMetadataRec has changed size; this code probably needs to be updated");
	
	if (!p_individual || !p_metadata)
		EIDOS_TERMINATION << "ERROR (SLiMSim::MetadataForIndividual): (internal error) bad parameters to MetadataForIndividual()." << EidosTerminate();
	
	p_metadata->pedigree_id_ = p_individual->PedigreeID();
	p_metadata->age_ = p_individual->age_;
	p_metadata->subpopulation_id_ = p_individual->subpopulation_.subpopulation_id_;
	p_metadata->sex_ = p_individual->sex_;
	
	p_metadata->flags_ = 0;
	if (p_individual->migrant_)
		p_metadata->flags_ |= SLIM_INDIVIDUAL_METADATA_MIGRATED;
}

void SLiMSim::DumpMutationTable(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::DumpMutationTable): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// Dump for debugging; should not be called in production code!
	
	tsk_mutation_table_t &mutations = tables_.mutations;
	
	for (tsk_size_t mutindex = 0; mutindex < mutations.num_rows; ++mutindex)
	{
		tsk_id_t node_id = mutations.node[mutindex];
		tsk_id_t site_id = mutations.site[mutindex];
		tsk_id_t parent_id = mutations.parent[mutindex];
		char *derived_state = mutations.derived_state + mutations.derived_state_offset[mutindex];
		tsk_size_t derived_state_length = mutations.derived_state_offset[mutindex + 1] - mutations.derived_state_offset[mutindex];
		//char *metadata_state = mutations.metadata + mutations.metadata_offset[mutindex];
		tsk_size_t metadata_length = mutations.metadata_offset[mutindex + 1] - mutations.metadata_offset[mutindex];
		
		/* DEBUG : output a mutation only if its derived state contains a certain mutation ID
		{
			bool contains_id = false;
			
			for (size_t mutid_index = 0; mutid_index < derived_state_length / sizeof(slim_mutationid_t); ++mutid_index)
				if (((slim_mutationid_t *)derived_state)[mutid_index] == 72)
					contains_id = true;
			
			if (!contains_id)
				continue;
		}
		// */
		
		std::cout << "Mutation index " << mutindex << " has node_id " << node_id << ", site_id " << site_id << ", position " << tables_.sites.position[site_id] << ", parent id " << parent_id << ", derived state length " << derived_state_length << ", metadata length " << metadata_length << std::endl;
		
		std::cout << "   derived state: ";
		for (size_t mutid_index = 0; mutid_index < derived_state_length / sizeof(slim_mutationid_t); ++mutid_index)
			std::cout << ((slim_mutationid_t *)derived_state)[mutid_index] << " ";
		std::cout << std::endl;
	}
}

void SLiMSim::CheckTreeSeqIntegrity(void)
{
	// Here we call tskit to check the integrity of the tree-sequence tables themselves – not against
	// SLiM's parallel data structures (done in CrosscheckTreeSeqIntegrity()), just on their own.
	// At present this is commented out because tsk_table_collection_check_integrity() does checks
	// that we always fail.  In particular, we have no population table in our table collection while
	// we are running, so we always get a TSK_ERR_POPULATION_OUT_OF_BOUNDS error.  When tskit is
	// made more flexible, in terms of being able to turn off checks that we do not want, we will
	// enable this code with the appropriate flags.  See https://github.com/tskit-dev/tskit/issues/593
	
	/*int ret = tsk_table_collection_check_integrity(&tables_, 0);
	if (ret < 0) handle_error("tsk_table_collection_check_integrity()", ret);*/
}

void SLiMSim::CrosscheckTreeSeqIntegrity(void)
{
#if DEBUG
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) tree sequence recording method called with recording off." << EidosTerminate();
#endif
	
	// first crosscheck the substitutions multimap against SLiM's substitutions vector
	{
		std::vector<Substitution *> vector_subs = population_.substitutions_;
		std::vector<Substitution *> multimap_subs;
		
		for (auto entry : population_.treeseq_substitutions_map_)
			multimap_subs.push_back(entry.second);
		
		std::sort(vector_subs.begin(), vector_subs.end());
		std::sort(multimap_subs.begin(), multimap_subs.end());
		
		if (vector_subs != multimap_subs)
			EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) mismatch between SLiM substitutions and the treeseq substitution multimap." << EidosTerminate();
	}
	
	// get all genomes from all subpopulations; we will cross-check them all simultaneously
	static std::vector<Genome *> genomes;
	genomes.clear();
	
	for (auto pop_iter : population_.subpops_)
	{
		Subpopulation *subpop = pop_iter.second;
		
		for (Genome *genome : subpop->parent_genomes_)
			genomes.push_back(genome);
	}
	
	// if we have no genomes to check, we return; we could check that the tree sequences are also empty, but we don't
	size_t genome_count = genomes.size();
	
	if (genome_count == 0)
		return;
	
	// check for correspondence between SLiM's genomes and the tree_seq's nodes, including their metadata
	// FIXME unimplemented
	
	// if we're recording mutations, we can check all of them
	if (recording_mutations_)
	{
		// prepare to walk all the genomes by making GenomeWalker objects for them all
		static std::vector<GenomeWalker> genome_walkers;
		genome_walkers.clear();
		genome_walkers.reserve(genome_count);
		
		for (Genome *genome : genomes)
			genome_walkers.emplace_back(genome);
		
		// make a copy of the full table collection, so that we can sort/clean/simplify without modifying anything
		int ret;
		tsk_table_collection_t *tables_copy;
		
		tables_copy = (tsk_table_collection_t *)malloc(sizeof(tsk_table_collection_t));
		ret = tsk_table_collection_copy(&tables_, tables_copy, 0);
		if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_table_collection_copy()", ret);
		
		// our tables copy needs to have a population table now, since this is required to build a tree sequence
		WritePopulationTable(tables_copy);
		
		// simplify before making our tree_sequence object; the sort and deduplicate and compute parents are required for the crosscheck, whereas the simplify
		// could perhaps be removed, which would cause the tsk_vargen_t to visit a bunch of stuff unrelated to the current individuals.
		// this code is adapted from SLiMSim::SimplifyTreeSequence(), but we don't need to update the TSK map table or the table position,
		// and we simplify down to just the extant individuals since we can't cross-check older individuals anyway...
		if (tables_copy->nodes.num_rows != 0)
		{
			std::vector<tsk_id_t> samples;
			
			for (auto iter : population_.subpops_)
				for (Genome *genome : iter.second->parent_genomes_)
					samples.push_back(genome->tsk_node_id_);
			
            int flags = TSK_NO_CHECK_INTEGRITY;
#if DEBUG
            flags = 0;
#endif
			ret = tsk_table_collection_sort(tables_copy, /* edge_start */ NULL, /* flags */ flags);
			if (ret < 0) handle_error("tsk_table_collection_sort", ret);
			
			ret = tsk_table_collection_deduplicate_sites(tables_copy, 0);
			if (ret < 0) handle_error("tsk_table_collection_deduplicate_sites", ret);
			
			ret = tsk_table_collection_simplify(tables_copy, samples.data(), (tsk_size_t)samples.size(), TSK_FILTER_SITES | TSK_FILTER_INDIVIDUALS | TSK_KEEP_INPUT_ROOTS, NULL);
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
		ret = tsk_treeseq_init(ts, tables_copy, TSK_BUILD_INDEXES);
		if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_treeseq_init()", ret);
		
		// allocate and set up the vargen object we'll use to walk through variants
		tsk_vargen_t *vg;
		
		vg = (tsk_vargen_t *)malloc(sizeof(tsk_vargen_t));
		ret = tsk_vargen_init(vg, ts, ts->samples, ts->num_samples, NULL, TSK_16_BIT_GENOTYPES | TSK_ISOLATED_NOT_MISSING);
		if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_vargen_alloc()", ret);
		
		// crosscheck by looping through variants
		do
		{
			tsk_variant_t *variant;
			
			ret = tsk_vargen_next(vg, &variant);
			if (ret < 0) handle_error("CrosscheckTreeSeqIntegrity tsk_vargen_next()", ret);
			
			if (ret == 1)
			{
				// We have a new variant; check it against SLiM.  A variant represents a site at which a tracked mutation exists.
				// The tsk_variant_t will tell us all the allelic states involved at that site, what the alleles are, and which genomes
				// in the sample are using them.  We will then check that all the genomes that the variant claims to involve have
				// the allele the variant attributes to them, and that no genomes contain any alleles at the position that are not
				// described by the variant.  The variants are returned in sorted order by position, so we can keep pointers into
				// every extant genome's mutruns, advance those pointers a step at a time, and check that everything matches at every
				// step.  Keep in mind that some mutations may have been fixed (substituted) or lost.
				slim_position_t variant_pos_int = (slim_position_t)variant->site->position;		// should be no loss of precision, fingers crossed
				
				// Get all the substitutions involved at this site, which should be present in every sample
				auto substitution_range_iter = population_.treeseq_substitutions_map_.equal_range(variant_pos_int);
				static std::vector<slim_mutationid_t> fixed_mutids;
				
				fixed_mutids.clear();
				for (auto substitution_iter = substitution_range_iter.first; substitution_iter != substitution_range_iter.second; ++substitution_iter)
					fixed_mutids.push_back(substitution_iter->second->mutation_id_);
				
				// Check all the genomes against the tsk_vargen_t's belief about this site
				for (size_t genome_index = 0; genome_index < genome_count; genome_index++)
				{
					GenomeWalker &genome_walker = genome_walkers[genome_index];
					int16_t genome_variant = variant->genotypes.i16[genome_index];
					tsk_size_t genome_allele_length = variant->allele_lengths[genome_variant];
					
					if (genome_allele_length % sizeof(slim_mutationid_t) != 0)
						EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) variant allele had length that was not a multiple of sizeof(slim_mutationid_t)." << EidosTerminate();
					genome_allele_length /= sizeof(slim_mutationid_t);
					
					//std::cout << "variant for genome: " << (int)genome_variant << " (allele length == " << genome_allele_length << ")" << std::endl;
					
					// BCH 4/29/2018: null genomes shouldn't ever contain any mutations, including fixed mutations
					if (genome_walker.WalkerGenome()->IsNull())
					{
						if (genome_allele_length == 0)
							continue;
						
						EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) null genome has non-zero treeseq allele length " << genome_allele_length << "." << EidosTerminate();
					}
					
					// (1) if the variant's allele is zero-length, we do nothing (if it incorrectly claims that a genome contains no
					// mutation, we'll catch that later)  (2) if the variant's allele is the length of one mutation id, we can simply
					// check that the next mutation in the genome in question exists and has the right mutation id; (3) if the variant's
					// allele has more than one mutation id, we have to check them all against all the mutations at the given position
					// in the genome in question, which is a bit annoying since the lists may not be in the same order.  Note that if
					// the variant is for a mutation that has fixed, it will not be present in the genome; we check for a substitution
					// with the right ID.
					slim_mutationid_t *genome_allele = (slim_mutationid_t *)variant->alleles[genome_variant];
					
					if (genome_allele_length == 0)
					{
						// If there are no fixed mutations at this site, we can continue; genomes that have a mutation at this site will
						// raise later when they realize they have been skipped over, so we don't have to check for that now...
						if (fixed_mutids.size() == 0)
							continue;
						
						EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) the treeseq has 0 mutations at position " << variant_pos_int << ", SLiM has " << fixed_mutids.size() << " fixed mutation(s)." << EidosTerminate();
					}
					else if (genome_allele_length == 1)
					{
						// The tree has just one mutation at this site; this is the common case, so we try to handle it quickly
						slim_mutationid_t allele_mutid = *genome_allele;
						Mutation *current_mut = genome_walker.CurrentMutation();
						
						if (current_mut)
						{
							slim_position_t current_mut_pos = current_mut->position_;
							
							if (current_mut_pos < variant_pos_int)
								EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) genome mutation was not represented in trees (single case)." << EidosTerminate();
							if (current_mut->position_ > variant_pos_int)
								current_mut = nullptr;	// not a candidate for this position, we'll see it again later
						}
						
						if (!current_mut && (fixed_mutids.size() == 1))
						{
							// We have one fixed mutation and no segregating mutation, versus one mutation in the tree; crosscheck
							if (allele_mutid != fixed_mutids[0])
								EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) the treeseq has mutid " << allele_mutid << " at position " << variant_pos_int << ", SLiM has a fixed mutation of id " << fixed_mutids[0] << EidosTerminate();
							
							continue;	// the match was against a fixed mutation, so don't go to the next mutation
						}
						else if (current_mut && (fixed_mutids.size() == 0))
						{
							// We have one segregating mutation and no fixed mutation, versus one mutation in the tree; crosscheck
							if (allele_mutid != current_mut->mutation_id_)
								EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) the treeseq has mutid " << allele_mutid << " at position " << variant_pos_int << ", SLiM has a segregating mutation of id " << current_mut->mutation_id_ << EidosTerminate();
						}
						else
						{
							// We have a count mismatch; there is one mutation in the tree, but we have !=1 in SLiM including substitutions
							EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) genome/allele size mismatch at position " << variant_pos_int << ": the treeseq has 1 mutation of mutid " << allele_mutid << ", SLiM has " << (current_mut ? 1 : 0) << " segregating and " << fixed_mutids.size() << " fixed mutation(s)." << EidosTerminate();
						}
						
						genome_walker.NextMutation();
						
						// Check the next mutation to see if it's at this position as well, and is missing from the tree;
						// this would get caught downstream, but for debugging it is clearer to catch it here
						Mutation *next_mut = genome_walker.CurrentMutation();
						
						if (next_mut && next_mut->position_ == variant_pos_int)
							EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) the treeseq is missing a stacked mutation with mutid " << next_mut->mutation_id_ << " at position " << variant_pos_int << "." << EidosTerminate();
					}
					else // (genome_allele_length > 1)
					{
						static std::vector<slim_mutationid_t> allele_mutids;
						static std::vector<slim_mutationid_t> genome_mutids;
						allele_mutids.clear();
						genome_mutids.clear();
						
						// tabulate all tree mutations
						for (tsk_size_t mutid_index = 0; mutid_index < genome_allele_length; ++mutid_index)
							allele_mutids.push_back(genome_allele[mutid_index]);
						
						// tabulate segregating SLiM mutations
						while (true)
						{
							Mutation *current_mut = genome_walker.CurrentMutation();
							
							if (current_mut)
							{
								slim_position_t current_mut_pos = current_mut->position_;
								
								if (current_mut_pos < variant_pos_int)
									EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) genome mutation was not represented in trees (bulk case)." << EidosTerminate();
								else if (current_mut_pos == variant_pos_int)
								{
									genome_mutids.push_back(current_mut->mutation_id_);
									genome_walker.NextMutation();
								}
								else break;
							}
							else break;
						}
						
						// tabulate fixed SLiM mutations
						genome_mutids.insert(genome_mutids.end(), fixed_mutids.begin(), fixed_mutids.end());
						
						// crosscheck, sorting so there is no order-dependency
						if (allele_mutids.size() != genome_mutids.size())
							EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) genome/allele size mismatch at position " << variant_pos_int << ": the treeseq has " << allele_mutids.size() << " mutations, SLiM has " << (genome_mutids.size() - fixed_mutids.size()) << " segregating and " << fixed_mutids.size() << " fixed mutation(s)." << EidosTerminate();
						
						std::sort(allele_mutids.begin(), allele_mutids.end());
						std::sort(genome_mutids.begin(), genome_mutids.end());
						
						for (tsk_size_t mutid_index = 0; mutid_index < genome_allele_length; ++mutid_index)
							if (allele_mutids[mutid_index] != genome_mutids[mutid_index])
								EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) genome/allele bulk mutid mismatch." << EidosTerminate();
					}
				}
			}
		}
		while (ret != 0);
		
		// we have finished all variants, so all the genomes we're tracking should be at their ends; any left-over mutations
		// should have been in the trees but weren't, so this is an error
		for (size_t genome_index = 0; genome_index < genome_count; genome_index++)
			if (!genome_walkers[genome_index].Finished())
				EIDOS_TERMINATION << "ERROR (SLiMSim::CrosscheckTreeSeqIntegrity): (internal error) mutations left in genome beyond those in tree." << EidosTerminate();
		
		// free
		ret = tsk_vargen_free(vg);
		if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_vargen_free()", ret);
		free(vg);
		
		ret = tsk_treeseq_free(ts);
		if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_treeseq_free()", ret);
		free(ts);
		
		ret = tsk_table_collection_free(tables_copy);
		if (ret != 0) handle_error("CrosscheckTreeSeqIntegrity tsk_table_collection_free()", ret);
		free(tables_copy);
	}
}

void SLiMSim::TSXC_Enable(void)
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
	treeseq_crosschecks_interval_ = 50;		// check every 50th generation, otherwise it is just too slow
	
	SLIM_ERRSTREAM << "// ********** Turning on tree-sequence recording with crosschecks (-TSXC)." << std::endl << std::endl;
}

typedef struct ts_subpop_info {
	slim_popsize_t countMH_ = 0, countF_ = 0;
	std::vector<IndividualSex> sex_;
	std::vector<tsk_id_t> nodes_;
	std::vector<slim_pedigreeid_t> pedigreeID_;
	std::vector<slim_age_t> age_;
	std::vector<double> spatial_x_;
	std::vector<double> spatial_y_;
	std::vector<double> spatial_z_;
	std::vector<uint32_t> flags_;
} ts_subpop_info;

void SLiMSim::__TabulateSubpopulationsFromTreeSequence(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, tsk_treeseq_t *p_ts, SLiMModelType p_file_model_type)
{
	size_t individual_count = p_ts->tables->individuals.num_rows;
	
	if (individual_count == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): loaded tree sequence files must contain a non-empty individuals table." << EidosTerminate();
	
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
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): unexpected individual metadata length; this file cannot be read." << EidosTerminate();
		
		IndividualMetadataRec *metadata = (IndividualMetadataRec *)(individual.metadata);
		
		// bounds-check the subpop id
		slim_objectid_t subpop_id = metadata->subpopulation_id_;
		if ((subpop_id < 0) || (subpop_id > SLIM_MAX_ID_VALUE))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): individuals loaded into a WF model must have subpop indices >= 0 and <= " << SLIM_MAX_ID_VALUE << "." << EidosTerminate();
		
		// find the ts_subpop_info rec for this individual's subpop
		auto subpop_info_insert = p_subpopInfoMap.insert(std::pair<slim_objectid_t, ts_subpop_info>(subpop_id, ts_subpop_info()));
		ts_subpop_info &subpop_info = (subpop_info_insert.first)->second;
		
		// check and tabulate sex within each subpop
		IndividualSex sex = metadata->sex_;
		
		switch (sex)
		{
			case IndividualSex::kHermaphrodite:
				if (sex_enabled_)
					EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): hermaphrodites may not be loaded into a model in which sex is enabled." << EidosTerminate();
				subpop_info.countMH_++;
				break;
			case IndividualSex::kFemale:
				if (!sex_enabled_)
					EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): females may not be loaded into a model in which sex is not enabled." << EidosTerminate();
				subpop_info.countF_++;
				break;
			case IndividualSex::kMale:
				if (!sex_enabled_)
					EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): males may not be loaded into a model in which sex is not enabled." << EidosTerminate();
				subpop_info.countMH_++;
				break;
			default:
				EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): unrecognized individual sex value " << sex << "." << EidosTerminate();
		}
		
		subpop_info.sex_.push_back(sex);
		
		// check that the individual has exactly two nodes; we are always diploid
		if (individual.nodes_length != 2)
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): unexpected node count; this file cannot be read." << EidosTerminate();
		
		subpop_info.nodes_.push_back(individual.nodes[0]);
		subpop_info.nodes_.push_back(individual.nodes[1]);
		
		// save off the pedigree ID, which we will use again
		subpop_info.pedigreeID_.push_back(metadata->pedigree_id_);
		
		// save off the flags for later use
		subpop_info.flags_.push_back(metadata->flags_);
		
		// bounds-check ages; we cross-translate ages of 0 and -1 if the model type has been switched
		slim_age_t age = metadata->age_;
		
		if ((p_file_model_type == SLiMModelType::kModelTypeNonWF) && (ModelType() == SLiMModelType::kModelTypeWF) && (age == 0))
			age = -1;
		if ((p_file_model_type == SLiMModelType::kModelTypeWF) && (ModelType() == SLiMModelType::kModelTypeNonWF) && (age == -1))
			age = 0;
		
		if (((age < 0) || (age > SLIM_MAX_ID_VALUE)) && (model_type_ == SLiMModelType::kModelTypeNonWF))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): individuals loaded into a nonWF model must have age values >= 0 and <= " << SLIM_MAX_ID_VALUE << "." << EidosTerminate();
		if ((age != -1) && (model_type_ == SLiMModelType::kModelTypeWF))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): individuals loaded into a WF model must have age values == -1." << EidosTerminate();
		
		subpop_info.age_.push_back(age);
		
		// no bounds-checks for spatial position
		if (individual.location_length != 3)
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): unexpected individual location length; this file cannot be read." << EidosTerminate();
		
		subpop_info.spatial_x_.push_back(individual.location[0]);
		subpop_info.spatial_y_.push_back(individual.location[1]);
		subpop_info.spatial_z_.push_back(individual.location[2]);
		
		// check the referenced nodes; right now this is not essential for re-creating the saved state, but is just a crosscheck
		// here we crosscheck the node information against expected values from other places in the tables or the model
		tsk_node_table_t &node_table = tables_.nodes;
		tsk_id_t node0 = individual.nodes[0];
		tsk_id_t node1 = individual.nodes[1];
		
		if (((node_table.flags[node0] & TSK_NODE_IS_SAMPLE) == 0) || ((node_table.flags[node1] & TSK_NODE_IS_SAMPLE) == 0))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): nodes for individual are not in-sample; this file cannot be read." << EidosTerminate();
		if ((node_table.individual[node0] != individual.id) || (node_table.individual[node1] != individual.id))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): individual-node inconsistency; this file cannot be read." << EidosTerminate();
		
		size_t node0_metadata_length = node_table.metadata_offset[node0 + 1] - node_table.metadata_offset[node0];
		size_t node1_metadata_length = node_table.metadata_offset[node1 + 1] - node_table.metadata_offset[node1];
		
		if ((node0_metadata_length != sizeof(GenomeMetadataRec)) || (node1_metadata_length != sizeof(GenomeMetadataRec)))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): unexpected node metadata length; this file cannot be read." << EidosTerminate();
		
		GenomeMetadataRec *node0_metadata = (GenomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node0]);
		GenomeMetadataRec *node1_metadata = (GenomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node1]);
		
		if ((node0_metadata->genome_id_ != metadata->pedigree_id_ * 2) || (node1_metadata->genome_id_ != metadata->pedigree_id_ * 2 + 1))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): genome id mismatch; this file cannot be read." << EidosTerminate();
		
		bool expected_is_null_0 = false, expected_is_null_1 = false;
		GenomeType expected_genome_type_0 = GenomeType::kAutosome, expected_genome_type_1 = GenomeType::kAutosome;
		
		if (sex_enabled_)
		{
			if (modeled_chromosome_type_ == GenomeType::kXChromosome)
			{
				expected_is_null_0 = (sex == IndividualSex::kMale) ? false : false;
				expected_is_null_1 = (sex == IndividualSex::kMale) ? true : false;
				expected_genome_type_0 = (sex == IndividualSex::kMale) ? GenomeType::kXChromosome : GenomeType::kXChromosome;
				expected_genome_type_1 = (sex == IndividualSex::kMale) ? GenomeType::kYChromosome : GenomeType::kXChromosome;
			}
			else if (modeled_chromosome_type_ == GenomeType::kYChromosome)
			{
				expected_is_null_0 = (sex == IndividualSex::kMale) ? true : true;
				expected_is_null_1 = (sex == IndividualSex::kMale) ? false : true;
				expected_genome_type_0 = (sex == IndividualSex::kMale) ? GenomeType::kXChromosome : GenomeType::kXChromosome;
				expected_genome_type_1 = (sex == IndividualSex::kMale) ? GenomeType::kYChromosome : GenomeType::kXChromosome;
			}
		}
		
		if ((node0_metadata->is_null_ != expected_is_null_0) || (node1_metadata->is_null_ != expected_is_null_1))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): node is_null unexpected; this file cannot be read." << EidosTerminate();
		if ((node0_metadata->type_ != expected_genome_type_0) || (node1_metadata->type_ != expected_genome_type_1))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateSubpopulationsFromTreeSequence): node type unexpected; this file cannot be read." << EidosTerminate();
	}
}

void SLiMSim::__CreateSubpopulationsFromTabulation(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Genome *> &p_nodeToGenomeMap)
{
	gSLiM_next_pedigree_id = 0;
	
	for (auto subpop_info_iter : p_subpopInfoMap)
	{
		slim_objectid_t subpop_id = subpop_info_iter.first;
		ts_subpop_info &subpop_info = subpop_info_iter.second;
		slim_popsize_t subpop_size = sex_enabled_ ? (subpop_info.countMH_ + subpop_info.countF_) : subpop_info.countMH_;
		double sex_ratio = sex_enabled_ ? (subpop_info.countMH_ / (double)subpop_size) : 0.5;
		
		// Create the new subpopulation – without recording it in the tree-seq tables
		recording_tree_ = false;
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, sex_ratio);
		recording_tree_ = true;
		
		// define a new Eidos variable to refer to the new subpopulation
		EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
		
		if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): new subpopulation symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
		
		simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
		
		// connect up the individuals and genomes in the new subpop with the tree-seq table entries
		int sex_count = sex_enabled_ ? 2 : 1;
		
		for (int sex_index = 0; sex_index < sex_count; ++sex_index)
		{
			IndividualSex generating_sex = (sex_enabled_ ? (sex_index == 0 ? IndividualSex::kFemale : IndividualSex::kMale) : IndividualSex::kHermaphrodite);
			slim_popsize_t tabulation_size = (sex_enabled_ ? (sex_index == 0 ? subpop_info.countF_ : subpop_info.countMH_) : subpop_info.countMH_);
			slim_popsize_t start_index = (generating_sex == IndividualSex::kMale) ? new_subpop->parent_first_male_index_ : 0;
			slim_popsize_t last_index = (generating_sex == IndividualSex::kFemale) ? (new_subpop->parent_first_male_index_ - 1) : (new_subpop->parent_subpop_size_ - 1);
			slim_popsize_t sex_size = last_index - start_index + 1;
			
			if (tabulation_size != sex_size)
				EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): (internal error) mismatch between tabulation size and subpop size." << EidosTerminate();
			
			slim_popsize_t tabulation_index = -1;
			
			for (slim_popsize_t ind_index = start_index; ind_index <= last_index; ++ind_index)
			{
				// scan for the next tabulation entry of the expected sex
				do {
					tabulation_index++;
					
					if (tabulation_index >= subpop_size)
						EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): (internal error) ran out of tabulated individuals." << EidosTerminate();
				} while (subpop_info.sex_[tabulation_index] != generating_sex);
				
				Individual *individual = new_subpop->parent_individuals_[ind_index];
				
				if (individual->sex_ != generating_sex)
					EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): (internal error) unexpected individual sex." << EidosTerminate();
				
				tsk_id_t node_id_0 = subpop_info.nodes_[tabulation_index * 2];
				tsk_id_t node_id_1 = subpop_info.nodes_[tabulation_index * 2 + 1];
				
				individual->genome1_->tsk_node_id_ = node_id_0;
				individual->genome2_->tsk_node_id_ = node_id_1;
				
				p_nodeToGenomeMap.insert(std::pair<tsk_id_t, Genome *>(node_id_0, individual->genome1_));
				p_nodeToGenomeMap.insert(std::pair<tsk_id_t, Genome *>(node_id_1, individual->genome2_));
				
				slim_pedigreeid_t pedigree_id = subpop_info.pedigreeID_[tabulation_index];
				individual->SetPedigreeID(pedigree_id);
				gSLiM_next_pedigree_id = std::max(gSLiM_next_pedigree_id, pedigree_id + 1);
				
				uint32_t flags = subpop_info.flags_[tabulation_index];
				if (flags & SLIM_INDIVIDUAL_METADATA_MIGRATED)
					individual->migrant_ = true;
				
				individual->genome1_->genome_id_ = pedigree_id * 2;
				individual->genome2_->genome_id_ = pedigree_id * 2 + 1;
				
				individual->age_ = subpop_info.age_[tabulation_index];
				individual->spatial_x_ = subpop_info.spatial_x_[tabulation_index];
				individual->spatial_y_ = subpop_info.spatial_y_[tabulation_index];
				individual->spatial_z_ = subpop_info.spatial_z_[tabulation_index];
				
				// check the referenced nodes; right now this is not essential for re-creating the saved state, but is just a crosscheck
				// here we crosscheck the node information against the realized values in the genomes of the individual
				tsk_node_table_t &node_table = tables_.nodes;
				size_t node0_metadata_length = node_table.metadata_offset[node_id_0 + 1] - node_table.metadata_offset[node_id_0];
				size_t node1_metadata_length = node_table.metadata_offset[node_id_1 + 1] - node_table.metadata_offset[node_id_1];
				
				if ((node0_metadata_length != sizeof(GenomeMetadataRec)) || (node1_metadata_length != sizeof(GenomeMetadataRec)))
					EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): unexpected node metadata length; this file cannot be read." << EidosTerminate();
				
				GenomeMetadataRec *node0_metadata = (GenomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_0]);
				GenomeMetadataRec *node1_metadata = (GenomeMetadataRec *)(node_table.metadata + node_table.metadata_offset[node_id_1]);
				Genome *genome0 = individual->genome1_, *genome1 = individual->genome2_;
				
				if ((node0_metadata->genome_id_ != genome0->genome_id_) || (node1_metadata->genome_id_ != genome1->genome_id_))
					EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): node-genome id mismatch; this file cannot be read." << EidosTerminate();
				if ((node0_metadata->is_null_ != genome0->IsNull()) || (node1_metadata->is_null_ != genome1->IsNull()))
					EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): node-genome null mismatch; this file cannot be read." << EidosTerminate();
				if ((node0_metadata->type_ != genome0->Type()) || (node1_metadata->type_ != genome1->Type()))
					EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateSubpopulationsFromTabulation): node-genome type mismatch; this file cannot be read." << EidosTerminate();
			}
		}
	}
}

void SLiMSim::__ConfigureSubpopulationsFromTables(EidosInterpreter *p_interpreter)
{
	tsk_population_table_t &pop_table = tables_.populations;
	tsk_size_t pop_count = pop_table.num_rows;
	
	// do a quick sanity check that the number of non-empty rows equals the expected subpopulation count, in WF models
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		tsk_size_t nonempty_count = 0;
		
		for (tsk_size_t pop_index = 0; pop_index < pop_count; pop_index++)
		{
			size_t metadata_length = pop_table.metadata_offset[pop_index + 1] - pop_table.metadata_offset[pop_index];
			
			if (metadata_length > 0)
				++nonempty_count;
		}
		
		if ((size_t)nonempty_count != population_.subpops_.size())
			EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): subpopulation count mismatch; this file cannot be read." << EidosTerminate();
	}
	
	for (tsk_size_t pop_index = 0; pop_index < pop_count; pop_index++)
	{
		size_t metadata_length = pop_table.metadata_offset[pop_index + 1] - pop_table.metadata_offset[pop_index];
		
		if (metadata_length == 0)
		{
			// empty rows in the population table correspond to unused subpop IDs; ignore them
			continue;
		}
		
		char *metadata_char = pop_table.metadata + pop_table.metadata_offset[pop_index];
		
		if (metadata_length < sizeof(SubpopulationMetadataRec))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): malformed population metadata; this file cannot be read." << EidosTerminate();
		
		SubpopulationMetadataRec *metadata = (SubpopulationMetadataRec *)metadata_char;
		SubpopulationMigrationMetadataRec *migration_recs = (SubpopulationMigrationMetadataRec *)(metadata + 1);
		slim_objectid_t subpop_id = metadata->subpopulation_id_;
		Subpopulation *subpop = SubpopulationWithID(subpop_id);
		
		if (!subpop)
		{
			// in a WF model it is an error to have an referenced subpop that is empty
			if (model_type_ == SLiMModelType::kModelTypeWF)
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): referenced subpopulation is empty; this file cannot be read." << EidosTerminate();
			
			// If a nonWF model an empty subpop is legal, so create it without recording
			recording_tree_ = false;
			subpop = population_.AddSubpopulation(subpop_id, 0, 0.5);
			recording_tree_ = true;
			
			// define a new Eidos variable to refer to the new subpopulation
			EidosSymbolTableEntry &symbol_entry = subpop->SymbolTableEntry();
			
			if (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_entry.first))
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): new subpopulation symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here; this file cannot be read." << EidosTerminate();
			
			simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
		}
		
		if (model_type_ == SLiMModelType::kModelTypeWF)
		{
			subpop->selfing_fraction_ = metadata->selfing_fraction_;
			subpop->female_clone_fraction_ = metadata->female_clone_fraction_;
			subpop->male_clone_fraction_ = metadata->male_clone_fraction_;
			subpop->child_sex_ratio_ = metadata->sex_ratio_;
			
			if (!sex_enabled_ && (subpop->female_clone_fraction_ != subpop->male_clone_fraction_))
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): cloning rate mismatch for non-sexual model; this file cannot be read." << EidosTerminate();
			if (sex_enabled_ && (subpop->selfing_fraction_ != 0.0))
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): selfing rate may be non-zero only for hermaphoditic models; this file cannot be read." << EidosTerminate();
			if ((subpop->female_clone_fraction_ < 0.0) || (subpop->female_clone_fraction_ > 1.0) ||
				(subpop->male_clone_fraction_ < 0.0) || (subpop->male_clone_fraction_ > 1.0) ||
				(subpop->selfing_fraction_ < 0.0) || (subpop->selfing_fraction_ > 1.0))
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): out-of-range value for cloning rate or selfing rate; this file cannot be read." << EidosTerminate();
			if (sex_enabled_ && ((subpop->child_sex_ratio_ < 0.0) || (subpop->child_sex_ratio_ > 1.0)))
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): out-of-range value for sex ratio; this file cannot be read." << EidosTerminate();
		}
		
		subpop->bounds_x0_ = metadata->bounds_x0_;
		subpop->bounds_x1_ = metadata->bounds_x1_;
		subpop->bounds_y0_ = metadata->bounds_y0_;
		subpop->bounds_y1_ = metadata->bounds_y1_;
		subpop->bounds_z0_ = metadata->bounds_z0_;
		subpop->bounds_z1_ = metadata->bounds_z1_;
		
		if (((spatial_dimensionality_ >= 1) && (subpop->bounds_x0_ >= subpop->bounds_x1_)) ||
			((spatial_dimensionality_ >= 2) && (subpop->bounds_y0_ >= subpop->bounds_y1_)) ||
			((spatial_dimensionality_ >= 3) && (subpop->bounds_z0_ >= subpop->bounds_z1_)))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): unsorted spatial bounds; this file cannot be read." << EidosTerminate();
		if (((spatial_dimensionality_ >= 1) && periodic_x_ && (subpop->bounds_x0_ != 0.0)) ||
			((spatial_dimensionality_ >= 2) && periodic_y_ && (subpop->bounds_y0_ != 0.0)) ||
			((spatial_dimensionality_ >= 3) && periodic_z_ && (subpop->bounds_z0_ != 0.0)))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): periodic bounds must have a minimum coordinate of 0.0; this file cannot be read." << EidosTerminate();
		
		uint32_t migration_rec_count = metadata->migration_rec_count_;
		
		if (metadata_length != sizeof(SubpopulationMetadataRec) + migration_rec_count * sizeof(SubpopulationMigrationMetadataRec))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): malformed migration metadata; this file cannot be read." << EidosTerminate();
		if ((model_type_ == SLiMModelType::kModelTypeNonWF) && (migration_rec_count > 0))
			EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): migration rates cannot be provided in a nonWF model; this file cannot be read." << EidosTerminate();
		
		for (size_t migration_index = 0; migration_index < migration_rec_count; ++migration_index)
		{
			slim_objectid_t source_id = migration_recs[migration_index].source_subpop_id_;
			double rate = migration_recs[migration_index].migration_rate_;
			
			if (source_id == subpop_id)
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): self-referential migration record; this file cannot be read." << EidosTerminate();
			if (subpop->migrant_fractions_.find(source_id) != subpop->migrant_fractions_.end())
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): duplicate migration record; this file cannot be read." << EidosTerminate();
			if ((rate < 0.0) || (rate > 1.0))
				EIDOS_TERMINATION << "ERROR (SLiMSim::__ConfigureSubpopulationsFromTables): out-of-range migration rate; this file cannot be read." << EidosTerminate();
			
			subpop->migrant_fractions_.insert(std::pair<slim_objectid_t, double>(source_id, rate));
		}
	}
}

typedef struct ts_mut_info {
	slim_position_t position;
	MutationMetadataRec *metadata;
	slim_refcount_t ref_count;
} ts_mut_info;

void SLiMSim::__TabulateMutationsFromTables(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, int p_file_version)
{
	std::size_t metadata_rec_size = ((p_file_version < 3) ? sizeof(MutationMetadataRec_PRENUC) : sizeof(MutationMetadataRec));
	tsk_mutation_table_t &mut_table = tables_.mutations;
	tsk_size_t mut_count = mut_table.num_rows;
	
	if ((mut_count > 0) && !recording_mutations_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateMutationsFromTables): cannot load mutations when mutation recording is disabled." << EidosTerminate();
	
	for (tsk_size_t mut_index = 0; mut_index < mut_count; ++mut_index)
	{
		const char *derived_state_bytes = mut_table.derived_state + mut_table.derived_state_offset[mut_index];
		tsk_size_t derived_state_length = mut_table.derived_state_offset[mut_index + 1] - mut_table.derived_state_offset[mut_index];
		const char *metadata_bytes = mut_table.metadata + mut_table.metadata_offset[mut_index];
		tsk_size_t metadata_length = mut_table.metadata_offset[mut_index + 1] - mut_table.metadata_offset[mut_index];
		
		if (derived_state_length % sizeof(slim_mutationid_t) != 0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateMutationsFromTables): unexpected mutation derived state length; this file cannot be read." << EidosTerminate();
		if (metadata_length % metadata_rec_size != 0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateMutationsFromTables): unexpected mutation metadata length; this file cannot be read." << EidosTerminate();
		if (derived_state_length / sizeof(slim_mutationid_t) != metadata_length / metadata_rec_size)
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateMutationsFromTables): (internal error) mutation metadata length does not match derived state length." << EidosTerminate();
		
		int stack_count = derived_state_length / sizeof(slim_mutationid_t);
		slim_mutationid_t *derived_state_vec = (slim_mutationid_t *)derived_state_bytes;
		const void *metadata_vec = metadata_bytes;	// either const MutationMetadataRec* or const MutationMetadataRec_PRENUC*
		tsk_id_t site_id = mut_table.site[mut_index];
		double position_double = tables_.sites.position[site_id];
		double position_double_round = round(position_double);
		
		if (position_double_round != position_double)
			EIDOS_TERMINATION << "ERROR (SLiMSim::__TabulateMutationsFromTables): mutation positions must be whole numbers for importation into SLiM; fractional positions are not allowed." << EidosTerminate();
		
		slim_position_t position = (slim_position_t)position_double_round;
		
		// tabulate the mutations referenced by this entry, overwriting previous tabulations (last state wins)
		for (int stack_index = 0; stack_index < stack_count; ++stack_index)
		{
			slim_mutationid_t mut_id = derived_state_vec[stack_index];
			auto mut_info_insert = p_mutMap.insert(std::pair<slim_mutationid_t, ts_mut_info>(mut_id, ts_mut_info()));
			ts_mut_info &mut_info = (mut_info_insert.first)->second;
			
			mut_info.position = position;
			
			// This method handles the fact that a file version of 2 or below will not contain a nucleotide field for its mutation metadata.
			// We hide this fact from the rest of the initialization code; ts_mut_info uses MutationMetadataRec, and we fill in a value of
			// -1 for the nucleotide_ field if we are using MutationMetadataRec_PRENUC due to the file version.
			if (p_file_version < 3)
			{
				MutationMetadataRec_PRENUC *prenuc_metadata = (MutationMetadataRec_PRENUC *)metadata_vec + stack_index;
				
				mut_info.metadata->mutation_type_id_ = prenuc_metadata->mutation_type_id_;
				mut_info.metadata->selection_coeff_ = prenuc_metadata->selection_coeff_;
				mut_info.metadata->subpop_index_ = prenuc_metadata->subpop_index_;
				mut_info.metadata->origin_generation_ = prenuc_metadata->origin_generation_;
				mut_info.metadata->nucleotide_ = -1;
			}
			else
			{
				mut_info.metadata = (MutationMetadataRec *)metadata_vec + stack_index;
			}
		}
	}
}

void SLiMSim::__TallyMutationReferencesWithTreeSequence(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, std::unordered_map<tsk_id_t, Genome *> p_nodeToGenomeMap, tsk_treeseq_t *p_ts)
{
	// allocate and set up the vargen object we'll use to walk through variants
	tsk_vargen_t *vg;
	
	vg = (tsk_vargen_t *)malloc(sizeof(tsk_vargen_t));
	int ret = tsk_vargen_init(vg, p_ts, p_ts->samples, p_ts->num_samples, NULL, TSK_16_BIT_GENOTYPES | TSK_ISOLATED_NOT_MISSING);
	if (ret != 0) handle_error("__TallyMutationReferencesWithTreeSequence tsk_vargen_init()", ret);
	
	// set up a map from sample indices in the vargen to Genome objects; the sample
	// may contain nodes that are ancestral and need to be excluded
	std::vector<Genome *> indexToGenomeMap;
	size_t sample_count = vg->num_samples;
	
	for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
	{
		tsk_id_t sample_node_id = vg->samples[sample_index];
		auto sample_nodeToGenome_iter = p_nodeToGenomeMap.find(sample_node_id);
		
		if (sample_nodeToGenome_iter != p_nodeToGenomeMap.end())
			indexToGenomeMap.push_back(sample_nodeToGenome_iter->second);
		else
			indexToGenomeMap.push_back(nullptr);	// this sample is not extant; no corresponding genome
	}
	
	// add mutations to genomes by looping through variants
	do
	{
		tsk_variant_t *variant;
		
		ret = tsk_vargen_next(vg, &variant);
		if (ret < 0) handle_error("__TallyMutationReferencesWithTreeSequence tsk_vargen_next()", ret);
		
		if (ret == 1)
		{
			// We have a new variant; set it into SLiM.  A variant represents a site at which a tracked mutation exists.
			// The tsk_variant_t will tell us all the allelic states involved at that site, what the alleles are, and which genomes
			// in the sample are using them.  We want to find any mutations that are shared across all non-null genomes.
			for (tsk_size_t allele_index = 0; allele_index < variant->num_alleles; ++allele_index)
			{
				tsk_size_t allele_length = variant->allele_lengths[allele_index];
				
				if (allele_length > 0)
				{
					// Calculate the number of extant genomes that reference this allele
					int32_t allele_refs = 0;
					
					for (size_t sample_index = 0; sample_index < sample_count; sample_index++)
						if ((variant->genotypes.i16[sample_index] == (int16_t)allele_index) && (indexToGenomeMap[sample_index] != nullptr))
							allele_refs++;
					
					// If that count is greater than zero (might be zero if only non-extant nodes reference the allele), tally it
					if (allele_refs)
					{
						if (allele_length % sizeof(slim_mutationid_t) != 0)
							EIDOS_TERMINATION << "ERROR (SLiMSim::__TallyMutationReferencesWithTreeSequence): (internal error) variant allele had length that was not a multiple of sizeof(slim_mutationid_t)." << EidosTerminate();
						allele_length /= sizeof(slim_mutationid_t);
						
						slim_mutationid_t *allele = (slim_mutationid_t *)variant->alleles[allele_index];
						
						for (tsk_size_t mutid_index = 0; mutid_index < allele_length; ++mutid_index)
						{
							slim_mutationid_t mut_id = allele[mutid_index];
							auto mut_info_iter = p_mutMap.find(mut_id);
							
							if (mut_info_iter == p_mutMap.end())
								EIDOS_TERMINATION << "ERROR (SLiMSim::__TallyMutationReferencesWithTreeSequence): mutation id " << mut_id << " was referenced but does not exist." << EidosTerminate();
							
							// Add allele_refs to the refcount for this mutation
							ts_mut_info &mut_info = mut_info_iter->second;
							
							mut_info.ref_count += allele_refs;
						}
					}
				}
			}
		}
	}
	while (ret != 0);
	
	// free
	ret = tsk_vargen_free(vg);
	if (ret != 0) handle_error("__TallyMutationReferencesWithTreeSequence tsk_vargen_free()", ret);
	free(vg);
}

void SLiMSim::__CreateMutationsFromTabulation(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutInfoMap, std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap)
{
	// count the number of non-null genomes there are; this is the count that would represent fixation
	slim_refcount_t fixation_count = 0;
	
	for (auto pop_iter : population_.subpops_)
		for (Genome *genome : pop_iter.second->parent_genomes_)
			if (!genome->IsNull())
				fixation_count++;
	
	// instantiate mutations
	for (auto mut_info_iter : p_mutInfoMap)
	{
		slim_mutationid_t mutation_id = mut_info_iter.first;
		ts_mut_info &mut_info = mut_info_iter.second;
		MutationMetadataRec *metadata_ptr = mut_info.metadata;
		MutationMetadataRec metadata;
		slim_position_t position = mut_info.position;
		
		// BCH 4 Feb 2020: bump the next mutation ID counter as needed here, so that this happens in all cases – even if
		// the mutation in the mutation table is fixed (so we will create a Substitution) or absent (so we will create
		// nothing).  Even in those cases, we have to ensure that we do not re-use the previously used mutation ID.
		if (gSLiM_next_mutation_id <= mutation_id)
			gSLiM_next_mutation_id = mutation_id + 1;
		
		// a mutation might not be refered by any extant genome; it might be present in an ancestral node,
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
			EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateMutationsFromTabulation): mutation type m" << metadata.mutation_type_id_ << " has not been defined." << EidosTerminate();
		
		if ((mut_info.ref_count == fixation_count) && (mutation_type_ptr->convert_to_substitution_))
		{
			// this mutation is fixed, and the muttype wants substitutions, so make a substitution
			Substitution *sub = new Substitution(mutation_id, mutation_type_ptr, position, metadata.selection_coeff_, metadata.subpop_index_, metadata.origin_generation_, generation_, metadata.nucleotide_);
			
			population_.treeseq_substitutions_map_.insert(std::pair<slim_position_t, Substitution *>(position, sub));
			population_.substitutions_.emplace_back(sub);
			
			// add -1 to our local map, so we know there's an entry but we also know it's a substitution
			p_mutIndexMap[mutation_id] = -1;
		}
		else
		{
			// construct the new mutation; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
			MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
			
			Mutation *new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_id, mutation_type_ptr, position, metadata.selection_coeff_, metadata.subpop_index_, metadata.origin_generation_, metadata.nucleotide_);
			
			// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
			p_mutIndexMap[mutation_id] = new_mut_index;
			population_.MutationRegistryAdd(new_mut);
			
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
			if (population_.keeping_muttype_registries_)
				EIDOS_TERMINATION << "ERROR (SLiMSim::__CreateMutationsFromTabulation): (internal error) separate muttype registries set up during pop load." << EidosTerminate();
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

void SLiMSim::__AddMutationsFromTreeSequenceToGenomes(std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, std::unordered_map<tsk_id_t, Genome *> p_nodeToGenomeMap, tsk_treeseq_t *p_ts)
{
	// This code is based on SLiMSim::CrosscheckTreeSeqIntegrity(), but it can be much simpler.
	// We also don't need to sort/deduplicate/simplify; the tables read in should be simplified already.
	if (!recording_mutations_)
		return;
	
	// allocate and set up the vargen object we'll use to walk through variants
	tsk_vargen_t *vg;
	
	vg = (tsk_vargen_t *)malloc(sizeof(tsk_vargen_t));
	int ret = tsk_vargen_init(vg, p_ts, p_ts->samples, p_ts->num_samples, NULL, TSK_16_BIT_GENOTYPES | TSK_ISOLATED_NOT_MISSING);
	if (ret != 0) handle_error("__AddMutationsFromTreeSequenceToGenomes tsk_vargen_init()", ret);
	
	// set up a map from sample indices in the vargen to Genome objects; the sample
	// may contain nodes that are ancestral and need to be excluded
	std::vector<Genome *> indexToGenomeMap;
	size_t sample_count = vg->num_samples;
	
	for (size_t sample_index = 0; sample_index < sample_count; ++sample_index)
	{
		tsk_id_t sample_node_id = vg->samples[sample_index];
		auto sample_nodeToGenome_iter = p_nodeToGenomeMap.find(sample_node_id);
		
		if (sample_nodeToGenome_iter != p_nodeToGenomeMap.end())
		{
			// we found a genome for this sample, so record it
			indexToGenomeMap.push_back(sample_nodeToGenome_iter->second);
		}
		else
		{
			// this sample is not extant; no corresponding genome, so record nullptr
			indexToGenomeMap.push_back(nullptr);
		}
	}
	
	// add mutations to genomes by looping through variants
	do
	{
		tsk_variant_t *variant;
		
		ret = tsk_vargen_next(vg, &variant);
		if (ret < 0) handle_error("__AddMutationsFromTreeSequenceToGenomes tsk_vargen_next()", ret);
		
		if (ret == 1)
		{
			// We have a new variant; set it into SLiM.  A variant represents a site at which a tracked mutation exists.
			// The tsk_variant_t will tell us all the allelic states involved at that site, what the alleles are, and which genomes
			// in the sample are using them.  We will then set all the genomes that the variant claims to involve to have
			// the allele the variant attributes to them.  The variants are returned in sorted order by position, so we can
			// always add new mutations to the ends of genomes.
			slim_position_t variant_pos_int = (slim_position_t)variant->site->position;
			
			for (size_t sample_index = 0; sample_index < sample_count; sample_index++)
			{
				Genome *genome = indexToGenomeMap[sample_index];
				
				if (genome)
				{
					uint16_t genome_variant = variant->genotypes.i16[sample_index];
					tsk_size_t genome_allele_length = variant->allele_lengths[genome_variant];
					
					if (genome_allele_length % sizeof(slim_mutationid_t) != 0)
						EIDOS_TERMINATION << "ERROR (SLiMSim::__AddMutationsFromTreeSequenceToGenomes): (internal error) variant allele had length that was not a multiple of sizeof(slim_mutationid_t)." << EidosTerminate();
					genome_allele_length /= sizeof(slim_mutationid_t);
					
					if (genome_allele_length > 0)
					{
						if (genome->IsNull())
							EIDOS_TERMINATION << "ERROR (SLiMSim::__AddMutationsFromTreeSequenceToGenomes): (internal error) null genome has non-zero treeseq allele length " << genome_allele_length << "." << EidosTerminate();
						
						slim_mutationid_t *genome_allele = (slim_mutationid_t *)variant->alleles[genome_variant];
						slim_mutrun_index_t run_index = (slim_mutrun_index_t)(variant_pos_int / genome->mutrun_length_);
						
						genome->WillModifyRun(run_index);
						
						MutationRun *mutrun = genome->mutruns_[run_index].get();
						
						for (tsk_size_t mutid_index = 0; mutid_index < genome_allele_length; ++mutid_index)
						{
							slim_mutationid_t mut_id = genome_allele[mutid_index];
							auto mut_index_iter = p_mutIndexMap.find(mut_id);
							
							if (mut_index_iter == p_mutIndexMap.end())
								EIDOS_TERMINATION << "ERROR (SLiMSim::__AddMutationsFromTreeSequenceToGenomes): mutation id " << mut_id << " was referenced but does not exist." << EidosTerminate();
							
							// Add the mutation to the genome unless it is fixed (mut_index == -1)
							MutationIndex mut_index = mut_index_iter->second;
							
							if (mut_index != -1)
								mutrun->emplace_back(mut_index);
						}
					}
				}
			}
		}
	}
	while (ret != 0);
	
	// free
	ret = tsk_vargen_free(vg);
	if (ret != 0) handle_error("__AddMutationsFromTreeSequenceToGenomes tsk_vargen_free()", ret);
	free(vg);
}

slim_generation_t SLiMSim::_InstantiateSLiMObjectsFromTables(EidosInterpreter *p_interpreter)
{
	// first, check the provenance table to make sure this is a SLiM-compatible file of a version we understand
	// if it is, set the generation from the provenance data
	// note that ReadTreeSequenceMetadata() presently throws an exception if asked to read a SLiM 3.0 .trees file;
	// the changes in the tables, metadata, etc., were just too extensive for it to be reasonable to do...
	slim_generation_t metadata_gen;
	SLiMModelType file_model_type;
	int file_version;
	
	if (tables_.sequence_length != chromosome_.last_position_ + 1)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InstantiateSLiMObjectsFromTables): chromosome length in loaded population does not match the configured chromosome length." << EidosTerminate();
	
	ReadTreeSequenceMetadata(&tables_, &metadata_gen, &file_model_type, &file_version);
	SetGeneration(metadata_gen);
	
	// rebase the times in the nodes to be in SLiM-land; see WriteTreeSequence for the inverse operation
	// BCH 4/4/2019: switched to using tree_seq_generation_ to avoid a parent/child timestamp conflict
	// This makes sense; as far as tree-seq recording is concerned, tree_seq_generation_ is the generation counter
	slim_generation_t time_adjustment = tree_seq_generation_;
	
	for (size_t node_index = 0; node_index < tables_.nodes.num_rows; ++node_index)
		tables_.nodes.time[node_index] -= time_adjustment;

	for (size_t mut_index = 0; mut_index < tables_.mutations.num_rows; ++mut_index)
		tables_.mutations.time[mut_index] -= time_adjustment;
	
	// allocate and set up the tree_sequence object that contains all the tree sequences
	// note that this tree sequence is based upon whatever sample the file was saved with, and may contain in-sample individuals
	// that are not presently alive, so we have to tread carefully; the individual table is the list of who is actually alive
	tsk_treeseq_t *ts;
	int ret = 0;
	
	ts = (tsk_treeseq_t *)malloc(sizeof(tsk_treeseq_t));
	ret = tsk_treeseq_init(ts, &tables_, TSK_BUILD_INDEXES);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_init()", ret);
	
	std::unordered_map<tsk_id_t, Genome *> nodeToGenomeMap;
	
	{
		std::unordered_map<slim_objectid_t, ts_subpop_info> subpopInfoMap;
		
		__TabulateSubpopulationsFromTreeSequence(subpopInfoMap, ts, file_model_type);
		__CreateSubpopulationsFromTabulation(subpopInfoMap, p_interpreter, nodeToGenomeMap);
		__ConfigureSubpopulationsFromTables(p_interpreter);
	}
	
	std::unordered_map<slim_mutationid_t, MutationIndex> mutIndexMap;
	
	{
		std::unordered_map<slim_mutationid_t, ts_mut_info> mutInfoMap;
		
		__TabulateMutationsFromTables(mutInfoMap, file_version);
		__TallyMutationReferencesWithTreeSequence(mutInfoMap, nodeToGenomeMap, ts);
		__CreateMutationsFromTabulation(mutInfoMap, mutIndexMap);
	}
	
	__AddMutationsFromTreeSequenceToGenomes(mutIndexMap, nodeToGenomeMap, ts);
	
	ret = tsk_treeseq_free(ts);
	if (ret != 0) handle_error("_InstantiateSLiMObjectsFromTables tsk_treeseq_free()", ret);
	free(ts);
	
	// Figure out how many remembered genomes we have; each remembered individual has two remembered genomes
	// First-generation individuals are also "remembered" in the present design, and so must be included
	size_t remembered_genome_count = 0;
	
	for (tsk_id_t j = 0; (size_t) j < tables_.individuals.num_rows; j++)
	{
		uint32_t flags = tables_.individuals.flags[j];
		if (flags & SLIM_TSK_INDIVIDUAL_REMEMBERED)
			remembered_genome_count += 2;
	}
	
	// Set up the remembered genomes, which are (we assume) the first remembered_genome_count node table entries
	// We could instead simply loop over the nodes and see if the individual they point to is remembered, which would
	// not require that remembered genomes be the first rows in the node table, if we ever want to relax that assumption.
	if (remembered_genomes_.size() != 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InstantiateSLiMObjectsFromTables): (internal error) remembered_genomes_ is not empty." << EidosTerminate();
	
	// BCH 4/27/2019: remembered_genomes_ are always the first remembered_genome_count entries in the node table...
	for (size_t i = 0; i < remembered_genome_count; ++i)
		remembered_genomes_.push_back((tsk_id_t)i);
	
	// ...but we should check that they are all in the individuals table, and either Remembered or FirstGen...
	for (size_t i = 0; i < remembered_genome_count; ++i)
	{
		assert((tsk_size_t)i < tables_.nodes.num_rows);
		tsk_id_t ind = tables_.nodes.individual[i];
		assert((ind >= 0) && ((tsk_size_t)ind < tables_.individuals.num_rows));
		tsk_flags_t __attribute__((__unused__)) ind_flags = tables_.individuals.flags[ind];
		assert(ind_flags & SLIM_TSK_INDIVIDUAL_REMEMBERED);
	}
	
	// ... and then we should sort them to match the order of the individual table, so that they satisfy
	// the invariants asserted in SLiMSim::AddIndividualsToTable(); see the comments there
	std::sort(remembered_genomes_.begin(), remembered_genomes_.end(), [this](tsk_id_t l, tsk_id_t r) {
		tsk_id_t l_ind = tables_.nodes.individual[l];
		tsk_id_t r_ind = tables_.nodes.individual[r];
		if (l_ind != r_ind)
			return l_ind < r_ind;
		return l < r;
	});
	
	// Clear ALIVE flags
	FixAliveIndividuals(&tables_);
	
	// Remove individuals that are not remembered
    std::vector<tsk_id_t> individual_map;
    for (tsk_id_t j = 0; (size_t) j < tables_.individuals.num_rows; j++)
    {
        uint32_t flags = tables_.individuals.flags[j];
        if (flags & SLIM_TSK_INDIVIDUAL_REMEMBERED)
            individual_map.push_back(j);
    }
    ReorderIndividualTable(&tables_, individual_map, false);
	
	// Re-tally mutation references so we have accurate frequency counts for our new mutations
	population_.UniqueMutationRuns();
	population_.TallyMutationReferences(nullptr, true);
	
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
	
	// return the current simulation generation as reconstructed from the file
	return metadata_gen;
}

slim_generation_t SLiMSim::_InitializePopulationFromTskitTextFile(const char *p_file, EidosInterpreter *p_interpreter)
{
	std::string directory_path(p_file);
	
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTskitTextFile): to load a tree-sequence file, tree-sequence recording must be enabled with initializeTreeSeq()." << EidosTerminate();
	
	// free the existing table collection
	FreeTreeSequence();
	
	// in nucleotide-based models, read the ancestral sequence
	if (nucleotide_based_)
	{
		std::string RefSeqFileName = directory_path + "/ReferenceSequence.txt";
		std::ifstream infile;
		
		infile.open(RefSeqFileName, std::ifstream::in);
		if (!infile.is_open())
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTskitTextFile): readFromPopulationFile() could not open "<< RefSeqFileName << "; this model is nucleotide-based, but the ancestral sequence is missing or unreadable." << EidosTerminate();
		
		infile >> *(chromosome_.AncestralSequence());	// raises if the sequence is the wrong length
		infile.close();
	}
	
	// read the files from disk
	std::string edge_path = directory_path + "/EdgeTable.txt";
	std::string node_path = directory_path + "/NodeTable.txt";
	std::string site_path = directory_path + "/SiteTable.txt";
	std::string mutation_path = directory_path + "/MutationTable.txt";
	std::string individual_path = directory_path + "/IndividualTable.txt";
	std::string population_path = directory_path + "/PopulationTable.txt";
	std::string provenance_path = directory_path + "/ProvenanceTable.txt";
	
	TreeSequenceDataFromAscii(node_path, edge_path, site_path, mutation_path, individual_path, population_path, provenance_path);
	
	// make the corresponding SLiM objects
	return _InstantiateSLiMObjectsFromTables(p_interpreter);
}

slim_generation_t SLiMSim::_InitializePopulationFromTskitBinaryFile(const char *p_file, EidosInterpreter *p_interpreter)
{
    int ret;

	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTskitBinaryFile): to load a tree-sequence file, tree-sequence recording must be enabled with initializeTreeSeq()." << EidosTerminate();
	
	// free the existing table collection
	FreeTreeSequence();
	
	ret = tsk_table_collection_load(&tables_, p_file, 0);
	if (ret != 0) handle_error("tsk_table_collection_load", ret);
	
	// BCH 4/25/2019: if indexes are present on tables_ we want to drop them; they are synced up
	// with the edge table, but we plan to modify the edge table so they will become invalid anyway, and
	// then they may cause a crash because of their unsynced-ness; see tskit issue #179
	ret = tsk_table_collection_drop_index(&tables_, 0);
	if (ret != 0) handle_error("tsk_table_collection_drop_index", ret);

	RecordTablePosition();
	
	// convert ASCII derived-state data, which is the required format on disk, back to our in-memory binary format
	DerivedStatesFromAscii(&tables_);
	
	// in nucleotide-based models, read the ancestral sequence
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
		
		ret = kastore_gets_int8(&store, "reference_sequence/data", (int8_t **)&buffer, &buffer_length);
		if (ret != 0)
			buffer = NULL;
		
		if (!buffer)
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTskitBinaryFile): this is a nucleotide-based model, but there is no reference nucleotide sequence." << EidosTerminate();
		if (buffer_length != chromosome_.AncestralSequence()->size())
			EIDOS_TERMINATION << "ERROR (SLiMSim::_InitializePopulationFromTskitBinaryFile): the reference nucleotide sequence length does not match the model." << EidosTerminate();
		
		chromosome_.AncestralSequence()->ReadNucleotidesFromBuffer(buffer);
		
		// buffer is owned by kastore and is freed by closing the store
        kastore_close(&store);
	}

	// make the corresponding SLiM objects
	return _InstantiateSLiMObjectsFromTables(p_interpreter);
}

size_t SLiMSim::MemoryUsageForTables(tsk_table_collection_t &p_tables)
{
	tsk_table_collection_t &t = p_tables;
	size_t usage = 0;
	
	usage += sizeof(tsk_individual_table_t);
	
	if (t.individuals.flags)
		usage += t.individuals.max_rows * sizeof(uint32_t);
	if (t.individuals.location_offset)
		usage += t.individuals.max_rows * sizeof(tsk_size_t);
	if (t.individuals.metadata_offset)
		usage += t.individuals.max_rows * sizeof(tsk_size_t);
	
	if (t.individuals.location)
		usage += t.individuals.max_location_length * sizeof(double);
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
	
	usage += sizeof(tsk_population_table_t);
	
	if (t.populations.metadata_offset)
		usage += t.populations.max_rows * sizeof(tsk_size_t);
	
	if (t.populations.metadata)
		usage += t.populations.max_metadata_length * sizeof(char);
	
	usage += sizeof(tsk_provenance_table_t);
	
	if (t.provenances.timestamp_offset)
		usage += t.provenances.max_rows * sizeof(tsk_size_t);
	if (t.provenances.record_offset)
		usage += t.provenances.max_rows * sizeof(tsk_size_t);
	
	if (t.provenances.timestamp)
		usage += t.provenances.max_timestamp_length * sizeof(char);
	if (t.provenances.record)
		usage += t.provenances.max_record_length * sizeof(char);
	
	usage += remembered_genomes_.size() * sizeof(tsk_id_t);
	
	return usage;
}






























































