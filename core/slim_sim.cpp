//
//  slim_sim.cpp
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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

#include <iostream>
#include <fstream>
#include <stdexcept>

#include "script_test.h"
#include "script_interpreter.h"
#include "script_functionsignature.h"


using std::multimap;
using std::string;
using std::endl;
using std::istream;
using std::istringstream;
using std::ifstream;
using std::vector;


SLiMSim::SLiMSim(std::istream &infile, int *p_override_seed_ptr) : population_(*this)
{
	// track the random number seed given, if there is one
	if (p_override_seed_ptr)
	{
		rng_seed_ = *p_override_seed_ptr;
		rng_seed_supplied_to_constructor_ = true;
	}
	else
	{
		rng_seed_ = 0;
		rng_seed_supplied_to_constructor_ = false;
	}
	
	// read all configuration information from the input file
	infile.clear();
	infile.seekg(0, std::fstream::beg);
	InitializeFromFile(infile);
}

SLiMSim::SLiMSim(const char *p_input_file, int *p_override_seed_ptr) : population_(*this)
{
	// track the random number seed given, if there is one
	if (p_override_seed_ptr)
	{
		rng_seed_ = *p_override_seed_ptr;
		rng_seed_supplied_to_constructor_ = true;
	}
	else
	{
		rng_seed_ = 0;
		rng_seed_supplied_to_constructor_ = false;
	}
	
	// Open our file stream
	std::ifstream infile(p_input_file);
	
	if (!infile.is_open())
	{
		SLIM_TERMINATION << std::endl;
		SLIM_TERMINATION << "ERROR (parameter file): could not open: " << p_input_file << std::endl << std::endl;
		SLIM_TERMINATION << slim_terminate();
	}
	
	// read all configuration information from the input file
	infile.seekg(0, std::fstream::beg);
	InitializeFromFile(infile);
}

SLiMSim::~SLiMSim(void)
{
	//SLIM_ERRSTREAM << "SLiMSim::~SLiMSim" << std::endl;
	
	for (auto mutation_type : mutation_types_)
		delete mutation_type.second;
	
	for (auto genomic_element_type : genomic_element_types_)
		delete genomic_element_type.second;
	
	for (auto script_block : script_blocks_)
		delete script_block;
	
	// All the script blocks that refer to the script are now gone
	delete script_;
	
	// We should not have any interpreter instances that still refer to us
	for (FunctionSignature *signature : sim_0_signatures)
		delete signature;
	
	if (self_symbol_)
		delete self_symbol_;
}

void SLiMSim::InitializeFromFile(std::istream &infile)
{
	if (!rng_seed_supplied_to_constructor_)
		rng_seed_ = GenerateSeedFromPIDAndTime();
	
	if (DEBUG_INPUT)
		SLIM_OUTSTREAM << "// InitializeFromFile()" << endl << endl;
	
	// Reset error position indicators used by SLiMgui
	gCharacterStartOfParseError = -1;
	gCharacterEndOfParseError = -1;
	
	// Read in the file; going through stringstream is fast...
	std::stringstream buffer;
	
	buffer << infile.rdbuf();
	
	std::string script_string = buffer.str();
	
	// Tokenize and parse
	script_ = new Script(script_string, 0);
	
	script_->Tokenize();
	script_->ParseSLiMFileToAST();
	
	// Extract SLiMScriptBlocks from the parse tree
	const ScriptASTNode *root_node = script_->AST();
	
	for (ScriptASTNode *script_block_node : root_node->children_)
	{
		SLiMScriptBlock *script_block = new SLiMScriptBlock(script_block_node);
		
		script_blocks_.push_back(script_block);
	}
	
	// Reset error position indicators used by SLiMgui
	gCharacterStartOfParseError = -1;
	gCharacterEndOfParseError = -1;
	
	// initialize rng; this is either a value given at the command line, or the value generate above by GenerateSeedFromPIDAndTime()
	InitializeRNGFromSeed(rng_seed_);
}

// get one line of input, sanitizing by removing comments and whitespace; used only by SLiMSim::InitializePopulationFromFile
void GetInputLine(istream &p_input_file, string &p_line);
void GetInputLine(istream &p_input_file, string &p_line)
{
	getline(p_input_file, p_line);
	
	// remove all after "//", the comment start sequence
	// BCH 16 Dec 2014: note this was "/" in SLiM 1.8 and earlier, changed to allow full filesystem paths to be specified.
	if (p_line.find("//") != string::npos)
		p_line.erase(p_line.find("//"));
	
	// remove leading and trailing whitespace (spaces and tabs)
	p_line.erase(0, p_line.find_first_not_of(" \t"));
	p_line.erase(p_line.find_last_not_of(" \t") + 1);
}

void SLiMSim::InitializePopulationFromFile(const char *p_file)
{
	std::map<int,Mutation*> mutations;
	string line, sub; 
	ifstream infile(p_file);
	
	if (!infile.is_open())
		SLIM_TERMINATION << "ERROR (Initialize): could not open initialization file" << slim_terminate();
	
	// Read and ignore initial stuff until we hit the Populations section
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		if (line.find("Populations") != string::npos)
			break;
	}
	
	// Now we are in the Populations section; read and instantiate each population until we hit the Mutations section
	while (!infile.eof())
	{ 
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Mutations") != string::npos)
			break;
		
		istringstream iss(line);
		
		iss >> sub;
		sub.erase(0, 1);  // p
		int subpop_index = atoi(sub.c_str());
		
		iss >> sub;
		int subpop_size = atoi(sub.c_str());
		
		// SLiM 2.0 output format has <H | S <ratio>> here; if that is missing or "H" is given, the population is hermaphroditic and the ratio given is irrelevant
		double sex_ratio = 0.0;
		
		if (iss >> sub)
		{
			if (sub == "S")
			{
				iss >> sub;
				
				sex_ratio = atof(sub.c_str());
			}
		}
		
		// Create the population population
		population_.AddSubpopulation(subpop_index, subpop_size, sex_ratio);
	}
	
	// Now we are in the Mutations section; read and instantiate all mutations and add them to our map and to the registry
	while (!infile.eof()) 
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Genomes") != string::npos)
			break;
		if (line.find("Individuals") != string::npos)	// SLiM 2.0 added this section
			break;
		
		istringstream iss(line);
		
		iss >> sub; 
		int mutation_id = atoi(sub.c_str());
		
		iss >> sub;
		sub.erase(0, 1);	// m
		int mutation_type_id = atoi(sub.c_str());
		
		iss >> sub;
		int position = atoi(sub.c_str());		// used to have a -1; switched to zero-based
		
		iss >> sub;
		double selection_coeff = atof(sub.c_str());
		
		iss >> sub;		// dominance coefficient, which is given in the mutation type and presumably matches here
		
		iss >> sub;
		sub.erase(0, 1);	// p
		int subpop_index = atoi(sub.c_str());
		
		iss >> sub;
		int generation = atoi(sub.c_str());
		
		// look up the mutation type from its index
		auto found_muttype_pair = mutation_types_.find(mutation_type_id);
		
		if (found_muttype_pair == mutation_types_.end()) 
			SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): mutation type m"<< mutation_type_id << " has not been defined" << slim_terminate();
		
		MutationType *mutation_type_ptr = found_muttype_pair->second;
		
		// construct the new mutation
		Mutation *new_mutation = new Mutation(mutation_type_ptr, position, selection_coeff, subpop_index, generation);
		
		// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
		mutations.insert(std::pair<const int,Mutation*>(mutation_id, new_mutation));
		population_.mutation_registry_.push_back(new_mutation);
	}
	
	// If there is an Individuals section (added in SLiM 2.0), we skip it; we don't need any of the information that it gives, it is mainly for human readability
	if (line.find("Individuals") != string::npos)
	{
		while (!infile.eof()) 
		{
			GetInputLine(infile, line);
			
			if (line.length() == 0)
				continue;
			if (line.find("Genomes") != string::npos)
				break;
		}
	}
	
	// Now we are in the Genomes section, which should take us to the end of the file
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		
		istringstream iss(line);
		
		iss >> sub;
		sub.erase(0, 1);	// p
		int pos = static_cast<int>(sub.find_first_of(":"));
		string subpop_substr = sub.substr(0, pos);
		const char *subpop_id_string = subpop_substr.c_str();
		int subpop_id = atoi(subpop_id_string);
		
		Subpopulation &subpop = population_.SubpopulationWithID(subpop_id);
		
		sub.erase(0, pos + 1);	// remove the subpop_id and the colon
		int genome_index = atoi(sub.c_str());		// used to have a -1; switched to zero-based
		Genome &genome = subpop.parent_genomes_[genome_index];
		
		// Now we might have [A|X|Y] (SLiM 2.0), or we might have the first mutation id - or we might have nothing at all
		if (iss >> sub)
		{
			char genome_type = sub[0];
			
			// check whether this token is a genome type
			if (genome_type == 'A' || genome_type == 'X' || genome_type == 'Y')
			{
				// Let's do a little error-checking against what has already been instantiated for us...
				if (genome_type == 'A' && genome.GenomeType() != GenomeType::kAutosome)
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as A (autosome), but the instantiated genome does not match" << slim_terminate();
				if (genome_type == 'X' && genome.GenomeType() != GenomeType::kXChromosome)
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as X (X-chromosome), but the instantiated genome does not match" << slim_terminate();
				if (genome_type == 'Y' && genome.GenomeType() != GenomeType::kYChromosome)
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as Y (Y-chromosome), but the instantiated genome does not match" << slim_terminate();
				
				if (iss >> sub)
				{
					if (sub == "<null>")
					{
						if (!genome.IsNull())
							SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as null, but the instantiated genome is non-null" << slim_terminate();
						
						continue;	// this line is over
					}
					else
					{
						if (genome.IsNull())
							SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as non-null, but the instantiated genome is null" << slim_terminate();
						
						// drop through, and sub will be interpreted as a mutation id below
					}
				}
				else
					continue;
			}
			
			do
			{
				int id = atoi(sub.c_str());
				
				auto found_mut_pair = mutations.find(id);
				
				if (found_mut_pair == mutations.end()) 
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): mutation " << id << " has not been defined" << slim_terminate();
				
				Mutation *mutation = found_mut_pair->second;
				
				genome.push_back(mutation);
			}
			while (iss >> sub);
		}
	}
	
	// Now that we have the info on everybody, update fitnesses so that we're ready to run the next generation
	// Note that generation+1 is used; we are computing fitnesses for the next generation
	for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
	{
		int subpop_id = subpop_pair.first;
		Subpopulation *subpop = subpop_pair.second;
		std::vector<SLiMScriptBlock*> fitness_callbacks = ScriptBlocksMatching(generation_ + 1, SLiMScriptBlockType::SLiMScriptFitnessCallback, -1, subpop_id);
		
		subpop->UpdateFitness(fitness_callbacks);
	}
}

