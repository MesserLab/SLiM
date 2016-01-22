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

#include "eidos_test.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"


using std::multimap;
using std::string;
using std::endl;
using std::istream;
using std::istringstream;
using std::ifstream;
using std::vector;


SLiMSim::SLiMSim(std::istream &p_infile) : population_(*this), self_symbol_(gID_sim, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMSim_Class)))
{
	// set up the symbol table we will use for all of our constants
	simulation_constants_ = new EidosSymbolTable(true, gEidosConstantsSymbolTable);
	
	// read all configuration information from the input file
	p_infile.clear();
	p_infile.seekg(0, std::fstream::beg);
	InitializeFromFile(p_infile);
}

SLiMSim::SLiMSim(const char *p_input_file) : population_(*this), self_symbol_(gID_sim, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMSim_Class)))
{
	// set up the symbol table we will use for all of our constants
	simulation_constants_ = new EidosSymbolTable(true, gEidosConstantsSymbolTable);
	
	// Open our file stream
	std::ifstream infile(p_input_file);
	
	if (!infile.is_open())
		EIDOS_TERMINATION << std::endl << "ERROR (SLiMSim::SLiMSim): could not open input file: " << p_input_file << "." << eidos_terminate();
	
	// read all configuration information from the input file
	infile.seekg(0, std::fstream::beg);
	InitializeFromFile(infile);
}

SLiMSim::~SLiMSim(void)
{
	//EIDOS_ERRSTREAM << "SLiMSim::~SLiMSim" << std::endl;
	
	delete simulation_constants_;
	simulation_constants_ = nullptr;
	
	for (auto mutation_type : mutation_types_)
		delete mutation_type.second;
	
	for (auto genomic_element_type : genomic_element_types_)
		delete genomic_element_type.second;
	
	for (auto script_block : script_blocks_)
		delete script_block;
	
	// All the script blocks that refer to the script are now gone
	delete script_;
	
	// We should not have any interpreter instances that still refer to our function map and signatures
	delete sim_0_function_map_;
	
	for (const EidosFunctionSignature *signature : sim_0_signatures_)
		delete signature;
}

void SLiMSim::InitializeRNGFromSeed(unsigned long int *p_override_seed_ptr)
{
	// track the random number seed given, if there is one
	unsigned long int rng_seed = (p_override_seed_ptr ? *p_override_seed_ptr : EidosGenerateSeedFromPIDAndTime());
	
	EidosInitializeRNGFromSeed(rng_seed);
	
	if (DEBUG_INPUT)
		SLIM_OUTSTREAM << "// Initial random seed:\n" << rng_seed << "\n" << endl;
}

void SLiMSim::InitializeFromFile(std::istream &p_infile)
{
	// Reset error position indicators used by SLiMgui
	EidosScript::ClearErrorPosition();
	
	// Read in the file; going through stringstream is fast...
	std::stringstream buffer;
	
	buffer << p_infile.rdbuf();
	
	// Tokenize and parse
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
		
		script_blocks_.emplace_back(new_script_block);
		
		// Define the symbol for the script block, if any
		if (new_script_block->block_id_ != -1)
		{
			EidosSymbolTableEntry &symbol_entry = new_script_block->ScriptBlockSymbolTableEntry();
			
			if (simulation_constants_->ContainsSymbol(symbol_entry.first))
				EIDOS_TERMINATION << "ERROR (SLiMSim::InitializeFromFile): script block symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate(new_script_block->root_node_->children_[0]->token_);
			
			simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
		}

	}
	
	// Reset error position indicators used by SLiMgui
	EidosScript::ClearErrorPosition();
	
	// Zero out error-reporting info so raises elsewhere don't get attributed to this script
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
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
	std::map<int64_t,Mutation*> mutations;
	string line, sub; 
	ifstream infile(p_file);
	
	if (!infile.is_open())
		EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): could not open initialization file." << eidos_terminate();
	
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
		slim_objectid_t subpop_index = SLiMEidosScript::ExtractIDFromStringWithPrefix(sub.c_str(), 'p', nullptr);
		
		iss >> sub;
		int64_t subpop_size_long = EidosInterpreter::IntegerForString(sub, nullptr);
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
		int64_t mutation_id = EidosInterpreter::IntegerForString(sub, nullptr);
		
		iss >> sub;
		slim_objectid_t mutation_type_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(sub.c_str(), 'm', nullptr);
		
		iss >> sub;
		int64_t position_long = EidosInterpreter::IntegerForString(sub, nullptr);
		slim_position_t position = SLiMCastToPositionTypeOrRaise(position_long);
		
		iss >> sub;
		double selection_coeff = EidosInterpreter::FloatForString(sub, nullptr);
		
		iss >> sub;		// dominance coefficient, which is given in the mutation type; we check below that the value read matches the mutation type
		double dominance_coeff = EidosInterpreter::FloatForString(sub, nullptr);
		
		iss >> sub;
		slim_objectid_t subpop_index = SLiMEidosScript::ExtractIDFromStringWithPrefix(sub.c_str(), 'p', nullptr);
		
		iss >> sub;
		int64_t generation_long = EidosInterpreter::IntegerForString(sub, nullptr);
		slim_generation_t generation = SLiMCastToGenerationTypeOrRaise(generation_long);
		
		// look up the mutation type from its index
		auto found_muttype_pair = mutation_types_.find(mutation_type_id);
		
		if (found_muttype_pair == mutation_types_.end()) 
			EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): mutation type m"<< mutation_type_id << " has not been defined." << eidos_terminate();
		
		MutationType *mutation_type_ptr = found_muttype_pair->second;
		
		if (fabs(mutation_type_ptr->dominance_coeff_ - dominance_coeff) > 0.001)	// a reasonable tolerance to allow for I/O roundoff
			EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): mutation type m"<< mutation_type_id << " has dominance coefficient " << mutation_type_ptr->dominance_coeff_ << " that does not match the population file dominance coefficient of " << dominance_coeff << "." << eidos_terminate();
		
		// construct the new mutation
		Mutation *new_mutation = new (gSLiM_Mutation_Pool->AllocateChunk()) Mutation(mutation_type_ptr, position, selection_coeff, subpop_index, generation);
		
		// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
		mutations.insert(std::pair<int64_t,Mutation*>(mutation_id, new_mutation));
		population_.mutation_registry_.emplace_back(new_mutation);
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
		int pos = static_cast<int>(sub.find_first_of(":"));
		const char *subpop_id_string = sub.substr(0, pos).c_str();
		slim_objectid_t subpop_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_string, 'p', nullptr);
		
		auto subpop_pair = population_.find(subpop_id);
		
		if (subpop_pair == population_.end())
			EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): referenced subpopulation p" << subpop_id << " not defined." << eidos_terminate();
		
		Subpopulation &subpop = *subpop_pair->second;
		
		sub.erase(0, pos + 1);	// remove the subpop_id and the colon
		int64_t genome_index_long = EidosInterpreter::IntegerForString(sub, nullptr);
		
		if ((genome_index_long < 0) || (genome_index_long > SLIM_MAX_SUBPOP_SIZE * 2))
			EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): genome index out of permitted range." << eidos_terminate();
		slim_popsize_t genome_index = static_cast<slim_popsize_t>(genome_index_long);	// range-check is above since we need to check against SLIM_MAX_SUBPOP_SIZE * 2
		
		Genome &genome = subpop.parent_genomes_[genome_index];
		
		// Now we might have [A|X|Y] (SLiM 2.0), or we might have the first mutation id - or we might have nothing at all
		if (iss >> sub)
		{
			// check whether this token is a genome type
			if ((sub.compare(gStr_A) == 0) || (sub.compare(gStr_X) == 0) || (sub.compare(gStr_Y) == 0))
			{
				// Let's do a little error-checking against what has already been instantiated for us...
				if ((sub.compare(gStr_A) == 0) && genome.GenomeType() != GenomeType::kAutosome)
					EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): genome is specified as A (autosome), but the instantiated genome does not match." << eidos_terminate();
				if ((sub.compare(gStr_X) == 0) && genome.GenomeType() != GenomeType::kXChromosome)
					EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): genome is specified as X (X-chromosome), but the instantiated genome does not match." << eidos_terminate();
				if ((sub.compare(gStr_Y) == 0) && genome.GenomeType() != GenomeType::kYChromosome)
					EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): genome is specified as Y (Y-chromosome), but the instantiated genome does not match." << eidos_terminate();
				
				if (iss >> sub)
				{
					if (sub == "<null>")
					{
						if (!genome.IsNull())
							EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): genome is specified as null, but the instantiated genome is non-null." << eidos_terminate();
						
						continue;	// this line is over
					}
					else
					{
						if (genome.IsNull())
							EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): genome is specified as non-null, but the instantiated genome is null." << eidos_terminate();
						
						// drop through, and sub will be interpreted as a mutation id below
					}
				}
				else
					continue;
			}
			
			do
			{
				int64_t mutation_id = EidosInterpreter::IntegerForString(sub, nullptr);
				
				auto found_mut_pair = mutations.find(mutation_id);
				
				if (found_mut_pair == mutations.end()) 
					EIDOS_TERMINATION << "ERROR (SLiMSim::InitializePopulationFromFile): mutation " << mutation_id << " has not been defined." << eidos_terminate();
				
				Mutation *mutation = found_mut_pair->second;
				
				genome.emplace_back(mutation);
			}
			while (iss >> sub);
		}
	}
	
	// Now that we have the info on everybody, update fitnesses so that we're ready to run the next generation
	// Note that generation+1 is used; we are computing fitnesses for the next generation
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
	{
		slim_objectid_t subpop_id = subpop_pair.first;
		Subpopulation *subpop = subpop_pair.second;
		std::vector<SLiMEidosBlock*> fitness_callbacks = ScriptBlocksMatching(generation_ + 1, SLiMEidosBlockType::SLiMEidosFitnessCallback, -1, subpop_id);
		
		subpop->UpdateFitness(fitness_callbacks);
	}
}

std::vector<SLiMEidosBlock*> SLiMSim::ScriptBlocksMatching(slim_generation_t p_generation, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_subpopulation_id)
{
	std::vector<SLiMEidosBlock*> matches;
	
	for (SLiMEidosBlock *script_block : script_blocks_)
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
			slim_objectid_t mutation_type_id = script_block->mutation_type_id_;
			
			if ((mutation_type_id != -1) && (p_mutation_type_id != mutation_type_id))
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
	
	return matches;
}

void SLiMSim::DeregisterScheduledScriptBlocks(void)
{
	// If we have blocks scheduled for deregistration, we sweep through and deregister them at the end of each stage of each generation.
	// This happens at a time when script blocks are not executing, so that we're guaranteed not to leave hanging pointers that could
	// cause a crash; it also guarantees that script blocks are applied consistently across each generation stage.  A single block
	// might be scheduled for deregistration more than once, but should only occur in script_blocks_ once, so we have to be careful
	// with our deallocations here; we deallocate a block only when we find it in script_blocks_.
	for (SLiMEidosBlock *block_to_dereg : scheduled_deregistrations_)
	{
		auto script_block_position = std::find(script_blocks_.begin(), script_blocks_.end(), block_to_dereg);
		
		if (script_block_position != script_blocks_.end())
		{
			// Remove the symbol for it first
			if (block_to_dereg->block_id_ != -1)
				simulation_constants_->RemoveConstantForSymbol(block_to_dereg->ScriptBlockSymbolTableEntry().first);
			
			// Then remove it from our script block list and deallocate it
			script_blocks_.erase(script_block_position);
			scripts_changed_ = true;
			delete block_to_dereg;
		}
	}
}

void SLiMSim::RunInitializeCallbacks(void)
{
	// zero out the initialization check counts
	num_mutation_types_ = 0;
	num_mutation_rates_ = 0;
	num_genomic_element_types_ = 0;
	num_genomic_elements_ = 0;
	num_recombination_rates_ = 0;
	num_gene_conversions_ = 0;
	num_sex_declarations_ = 0;
	
	if (DEBUG_INPUT)
		SLIM_OUTSTREAM << "// RunInitializeCallbacks():" << endl;
	
	// execute initialize() callbacks, which should always have a generation of 0 set
	std::vector<SLiMEidosBlock*> init_blocks = ScriptBlocksMatching(0, SLiMEidosBlockType::SLiMEidosInitializeCallback, -1, -1);
	
	for (auto script_block : init_blocks)
	{
		if (script_block->active_)
			population_.ExecuteScript(script_block, generation_, chromosome_);
	}
	
	DeregisterScheduledScriptBlocks();
	
	// check for complete initialization
	if (num_mutation_rates_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): A mutation rate must be supplied in an initialize() callback with initializeMutationRate()." << eidos_terminate();
	
	if (num_mutation_types_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one mutation type must be defined in an initialize() callback with initializeMutationType()." << eidos_terminate();
	
	if (num_genomic_element_types_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one genomic element type must be defined in an initialize() callback with initializeGenomicElementType()." << eidos_terminate();
	
	if (num_genomic_elements_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one genomic element must be defined in an initialize() callback with initializeGenomicElement()." << eidos_terminate();
	
	if (num_recombination_rates_ == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): At least one recombination rate interval must be defined in an initialize() callback with initializeRecombinationRate()." << eidos_terminate();
	
	// figure out our first generation; it is the earliest generation in which an Eidos event is set up to run,
	// since an Eidos event that adds a subpopulation is necessary to get things started
	time_start_ = SLIM_MAX_GENERATION;
	
	for (auto script_block : script_blocks_)
		if ((script_block->type_ == SLiMEidosBlockType::SLiMEidosEvent) && (script_block->start_generation_ < time_start_) && (script_block->start_generation_ > 0))
			time_start_ = script_block->start_generation_;
	
	if (time_start_ == SLIM_MAX_GENERATION)
		EIDOS_TERMINATION << "ERROR (SLiMSim::RunInitializeCallbacks): No Eidos event found to start the simulation." << eidos_terminate();
	
	// emit our start log
	SLIM_OUTSTREAM << "\n// Starting run at generation <start>:\n" << time_start_ << " " << "\n" << std::endl;
	
	// start at the beginning
	generation_ = time_start_;
	
	// set up the "sim" symbol now that initialization is complete
	simulation_constants_->InitializeConstantSymbolEntry(SymbolTableEntry());
	
	// initialize chromosome
	chromosome_.InitializeDraws();
}

slim_generation_t SLiMSim::EstimatedLastGeneration(void)
{
	slim_generation_t last_gen = 1;
	
	// The estimate is derived from the last generation in which an Eidos block is registered.
	// Any block type works, since the simulation could plausibly be stopped within a callback.
	// However, blocks that do not specify an end generation don't count.
	for (auto script_block : script_blocks_)
		if ((script_block->end_generation_ > last_gen) && (script_block->end_generation_ != SLIM_MAX_GENERATION))
			last_gen = script_block->end_generation_;
	
	return last_gen;
}

// This function is called only by the SLiM self-testing machinery.  It has no exception handling; raises will
// blow through to the catch block in the test harness so that they can be handled there.
bool SLiMSim::_RunOneGeneration(void)
{
	// Define the current script around each generation execution, for error reporting
	gEidosCurrentScript = script_;
	gEidosExecutingRuntimeScript = false;
	
	if (generation_ == 0)
	{
		RunInitializeCallbacks();
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return true;
	}
	else
	{
		// ******************************************************************
		//
		// Stage 1: Execute script events for the current generation
		//
		std::vector<SLiMEidosBlock*> blocks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosEvent, -1, -1);
		
		for (auto script_block : blocks)
			if (script_block->active_)
				population_.ExecuteScript(script_block, generation_, chromosome_);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		
		// ******************************************************************
		//
		// Stage 2: Evolve all subpopulations
		//
		std::vector<SLiMEidosBlock*> mate_choice_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosMateChoiceCallback, -1, -1);
		std::vector<SLiMEidosBlock*> modify_child_callbacks = ScriptBlocksMatching(generation_, SLiMEidosBlockType::SLiMEidosModifyChildCallback, -1, -1);
		bool mate_choice_callbacks_present = mate_choice_callbacks.size();
		bool modify_child_callbacks_present = modify_child_callbacks.size();
		bool no_active_callbacks = true;
		
		// if there are no active callbacks, we can pretend there are no callbacks at all
		if (mate_choice_callbacks_present || modify_child_callbacks_present)
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
		}
		
		if (no_active_callbacks)
		{
			for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
				population_.EvolveSubpopulation(*subpop_pair.second, chromosome_, generation_, false, false);
		}
		else
		{
			// cache a list of callbacks registered for each subpop
			for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
			{
				slim_objectid_t subpop_id = subpop_pair.first;
				Subpopulation *subpop = subpop_pair.second;
				
				// Get mateChoice() callbacks that apply to this subpopulation
				for (SLiMEidosBlock *callback : mate_choice_callbacks)
				{
					slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
					
					if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
						subpop->registered_mate_choice_callbacks_.emplace_back(callback);
				}
				
				// Get modifyChild() callbacks that apply to this subpopulation
				for (SLiMEidosBlock *callback : modify_child_callbacks)
				{
					slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
					
					if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
						subpop->registered_modify_child_callbacks_.emplace_back(callback);
				}
			}
			
			// then evolve each subpop
			for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
				population_.EvolveSubpopulation(*subpop_pair.second, chromosome_, generation_, mate_choice_callbacks_present, modify_child_callbacks_present);
			
			// then remove the cached callbacks, for safety and because we'd have to do it eventually anyway
			for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
			{
				Subpopulation *subpop = subpop_pair.second;
				
				subpop->registered_mate_choice_callbacks_.clear();
				subpop->registered_modify_child_callbacks_.clear();
			}
		}
		
		// then switch to the child generation; we don't want to do this until all callbacks have executed for all subpops
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
			subpop_pair.second->child_generation_valid_ = true;
		
		population_.child_generation_valid_ = true;
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		
		// ******************************************************************
		//
		// Stage 3: Swap generations and advance to the next generation
		//
		population_.SwapGenerations();
		
		// advance the generation counter as soon as the generation is done
		cached_value_generation_.reset();
		generation_++;
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		
		// Decide whether the simulation is over.  We need to call EstimatedLastGeneration() every time; we can't
		// cache it, because it can change based upon changes in script registration / deregistration.
		if (sim_declared_finished_)
			return false;
		
		return (generation_ <= EstimatedLastGeneration());
	}
}