std::vector<SLiMScriptBlock*> SLiMSim::ScriptBlocksMatching(int p_generation, SLiMScriptBlockType p_event_type, int p_mutation_type_id, int p_subpopulation_id)
{
	std::vector<SLiMScriptBlock*> matches;
	
	for (SLiMScriptBlock *script_block : script_blocks_)
	{
		// check that the generation is in range
		if ((script_block->start_generation_ > p_generation) || (script_block->end_generation_ < p_generation))
			continue;
		
		// check that the script type matches (event, callback, etc.)
		if (script_block->type_ != p_event_type)
			continue;
		
		// check that the mutation type id matches, if requested
		if (p_mutation_type_id != -1)
		{
			int mutation_type_id = script_block->mutation_type_id_;
			
			if ((mutation_type_id != -1) && (p_mutation_type_id != mutation_type_id))
				continue;
		}
		
		// check that the subpopulation id matches, if requested
		if (p_subpopulation_id != -1)
		{
			int subpopulation_id = script_block->subpopulation_id_;
			
			if ((subpopulation_id != -1) && (p_subpopulation_id != subpopulation_id))
				continue;
		}
		
		// OK, everything matches, so we want to return this script block
		matches.push_back(script_block);
	}
	
	return matches;
}

void SLiMSim::DeregisterScheduledScriptBlocks(void)
{
	// If we have blocks scheduled for deregistration, we sweep through and deregister them at the end of each stage of each generation.
	// This happens at a time when script blocks are not executing, so that we're guaranteed not to leave hanging pointers that could
	// cause a crash; it also guarantees that script blocks are applied consistently across each generation stage.  A single block
	// might be scheduled for deregistration more than once, but should only occur in script_blocks_ once, so we have to be careful
	// with our deallocations here; we deallocate a block only when we find it in script_blocks_.
	for (SLiMScriptBlock *block_to_dereg : scheduled_deregistrations_)
	{
		auto script_block_position = std::find(script_blocks_.begin(), script_blocks_.end(), block_to_dereg);
		
		if (script_block_position != script_blocks_.end())
		{
			script_blocks_.erase(script_block_position);
			scripts_changed_ = true;
			delete block_to_dereg;
		}
	}
}

void SLiMSim::RunZeroGeneration(void)
{
	// zero out the initialization check counts
	num_mutation_types = 0;
	num_mutation_rates = 0;
	num_genomic_element_types = 0;
	num_genomic_elements = 0;
	num_recombination_rates = 0;
	num_gene_conversions = 0;
	num_generations = 0;
	num_sex_declarations = 0;
	
	if (DEBUG_INPUT)
		SLIM_OUTSTREAM << "// RunZeroGeneration():" << endl;
	
	// execute script events for generation 0
	std::vector<SLiMScriptBlock*> blocks = ScriptBlocksMatching(0, SLiMScriptBlockType::SLiMScriptEvent, -1, -1);
	
	for (auto script_block : blocks)
	{
		if (script_block->active_)
			population_.ExecuteScript(script_block, generation_, chromosome_);
	}
	
	DeregisterScheduledScriptBlocks();
	
	// check for complete initialization
	if (num_mutation_rates == 0)
		SLIM_TERMINATION << "ERROR (RunZeroGeneration): A mutation rate must be supplied in generation 0 with setMutationRate0()." << slim_terminate();
	
	if (num_mutation_types == 0)
		SLIM_TERMINATION << "ERROR (RunZeroGeneration): At least one mutation type must be defined in generation 0 with addMutationType0()." << slim_terminate();
	
	if (num_genomic_element_types == 0)
		SLIM_TERMINATION << "ERROR (RunZeroGeneration): At least one genomic element type must be defined in generation 0 with addGenomicElementType0()." << slim_terminate();
	
	if (num_genomic_elements == 0)
		SLIM_TERMINATION << "ERROR (RunZeroGeneration): At least one genomic element must be defined in generation 0 with addGenomicElement0()." << slim_terminate();
	
	if (num_recombination_rates == 0)
		SLIM_TERMINATION << "ERROR (RunZeroGeneration): At least one recombination rate interval must be defined in generation 0 with addRecombinationIntervals0()." << slim_terminate();
	
	if (num_generations == 0)
		SLIM_TERMINATION << "ERROR (RunZeroGeneration): The simulation duration must be defined in generation 0 with setGenerationRange0()." << slim_terminate();
	
	// emit our start log
	SLIM_OUTSTREAM << "\n// Starting run with <start> <duration>:\n" << time_start_ << " " << time_duration_ << "\n" << std::endl;
	
	// start at the beginning
	generation_ = time_start_;
	
	// initialize chromosome
	chromosome_.InitializeDraws();
}