// This function is called by both SLiM and SLiMgui to run a generation.  In SLiM, it simply calls _RunOneGeneration(),
// with no exception handling; in that scenario exceptions should not be thrown, since eidos_terminate() will log an
// error and then call exit().  In SLiMgui, eidos_terminate() will raise an exception, and it will be caught right
// here and converted to an "invalid simulation" state (simulationValid == false), which will be noticed by SLiMgui
// and will cause error reporting to occur based upon the error-tracking variables set.
bool SLiMSim::RunOneGeneration(void)
{
#ifdef SLIMGUI
	if (simulationValid)
	{
		try
		{
#endif
			return _RunOneGeneration();
#ifdef SLIMGUI
		}
		catch (std::runtime_error err)
		{
			simulationValid = false;
			
			// In the event of a raise, we clear gEidosCurrentScript, which is not normally part of the error-
			// reporting state, but is used only to inform eidos_terminate() about the current script at the point
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


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

// a static member function is used as a funnel, so that we can get a pointer to function for it
EidosValue_SP SLiMSim::StaticFunctionDelegationFunnel(void *p_delegate, const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	SLiMSim *sim = static_cast<SLiMSim *>(p_delegate);
	
	return sim->FunctionDelegationFunnel(p_function_name, p_arguments, p_argument_count, p_interpreter);
}

// the static member function calls this member function; now we're completely in context and can execute the function
EidosValue_SP SLiMSim::FunctionDelegationFunnel(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused(p_interpreter)
	
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	
	// we only define initialize...() functions; so we must be in an initialize() callback
	if (generation_ != 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): FunctionDelegationFunnel() called outside of initialize time." << eidos_terminate();
	
	
	//
	//	*********************	(void)initializeGenomicElement(io<GenomicElementType>$ genomicElementType, integer$ start, integer$ end)
	//
	#pragma mark initializeGenomicElement()
	
	if (p_function_name.compare(gStr_initializeGenomicElement) == 0)
	{
		GenomicElementType *genomic_element_type_ptr = nullptr;
		slim_position_t start_position = SLiMCastToPositionTypeOrRaise(arg1_value->IntAtIndex(0, nullptr));
		slim_position_t end_position = SLiMCastToPositionTypeOrRaise(arg2_value->IntAtIndex(0, nullptr));
		
		if (arg0_value->Type() == EidosValueType::kValueInt)
		{
			slim_objectid_t genomic_element_type = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
			auto found_getype_pair = genomic_element_types_.find(genomic_element_type);
			
			if (found_getype_pair == genomic_element_types_.end())
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElement() genomic element type g" << genomic_element_type << " not defined." << eidos_terminate();
			
			genomic_element_type_ptr = found_getype_pair->second;
		}
		else
		{
			genomic_element_type_ptr = dynamic_cast<GenomicElementType *>(arg0_value->ObjectElementAtIndex(0, nullptr));
		}
		
		if (end_position < start_position)
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElement() end position " << end_position << " is less than start position " << start_position << "." << eidos_terminate();
		
		GenomicElement new_genomic_element(genomic_element_type_ptr, start_position, end_position);
		
		bool old_log = GenomicElement::LogGenomicElementCopyAndAssign(false);
		chromosome_.emplace_back(new_genomic_element);
		GenomicElement::LogGenomicElementCopyAndAssign(old_log);
		
		chromosome_changed_ = true;
		
		if (DEBUG_INPUT)
			output_stream << "initializeGenomicElement(g" << genomic_element_type_ptr->genomic_element_type_id_ << ", " << start_position << ", " << end_position << ");" << endl;
		
		num_genomic_elements_++;
	}
	
	
	//
	//	*********************	(object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions)
	//
	#pragma mark initializeGenomicElementType()
	
	else if (p_function_name.compare(gStr_initializeGenomicElementType) == 0)
	{
		slim_objectid_t map_identifier = (arg0_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(arg0_value->StringAtIndex(0, nullptr), 'g', nullptr);
		
		if (genomic_element_types_.count(map_identifier) > 0) 
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElementType() genomic element type g" << map_identifier << " already defined." << eidos_terminate();
		
		int mut_type_id_count = arg1_value->Count();
		int proportion_count = arg2_value->Count();
		
		if (mut_type_id_count != proportion_count)
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElementType() requires the sizes of mutationTypes and proportions to be equal." << eidos_terminate();
		
		std::vector<MutationType*> mutation_types;
		std::vector<double> mutation_fractions;
		
		for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
		{
			MutationType *mutation_type_ptr = nullptr;
			double proportion = arg2_value->FloatAtIndex(mut_type_index, nullptr);
			
			if (proportion < 0)		// == 0 is allowed but must be fixed before the simulation executes; see InitializeDraws()
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElementType() proportions must be greater than or equal to zero (" << proportion << " supplied)." << eidos_terminate();
			
			if (arg1_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg1_value->IntAtIndex(mut_type_index, nullptr));
				auto found_muttype_pair = mutation_types_.find(mutation_type_id);
				
				if (found_muttype_pair != mutation_types_.end())
					mutation_type_ptr = found_muttype_pair->second;
				
				if (!mutation_type_ptr)
					EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElementType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
			}
			else
			{
				mutation_type_ptr = dynamic_cast<MutationType *>(arg1_value->ObjectElementAtIndex(mut_type_index, nullptr));
			}
			
			if (std::find(mutation_types.begin(), mutation_types.end(), mutation_type_ptr) != mutation_types.end())
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElementType() mutation type m" << mutation_type_ptr->mutation_type_id_ << " used more than once." << eidos_terminate();
			
			mutation_types.emplace_back(mutation_type_ptr);
			mutation_fractions.emplace_back(proportion);
		}
		
		GenomicElementType *new_genomic_element_type = new GenomicElementType(map_identifier, mutation_types, mutation_fractions);
		genomic_element_types_.insert(std::pair<const slim_objectid_t,GenomicElementType*>(map_identifier, new_genomic_element_type));
		genomic_element_types_changed_ = true;
		
		// define a new Eidos variable to refer to the new genomic element type
		EidosSymbolTableEntry &symbol_entry = new_genomic_element_type->SymbolTableEntry();
		
		if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGenomicElementType() symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate();
		
		simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
		
		if (DEBUG_INPUT)
		{
			output_stream << "initializeGenomicElementType(" << map_identifier;
			
			for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
				output_stream << ", " << mutation_types[mut_type_index]->mutation_type_id_ << ", " << arg2_value->FloatAtIndex(mut_type_index, nullptr);
			
			output_stream << ");" << endl;
		}
		
		num_genomic_element_types_++;
		return symbol_entry.second;
	}
	
	
	//
	//	*********************	(object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	//
	#pragma mark initializeMutationType()
	
	else if (p_function_name.compare(gStr_initializeMutationType) == 0)
	{
		slim_objectid_t map_identifier = (arg0_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(arg0_value->StringAtIndex(0, nullptr), 'm', nullptr);
		double dominance_coeff = arg1_value->FloatAtIndex(0, nullptr);
		string dfe_type_string = arg2_value->StringAtIndex(0, nullptr);
		DFEType dfe_type;
		int expected_dfe_param_count = 0;
		std::vector<double> dfe_parameters;
		
		if (mutation_types_.count(map_identifier) > 0) 
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeMutationType() mutation type m" << map_identifier << " already defined." << eidos_terminate();
		
		if (dfe_type_string.compare(gStr_f) == 0)
		{
			dfe_type = DFEType::kFixed;
			expected_dfe_param_count = 1;
		}
		else if (dfe_type_string.compare(gStr_g) == 0)
		{
			dfe_type = DFEType::kGamma;
			expected_dfe_param_count = 2;
		}
		else if (dfe_type_string.compare(gStr_e) == 0)
		{
			dfe_type = DFEType::kExponential;
			expected_dfe_param_count = 1;
		}
		else
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeMutationType() distributionType \"" << dfe_type_string << "\" must be \"f\", \"g\", or \"e\"." << eidos_terminate();
		
		if (p_argument_count != 3 + expected_dfe_param_count)
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeMutationType() distributionType \"" << dfe_type << "\" requires exactly " << expected_dfe_param_count << " DFE parameter" << (expected_dfe_param_count == 1 ? "" : "s") << "." << eidos_terminate();
		
		for (int dfe_param_index = 0; dfe_param_index < expected_dfe_param_count; ++dfe_param_index)
		{
			EidosValue *dfe_param_value = p_arguments[3 + dfe_param_index].get();
			EidosValueType dfe_param_type = dfe_param_value->Type();
			
			if ((dfe_param_type != EidosValueType::kValueFloat) && (dfe_param_type != EidosValueType::kValueInt))
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeMutationType() requires that DFE parameters be numeric (integer or float)." << eidos_terminate();
			
			dfe_parameters.emplace_back(dfe_param_value->FloatAtIndex(0, nullptr));
			// intentionally no bounds checks for DFE parameters
		}
		
#ifdef SLIMGUI
		// each new mutation type gets a unique zero-based index, used by SLiMgui to categorize mutations
		MutationType *new_mutation_type = new MutationType(map_identifier, dominance_coeff, dfe_type, dfe_parameters, num_mutation_types_);
#else
		MutationType *new_mutation_type = new MutationType(map_identifier, dominance_coeff, dfe_type, dfe_parameters);
#endif
		
		mutation_types_.insert(std::pair<const slim_objectid_t,MutationType*>(map_identifier, new_mutation_type));
		mutation_types_changed_ = true;
		
		// define a new Eidos variable to refer to the new mutation type
		EidosSymbolTableEntry &symbol_entry = new_mutation_type->SymbolTableEntry();
		
		if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeMutationType() symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate();
		
		simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
		
		if (DEBUG_INPUT)
		{
			output_stream << "initializeMutationType(" << map_identifier << ", " << dominance_coeff << ", \"" << dfe_type << "\"";
			
			for (double dfe_param : dfe_parameters)
				output_stream << ", " << dfe_param;
			
			output_stream << ");" << endl;
		}
		
		num_mutation_types_++;
		return symbol_entry.second;
	}
	
	
	//
	//	*********************	(void)initializeRecombinationRate(numeric rates, [integer ends])
	//
	#pragma mark initializeRecombinationRate()
	
	else if (p_function_name.compare(gStr_initializeRecombinationRate) == 0)
	{
		int rate_count = arg0_value->Count();
		
		if (p_argument_count == 1)
		{
			if (rate_count != 1)
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeRecombinationRate() requires rates to be a singleton if ends is not supplied." << eidos_terminate();
			
			double recombination_rate = arg0_value->FloatAtIndex(0, nullptr);
			
			// check values
			if (recombination_rate < 0.0)		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeRecombinationRate() requires rates to be >= 0 (" << recombination_rate << " supplied)." << eidos_terminate();
			
			// then adopt them
			chromosome_.recombination_rates_.clear();
			chromosome_.recombination_end_positions_.clear();
			
			chromosome_.recombination_rates_.emplace_back(recombination_rate);
			//chromosome_.recombination_end_positions_.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
		}
		else if (p_argument_count == 2)
		{
			int end_count = arg1_value->Count();
			
			if ((end_count != rate_count) || (end_count == 0))
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeRecombinationRate() requires ends and rates to be of equal and nonzero size." << eidos_terminate();
			
			// check values
			for (int value_index = 0; value_index < end_count; ++value_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(value_index, nullptr);
				slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(arg1_value->IntAtIndex(value_index, nullptr));
				
				if (value_index > 0)
					if (recombination_end_position <= arg1_value->IntAtIndex(value_index - 1, nullptr))
						EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeRecombinationRate() requires ends to be in strictly ascending order." << eidos_terminate();
				
				if (recombination_rate < 0.0)		// intentionally no upper bound
					EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeRecombinationRate() requires rates to be >= 0 (" << recombination_rate << " supplied)." << eidos_terminate();
			}
			
			// then adopt them
			chromosome_.recombination_rates_.clear();
			chromosome_.recombination_end_positions_.clear();
			
			for (int interval_index = 0; interval_index < end_count; ++interval_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(interval_index, nullptr);
				slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(arg1_value->IntAtIndex(interval_index, nullptr));
				
				chromosome_.recombination_rates_.emplace_back(recombination_rate);
				chromosome_.recombination_end_positions_.emplace_back(recombination_end_position);
			}
		}
		
		chromosome_changed_ = true;
		
		if (DEBUG_INPUT)
		{
			int ratesSize = (int)chromosome_.recombination_rates_.size();
			int endsSize = (int)chromosome_.recombination_end_positions_.size();
			
			output_stream << "initializeRecombinationRate(";
			
			if (ratesSize > 1)
				output_stream << "c(";
			for (int interval_index = 0; interval_index < ratesSize; ++interval_index)
				output_stream << (interval_index == 0 ? "" : ", ") << chromosome_.recombination_rates_[interval_index];
			if (ratesSize > 1)
				output_stream << ")";
			
			if (endsSize > 0)
			{
				output_stream << ", ";
				
				if (endsSize > 1)
					output_stream << "c(";
				for (int interval_index = 0; interval_index < endsSize; ++interval_index)
					output_stream << (interval_index == 0 ? "" : ", ") << chromosome_.recombination_end_positions_[interval_index];
				if (endsSize > 1)
					output_stream << ")";
			}
			
			output_stream << ");" << endl;
		}
		
		num_recombination_rates_++;
	}
	
	
	//
	//	*********************	(void)initializeGeneConversion(numeric$ conversionFraction, numeric$ meanLength)
	//
	#pragma mark initializeGeneConversion()
	
	else if (p_function_name.compare(gStr_initializeGeneConversion) == 0)
	{
		if (num_gene_conversions_ > 0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGeneConversion() may be called only once." << eidos_terminate();
		
		double gene_conversion_fraction = arg0_value->FloatAtIndex(0, nullptr);
		double gene_conversion_avg_length = arg1_value->FloatAtIndex(0, nullptr);
		
		if ((gene_conversion_fraction < 0.0) || (gene_conversion_fraction > 1.0))
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGeneConversion() conversionFraction must be between 0.0 and 1.0 inclusive (" << gene_conversion_fraction << " supplied)." << eidos_terminate();
		if (gene_conversion_avg_length <= 0.0)		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeGeneConversion() meanLength must be greater than 0.0 (" << gene_conversion_avg_length << " supplied)." << eidos_terminate();
		
		chromosome_.gene_conversion_fraction_ = gene_conversion_fraction;
		chromosome_.gene_conversion_avg_length_ = gene_conversion_avg_length;
		
		if (DEBUG_INPUT)
			output_stream << "initializeGeneConversion(" << gene_conversion_fraction << ", " << gene_conversion_avg_length << ");" << endl;
		
		num_gene_conversions_++;
	}
	
	
	//
	//	*********************	(void)initializeMutationRate(numeric$ rate)
	//
#pragma mark initializeMutationRate()
	
	else if (p_function_name.compare(gStr_initializeMutationRate) == 0)
	{
		if (num_mutation_rates_ > 0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeMutationRate() may be called only once." << eidos_terminate();
		
		double rate = arg0_value->FloatAtIndex(0, nullptr);
		
		if (rate < 0.0)		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeMutationRate() requires rate >= 0 (" << rate << " supplied)." << eidos_terminate();
		
		chromosome_.overall_mutation_rate_ = rate;
		
		if (DEBUG_INPUT)
			output_stream << "initializeMutationRate(" << chromosome_.overall_mutation_rate_ << ");" << endl;
		
		num_mutation_rates_++;
	}
	
	
	//
	//	*********************	(void)initializeSex(string$ chromosomeType, [numeric$ xDominanceCoeff])
	//
	#pragma mark initializeSex()
	
	else if (p_function_name.compare(gStr_initializeSex) == 0)
	{
		if (num_sex_declarations_ > 0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeSex() may be called only once." << eidos_terminate();
		
		string chromosome_type = arg0_value->StringAtIndex(0, nullptr);
		
		if (chromosome_type.compare(gStr_A) == 0)
			modeled_chromosome_type_ = GenomeType::kAutosome;
		else if (chromosome_type.compare(gStr_X) == 0)
			modeled_chromosome_type_ = GenomeType::kXChromosome;
		else if (chromosome_type.compare(gStr_Y) == 0)
			modeled_chromosome_type_ = GenomeType::kYChromosome;
		else
			EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeSex() requires a chromosomeType of \"A\", \"X\", or \"Y\" (\"" << chromosome_type << "\" supplied)." << eidos_terminate();
		
		if (p_argument_count == 2)
		{
			if (modeled_chromosome_type_ == GenomeType::kXChromosome)
				x_chromosome_dominance_coeff_ = arg1_value->FloatAtIndex(0, nullptr);		// intentionally no bounds check
			else
				EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): initializeSex() xDominanceCoeff may be supplied only for chromosomeType \"X\"." << eidos_terminate();
		}
		
		if (DEBUG_INPUT)
		{
			output_stream << "initializeSex(\"" << chromosome_type << "\"";
			
			if (p_argument_count == 2)
				output_stream << ", " << x_chromosome_dominance_coeff_;
			
			output_stream << ");" << endl;
		}
		
		sex_enabled_ = true;
		num_sex_declarations_++;
	}
	
	// the initialize...() functions all return invisible NULL
	return gStaticEidosValueNULLInvisible;
}

const std::vector<const EidosFunctionSignature*> *SLiMSim::ZeroGenerationFunctionSignatures(void)
{
	// Allocate our own EidosFunctionSignature objects; they cannot be statically allocated since they point to us
	if (!sim_0_signatures_.size())
	{
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeGenomicElement, EidosFunctionIdentifier::kDelegatedFunction, kEidosValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))
									->AddIntObject_S("genomicElementType", gSLiM_GenomicElementType_Class)->AddInt_S("start")->AddInt_S("end"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeGenomicElementType, EidosFunctionIdentifier::kDelegatedFunction, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_GenomicElementType_Class, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))
									->AddIntString_S("id")->AddIntObject("mutationTypes", gSLiM_MutationType_Class)->AddNumeric("proportions"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeMutationType, EidosFunctionIdentifier::kDelegatedFunction, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))
									->AddIntString_S("id")->AddNumeric_S("dominanceCoeff")->AddString_S("distributionType")->AddEllipsis());
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeRecombinationRate, EidosFunctionIdentifier::kDelegatedFunction, kEidosValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))
									->AddNumeric("rates")->AddInt_O("ends"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeGeneConversion, EidosFunctionIdentifier::kDelegatedFunction, kEidosValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))
									->AddNumeric_S("conversionFraction")->AddNumeric_S("meanLength"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeMutationRate, EidosFunctionIdentifier::kDelegatedFunction, kEidosValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))
									->AddNumeric_S("rate"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSex, EidosFunctionIdentifier::kDelegatedFunction, kEidosValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))
									->AddString_S("chromosomeType")->AddNumeric_OS("xDominanceCoeff"));
	}
	
	return &sim_0_signatures_;
}

const std::vector<const EidosMethodSignature*> *SLiMSim::AllMethodSignatures(void)
{
	static std::vector<const EidosMethodSignature*> *methodSignatures = nullptr;
	
	if (!methodSignatures)
	{
		auto baseMethods =					gEidos_UndefinedClassObject->Methods();
		auto methodsChromosome =			gSLiM_Chromosome_Class->Methods();
		auto methodsGenome =				gSLiM_Genome_Class->Methods();
		auto methodsGenomicElement =		gSLiM_GenomicElement_Class->Methods();
		auto methodsGenomicElementType =	gSLiM_GenomicElementType_Class->Methods();
		auto methodsMutation =				gSLiM_Mutation_Class->Methods();
		auto methodsMutationType =			gSLiM_MutationType_Class->Methods();
		auto methodsSLiMEidosBlock =		gSLiM_SLiMEidosBlock_Class->Methods();
		auto methodsSLiMSim =				gSLiM_SLiMSim_Class->Methods();
		auto methodsSubpopulation =			gSLiM_Subpopulation_Class->Methods();
		auto methodsSubstitution =			gSLiM_Substitution_Class->Methods();
		
		methodSignatures = new std::vector<const EidosMethodSignature*>(*baseMethods);
		
		methodSignatures->insert(methodSignatures->end(), methodsChromosome->begin(), methodsChromosome->end());
		methodSignatures->insert(methodSignatures->end(), methodsGenome->begin(), methodsGenome->end());
		methodSignatures->insert(methodSignatures->end(), methodsGenomicElement->begin(), methodsGenomicElement->end());
		methodSignatures->insert(methodSignatures->end(), methodsGenomicElementType->begin(), methodsGenomicElementType->end());
		methodSignatures->insert(methodSignatures->end(), methodsMutation->begin(), methodsMutation->end());
		methodSignatures->insert(methodSignatures->end(), methodsMutationType->begin(), methodsMutationType->end());
		methodSignatures->insert(methodSignatures->end(), methodsSLiMEidosBlock->begin(), methodsSLiMEidosBlock->end());
		methodSignatures->insert(methodSignatures->end(), methodsSLiMSim->begin(), methodsSLiMSim->end());
		methodSignatures->insert(methodSignatures->end(), methodsSubpopulation->begin(), methodsSubpopulation->end());
		methodSignatures->insert(methodSignatures->end(), methodsSubstitution->begin(), methodsSubstitution->end());
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(methodSignatures->begin(), methodSignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(methodSignatures->begin(), methodSignatures->end());
		methodSignatures->resize(std::distance(methodSignatures->begin(), unique_end_iter));
		
		// print out any signatures that are identical by name
		std::sort(methodSignatures->begin(), methodSignatures->end(), CompareEidosCallSignatures);
		
		const EidosMethodSignature *previous_sig = nullptr;
		
		for (const EidosMethodSignature *sig : *methodSignatures)
		{
			if (previous_sig && (sig->function_name_.compare(previous_sig->function_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the method signatures are identical.
				if ((typeid(*sig) != typeid(*previous_sig)) ||
					(sig->is_class_method != previous_sig->is_class_method) ||
					(sig->function_name_ != previous_sig->function_name_) ||
					(sig->function_id_ != previous_sig->function_id_) ||
					(sig->return_mask_ != previous_sig->return_mask_) ||
					(sig->return_class_ != previous_sig->return_class_) ||
					(sig->arg_masks_ != previous_sig->arg_masks_) ||
					(sig->arg_names_ != previous_sig->arg_names_) ||
					(sig->arg_classes_ != previous_sig->arg_classes_) ||
					(sig->has_optional_args_ != previous_sig->has_optional_args_) ||
					(sig->has_ellipsis_ != previous_sig->has_ellipsis_))
					std::cout << "Duplicate method name with a different signature: " << sig->function_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
		
		// log a full list
		//std::cout << "----------------" << std::endl;
		//for (const EidosMethodSignature *sig : *methodSignatures)
		//	std::cout << sig->function_name_ << " (" << sig << ")" << std::endl;
	}
	
	return methodSignatures;
}

const std::vector<const EidosPropertySignature*> *SLiMSim::AllPropertySignatures(void)
{
	static std::vector<const EidosPropertySignature*> *propertySignatures = nullptr;
	
	if (!propertySignatures)
	{
		auto baseProperties =					gEidos_UndefinedClassObject->Properties();
		auto propertiesChromosome =				gSLiM_Chromosome_Class->Properties();
		auto propertiesGenome =					gSLiM_Genome_Class->Properties();
		auto propertiesGenomicElement =			gSLiM_GenomicElement_Class->Properties();
		auto propertiesGenomicElementType =		gSLiM_GenomicElementType_Class->Properties();
		auto propertiesMutation =				gSLiM_Mutation_Class->Properties();
		auto propertiesMutationType =			gSLiM_MutationType_Class->Properties();
		auto propertiesSLiMEidosBlock =			gSLiM_SLiMEidosBlock_Class->Properties();
		auto propertiesSLiMSim =				gSLiM_SLiMSim_Class->Properties();
		auto propertiesSubpopulation =			gSLiM_Subpopulation_Class->Properties();
		auto propertiesSubstitution =			gSLiM_Substitution_Class->Properties();
		
		propertySignatures = new std::vector<const EidosPropertySignature*>(*baseProperties);
		
		propertySignatures->insert(propertySignatures->end(), propertiesChromosome->begin(), propertiesChromosome->end());
		propertySignatures->insert(propertySignatures->end(), propertiesGenome->begin(), propertiesGenome->end());
		propertySignatures->insert(propertySignatures->end(), propertiesGenomicElement->begin(), propertiesGenomicElement->end());
		propertySignatures->insert(propertySignatures->end(), propertiesGenomicElementType->begin(), propertiesGenomicElementType->end());
		propertySignatures->insert(propertySignatures->end(), propertiesMutation->begin(), propertiesMutation->end());
		propertySignatures->insert(propertySignatures->end(), propertiesMutationType->begin(), propertiesMutationType->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSLiMEidosBlock->begin(), propertiesSLiMEidosBlock->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSLiMSim->begin(), propertiesSLiMSim->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSubpopulation->begin(), propertiesSubpopulation->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSubstitution->begin(), propertiesSubstitution->end());
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(propertySignatures->begin(), propertySignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(propertySignatures->begin(), propertySignatures->end());
		propertySignatures->resize(std::distance(propertySignatures->begin(), unique_end_iter));
		
		// print out any signatures that are identical by name
		std::sort(propertySignatures->begin(), propertySignatures->end(), CompareEidosPropertySignatures);
		
		const EidosPropertySignature *previous_sig = nullptr;
		
		for (const EidosPropertySignature *sig : *propertySignatures)
		{
			if (previous_sig && (sig->property_name_.compare(previous_sig->property_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the property signatures are identical.
				if ((sig->property_id_ != previous_sig->property_id_) ||
					(sig->read_only_ != previous_sig->read_only_) ||
					(sig->value_mask_ != previous_sig->value_mask_) ||
					(sig->value_class_ != previous_sig->value_class_))
					std::cout << "Duplicate property name with different signature: " << sig->property_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return propertySignatures;
}

EidosSymbolTable *SLiMSim::SymbolsFromBaseSymbols(EidosSymbolTable *p_base_symbols)
{
	// Since we keep our own symbol table long-term, this function does not actually re-derive a new table, but just returns the cached table
	if (p_base_symbols != gEidosConstantsSymbolTable)
		EIDOS_TERMINATION << "ERROR (SLiMSim::SymbolsFromBaseSymbols): (internal error) SLiM requires that its parent symbol table be the standard Eidos symbol table." << eidos_terminate();
	
	return simulation_constants_;
}

EidosFunctionMap *SLiMSim::FunctionMapFromBaseMap(EidosFunctionMap *p_base_map)
{
	// Add signatures for functions we define – initialize...() functions only, right now
	if (generation_ == 0)
	{
		if (!sim_0_function_map_)
		{
			const std::vector<const EidosFunctionSignature*> *signatures = ZeroGenerationFunctionSignatures();
			
			// construct a new map based on the base map, add our functions, and return it, which gives the pointer to the interpreter
			// this is slow, but it doesn't matter; if we start adding functions outside of initialize time, this will need to be revisited
			sim_0_function_map_ = new EidosFunctionMap(*p_base_map);
			
			for (const EidosFunctionSignature *signature : *signatures)
				sim_0_function_map_->insert(EidosFunctionMapPair(signature->function_name_, signature));
		}
		
		return sim_0_function_map_;
	}
	
	return p_base_map;
}

const EidosObjectClass *SLiMSim::Class(void) const
{
	return gSLiM_SLiMSim_Class;
}

EidosValue_SP SLiMSim::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_chromosome:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(&chromosome_, gSLiM_Chromosome_Class));
		case gID_chromosomeType:
		{
			switch (modeled_chromosome_type_)
			{
				case GenomeType::kAutosome:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_A));
				case GenomeType::kXChromosome:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_X));
				case GenomeType::kYChromosome:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_Y));
			}
		}
		case gID_genomicElementTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_GenomicElementType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto ge_type = genomic_element_types_.begin(); ge_type != genomic_element_types_.end(); ++ge_type)
				vec->PushObjectElement(ge_type->second);
			
			return result_SP;
		}
		case gID_mutations:
		{
			Genome &mutation_registry = population_.mutation_registry_;
			int mutation_count = mutation_registry.size();
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->Reserve(mutation_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
				vec->PushObjectElement(mutation_registry[mut_index]);
			
			return result_SP;
		}
		case gID_mutationTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_MutationType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto mutation_type = mutation_types_.begin(); mutation_type != mutation_types_.end(); ++mutation_type)
				vec->PushObjectElement(mutation_type->second);
			
			return result_SP;
		}
		case gID_scriptBlocks:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_SLiMEidosBlock_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto script_block = script_blocks_.begin(); script_block != script_blocks_.end(); ++script_block)
				vec->PushObjectElement(*script_block);
			
			return result_SP;
		}
		case gID_sexEnabled:
			return (sex_enabled_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_subpopulations:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Subpopulation_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto pop = population_.begin(); pop != population_.end(); ++pop)
				vec->PushObjectElement(pop->second);
			
			return result_SP;
		}
		case gID_substitutions:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Substitution_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto sub_iter = population_.substitutions_.begin(); sub_iter != population_.substitutions_.end(); ++sub_iter)
				vec->PushObjectElement(*sub_iter);
			
			return result_SP;
		}
			
			// variables
		case gID_dominanceCoeffX:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_chromosome_dominance_coeff_));
		case gID_generation:
		{
			if (!cached_value_generation_)
				cached_value_generation_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(generation_));
			return cached_value_generation_;
		}
		case gID_tag:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void SLiMSim::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_generation:
		{
			int64_t value = p_value.IntAtIndex(0, nullptr);
			
			generation_ = SLiMCastToGenerationTypeOrRaise(value);
			cached_value_generation_.reset();
			
			// FIXME need to handle mutationLossTimes, mutationFixationTimes, fitnessHistory for SLiMgui here
			// FIXME also need to do something with population_.substitutions_, perhaps; remove all subs that occurred on/after the new generation?
			
			return;
		}
			
		case gID_dominanceCoeffX:
		{
			if (!sex_enabled_ || (modeled_chromosome_type_ != GenomeType::kXChromosome))
				EIDOS_TERMINATION << "ERROR (SLiMSim::SetProperty): attempt to set property dominanceCoeffX when not simulating an X chromosome." << eidos_terminate();
			
			double value = p_value.FloatAtIndex(0, nullptr);
			
			x_chromosome_dominance_coeff_ = value;		// intentionally no bounds check
			return;
		}
			
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP SLiMSim::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
	EidosValue *arg3_value = ((p_argument_count >= 4) ? p_arguments[3].get() : nullptr);
	EidosValue *arg4_value = ((p_argument_count >= 5) ? p_arguments[4].get() : nullptr);
	EidosValue *arg5_value = ((p_argument_count >= 6) ? p_arguments[5].get() : nullptr);
	
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	- (object<Subpopulation>)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio])
			//