bool SLiMSim::RunOneGeneration(void)
{
#ifdef SLIMGUI
	if (simulationValid)
	{
#endif
		if ((generation_ == 0) || (generation_ < time_start_ + time_duration_))		// at generation 0 we don't know start or duration yet
		{
#ifdef SLIMGUI
			try
			{
#endif
				if (generation_ == 0)
				{
					RunZeroGeneration();
				}
				else
				{
					//
					// Stage 1: execute script events for the current generation
					//
					std::vector<SLiMScriptBlock*> blocks = ScriptBlocksMatching(generation_, SLiMScriptBlockType::SLiMScriptEvent, -1, -1);
					
					for (auto script_block : blocks)
						if (script_block->active_)
							population_.ExecuteScript(script_block, generation_, chromosome_);
					
					DeregisterScheduledScriptBlocks();
					
					//
					// Stage 2: evolve all subpopulations
					//
					std::vector<SLiMScriptBlock*> mate_choice_callbacks = ScriptBlocksMatching(generation_, SLiMScriptBlockType::SLiMScriptMateChoiceCallback, -1, -1);
					std::vector<SLiMScriptBlock*> modify_child_callbacks = ScriptBlocksMatching(generation_, SLiMScriptBlockType::SLiMScriptModifyChildCallback, -1, -1);
					
					for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
					{
						int subpop_id = subpop_pair.first;
						std::vector<SLiMScriptBlock*> subpop_mate_choice_callbacks;
						std::vector<SLiMScriptBlock*> subpop_modify_child_callbacks;
						
						// Get mateChoice() callbacks that apply to this subpopulation
						for (SLiMScriptBlock *callback : mate_choice_callbacks)
						{
							int callback_subpop_id = callback->subpopulation_id_;
							
							if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
								subpop_mate_choice_callbacks.push_back(callback);
						}
						
						// Get modifyChild() callbacks that apply to this subpopulation
						for (SLiMScriptBlock *callback : modify_child_callbacks)
						{
							int callback_subpop_id = callback->subpopulation_id_;
							
							if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
								subpop_modify_child_callbacks.push_back(callback);
						}
						
						// Update fitness values, using the callbacks
						population_.EvolveSubpopulation(subpop_pair.first, chromosome_, generation_, subpop_mate_choice_callbacks, subpop_modify_child_callbacks);
					}
					
					DeregisterScheduledScriptBlocks();
					
					//
					// Stage 3: swap generations
					//
					population_.SwapGenerations();
					
					// advance the generation counter as soon as the generation is done
					generation_++;
				}
				
				return (generation_ < time_start_ + time_duration_);
#ifdef SLIMGUI
			}
			catch (std::runtime_error err)
			{
				simulationValid = false;
				
				return false;
			}
#endif
		}
#ifdef SLIMGUI
	}
#endif
	
	return false;
}

void SLiMSim::RunToEnd(void)
{
	while ((generation_ == 0) || (generation_ < time_start_ + time_duration_))		// at generation 0 we don't know start or duration yet
		RunOneGeneration();
}


//
//	SLiMscript support
//
#pragma mark SLiMscript support

void SLiMSim::GenerateCachedSymbolTableEntry(void)
{
	self_symbol_ = new SymbolTableEntry("sim", (new ScriptValue_Object(this))->SetExternallyOwned(true)->SetInSymbolTable(true));
}

// a static member function is used as a funnel, so that we can get a pointer to function for it
ScriptValue *SLiMSim::StaticFunctionDelegationFunnel(void *delegate, std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter)
{
	SLiMSim *sim = static_cast<SLiMSim *>(delegate);
	
	return sim->FunctionDelegationFunnel(p_function_name, p_arguments, p_interpreter);
}