#pragma mark -addSubpop()
			
		case gID_addSubpop:
		{
			slim_objectid_t subpop_id = (arg0_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(arg0_value->StringAtIndex(0, nullptr), 'p', nullptr);
			slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(arg1_value->IntAtIndex(0, nullptr));
			
			if (arg2_value && !sex_enabled_)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): addSubpop() sex ratio supplied in non-sexual simulation." << eidos_terminate();
			
			double sex_ratio = (arg2_value ? arg2_value->FloatAtIndex(0, nullptr) : 0.5);		// 0.5 is the default whenever sex is enabled and a ratio is not given
			
			// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
			Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, sex_ratio);
			
			// define a new Eidos variable to refer to the new subpopulation
			EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
			
			if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): addSubpop() symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate();
			
			simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
			
			return symbol_entry.second;
		}
			
			
			//
			//	*********************	- (object<Subpopulation>)addSubpopSplit(is$ subpopID, integer$ size, io<Subpopulation>$ sourceSubpop, [float$ sexRatio])
			//
#pragma mark -addSubpopSplit()
			
		case gID_addSubpopSplit:
		{
			slim_objectid_t subpop_id = (arg0_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(arg0_value->StringAtIndex(0, nullptr), 'p', nullptr);
			slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(arg1_value->IntAtIndex(0, nullptr));
			Subpopulation *source_subpop = nullptr;
			
			if (arg2_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t source_subpop_id = SLiMCastToObjectidTypeOrRaise(arg2_value->IntAtIndex(0, nullptr));
				SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
				
				if (sim)
				{
					auto found_subpop_pair = sim->Population().find(source_subpop_id);
					
					if (found_subpop_pair != sim->Population().end())
						source_subpop = found_subpop_pair->second;
				}
				
				if (!source_subpop)
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): addSubpopSplit() subpopulation p" << source_subpop_id << " not defined." << eidos_terminate();
			}
			else
			{
				source_subpop = dynamic_cast<Subpopulation *>(arg2_value->ObjectElementAtIndex(0, nullptr));
			}
			
			if (arg3_value && !sex_enabled_)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): addSubpopSplit() sex ratio supplied in non-sexual simulation." << eidos_terminate();
			
			double sex_ratio = (arg3_value ? arg3_value->FloatAtIndex(0, nullptr) : 0.5);		// 0.5 is the default whenever sex is enabled and a ratio is not given
			
			// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
			Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, *source_subpop, subpop_size, sex_ratio);
			
			// define a new Eidos variable to refer to the new subpopulation
			EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
			
			if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): addSubpopSplit() symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate();
			
			simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
			
			return symbol_entry.second;
		}
			
			
			//
			//	*********************	- (void)deregisterScriptBlock(io<SLiMEidosBlock> scriptBlocks)
			//
#pragma mark -deregisterScriptBlock()
			
		case gID_deregisterScriptBlock:
		{
			int block_count = arg0_value->Count();
			
			// We just schedule the blocks for deregistration; we do not deregister them immediately, because that would leave stale pointers lying around
			for (int block_index = 0; block_index < block_count; ++block_index)
			{
				SLiMEidosBlock *block = nullptr;
				
				if (arg0_value->Type() == EidosValueType::kValueInt)
				{
					slim_objectid_t block_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(block_index, nullptr));
					
					for (SLiMEidosBlock *found_block : script_blocks_)
						if (found_block->block_id_ == block_id)
						{
							block = found_block;
							break;
						}
					
					if (!block)
						EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): deregisterScriptBlock() SLiMEidosBlock s" << block_id << " not defined." << eidos_terminate();
				}
				else
				{
					block = dynamic_cast<SLiMEidosBlock *>(arg0_value->ObjectElementAtIndex(block_index, nullptr));
				}
				
				if (std::find(scheduled_deregistrations_.begin(), scheduled_deregistrations_.end(), block) != scheduled_deregistrations_.end())
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): deregisterScriptBlock() called twice on the same script block." << eidos_terminate();
				
				scheduled_deregistrations_.emplace_back(block);
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (float)mutationFrequencies(No<Subpopulation> subpops, [object<Mutation> mutations])
			//
#pragma mark -mutationFrequencies()
			
		case gID_mutationFrequencies:
		{
			slim_refcount_t total_genome_count = 0;
			
			// The code below blows away the reference counts kept by Mutation, which must be valid at the end of each generation for
			// SLiM's internal machinery to work properly, so for simplicity we require that we're in the parental generation.
			if (population_.child_generation_valid_)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): mutationFrequencies() may only be called when the parental generation is active (before offspring generation)." << eidos_terminate();
			
			// first zero out the refcounts in all registered Mutation objects
			Mutation **registry_iter = population_.mutation_registry_.begin_pointer();
			Mutation **registry_iter_end = population_.mutation_registry_.end_pointer();
			
			for (; registry_iter != registry_iter_end; ++registry_iter)
				(*registry_iter)->reference_count_ = 0;
			
			// figure out which subpopulations we are supposed to tally across
			vector<Subpopulation*> subpops_to_tally;
			
			if (arg0_value->Type() == EidosValueType::kValueNULL)
			{
				// no requested subpops, so we should loop over all subpops
				for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
					subpops_to_tally.emplace_back(subpop_pair.second);
			}
			else
			{
				// requested subpops, so get them
				int requested_subpop_count = arg0_value->Count();
				
				if (requested_subpop_count)
				{
					for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
						subpops_to_tally.emplace_back((Subpopulation *)(arg0_value->ObjectElementAtIndex(requested_subpop_index, nullptr)));
				}
			}
			
			// then increment the refcounts through all pointers to Mutation in all genomes; the script command may have specified
			// a limited set of mutations, but it would actually make us slower to try to limit our counting to that subset
			for (Subpopulation *subpop : subpops_to_tally)
			{
				slim_popsize_t subpop_genome_count = 2 * subpop->parent_subpop_size_;
				std::vector<Genome> &subpop_genomes = subpop->parent_genomes_;
				
				for (slim_popsize_t i = 0; i < subpop_genome_count; i++)								// parent genomes
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
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			EidosValue_SP result_SP = EidosValue_SP(float_result);
			double denominator = 1.0 / total_genome_count;
			
			if (arg1_value)
			{
				// a vector of mutations was given, so loop through them and take their tallies
				int arg1_count = arg1_value->Count();
				
				if (arg1_count)
				{
					for (int value_index = 0; value_index < arg1_count; ++value_index)
						float_result->PushFloat(((Mutation *)(arg1_value->ObjectElementAtIndex(value_index, nullptr)))->reference_count_ * denominator);
				}
			}
			else
			{
				// no mutation vectoe was given, so return all frequencies from the registry
				for (registry_iter = population_.mutation_registry_.begin_pointer(); registry_iter != registry_iter_end; ++registry_iter)
					float_result->PushFloat((*registry_iter)->reference_count_ * denominator);
			}
			
			return result_SP;
		}
			
			
			//
			//	*********************	- (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
			//