// the static member function calls this member function; now we're completely in context and can execute the function
ScriptValue *SLiMSim::FunctionDelegationFunnel(std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter)
{
#pragma unused(p_interpreter)
	
	int num_arguments = (int)p_arguments.size();
	ScriptValue *arg0_value = ((num_arguments >= 1) ? p_arguments[0] : nullptr);
	ScriptValue *arg1_value = ((num_arguments >= 2) ? p_arguments[1] : nullptr);
	ScriptValue *arg2_value = ((num_arguments >= 3) ? p_arguments[2] : nullptr);
	
	// we only define zero-generation functions; so we must be in generation zero
	if (generation_ != 0)
		SLIM_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): FunctionDelegationFunnel() called outside of generation 0." << slim_terminate();
	
	
	//
	//	*********************	addGenomicElement0(integer$ genomicElementTypeID, integer$ start, integer$ end)
	//
	#pragma mark addGenomicElement0()
	
	if (p_function_name.compare("addGenomicElement0") == 0)
	{
		int genomic_element_type = (int)arg0_value->IntAtIndex(0);
		int start_position = (int)arg1_value->IntAtIndex(0);		// used to have a -1; switched to zero-based
		int end_position = (int)arg2_value->IntAtIndex(0);			// used to have a -1; switched to zero-based
		auto found_getype_pair = genomic_element_types_.find(genomic_element_type);
		
		if (found_getype_pair == genomic_element_types_.end())
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addGenomicElement0() genomic element type g" << genomic_element_type << " not defined" << slim_terminate();
		
		// FIXME bounds-check start and end
		
		GenomicElementType *genomic_element_type_ptr = found_getype_pair->second;
		GenomicElement new_genomic_element(genomic_element_type_ptr, start_position, end_position);
		
		bool old_log = GenomicElement::LogGenomicElementCopyAndAssign(false);
		chromosome_.push_back(new_genomic_element);
		GenomicElement::LogGenomicElementCopyAndAssign(old_log);
		
		chromosome_changed_ = true;
		
		if (DEBUG_INPUT)
			SLIM_OUTSTREAM << "addGenomicElement0(" << genomic_element_type << ", " << start_position << ", " << end_position << ");" << endl;
		
		num_genomic_elements++;
	}
	
	
	//
	//	*********************	addGenomicElementType0(integer$ id, integer mutationTypeIDs, numeric proportions)
	//
	#pragma mark addGenomicElementType0()
	
	else if (p_function_name.compare("addGenomicElementType0") == 0)
	{
		int map_identifier = (int)arg0_value->IntAtIndex(0);
		
		if (map_identifier < 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addGenomicElementType0() requires id >= 0." << slim_terminate();
		if (genomic_element_types_.count(map_identifier) > 0) 
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addGenomicElementType0() genomic element type g" << map_identifier << " already defined." << slim_terminate();
		
		int mut_type_id_count = arg1_value->Count();
		int proportion_count = arg2_value->Count();
		
		if ((mut_type_id_count != proportion_count) || (mut_type_id_count == 0))
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addGenomicElementType0() requires the sizes of mutationTypeIDs and proportions to be equal and nonzero." << slim_terminate();
		
		std::vector<MutationType*> mutation_types;
		std::vector<double> mutation_fractions;
		
		for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
		{ 
			int mutation_type_id = (int)arg1_value->IntAtIndex(mut_type_index);
			double proportion = arg2_value->FloatAtIndex(mut_type_index);
			
			if (proportion <= 0)
				SLIM_TERMINATION << "ERROR (RunZeroGeneration): addGenomicElementType0() proportions must be greater than zero." << slim_terminate();
			
			auto found_muttype_pair = mutation_types_.find(mutation_type_id);
			
			if (found_muttype_pair == mutation_types_.end())
				SLIM_TERMINATION << "ERROR (RunZeroGeneration): addGenomicElementType0() mutation type m" << mutation_type_id << " not defined" << slim_terminate();
			
			MutationType *mutation_type_ptr = found_muttype_pair->second;
			
			mutation_types.push_back(mutation_type_ptr);
			mutation_fractions.push_back(proportion);
		}
		
		GenomicElementType *new_genomic_element_type = new GenomicElementType(map_identifier, mutation_types, mutation_fractions);
		genomic_element_types_.insert(std::pair<const int,GenomicElementType*>(map_identifier, new_genomic_element_type));
		genomic_element_types_changed_ = true;
		
		if (DEBUG_INPUT)
		{
			SLIM_OUTSTREAM << "addGenomicElementType0(" << map_identifier;
			
			for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
				SLIM_OUTSTREAM << ", " << arg1_value->IntAtIndex(mut_type_index) << ", " << arg2_value->FloatAtIndex(mut_type_index);
			
			SLIM_OUTSTREAM << ");" << endl;
		}
		
		num_genomic_element_types++;
	}
	
	
	//
	//	*********************	addMutationType0(integer$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	//
	#pragma mark addMutationType0()
	
	else if (p_function_name.compare("addMutationType0") == 0)
	{
		int map_identifier = (int)arg0_value->IntAtIndex(0);
		double dominance_coeff = arg1_value->FloatAtIndex(0);
		string dfe_type_string = arg2_value->StringAtIndex(0);
		int expected_dfe_param_count = 0;
		std::vector<double> dfe_parameters;
		
		if (map_identifier < 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addMutationType0() requires id >= 0." << slim_terminate();
		if (mutation_types_.count(map_identifier) > 0) 
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addMutationType0() mutation type m" << map_identifier << " already defined" << slim_terminate();
		
		if (dfe_type_string.compare("f") == 0)
			expected_dfe_param_count = 1;
		else if (dfe_type_string.compare("g") == 0)
			expected_dfe_param_count = 2;
		else if (dfe_type_string.compare("e") == 0)
			expected_dfe_param_count = 1;
		else
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addMutationType0() distributionType \"" << dfe_type_string << "must be \"f\", \"g\", or \"e\"." << slim_terminate();
		
		char dfe_type = dfe_type_string[0];
		
		if (num_arguments != 3 + expected_dfe_param_count)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addMutationType0() distributionType \"" << dfe_type << "\" requires exactly " << expected_dfe_param_count << " DFE parameter" << (expected_dfe_param_count == 1 ? "" : "s") << "." << slim_terminate();
		
		for (int dfe_param_index = 0; dfe_param_index < expected_dfe_param_count; ++dfe_param_index)
			dfe_parameters.push_back(p_arguments[3 + dfe_param_index]->FloatAtIndex(0));
		
#ifdef SLIMGUI
		// each new mutation type gets a unique zero-based index, used by SLiMgui to categorize mutations
		MutationType *new_mutation_type = new MutationType(map_identifier, dominance_coeff, dfe_type, dfe_parameters, num_mutation_types);
#else
		MutationType *new_mutation_type = new MutationType(map_identifier, dominance_coeff, dfe_type, dfe_parameters);
#endif
		
		mutation_types_.insert(std::pair<const int,MutationType*>(map_identifier, new_mutation_type));
		mutation_types_changed_ = true;
		
		if (DEBUG_INPUT)
		{
			SLIM_OUTSTREAM << "addMutationType0(" << map_identifier << ", " << dominance_coeff << ", \"" << dfe_type << "\"";
			
			for (double dfe_param : dfe_parameters)
				SLIM_OUTSTREAM << ", " << dfe_param;
			
			SLIM_OUTSTREAM << ");" << endl;
		}
		
		num_mutation_types++;
	}
	
	
	//
	//	*********************	addRecombinationIntervals0(integer ends, numeric rates)
	//
	#pragma mark addRecombinationIntervals0()
	
	else if (p_function_name.compare("addRecombinationIntervals0") == 0)
	{
		int end_count = arg0_value->Count();
		int rate_count = arg1_value->Count();
		
		if ((end_count != rate_count) || (end_count == 0))
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): addRecombinationIntervals0() requires the sizes of ends and rates to be equal and nonzero." << slim_terminate();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			int recombination_end_position = (int)arg0_value->IntAtIndex(interval_index);	// used to have a -1; switched to zero-based
			double recombination_rate = arg1_value->FloatAtIndex(interval_index);
			
			// FIXME bounds-check position and rate
			
			chromosome_.recombination_end_positions_.push_back(recombination_end_position);
			chromosome_.recombination_rates_.push_back(recombination_rate);
		}
		
		chromosome_changed_ = true;
		
		if (DEBUG_INPUT)
		{
			SLIM_OUTSTREAM << "addRecombinationIntervals0(";
			
			if (end_count > 1)
				SLIM_OUTSTREAM << "c(";
			for (int interval_index = 0; interval_index < end_count; ++interval_index)
				SLIM_OUTSTREAM << (interval_index == 0 ? "" : ", ") << (int)arg0_value->IntAtIndex(interval_index);
			if (end_count > 1)
				SLIM_OUTSTREAM << ")";
			
			SLIM_OUTSTREAM << ", ";
			
			if (end_count > 1)
				SLIM_OUTSTREAM << "c(";
			for (int interval_index = 0; interval_index < end_count; ++interval_index)
				SLIM_OUTSTREAM << (interval_index == 0 ? "" : ", ") << arg1_value->FloatAtIndex(interval_index);
			if (end_count > 1)
				SLIM_OUTSTREAM << ")";
			
			SLIM_OUTSTREAM << ");" << endl;
		}
		
		num_recombination_rates++;
	}
	
	
	//
	//	*********************	setGeneConversion0(numeric$ conversionFraction, numeric$ meanLength)
	//
	#pragma mark setGeneConversion0()
	
	else if (p_function_name.compare("setGeneConversion0") == 0)
	{
		if (num_gene_conversions > 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setGeneConversion0() may be called only once." << slim_terminate();
		
		double gene_conversion_fraction = arg0_value->FloatAtIndex(0);
		double gene_conversion_avg_length = arg1_value->FloatAtIndex(0);
		
		if ((gene_conversion_fraction < 0.0) || (gene_conversion_fraction > 1.0))
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setGeneConversion0() conversionFraction must be between 0.0 and 1.0 (inclusive)." << slim_terminate();
		if (gene_conversion_avg_length <= 0.0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setGeneConversion0() meanLength must be greater than 0.0." << slim_terminate();
		
		chromosome_.gene_conversion_fraction_ = gene_conversion_fraction;
		chromosome_.gene_conversion_avg_length_ = gene_conversion_avg_length;
		
		if (DEBUG_INPUT)
			SLIM_OUTSTREAM << "setGeneConversion0(" << gene_conversion_fraction << ", " << gene_conversion_avg_length << ");" << endl;
		
		num_gene_conversions++;
	}
	
	
	//
	//	*********************	setGenerationRange0(integer$ duration, [integer$ startGeneration])
	//
	#pragma mark setGenerationRange0()
	
	else if (p_function_name.compare("setGenerationRange0") == 0)
	{
		if (num_generations > 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setGenerationRange0() may be called only once." << slim_terminate();
		
		int duration = (int)arg0_value->IntAtIndex(0);
		int start = (num_arguments == 2 ? (int)arg1_value->IntAtIndex(0) : 1);
		
		if (duration <= 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setGenerationRange0() requires duration greater than 0." << slim_terminate();
		if (start <= 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setGenerationRange0() requires startGeneration greater than 0." << slim_terminate();
		
		time_duration_ = duration;
		time_start_ = start;
		
		if (DEBUG_INPUT)
			SLIM_OUTSTREAM << "setGenerationRange0(" << duration << ", " << start << ");" << endl;
		
		num_generations++;
	}
	
	
	//
	//	*********************	setMutationRate0(numeric$ rate)
	//
#pragma mark setMutationRate0()
	
	else if (p_function_name.compare("setMutationRate0") == 0)
	{
		if (num_mutation_rates > 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setMutationRate0() may be called only once." << slim_terminate();
		
		double rate = arg0_value->FloatAtIndex(0);
		
		if (rate < 0.0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setMutationRate0() requires rate >= 0." << slim_terminate();
		
		chromosome_.overall_mutation_rate_ = rate;
		
		if (DEBUG_INPUT)
			SLIM_OUTSTREAM << "setMutationRate0(" << chromosome_.overall_mutation_rate_ << ");" << endl;
		
		num_mutation_rates++;
	}
	
	
	//
	//	*********************	setRandomSeed0(integer$ seed)
	//
#pragma mark setRandomSeed0()
	
	else if (p_function_name.compare("setRandomSeed0") == 0)
	{
		if (rng_seed_supplied_to_constructor_)
		{
			if (DEBUG_INPUT)
				SLIM_OUTSTREAM << "setRandomSeed0(" << rng_seed_ << ");\t\t// override by -seed option" << endl;
		}
		else
		{
			rng_seed_ = (int)arg0_value->IntAtIndex(0);
			InitializeRNGFromSeed(rng_seed_);
			
			if (DEBUG_INPUT)
				SLIM_OUTSTREAM << "setRandomSeed0(" << rng_seed_ << ");" << endl;
		}
	}
	
	
	//
	//	*********************	setSexEnabled0(string$ chromosomeType, [numeric$ xDominanceCoeff])
	//
	#pragma mark setSexEnabled0()
	
	else if (p_function_name.compare("setSexEnabled0") == 0)
	{
		if (num_sex_declarations > 0)
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setSexEnabled0() may be called only once." << slim_terminate();
		
		string chromosome_type = arg0_value->StringAtIndex(0);
		
		if (chromosome_type.compare("A") == 0)
			modeled_chromosome_type_ = GenomeType::kAutosome;
		else if (chromosome_type.compare("X") == 0)
			modeled_chromosome_type_ = GenomeType::kXChromosome;
		else if (chromosome_type.compare("Y") == 0)
			modeled_chromosome_type_ = GenomeType::kYChromosome;
		else
			SLIM_TERMINATION << "ERROR (RunZeroGeneration): setSexEnabled0() requires a chromosomeType of \"A\", \"X\", or \"Y\"." << slim_terminate();
		
		if (num_arguments == 2)
		{
			if (modeled_chromosome_type_ == GenomeType::kXChromosome)
				x_chromosome_dominance_coeff_ = arg1_value->FloatAtIndex(0);
			else
				SLIM_TERMINATION << "ERROR (RunZeroGeneration): xDominanceCoeff may be supplied to setSexEnabled0() only for chromosomeType \"X\"." << slim_terminate();
		}
		
		if (DEBUG_INPUT)
		{
			SLIM_OUTSTREAM << "setSexEnabled0(\"" << chromosome_type << "\"";
			
			if (num_arguments == 2)
				SLIM_OUTSTREAM << ", " << x_chromosome_dominance_coeff_;
			
			SLIM_OUTSTREAM << ");" << endl;
		}
		
		sex_enabled_ = true;
		num_sex_declarations++;
	}
	
	// the zero-generation functions all return invisible NULL
	return ScriptValue_NULL::ScriptValue_NULL_Invisible();
}

std::vector<FunctionSignature*> *SLiMSim::InjectedFunctionSignatures(void)
{
	if (generation_ == 0)
	{
		// Allocate our own FunctionSignature objects; they cannot be statically allocated since they point to us
		if (!sim_0_signatures.size())
		{
			sim_0_signatures.push_back((new FunctionSignature("addGenomicElement0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S()->AddInt_S()->AddInt_S());
			sim_0_signatures.push_back((new FunctionSignature("addGenomicElementType0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S()->AddInt()->AddNumeric());
			sim_0_signatures.push_back((new FunctionSignature("addMutationType0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S()->AddNumeric_S()->AddString_S()->AddEllipsis());
			sim_0_signatures.push_back((new FunctionSignature("addRecombinationIntervals0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt()->AddNumeric());
			sim_0_signatures.push_back((new FunctionSignature("setGeneConversion0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddNumeric_S()->AddNumeric_S());
			sim_0_signatures.push_back((new FunctionSignature("setGenerationRange0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S()->AddInt_OS());
			sim_0_signatures.push_back((new FunctionSignature("setMutationRate0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddNumeric_S());
			sim_0_signatures.push_back((new FunctionSignature("setRandomSeed0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S());
			sim_0_signatures.push_back((new FunctionSignature("setSexEnabled0", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddString_S()->AddNumeric_OS());
		}
		
		return &sim_0_signatures;
	}
	
	return nullptr;
}

void SLiMSim::InjectIntoInterpreter(ScriptInterpreter &p_interpreter, SLiMScriptBlock *p_script_block)
{
	SymbolTable &global_symbols = p_interpreter.BorrowSymbolTable();
	
	// A constant for reference to the SLiMScriptBlock
	if (p_script_block)
		global_symbols.ReplaceConstantSymbolEntry(p_script_block->CachedSymbolTableEntry());
	
	// Add signatures for functions we define – zero-generation functions only, right now
	if (generation_ == 0)
	{
		std::vector<FunctionSignature*> *signatures = InjectedFunctionSignatures();
		
		if (signatures)
		{
			// construct a new map based on the built-in map, add our functions, and register it, which gives the pointer to the interpreter
			// this is slow, but it doesn't matter; if we start adding functions outside of zero-gen, this will need to be revisited
			FunctionMap *derived_function_map = new FunctionMap(*ScriptInterpreter::BuiltInFunctionMap());
			
			for (FunctionSignature *signature : *signatures)
				derived_function_map->insert(FunctionMapPair(signature->function_name_, signature));
			
			p_interpreter.RegisterFunctionMap(derived_function_map);
		}
	}
	
	// Inject for generations > 0 : no zero-generation functions, but global symbols
	if (generation_ != 0)
	{
		// A constant for reference to the simulation
		global_symbols.ReplaceConstantSymbolEntry(CachedSymbolTableEntry());
		
		// Add constants for our genomic element types, like g1, g2, ...
		for (auto getype_pair : genomic_element_types_)
			global_symbols.ReplaceConstantSymbolEntry(getype_pair.second->CachedSymbolTableEntry());
		
		// Add constants for our mutation types
		for (auto mut_type_pair : mutation_types_)
			global_symbols.ReplaceConstantSymbolEntry(mut_type_pair.second->CachedSymbolTableEntry());
		
		// Add constants for our subpopulations
		for (auto pop_pair : population_)
			global_symbols.ReplaceConstantSymbolEntry(pop_pair.second->CachedSymbolTableEntry());
		
		// Add constants for our scripts
		for (SLiMScriptBlock *script_block : script_blocks_)
		{
			int id = script_block->block_id_;
			
			if (id != -1)	// add symbols only for non-anonymous blocks
				global_symbols.ReplaceConstantSymbolEntry(script_block->CachedScriptBlockSymbolTableEntry());
		}
	}
}

std::string SLiMSim::ElementType(void) const
{
	return "SLiMSim";
}

std::vector<std::string> SLiMSim::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("chromosome");			// chromosome_
	constants.push_back("chromosomeType");		// modeled_chromosome_type_
	constants.push_back("genomicElementTypes");	// genomic_element_types_
	constants.push_back("mutations");			// population_.mutation_registry_
	constants.push_back("mutationTypes");		// mutation_types_
	constants.push_back("scriptBlocks");		// script_blocks_
	constants.push_back("sexEnabled");			// sex_enabled_
	constants.push_back("start");				// time_start_
	constants.push_back("subpopulations");		// population_
	constants.push_back("substitutions");		// population_.substitutions_
	
	return constants;
}

std::vector<std::string> SLiMSim::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back("dominanceCoeffX");		// x_chromosome_dominance_coeff_; settable only when we're modeling sex chromosomes
	variables.push_back("duration");			// time_duration_
	variables.push_back("generation");			// generation_
	variables.push_back("randomSeed");			// rng_seed_
	
	return variables;
}

ScriptValue *SLiMSim::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("chromosome") == 0)
		return new ScriptValue_Object(&chromosome_);
	if (p_member_name.compare("chromosomeType") == 0)
	{
		switch (modeled_chromosome_type_)
		{
			case GenomeType::kAutosome:		return new ScriptValue_String("Autosome");
			case GenomeType::kXChromosome:	return new ScriptValue_String("X chromosome");
			case GenomeType::kYChromosome:	return new ScriptValue_String("Y chromosome");
		}
	}
	if (p_member_name.compare("genomicElementTypes") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto ge_type = genomic_element_types_.begin(); ge_type != genomic_element_types_.end(); ++ge_type)
			vec->PushElement(ge_type->second);
		
		return vec;
	}
	if (p_member_name.compare("mutations") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		Genome &mutation_registry = population_.mutation_registry_;
		int mutation_count = mutation_registry.size();
		
		for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
			vec->PushElement(mutation_registry[mut_index]);
		
		return vec;
	}
	if (p_member_name.compare("mutationTypes") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto mutation_type = mutation_types_.begin(); mutation_type != mutation_types_.end(); ++mutation_type)
			vec->PushElement(mutation_type->second);
		
		return vec;
	}
	if (p_member_name.compare("scriptBlocks") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto script_block = script_blocks_.begin(); script_block != script_blocks_.end(); ++script_block)
			vec->PushElement(*script_block);
		
		return vec;
	}
	if (p_member_name.compare("sexEnabled") == 0)
		return new ScriptValue_Logical(sex_enabled_);
	if (p_member_name.compare("start") == 0)
		return new ScriptValue_Int(time_start_);
	if (p_member_name.compare("subpopulations") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto pop = population_.begin(); pop != population_.end(); ++pop)
			vec->PushElement(pop->second);
		
		return vec;
	}
	if (p_member_name.compare("substitutions") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto sub_iter = population_.substitutions_.begin(); sub_iter != population_.substitutions_.end(); ++sub_iter)
			vec->PushElement(*sub_iter);
		
		return vec;
	}
	
	// variables
	if (p_member_name.compare("dominanceCoeffX") == 0)
		return new ScriptValue_Float(x_chromosome_dominance_coeff_);
	if (p_member_name.compare("duration") == 0)
		return new ScriptValue_Int(time_duration_);
	if (p_member_name.compare("generation") == 0)
		return new ScriptValue_Int(generation_);
	if (p_member_name.compare("randomSeed") == 0)
		return new ScriptValue_Int(rng_seed_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void SLiMSim::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	if (p_member_name.compare("generationDuration") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		int64_t value = p_value->IntAtIndex(0);
		RangeCheckValue(__func__, p_member_name, (value > 0) && (value <= 1000000000));
		
		time_duration_ = (int)value;
		return;
	}
	
	if (p_member_name.compare("generation") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		int64_t value = p_value->IntAtIndex(0);
		RangeCheckValue(__func__, p_member_name, (value >= time_start_) && (value <= time_start_ + time_duration_));
		
		generation_ = (int)value;
		return;
	}
	
	if (p_member_name.compare("dominanceCoeffX") == 0)
	{
		if (!sex_enabled_ || (modeled_chromosome_type_ != GenomeType::kXChromosome))
			SLIM_TERMINATION << "ERROR (SLiMSim::SetValueForMember): attempt to set member dominanceCoeffX when not simulating an X chromosome." << slim_terminate();
		
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
		
		double value = p_value->FloatAtIndex(0);
		
		x_chromosome_dominance_coeff_ = value;
		return;
	}
	
	if (p_member_name.compare("randomSeed") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		int64_t value = p_value->IntAtIndex(0);
		
		rng_seed_ = (int)value;
		
		// Reseed; note that rng_seed_supplied_to_constructor_ prevents setRandomSeed0() but does not prevent this, by design
		InitializeRNGFromSeed(rng_seed_);
		
		return;
	}
	
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> SLiMSim::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	methods.push_back("addSubpop");
	methods.push_back("addSubpopSplit");
	methods.push_back("deregisterScriptBlock");
	methods.push_back("mutationFrequencies");
	methods.push_back("outputFixedMutations");
	methods.push_back("outputFull");
	methods.push_back("outputMutations");
	methods.push_back("readFromPopulationFile");
	methods.push_back("registerScriptEvent");
	methods.push_back("registerScriptFitnessCallback");
	methods.push_back("registerScriptMateChoiceCallback");
	methods.push_back("registerScriptModifyChildCallback");
	
	return methods;
}

const FunctionSignature *SLiMSim::SignatureForMethod(std::string const &p_method_name) const
{
	// Signatures are all preallocated, for speed
	static FunctionSignature *addSubpopSig = nullptr;
	static FunctionSignature *addSubpopSplitSig = nullptr;
	static FunctionSignature *deregisterScriptBlockSig = nullptr;
	static FunctionSignature *mutationFrequenciesSig = nullptr;
	static FunctionSignature *outputFixedMutationsSig = nullptr;
	static FunctionSignature *outputFullSig = nullptr;
	static FunctionSignature *outputMutationsSig = nullptr;
	static FunctionSignature *readFromPopulationFileSig = nullptr;
	static FunctionSignature *registerScriptEventSig = nullptr;
	static FunctionSignature *registerScriptFitnessCallbackSig = nullptr;
	static FunctionSignature *registerScriptMateChoiceCallbackSig = nullptr;
	static FunctionSignature *registerScriptModifyChildCallbackSig = nullptr;
	
	if (!addSubpopSig)
	{
		addSubpopSig = (new FunctionSignature("addSubpop", FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddInt_S()->AddInt_S()->AddFloat_OS();
		addSubpopSplitSig = (new FunctionSignature("addSubpopSplit", FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddInt_S()->AddInt_S()->AddObject_S()->AddFloat_OS();
		deregisterScriptBlockSig = (new FunctionSignature("deregisterScriptBlock", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddObject();
		mutationFrequenciesSig = (new FunctionSignature("mutationFrequencies", FunctionIdentifier::kNoFunction, kScriptValueMaskFloat))->SetInstanceMethod()->AddObject()->AddObject_O();
		outputFixedMutationsSig = (new FunctionSignature("outputFixedMutations", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod();
		outputFullSig = (new FunctionSignature("outputFull", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddString_OS();
		outputMutationsSig = (new FunctionSignature("outputMutations", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddObject();
		readFromPopulationFileSig = (new FunctionSignature("readFromPopulationFile", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddString_S();
		registerScriptEventSig = (new FunctionSignature("registerScriptEvent", FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_OS()->AddInt_OS();
		registerScriptFitnessCallbackSig = (new FunctionSignature("registerScriptFitnessCallback", FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_S()->AddInt_OS()->AddInt_OS()->AddInt_OS();
		registerScriptMateChoiceCallbackSig = (new FunctionSignature("registerScriptMateChoiceCallback", FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_OS()->AddInt_OS()->AddInt_OS();
		registerScriptModifyChildCallbackSig = (new FunctionSignature("registerScriptModifyChildCallback", FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_OS()->AddInt_OS()->AddInt_OS();
	}
	
	if (p_method_name.compare("addSubpop") == 0)
		return addSubpopSig;
	else if (p_method_name.compare("addSubpopSplit") == 0)
		return addSubpopSplitSig;
	else if (p_method_name.compare("deregisterScriptBlock") == 0)
		return deregisterScriptBlockSig;
	else if (p_method_name.compare("mutationFrequencies") == 0)
		return mutationFrequenciesSig;
	else if (p_method_name.compare("outputFixedMutations") == 0)
		return outputFixedMutationsSig;
	else if (p_method_name.compare("outputFull") == 0)
		return outputFullSig;
	else if (p_method_name.compare("outputMutations") == 0)
		return outputMutationsSig;
	else if (p_method_name.compare("readFromPopulationFile") == 0)
		return readFromPopulationFileSig;
	else if (p_method_name.compare("registerScriptEvent") == 0)
		return registerScriptEventSig;
	else if (p_method_name.compare("registerScriptFitnessCallback") == 0)
		return registerScriptFitnessCallbackSig;
	else if (p_method_name.compare("registerScriptMateChoiceCallback") == 0)
		return registerScriptMateChoiceCallbackSig;
	else if (p_method_name.compare("registerScriptModifyChildCallback") == 0)
		return registerScriptModifyChildCallbackSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *SLiMSim::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter)
{
	int num_arguments = (int)p_arguments.size();
	ScriptValue *arg0_value = ((num_arguments >= 1) ? p_arguments[0] : nullptr);
	ScriptValue *arg1_value = ((num_arguments >= 2) ? p_arguments[1] : nullptr);
	ScriptValue *arg2_value = ((num_arguments >= 3) ? p_arguments[2] : nullptr);
	ScriptValue *arg3_value = ((num_arguments >= 4) ? p_arguments[3] : nullptr);
	ScriptValue *arg4_value = ((num_arguments >= 5) ? p_arguments[4] : nullptr);
	ScriptValue *arg5_value = ((num_arguments >= 6) ? p_arguments[5] : nullptr);
	
	
	//
	//	*********************	- (object$)addSubpop(integer$ subpopID, integer$ size, [float$ sexRatio])
	//
#pragma mark -addSubpop()
	
	if (p_method_name.compare("addSubpop") == 0)
	{
		int subpop_id = (int)arg0_value->IntAtIndex(0);
		int subpop_size = (int)arg1_value->IntAtIndex(0);
		double sex_ratio = (arg2_value ? arg2_value->FloatAtIndex(0) : 0.5);		// 0.5 is the default whenever sex is enabled and a ratio is not given
		
		// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, sex_ratio);
		
		return new ScriptValue_Object(new_subpop);
	}
	
	
	//
	//	*********************	- (object$)addSubpopSplit(integer$ subpopID, integer$ size, object$ sourceSubpop, [float$ sexRatio])
	//
#pragma mark -addSubpopSplit()
	
	else if (p_method_name.compare("addSubpopSplit") == 0)
	{
		int subpop_id = (int)arg0_value->IntAtIndex(0);
		int subpop_size = (int)arg1_value->IntAtIndex(0);
		Subpopulation *source_subpop = (Subpopulation *)(arg2_value->ElementAtIndex(0));
		double sex_ratio = (arg3_value ? arg3_value->FloatAtIndex(0) : 0.5);		// 0.5 is the default whenever sex is enabled and a ratio is not given
		
		// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
		Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, *source_subpop, subpop_size, sex_ratio);
		
		return new ScriptValue_Object(new_subpop);
	}
	
	
	//
	//	*********************	- (void)deregisterScriptBlock(object scriptBlocks)
	//
#pragma mark -deregisterScriptBlock()
	
	else if (p_method_name.compare("deregisterScriptBlock") == 0)
	{
		int block_count = arg0_value->Count();
		
		// We just schedule the blocks for deregistration; we do not deregister them immediately, because that would leave stale pointers lying around
		for (int block_index = 0; block_index < block_count; ++block_index)
			scheduled_deregistrations_.push_back((SLiMScriptBlock *)(arg0_value->ElementAtIndex(block_index)));
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (float)mutationFrequencies(object subpops, [object mutations])
	//
#pragma mark -mutationFrequencies()
	
	else if (p_method_name.compare("mutationFrequencies") == 0)
	{
		int total_genome_count = 0;
		
		// The code below blows away the reference counts kept by Mutation, which must be valid at the end of each generation for
		// SLiM's internal machinery to work properly, so for simplicity we require that we're in the parental generation.
		if (population_.child_generation_valid)
			SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): mutationFrequencies() may only be called when the parental generation is active (before offspring generation)." << slim_terminate();
		
		// first zero out the refcounts in all registered Mutation objects
		Mutation **registry_iter = population_.mutation_registry_.begin_pointer();
		Mutation **registry_iter_end = population_.mutation_registry_.end_pointer();
		
		for (; registry_iter != registry_iter_end; ++registry_iter)
			(*registry_iter)->reference_count_ = 0;
		
		// figure out which subpopulations we are supposed to tally across
		vector<Subpopulation*> subpops_to_tally;
		
		if (arg0_value->Type() == ScriptValueType::kValueNULL)
		{
			// no requested subpops, so we should loop over all subpops
			for (const std::pair<const int,Subpopulation*> &subpop_pair : population_)
				subpops_to_tally.push_back(subpop_pair.second);
		}
		else
		{
			// requested subpops, so get them
			int requested_subpop_count = arg0_value->Count();
			
			if (requested_subpop_count)
			{
				if (((ScriptValue_Object *)arg0_value)->ElementType().compare("Subpopulation") != 0)
					SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): mutationFrequencies() requires that subpops have object element type Subpopulation." << slim_terminate();
				
				for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
					subpops_to_tally.push_back((Subpopulation *)(arg0_value->ElementAtIndex(requested_subpop_index)));
			}
		}
		
		// then increment the refcounts through all pointers to Mutation in all genomes; the script command may have specified
		// a limited set of mutations, but it would actually make us slower to try to limit our counting to that subset
		for (Subpopulation *subpop : subpops_to_tally)
		{
			int subpop_genome_count = 2 * subpop->parent_subpop_size_;
			std::vector<Genome> &subpop_genomes = subpop->parent_genomes_;
			
			for (int i = 0; i < subpop_genome_count; i++)								// parent genomes
			{
				Genome &genome = subpop_genomes[i];
				
				if (!genome.IsNull())
				{
					Mutation **genome_iter = genome.begin_pointer();
					Mutation **genome_end_iter = genome.end_pointer();
					
					for (; genome_iter != genome_end_iter; ++genome_iter)
						++((*genome_iter)->reference_count_);
					
					total_genome_count++;	// count only non-null genomes to determine fixation
				}
			}
		}
		
		// OK, now construct our result vector from the tallies for just the requested mutations
		ScriptValue_Float *float_result = new ScriptValue_Float();
		double denominator = 1.0 / total_genome_count;
		
		if (arg1_value)
		{
			// a vector of mutations was given, so loop through them and take their tallies
			int arg1_count = arg1_value->Count();
			
			if (arg1_count)
			{
				if (((ScriptValue_Object *)arg1_value)->ElementType().compare("Mutation") != 0)
					SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): mutationFrequencies() requires that mutations has object element type Mutation." << slim_terminate();
				
				for (int value_index = 0; value_index < arg1_count; ++value_index)
					float_result->PushFloat(((Mutation *)(arg1_value->ElementAtIndex(value_index)))->reference_count_ * denominator);
			}
		}
		else
		{
			// no mutation vectoe was given, so return all frequencies from the registry
			for (registry_iter = population_.mutation_registry_.begin_pointer(); registry_iter != registry_iter_end; ++registry_iter)
				float_result->PushFloat((*registry_iter)->reference_count_ * denominator);
		}
		
		return float_result;
	}
	
	
	//
	//	*********************	- (void)outputFixedMutations(void)
	//
#pragma mark -outputFixedMutations()
	
	else if (p_method_name.compare("outputFixedMutations") == 0)
	{
		SLIM_OUTSTREAM << "#OUT: " << generation_ << " F " << endl;
		SLIM_OUTSTREAM << "Mutations:" << endl;
		
		std::vector<Substitution*> &subs = population_.substitutions_;
		
		for (int i = 0; i < subs.size(); i++)
		{
			SLIM_OUTSTREAM << i;				// used to have a +1; switched to zero-based
			subs[i]->print(SLIM_OUTSTREAM);
		}
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (void)outputFull([string$ filePath])
	//
#pragma mark -outputFull()
	
	else if (p_method_name.compare("outputFull") == 0)
	{
		if (num_arguments == 0)
		{
			SLIM_OUTSTREAM << "#OUT: " << generation_ << " A" << endl;
			population_.PrintAll(SLIM_OUTSTREAM);
		}
		else if (num_arguments == 1)
		{
			string outfile_path = arg0_value->StringAtIndex(0);
			std::ofstream outfile;
			outfile.open(outfile_path.c_str());
			
			if (outfile.is_open())
			{
				// We no longer have input parameters to print; possibly this should print all the zero-generation functions called...
//				const std::vector<std::string> &input_parameters = p_sim.InputParameters();
//				
//				for (int i = 0; i < input_parameters.size(); i++)
//					outfile << input_parameters[i] << endl;
				
				outfile << "#OUT: " << generation_ << " A " << outfile_path << endl;
				population_.PrintAll(outfile);
				outfile.close(); 
			}
			else
			{
				SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): outputFull() could not open "<< outfile_path << endl << slim_terminate();
			}
		}
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (void)outputMutations(object mutations)
	//
#pragma mark -outputMutations()
	
	else if (p_method_name.compare("outputMutations") == 0)
	{
		// Extract all of the Mutation objects in mutations; would be nice if there was a simpler way to do this
		ScriptValue_Object *mutations_object = (ScriptValue_Object *)arg0_value;
		int mutations_count = mutations_object->Count();
		std::vector<Mutation*> mutations;
		
		for (int mutation_index = 0; mutation_index < mutations_count; mutation_index++)
			mutations.push_back((Mutation *)(mutations_object->ElementAtIndex(mutation_index)));
		
		// find all polymorphism of the mutations that are to be tracked
		if (mutations_count > 0)
		{
			for (const std::pair<const int,Subpopulation*> &subpop_pair : population_)
			{
				multimap<const int,Polymorphism> polymorphisms;
				
				for (int i = 0; i < 2 * subpop_pair.second->parent_subpop_size_; i++)				// go through all parents
				{
					for (int k = 0; k < subpop_pair.second->parent_genomes_[i].size(); k++)			// go through all mutations
					{
						Mutation *scan_mutation = subpop_pair.second->child_genomes_[i][k];
						
						// do a linear search for each mutation, ouch; but this is output code, so it doesn't need to be fast, probably.
						// if speed is a problem here, we could provide special versions of this function for common tasks like
						// printing all the mutations that match a given mutation type... or maybe instead of a vector of Mutation*
						// we could use a set or an ordered map or some such, with a faster search time...  FIXME
						if (std::find(mutations.begin(), mutations.end(), scan_mutation) != mutations.end())
							AddMutationToPolymorphismMap(&polymorphisms, *scan_mutation);
					}
				}
				
				// output the frequencies of these mutations in each subpopulation
				// FIXME the format here comes from the old tracked mutations code; what format do we actually want?
				for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
				{ 
					SLIM_OUTSTREAM << "#OUT: " << generation_ << " T p" << subpop_pair.first << " ";
					polymorphism_pair.second.print(SLIM_OUTSTREAM, polymorphism_pair.first, false /* no id */);
				}
			}
		}
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (void)readFromPopulationFile(string$ filePath)
	//
#pragma mark -readFromPopulationFile()
	
	else if (p_method_name.compare("readFromPopulationFile") == 0)
	{
		string file_path = arg0_value->StringAtIndex(0);
		
		InitializePopulationFromFile(file_path.c_str());
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (object$)registerScriptEvent(integer$ id, string$ source, [integer$ start], [integer$ end])
	//
#pragma mark -registerScriptEvent()
	
	else if (p_method_name.compare("registerScriptEvent") == 0)
	{
		int script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
		string script_string = arg1_value->StringAtIndex(0);
		int start_generation = (arg2_value ? (int)arg2_value->IntAtIndex(0) : 1);
		int end_generation = (arg3_value ? (int)arg3_value->IntAtIndex(0) : INT_MAX);
		
		if (arg0_value->Type() != ScriptValueType::kValueNULL)
		{
			script_id = (int)arg0_value->IntAtIndex(0);
			
			if (script_id < -1)
				SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptEvent() requires an id >= 0 (or -1, or NULL)." << endl << slim_terminate();
		}
		
		if ((start_generation < 0) || (end_generation < 0) || (start_generation > end_generation))
			SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires start <= end, and both values must be >= 0." << endl << slim_terminate();
		
		SLiMScriptBlock *script_block = new SLiMScriptBlock(script_id, script_string, SLiMScriptBlockType::SLiMScriptEvent, start_generation, end_generation);
		
		script_blocks_.push_back(script_block);		// takes ownership form us
		scripts_changed_ = true;
		
		return new ScriptValue_Object(script_block);
	}
	
	
	//
	//	*********************	- (object$)registerScriptFitnessCallback(integer$ id, string$ source, integer$ mutTypeID, [integer$ subpopID], [integer$ start], [integer$ end])
	//
#pragma mark -registerScriptFitnessCallback()
	
	else if (p_method_name.compare("registerScriptFitnessCallback") == 0)
	{
		int script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
		string script_string = arg1_value->StringAtIndex(0);
		int mut_type_id = (int)arg2_value->IntAtIndex(0);
		int subpop_id = -1;
		int start_generation = (arg4_value ? (int)arg4_value->IntAtIndex(0) : 1);
		int end_generation = (arg5_value ? (int)arg5_value->IntAtIndex(0) : INT_MAX);
		
		if (arg0_value->Type() != ScriptValueType::kValueNULL)
		{
			script_id = (int)arg0_value->IntAtIndex(0);
			
			if (script_id < -1)
				SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an id >= 0 (or -1, or NULL)." << endl << slim_terminate();
		}
		
		if (mut_type_id < 0)
			SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires a mutTypeID >= 0." << endl << slim_terminate();
		
		if (arg3_value && (arg3_value->Type() != ScriptValueType::kValueNULL))
		{
			subpop_id = (int)arg3_value->IntAtIndex(0);
			
			if (subpop_id < -1)
				SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an subpopID >= 0 (or -1, or NULL)." << endl << slim_terminate();
		}
		
		if ((start_generation < 0) || (end_generation < 0) || (start_generation > end_generation))
			SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires start <= end, and both values must be >= 0." << endl << slim_terminate();
		
		SLiMScriptBlock *script_block = new SLiMScriptBlock(script_id, script_string, SLiMScriptBlockType::SLiMScriptFitnessCallback, start_generation, end_generation);
		
		script_block->mutation_type_id_ = mut_type_id;
		script_block->subpopulation_id_ = subpop_id;
		
		script_blocks_.push_back(script_block);		// takes ownership form us
		scripts_changed_ = true;
		
		return new ScriptValue_Object(script_block);
	}
	
	
	//
	//	*********************	- (object$)registerScriptMateChoiceCallback(integer$ id, string$ source, [integer$ subpopID], [integer$ start], [integer$ end])
	//	*********************	- (object$)registerScriptModifyChildCallback(integer$ id, string$ source, [integer$ subpopID], [integer$ start], [integer$ end])
	//
#pragma mark -registerScriptMateChoiceCallback()
#pragma mark -registerScriptModifyChildCallback()
	
	else if ((p_method_name.compare("registerScriptMateChoiceCallback") == 0) || (p_method_name.compare("registerScriptModifyChildCallback") == 0))
	{
		int script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
		string script_string = arg1_value->StringAtIndex(0);
		int subpop_id = -1;
		int start_generation = (arg3_value ? (int)arg3_value->IntAtIndex(0) : 1);
		int end_generation = (arg4_value ? (int)arg4_value->IntAtIndex(0) : INT_MAX);
		
		if (arg0_value->Type() != ScriptValueType::kValueNULL)
		{
			script_id = (int)arg0_value->IntAtIndex(0);
			
			if (script_id < -1)
				SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an id >= 0 (or -1, or NULL)." << endl << slim_terminate();
		}
		
		if (arg2_value && (arg2_value->Type() != ScriptValueType::kValueNULL))
		{
			subpop_id = (int)arg2_value->IntAtIndex(0);
			
			if (subpop_id < -1)
				SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an subpopID >= 0 (or -1, or NULL)." << endl << slim_terminate();
		}
		
		if ((start_generation < 0) || (end_generation < 0) || (start_generation > end_generation))
			SLIM_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires start <= end, and both values must be >= 0." << endl << slim_terminate();
		
		SLiMScriptBlockType block_type = ((p_method_name.compare("registerScriptMateChoiceCallback") == 0) ? SLiMScriptBlockType::SLiMScriptMateChoiceCallback : SLiMScriptBlockType::SLiMScriptModifyChildCallback);
		SLiMScriptBlock *script_block = new SLiMScriptBlock(script_id, script_string, block_type, start_generation, end_generation);
		
		script_block->subpopulation_id_ = subpop_id;
		
		script_blocks_.push_back(script_block);		// takes ownership form us
		scripts_changed_ = true;
		
		return new ScriptValue_Object(script_block);
	}
	
	
	else
		return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_interpreter);
}
































