#pragma mark -mutationsOfType()
			
		case gID_mutationsOfType:
		{
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = mutation_types_.find(mutation_type_id);
				
				if (found_muttype_pair == mutation_types_.end())
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): mutationsOfType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
				
				mutation_type_ptr = found_muttype_pair->second;
			}
			else
			{
				mutation_type_ptr = (MutationType *)(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			// Count the number of mutations of the given type, so we can reserve the right vector size
			// To avoid having to scan the registry twice for the simplest case of a single mutation, we cache the first mutation found
			Genome &mutation_registry = population_.mutation_registry_;
			int mutation_count = mutation_registry.size();
			int match_count = 0, mut_index;
			Mutation *first_match = nullptr;
			
			for (mut_index = 0; mut_index < mutation_count; ++mut_index)
			{
				Mutation *mut = mutation_registry[mut_index];
				
				if (mut->mutation_type_ptr_ == mutation_type_ptr)
				{
					if (++match_count == 1)
						first_match = mut;
				}
			}
			
			// Now allocate the result vector and assemble it
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->Reserve(match_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			if (match_count == 0)
			{
			}
			else if (match_count == 1)
			{
				vec->PushObjectElement(first_match);
			}
			else
			{
				for (mut_index = 0; mut_index < mutation_count; ++mut_index)
				{
					Mutation *mut = mutation_registry[mut_index];
					
					if (mut->mutation_type_ptr_ == mutation_type_ptr)
						vec->PushObjectElement(mut);
				}
			}
			
			return result_SP;
		}
			
			
			//
			//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
			//
#pragma mark -countOfMutationsOfType()
			
		case gID_countOfMutationsOfType:
		{
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = mutation_types_.find(mutation_type_id);
				
				if (found_muttype_pair == mutation_types_.end())
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): countOfMutationsOfType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
				
				mutation_type_ptr = found_muttype_pair->second;
			}
			else
			{
				mutation_type_ptr = (MutationType *)(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			// Count the number of mutations of the given type
			Genome &mutation_registry = population_.mutation_registry_;
			int mutation_count = mutation_registry.size();
			int match_count = 0, mut_index;
			
			for (mut_index = 0; mut_index < mutation_count; ++mut_index)
				if (mutation_registry[mut_index]->mutation_type_ptr_ == mutation_type_ptr)
					++match_count;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(match_count));
		}
			
			
			//
			//	*********************	- (void)outputFixedMutations(void)
			//
#pragma mark -outputFixedMutations()
			
		case gID_outputFixedMutations:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			
			output_stream << "#OUT: " << generation_ << " F " << endl;
			output_stream << "Mutations:" << endl;
			
			std::vector<Substitution*> &subs = population_.substitutions_;
			
			for (unsigned int i = 0; i < subs.size(); i++)
			{
				output_stream << i;
				subs[i]->print(output_stream);
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (void)outputFull([string$ filePath])
			//
#pragma mark -outputFull()
			
		case gID_outputFull:
		{
			if (p_argument_count == 0)
			{
				std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
				
				output_stream << "#OUT: " << generation_ << " A" << endl;
				population_.PrintAll(output_stream);
			}
			else if (p_argument_count == 1)
			{
				string outfile_path = arg0_value->StringAtIndex(0, nullptr);
				std::ofstream outfile;
				outfile.open(outfile_path.c_str());
				
				if (outfile.is_open())
				{
					// We no longer have input parameters to print; possibly this should print all the initialize...() functions called...
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
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): outputFull() could not open "<< outfile_path << "." << eidos_terminate();
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (void)outputMutations(object<Mutation> mutations)
			//
#pragma mark -outputMutations()
			
		case gID_outputMutations:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			
			// Extract all of the Mutation objects in mutations; would be nice if there was a simpler way to do this
			EidosValue_Object *mutations_object = (EidosValue_Object *)arg0_value;
			int mutations_count = mutations_object->Count();
			std::vector<Mutation*> mutations;
			
			for (int mutation_index = 0; mutation_index < mutations_count; mutation_index++)
				mutations.emplace_back((Mutation *)(mutations_object->ObjectElementAtIndex(mutation_index, nullptr)));
			
			// find all polymorphism of the mutations that are to be tracked
			if (mutations_count > 0)
			{
				for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_)
				{
					multimap<const slim_position_t,Polymorphism> polymorphisms;
					
					for (slim_popsize_t i = 0; i < 2 * subpop_pair.second->parent_subpop_size_; i++)				// go through all parents
					{
						for (int k = 0; k < subpop_pair.second->parent_genomes_[i].size(); k++)			// go through all mutations
						{
							Mutation *scan_mutation = subpop_pair.second->parent_genomes_[i][k];
							
							// do a linear search for each mutation, ouch; but this is output code, so it doesn't need to be fast, probably.
							if (std::find(mutations.begin(), mutations.end(), scan_mutation) != mutations.end())
								AddMutationToPolymorphismMap(&polymorphisms, scan_mutation);
						}
					}
					
					// output the frequencies of these mutations in each subpopulation; note the format here comes from the old tracked mutations code
					for (const std::pair<const slim_position_t,Polymorphism> &polymorphism_pair : polymorphisms) 
					{ 
						output_stream << "#OUT: " << generation_ << " T p" << subpop_pair.first << " ";
						polymorphism_pair.second.print(output_stream, false /* no id */);
					}
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (void)readFromPopulationFile(string$ filePath)
			//
#pragma mark -readFromPopulationFile()
			
		case gID_readFromPopulationFile:
		{
			string file_path = arg0_value->StringAtIndex(0, nullptr);
			
			// first we clear out all variables of type Subpopulation from the symbol table; they will all be invalid momentarily
			// note that we do this not only in our constants table, but in the user's variables as well; we can leave no stone unturned
			EidosSymbolTable &symbols = p_interpreter.SymbolTable();
			std::vector<std::string> all_symbols = symbols.AllSymbols();
			std::vector<EidosGlobalStringID> symbols_to_remove;
			
			for (string symbol_name : all_symbols)
			{
				EidosGlobalStringID symbol_ID = EidosGlobalStringIDForString(symbol_name);
				EidosValue_SP symbol_value = symbols.GetValueOrRaiseForSymbol(symbol_ID);
				
				if (symbol_value->Type() == EidosValueType::kValueObject)
				{
					const EidosObjectClass *symbol_class = static_pointer_cast<EidosValue_Object>(symbol_value)->Class();
					
					if ((symbol_class == gSLiM_Subpopulation_Class) || (symbol_class == gSLiM_Genome_Class) || (symbol_class == gSLiM_Mutation_Class) || (symbol_class == gSLiM_Substitution_Class))
						symbols_to_remove.emplace_back(symbol_ID);
				}
			}
			
			for (EidosGlobalStringID symbol_ID : symbols_to_remove)
				symbols.RemoveConstantForSymbol(symbol_ID);
			
			// then we dispose of all existing subpopulations, mutations, etc.
			population_.RemoveAllSubpopulationInfo();
			
			// then read from the file to get our new info
			InitializePopulationFromFile(file_path.c_str());
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (void)recalculateFitness([integer $generation])
			//
#pragma mark -recalculateFitness()
			
		case gID_recalculateFitness:
		{
			// Trigger a fitness recalculation.  This is suggested after making a change that would modify fitness values, such as altering
			// a selection coefficient or dominance coefficient, changing the mutation type for a mutation, etc.  It will have the side
			// effect of calling fitness() callbacks, so this is quite a heavyweight operation.
			slim_generation_t gen = (p_argument_count >= 1) ? SLiMCastToGenerationTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : generation_;
			
			population_.RecalculateFitness(gen);
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (object<SLiMEidosBlock>)registerEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
			//
#pragma mark -registerEvent()
			
		case gID_registerEvent:
		{
			slim_objectid_t script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
			string script_string = arg1_value->StringAtIndex(0, nullptr);
			slim_generation_t start_generation = (arg2_value ? SLiMCastToGenerationTypeOrRaise(arg2_value->IntAtIndex(0, nullptr)) : 1);
			slim_generation_t end_generation = (arg3_value ? SLiMCastToGenerationTypeOrRaise(arg3_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION);
			
			if (arg0_value->Type() != EidosValueType::kValueNULL)
				script_id = (arg0_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(arg0_value->StringAtIndex(0, nullptr), 's', nullptr);
			
			if (start_generation > end_generation)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): registerEvent() requires start <= end." << eidos_terminate();
			
			SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, SLiMEidosBlockType::SLiMEidosEvent, start_generation, end_generation);
			
			script_blocks_.emplace_back(new_script_block);		// takes ownership from us
			scripts_changed_ = true;
			
			new_script_block->TokenizeAndParse();	// can raise
			
			if (script_id != -1)
			{
				// define a new Eidos variable to refer to the new script
				EidosSymbolTableEntry &symbol_entry = new_script_block->ScriptBlockSymbolTableEntry();
				
				if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): registerEvent() symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate();
				
				simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
			}
			
			return new_script_block->SelfSymbolTableEntry().second;
		}
			
			
			//
			//	*********************	- (object<SLiMEidosBlock>)registerFitnessCallback(Nis$ id, string$ source, io<MutationType>$ mutType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
			//
#pragma mark -registerFitnessCallback()
			
		case gID_registerFitnessCallback:
		{
			slim_objectid_t script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
			string script_string = arg1_value->StringAtIndex(0, nullptr);
			slim_objectid_t mut_type_id = (arg2_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg2_value->IntAtIndex(0, nullptr)) : ((MutationType *)arg2_value->ObjectElementAtIndex(0, nullptr))->mutation_type_id_;
			slim_objectid_t subpop_id = -1;
			slim_generation_t start_generation = (arg4_value ? SLiMCastToGenerationTypeOrRaise(arg4_value->IntAtIndex(0, nullptr)) : 1);
			slim_generation_t end_generation = (arg5_value ? SLiMCastToGenerationTypeOrRaise(arg5_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION);
			
			if (arg0_value->Type() != EidosValueType::kValueNULL)
				script_id = (arg0_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(arg0_value->StringAtIndex(0, nullptr), 's', nullptr);
			
			if (arg3_value && (arg3_value->Type() != EidosValueType::kValueNULL))
				subpop_id = (arg3_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg3_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)arg3_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
			
			if (start_generation > end_generation)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): registerFitnessCallback() requires start <= end." << eidos_terminate();
			
			SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, SLiMEidosBlockType::SLiMEidosFitnessCallback, start_generation, end_generation);
			
			new_script_block->mutation_type_id_ = mut_type_id;
			new_script_block->subpopulation_id_ = subpop_id;
			
			script_blocks_.emplace_back(new_script_block);		// takes ownership from us
			scripts_changed_ = true;
			
			new_script_block->TokenizeAndParse();	// can raise
			
			if (script_id != -1)
			{
				// define a new Eidos variable to refer to the new script
				EidosSymbolTableEntry &symbol_entry = new_script_block->ScriptBlockSymbolTableEntry();
				
				if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): registerFitnessCallback() symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate();
				
				simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
			}
			
			return new_script_block->SelfSymbolTableEntry().second;
		}
			
			
			//
			//	*********************	- (object<SLiMEidosBlock>)registerMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
			//	*********************	- (object<SLiMEidosBlock>)registerModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
			//
#pragma mark -registerMateChoiceCallback()
#pragma mark -registerModifyChildCallback()
			
		case gID_registerMateChoiceCallback:
		case gID_registerModifyChildCallback:
		{
			slim_objectid_t script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
			string script_string = arg1_value->StringAtIndex(0, nullptr);
			slim_objectid_t subpop_id = -1;
			slim_generation_t start_generation = (arg3_value ? SLiMCastToGenerationTypeOrRaise(arg3_value->IntAtIndex(0, nullptr)) : 1);
			slim_generation_t end_generation = (arg4_value ? SLiMCastToGenerationTypeOrRaise(arg4_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION);
			
			if (arg0_value->Type() != EidosValueType::kValueNULL)
				script_id = (arg0_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(arg0_value->StringAtIndex(0, nullptr), 's', nullptr);
			
			if (arg2_value && (arg2_value->Type() != EidosValueType::kValueNULL))
				subpop_id = (arg2_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(arg2_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)arg2_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
			
			if (start_generation > end_generation)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): " << StringForEidosGlobalStringID(p_method_id) << " requires start <= end." << eidos_terminate();
			
			SLiMEidosBlockType block_type = ((p_method_id == gID_registerMateChoiceCallback) ? SLiMEidosBlockType::SLiMEidosMateChoiceCallback : SLiMEidosBlockType::SLiMEidosModifyChildCallback);
			SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_generation, end_generation);
			
			new_script_block->subpopulation_id_ = subpop_id;
			
			script_blocks_.emplace_back(new_script_block);		// takes ownership from us
			scripts_changed_ = true;
			
			new_script_block->TokenizeAndParse();	// can raise
			
			if (script_id != -1)
			{
				// define a new Eidos variable to refer to the new script
				EidosSymbolTableEntry &symbol_entry = new_script_block->ScriptBlockSymbolTableEntry();
				
				if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): " << StringForEidosGlobalStringID(p_method_id) << " symbol " << symbol_entry.first << " was already defined prior to its definition here." << eidos_terminate();
				
				simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
			}
			
			return new_script_block->SelfSymbolTableEntry().second;
		}
			
			
			//
			//	*********************	- (void)simulationFinished(void)
			//
#pragma mark -simulationFinished()
			
		case gID_simulationFinished:
		{
			sim_declared_finished_ = true;
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	SLiMSim_Class
//
#pragma mark -
#pragma mark SLiMSim_Class

class SLiMSim_Class : public EidosObjectClass
{
public:
	SLiMSim_Class(const SLiMSim_Class &p_original) = delete;	// no copy-construct
	SLiMSim_Class& operator=(const SLiMSim_Class&) = delete;	// no copying
	
	SLiMSim_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_SLiMSim_Class = new SLiMSim_Class();


SLiMSim_Class::SLiMSim_Class(void)
{
}

const std::string &SLiMSim_Class::ElementType(void) const
{
	return gStr_SLiMSim;
}

const std::vector<const EidosPropertySignature *> *SLiMSim_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_chromosome));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_chromosomeType));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomicElementTypes));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_mutations));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_mutationTypes));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_scriptBlocks));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_sexEnabled));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_subpopulations));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_substitutions));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_dominanceCoeffX));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_generation));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *SLiMSim_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *chromosomeSig = nullptr;
	static EidosPropertySignature *chromosomeTypeSig = nullptr;
	static EidosPropertySignature *genomicElementTypesSig = nullptr;
	static EidosPropertySignature *mutationsSig = nullptr;
	static EidosPropertySignature *mutationTypesSig = nullptr;
	static EidosPropertySignature *scriptBlocksSig = nullptr;
	static EidosPropertySignature *sexEnabledSig = nullptr;
	static EidosPropertySignature *subpopulationsSig = nullptr;
	static EidosPropertySignature *substitutionsSig = nullptr;
	static EidosPropertySignature *dominanceCoeffXSig = nullptr;
	static EidosPropertySignature *generationSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!chromosomeSig)
	{
		chromosomeSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,			gID_chromosome,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class));
		chromosomeTypeSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosomeType,		gID_chromosomeType,			true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		genomicElementTypesSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElementTypes,	gID_genomicElementTypes,	true,	kEidosValueMaskObject, gSLiM_GenomicElementType_Class));
		mutationsSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutations,			gID_mutations,				true,	kEidosValueMaskObject, gSLiM_Mutation_Class));
		mutationTypesSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationTypes,		gID_mutationTypes,			true,	kEidosValueMaskObject, gSLiM_MutationType_Class));
		scriptBlocksSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_scriptBlocks,		gID_scriptBlocks,			true,	kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class));
		sexEnabledSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_sexEnabled,			gID_sexEnabled,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton));
		subpopulationsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulations,		gID_subpopulations,			true,	kEidosValueMaskObject, gSLiM_Subpopulation_Class));
		substitutionsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_substitutions,		gID_substitutions,			true,	kEidosValueMaskObject, gSLiM_Substitution_Class));
		dominanceCoeffXSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_dominanceCoeffX,		gID_dominanceCoeffX,		false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		generationSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_generation,			gID_generation,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		tagSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					gID_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_chromosome:			return chromosomeSig;
		case gID_chromosomeType:		return chromosomeTypeSig;
		case gID_genomicElementTypes:	return genomicElementTypesSig;
		case gID_mutations:				return mutationsSig;
		case gID_mutationTypes:			return mutationTypesSig;
		case gID_scriptBlocks:			return scriptBlocksSig;
		case gID_sexEnabled:			return sexEnabledSig;
		case gID_subpopulations:		return subpopulationsSig;
		case gID_substitutions:			return substitutionsSig;
		case gID_dominanceCoeffX:		return dominanceCoeffXSig;
		case gID_generation:			return generationSig;
		case gID_tag:					return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *SLiMSim_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_addSubpop));
		methods->emplace_back(SignatureForMethodOrRaise(gID_addSubpopSplit));
		methods->emplace_back(SignatureForMethodOrRaise(gID_deregisterScriptBlock));
		methods->emplace_back(SignatureForMethodOrRaise(gID_mutationFrequencies));
		methods->emplace_back(SignatureForMethodOrRaise(gID_mutationsOfType));
		methods->emplace_back(SignatureForMethodOrRaise(gID_countOfMutationsOfType));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputFixedMutations));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputFull));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputMutations));
		methods->emplace_back(SignatureForMethodOrRaise(gID_readFromPopulationFile));
		methods->emplace_back(SignatureForMethodOrRaise(gID_recalculateFitness));
		methods->emplace_back(SignatureForMethodOrRaise(gID_registerEvent));
		methods->emplace_back(SignatureForMethodOrRaise(gID_registerFitnessCallback));
		methods->emplace_back(SignatureForMethodOrRaise(gID_registerMateChoiceCallback));
		methods->emplace_back(SignatureForMethodOrRaise(gID_registerModifyChildCallback));
		methods->emplace_back(SignatureForMethodOrRaise(gID_simulationFinished));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *SLiMSim_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *addSubpopSig = nullptr;
	static EidosInstanceMethodSignature *addSubpopSplitSig = nullptr;
	static EidosInstanceMethodSignature *deregisterScriptBlockSig = nullptr;
	static EidosInstanceMethodSignature *mutationFrequenciesSig = nullptr;
	static EidosInstanceMethodSignature *mutationsOfTypeSig = nullptr;
	static EidosInstanceMethodSignature *countOfMutationsOfTypeSig = nullptr;
	static EidosInstanceMethodSignature *outputFixedMutationsSig = nullptr;
	static EidosInstanceMethodSignature *outputFullSig = nullptr;
	static EidosInstanceMethodSignature *outputMutationsSig = nullptr;
	static EidosInstanceMethodSignature *readFromPopulationFileSig = nullptr;
	static EidosInstanceMethodSignature *recalculateFitnessSig = nullptr;
	static EidosInstanceMethodSignature *registerEventSig = nullptr;
	static EidosInstanceMethodSignature *registerFitnessCallbackSig = nullptr;
	static EidosInstanceMethodSignature *registerMateChoiceCallbackSig = nullptr;
	static EidosInstanceMethodSignature *registerModifyChildCallbackSig = nullptr;
	static EidosInstanceMethodSignature *simulationFinishedSig = nullptr;
	
	if (!addSubpopSig)
	{
		addSubpopSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSubpop, kEidosValueMaskObject, gSLiM_Subpopulation_Class))->AddIntString_S("subpopID")->AddInt_S("size")->AddFloat_OS("sexRatio");
		addSubpopSplitSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSubpopSplit, kEidosValueMaskObject, gSLiM_Subpopulation_Class))->AddIntString_S("subpopID")->AddInt_S("size")->AddIntObject_S("sourceSubpop", gSLiM_Subpopulation_Class)->AddFloat_OS("sexRatio");
		deregisterScriptBlockSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_deregisterScriptBlock, kEidosValueMaskNULL))->AddIntObject("scriptBlocks", gSLiM_SLiMEidosBlock_Class);
		mutationFrequenciesSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationFrequencies, kEidosValueMaskFloat))->AddObject_N("subpops", gSLiM_Subpopulation_Class)->AddObject_O("mutations", gSLiM_Mutation_Class);
		mutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
		countOfMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
		outputFixedMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputFixedMutations, kEidosValueMaskNULL));
		outputFullSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputFull, kEidosValueMaskNULL))->AddString_OS("filePath");
		outputMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMutations, kEidosValueMaskNULL))->AddObject("mutations", gSLiM_Mutation_Class);
		readFromPopulationFileSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_readFromPopulationFile, kEidosValueMaskNULL))->AddString_S("filePath");
		recalculateFitnessSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_recalculateFitness, kEidosValueMaskNULL))->AddInt_OS("generation");
		registerEventSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerEvent, kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddInt_OS("start")->AddInt_OS("end");
		registerFitnessCallbackSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerFitnessCallback, kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_S("mutType", gSLiM_MutationType_Class)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class)->AddInt_OS("start")->AddInt_OS("end");
		registerMateChoiceCallbackSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerMateChoiceCallback, kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class)->AddInt_OS("start")->AddInt_OS("end");
		registerModifyChildCallbackSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerModifyChildCallback, kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class)->AddInt_OS("start")->AddInt_OS("end");
		simulationFinishedSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_simulationFinished, kEidosValueMaskNULL));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_addSubpop:								return addSubpopSig;
		case gID_addSubpopSplit:						return addSubpopSplitSig;
		case gID_deregisterScriptBlock:					return deregisterScriptBlockSig;
		case gID_mutationFrequencies:					return mutationFrequenciesSig;
		case gID_mutationsOfType:						return mutationsOfTypeSig;
		case gID_countOfMutationsOfType:				return countOfMutationsOfTypeSig;
		case gID_outputFixedMutations:					return outputFixedMutationsSig;
		case gID_outputFull:							return outputFullSig;
		case gID_outputMutations:						return outputMutationsSig;
		case gID_readFromPopulationFile:				return readFromPopulationFileSig;
		case gID_recalculateFitness:					return recalculateFitnessSig;
		case gID_registerEvent:							return registerEventSig;
		case gID_registerFitnessCallback:				return registerFitnessCallbackSig;
		case gID_registerMateChoiceCallback:			return registerMateChoiceCallbackSig;
		case gID_registerModifyChildCallback:			return registerModifyChildCallbackSig;
		case gID_simulationFinished:					return simulationFinishedSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP SLiMSim_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}




























































