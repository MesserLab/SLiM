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
#include "eidos_function_signature.h"


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
	
	// set up SLiM registrations in Eidos
	Eidos_RegisterGlobalStringsAndIDs();
	SLiM_RegisterGlobalStringsAndIDs();
	
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
	
	// set up SLiM registrations in Eidos
	Eidos_RegisterGlobalStringsAndIDs();
	SLiM_RegisterGlobalStringsAndIDs();
	
	// Open our file stream
	std::ifstream infile(p_input_file);
	
	if (!infile.is_open())
	{
		EIDOS_TERMINATION << std::endl;
		EIDOS_TERMINATION << "ERROR (parameter file): could not open: " << p_input_file << std::endl << std::endl;
		EIDOS_TERMINATION << eidos_terminate();
	}
	
	// read all configuration information from the input file
	infile.seekg(0, std::fstream::beg);
	InitializeFromFile(infile);
}

SLiMSim::~SLiMSim(void)
{
	//EIDOS_ERRSTREAM << "SLiMSim::~SLiMSim" << std::endl;
	
	for (auto mutation_type : mutation_types_)
		delete mutation_type.second;
	
	for (auto genomic_element_type : genomic_element_types_)
		delete genomic_element_type.second;
	
	for (auto script_block : script_blocks_)
		delete script_block;
	
	// All the script blocks that refer to the script are now gone
	delete script_;
	
	// We should not have any interpreter instances that still refer to us
	for (EidosFunctionSignature *signature : sim_0_signatures)
		delete signature;
	
	if (self_symbol_)
	{
		delete self_symbol_->second;
		delete self_symbol_;
	}
	
	if (cached_value_generation_)
		delete cached_value_generation_;
}

void SLiMSim::InitializeFromFile(std::istream &infile)
{
	if (!rng_seed_supplied_to_constructor_)
		rng_seed_ = EidosGenerateSeedFromPIDAndTime();
	
	// Reset error position indicators used by SLiMgui
	gEidosCharacterStartOfParseError = -1;
	gEidosCharacterEndOfParseError = -1;
	
	// Read in the file; going through stringstream is fast...
	std::stringstream buffer;
	
	buffer << infile.rdbuf();
	
	// Tokenize and parse
	script_ = new SLiMEidosScript(buffer.str(), 0);
	
	script_->Tokenize();
	script_->ParseSLiMFileToAST();
	
	// Extract SLiMEidosBlocks from the parse tree
	const EidosASTNode *root_node = script_->AST();
	
	for (EidosASTNode *script_block_node : root_node->children_)
	{
		SLiMEidosBlock *script_block = new SLiMEidosBlock(script_block_node);
		
		script_blocks_.push_back(script_block);
	}
	
	// Reset error position indicators used by SLiMgui
	gEidosCharacterStartOfParseError = -1;
	gEidosCharacterEndOfParseError = -1;
	
	// initialize rng; this is either a value given at the command line, or the value generate above by EidosGenerateSeedFromPIDAndTime()
	EidosInitializeRNGFromSeed(rng_seed_);
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
		EIDOS_TERMINATION << "ERROR (Initialize): could not open initialization file" << eidos_terminate();
	
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
			EIDOS_TERMINATION << "ERROR (InitializePopulationFromFile): mutation type m"<< mutation_type_id << " has not been defined" << eidos_terminate();
		
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
		const char *subpop_id_string = sub.substr(0, pos).c_str();
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
					EIDOS_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as A (autosome), but the instantiated genome does not match" << eidos_terminate();
				if (genome_type == 'X' && genome.GenomeType() != GenomeType::kXChromosome)
					EIDOS_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as X (X-chromosome), but the instantiated genome does not match" << eidos_terminate();
				if (genome_type == 'Y' && genome.GenomeType() != GenomeType::kYChromosome)
					EIDOS_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as Y (Y-chromosome), but the instantiated genome does not match" << eidos_terminate();
				
				if (iss >> sub)
				{
					if (sub == "<null>")
					{
						if (!genome.IsNull())
							EIDOS_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as null, but the instantiated genome is non-null" << eidos_terminate();
						
						continue;	// this line is over
					}
					else
					{
						if (genome.IsNull())
							EIDOS_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as non-null, but the instantiated genome is null" << eidos_terminate();
						
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
					EIDOS_TERMINATION << "ERROR (InitializePopulationFromFile): mutation " << id << " has not been defined" << eidos_terminate();
				
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
		std::vector<SLiMEidosBlock*> fitness_callbacks = ScriptBlocksMatching(generation_ + 1, SLiMEidosBlockType::SLiMEidosFitnessCallback, -1, subpop_id);
		
		subpop->UpdateFitness(fitness_callbacks);
	}
}

std::vector<SLiMEidosBlock*> SLiMSim::ScriptBlocksMatching(int p_generation, SLiMEidosBlockType p_event_type, int p_mutation_type_id, int p_subpopulation_id)
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
	for (SLiMEidosBlock *block_to_dereg : scheduled_deregistrations_)
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

void SLiMSim::RunInitializeCallbacks(void)
{
	// zero out the initialization check counts
	num_mutation_types = 0;
	num_mutation_rates = 0;
	num_genomic_element_types = 0;
	num_genomic_elements = 0;
	num_recombination_rates = 0;
	num_gene_conversions = 0;
	num_sex_declarations = 0;
	
	if (DEBUG_INPUT)
		EIDOS_OUTSTREAM << "// RunInitializeCallbacks():" << endl;
	
	// execute initialize() callbacks, which should always have a generation of 0 set
	std::vector<SLiMEidosBlock*> init_blocks = ScriptBlocksMatching(0, SLiMEidosBlockType::SLiMEidosInitializeCallback, -1, -1);
	
	for (auto script_block : init_blocks)
	{
		if (script_block->active_)
			population_.ExecuteScript(script_block, generation_, chromosome_);
	}
	
	DeregisterScheduledScriptBlocks();
	
	// check for complete initialization
	if (num_mutation_rates == 0)
		EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): A mutation rate must be supplied in an initialize() callback with initializeMutationRate()." << eidos_terminate();
	
	if (num_mutation_types == 0)
		EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): At least one mutation type must be defined in an initialize() callback with initializeMutationType()." << eidos_terminate();
	
	if (num_genomic_element_types == 0)
		EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): At least one genomic element type must be defined in an initialize() callback with initializeGenomicElementType()." << eidos_terminate();
	
	if (num_genomic_elements == 0)
		EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): At least one genomic element must be defined in an initialize() callback with initializeGenomicElement()." << eidos_terminate();
	
	if (num_recombination_rates == 0)
		EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): At least one recombination rate interval must be defined in an initialize() callback with initializeRecombinationRate()." << eidos_terminate();
	
	// figure out our first generation; it is the earliest generation in which an Eidos event is set up to run,
	// since an Eidos event that adds a subpopulation is necessary to get things started
	time_start_ = INT_MAX;
	
	for (auto script_block : script_blocks_)
		if ((script_block->type_ == SLiMEidosBlockType::SLiMEidosEvent) && (script_block->start_generation_ < time_start_) && (script_block->start_generation_ > 0))
			time_start_ = script_block->start_generation_;
	
	if (time_start_ == INT_MAX)
		EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): No Eidos event found to start the simulation." << eidos_terminate();
	
	// emit our start log
	EIDOS_OUTSTREAM << "\n// Starting run at generation <start>:\n" << time_start_ << " " << "\n" << std::endl;
	
	// start at the beginning
	generation_ = time_start_;
	
	// initialize chromosome
	chromosome_.InitializeDraws();
}

int SLiMSim::EstimatedLastGeneration()
{
	int last_gen = 1;
	
	// The estimate is derived from the last generation in which an Eidos block is registered.
	// Any block type works, since the simulation could plausibly be stopped within a callback.
	// However, blocks that do not specify an end generation don't count.
	for (auto script_block : script_blocks_)
		if ((script_block->end_generation_ > last_gen) && (script_block->end_generation_ != INT_MAX))
			last_gen = script_block->end_generation_;
	
	return last_gen;
}

bool SLiMSim::RunOneGeneration(void)
{
#ifdef SLIMGUI
	if (simulationValid)
	{
		try
		{
#endif
			if (generation_ == 0)
			{
				RunInitializeCallbacks();
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
				
				// cache a list of callbacks registered for each subpop
				for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
				{
					int subpop_id = subpop_pair.first;
					Subpopulation *subpop = subpop_pair.second;
					
					// Get mateChoice() callbacks that apply to this subpopulation
					for (SLiMEidosBlock *callback : mate_choice_callbacks)
					{
						int callback_subpop_id = callback->subpopulation_id_;
						
						if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
							subpop->registered_mate_choice_callbacks_.push_back(callback);
					}
					
					// Get modifyChild() callbacks that apply to this subpopulation
					for (SLiMEidosBlock *callback : modify_child_callbacks)
					{
						int callback_subpop_id = callback->subpopulation_id_;
						
						if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
							subpop->registered_modify_child_callbacks_.push_back(callback);
					}
				}
				
				// then evolve each subpop
				for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
					population_.EvolveSubpopulation(subpop_pair.first, chromosome_, generation_, mate_choice_callbacks_present, modify_child_callbacks_present);
				
				// then remove the cached callbacks, for safety and because we'd have to do it eventually anyway
				for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
				{
					Subpopulation *subpop = subpop_pair.second;
					
					subpop->registered_mate_choice_callbacks_.clear();
					subpop->registered_modify_child_callbacks_.clear();
				}
				
				// then switch to the child generation; we don't want to do this until all callbacks have executed for all subpops
				for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
					subpop_pair.second->child_generation_valid = true;
				
				population_.child_generation_valid = true;
				
				// the stage is done, so deregister script blocks as requested
				DeregisterScheduledScriptBlocks();
				
				
				// ******************************************************************
				//
				// Stage 3: Swap generations and advance to the next generation
				//
				population_.SwapGenerations();
				
				// advance the generation counter as soon as the generation is done
				if (cached_value_generation_)
				{
					delete cached_value_generation_;
					cached_value_generation_ = nullptr;
				}
				generation_++;
				
				// Decide whether the simulation is over.  We need to call EstimatedLastGeneration() every time; we can't
				// cache it, because it can change based upon changes in script registration / deregistration.
				return (generation_ <= EstimatedLastGeneration());
			}
#ifdef SLIMGUI
		}
		catch (std::runtime_error err)
		{
			simulationValid = false;
			
			return false;
		}
	}
#endif
	
	return false;
}

void SLiMSim::RunToEnd(void)
{
	while (RunOneGeneration());
}


//
//	Eidos support
//
#pragma mark Eidos support

void SLiMSim::GenerateCachedSymbolTableEntry(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	self_symbol_ = new EidosSymbolTableEntry(gStr_sim, (new EidosValue_Object_singleton_const(this))->SetExternalPermanent());
}

// a static member function is used as a funnel, so that we can get a pointer to function for it
EidosValue *SLiMSim::StaticFunctionDelegationFunnel(void *delegate, const std::string &p_function_name, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	SLiMSim *sim = static_cast<SLiMSim *>(delegate);
	
	return sim->FunctionDelegationFunnel(p_function_name, p_arguments, p_argument_count, p_interpreter);
}

// the static member function calls this member function; now we're completely in context and can execute the function
EidosValue *SLiMSim::FunctionDelegationFunnel(const std::string &p_function_name, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused(p_interpreter)
	
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2] : nullptr);
	
	// we only define initialize...() functions; so we must be in an initialize() callback
	if (generation_ != 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::FunctionDelegationFunnel): FunctionDelegationFunnel() called outside of initialize time." << eidos_terminate();
	
	
	//
	//	*********************	initializeGenomicElement(integer$ genomicElementTypeID, integer$ start, integer$ end)
	//
	#pragma mark initializeGenomicElement()
	
	if (p_function_name.compare(gStr_initializeGenomicElement) == 0)
	{
		int genomic_element_type = (int)arg0_value->IntAtIndex(0);
		int start_position = (int)arg1_value->IntAtIndex(0);		// used to have a -1; switched to zero-based
		int end_position = (int)arg2_value->IntAtIndex(0);			// used to have a -1; switched to zero-based
		auto found_getype_pair = genomic_element_types_.find(genomic_element_type);
		
		if (found_getype_pair == genomic_element_types_.end())
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGenomicElement() genomic element type g" << genomic_element_type << " not defined" << eidos_terminate();
		
		// FIXME bounds-check start and end
		
		GenomicElementType *genomic_element_type_ptr = found_getype_pair->second;
		GenomicElement new_genomic_element(genomic_element_type_ptr, start_position, end_position);
		
		bool old_log = GenomicElement::LogGenomicElementCopyAndAssign(false);
		chromosome_.push_back(new_genomic_element);
		GenomicElement::LogGenomicElementCopyAndAssign(old_log);
		
		chromosome_changed_ = true;
		
		if (DEBUG_INPUT)
			EIDOS_OUTSTREAM << "initializeGenomicElement(" << genomic_element_type << ", " << start_position << ", " << end_position << ");" << endl;
		
		num_genomic_elements++;
	}
	
	
	//
	//	*********************	initializeGenomicElementType(integer$ id, integer mutationTypeIDs, numeric proportions)
	//
	#pragma mark initializeGenomicElementType()
	
	else if (p_function_name.compare(gStr_initializeGenomicElementType) == 0)
	{
		int map_identifier = (int)arg0_value->IntAtIndex(0);
		
		if (map_identifier < 0)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGenomicElementType() requires id >= 0." << eidos_terminate();
		if (genomic_element_types_.count(map_identifier) > 0) 
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGenomicElementType() genomic element type g" << map_identifier << " already defined." << eidos_terminate();
		
		int mut_type_id_count = arg1_value->Count();
		int proportion_count = arg2_value->Count();
		
		if ((mut_type_id_count != proportion_count) || (mut_type_id_count == 0))
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGenomicElementType() requires the sizes of mutationTypeIDs and proportions to be equal and nonzero." << eidos_terminate();
		
		std::vector<MutationType*> mutation_types;
		std::vector<double> mutation_fractions;
		
		for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
		{ 
			int mutation_type_id = (int)arg1_value->IntAtIndex(mut_type_index);
			double proportion = arg2_value->FloatAtIndex(mut_type_index);
			
			if (proportion <= 0)
				EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGenomicElementType() proportions must be greater than zero." << eidos_terminate();
			
			auto found_muttype_pair = mutation_types_.find(mutation_type_id);
			
			if (found_muttype_pair == mutation_types_.end())
				EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGenomicElementType() mutation type m" << mutation_type_id << " not defined" << eidos_terminate();
			
			MutationType *mutation_type_ptr = found_muttype_pair->second;
			
			mutation_types.push_back(mutation_type_ptr);
			mutation_fractions.push_back(proportion);
		}
		
		GenomicElementType *new_genomic_element_type = new GenomicElementType(map_identifier, mutation_types, mutation_fractions);
		genomic_element_types_.insert(std::pair<const int,GenomicElementType*>(map_identifier, new_genomic_element_type));
		genomic_element_types_changed_ = true;
		
		if (DEBUG_INPUT)
		{
			EIDOS_OUTSTREAM << "initializeGenomicElementType(" << map_identifier;
			
			for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
				EIDOS_OUTSTREAM << ", " << arg1_value->IntAtIndex(mut_type_index) << ", " << arg2_value->FloatAtIndex(mut_type_index);
			
			EIDOS_OUTSTREAM << ");" << endl;
		}
		
		num_genomic_element_types++;
	}
	
	
	//
	//	*********************	initializeMutationType(integer$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	//
	#pragma mark initializeMutationType()
	
	else if (p_function_name.compare(gStr_initializeMutationType) == 0)
	{
		int map_identifier = (int)arg0_value->IntAtIndex(0);
		double dominance_coeff = arg1_value->FloatAtIndex(0);
		string dfe_type_string = arg2_value->StringAtIndex(0);
		int expected_dfe_param_count = 0;
		std::vector<double> dfe_parameters;
		
		if (map_identifier < 0)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeMutationType() requires id >= 0." << eidos_terminate();
		if (mutation_types_.count(map_identifier) > 0) 
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeMutationType() mutation type m" << map_identifier << " already defined" << eidos_terminate();
		
		if (dfe_type_string.compare("f") == 0)
			expected_dfe_param_count = 1;
		else if (dfe_type_string.compare("g") == 0)
			expected_dfe_param_count = 2;
		else if (dfe_type_string.compare("e") == 0)
			expected_dfe_param_count = 1;
		else
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeMutationType() distributionType \"" << dfe_type_string << "must be \"f\", \"g\", or \"e\"." << eidos_terminate();
		
		char dfe_type = dfe_type_string[0];
		
		if (p_argument_count != 3 + expected_dfe_param_count)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeMutationType() distributionType \"" << dfe_type << "\" requires exactly " << expected_dfe_param_count << " DFE parameter" << (expected_dfe_param_count == 1 ? "" : "s") << "." << eidos_terminate();
		
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
			EIDOS_OUTSTREAM << "initializeMutationType(" << map_identifier << ", " << dominance_coeff << ", \"" << dfe_type << "\"";
			
			for (double dfe_param : dfe_parameters)
				EIDOS_OUTSTREAM << ", " << dfe_param;
			
			EIDOS_OUTSTREAM << ");" << endl;
		}
		
		num_mutation_types++;
	}
	
	
	//
	//	*********************	initializeRecombinationRate(numeric rates, [integer ends])
	//
	#pragma mark initializeRecombinationRate()
	
	else if (p_function_name.compare(gStr_initializeRecombinationRate) == 0)
	{
		int rate_count = arg0_value->Count();
		
		if (p_argument_count == 1)
		{
			if (rate_count != 1)
				EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeRecombinationRate() requires rates to be a singleton if ends is not supplied." << eidos_terminate();
			
			double recombination_rate = arg0_value->FloatAtIndex(0);
			
			// check values
			if (recombination_rate < 0.0)
				EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeRecombinationRate() requires rates to be >= 0." << eidos_terminate();
			
			// then adopt them
			chromosome_.recombination_rates_.clear();
			chromosome_.recombination_end_positions_.clear();
			
			chromosome_.recombination_rates_.push_back(recombination_rate);
			//chromosome_.recombination_end_positions_.push_back(?);	// deferred; patched in Chromosome::InitializeDraws().
		}
		else if (p_argument_count == 2)
		{
			int end_count = arg1_value->Count();
			
			if ((end_count != rate_count) || (end_count == 0))
				EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeRecombinationRate() requires ends and rates to be of equal and nonzero size." << eidos_terminate();
			
			// check values
			for (int value_index = 0; value_index < end_count; ++value_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(value_index);
				int64_t recombination_end_position = arg1_value->IntAtIndex(value_index);
				
				if (value_index > 0)
					if (recombination_end_position <= arg1_value->IntAtIndex(value_index - 1))
						EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeRecombinationRate() requires ends to be in ascending order." << eidos_terminate();
				
				if (recombination_rate < 0.0)
					EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeRecombinationRate() requires rates to be >= 0." << eidos_terminate();
			}
			
			// then adopt them
			chromosome_.recombination_rates_.clear();
			chromosome_.recombination_end_positions_.clear();
			
			for (int interval_index = 0; interval_index < end_count; ++interval_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(interval_index);
				int64_t recombination_end_position = arg1_value->IntAtIndex(interval_index);	// used to have a -1; switched to zero-based
				
				chromosome_.recombination_rates_.push_back(recombination_rate);
				chromosome_.recombination_end_positions_.push_back((int)recombination_end_position);
			}
		}
		
		chromosome_changed_ = true;
		
		if (DEBUG_INPUT)
		{
			int ratesSize = (int)chromosome_.recombination_rates_.size();
			int endsSize = (int)chromosome_.recombination_end_positions_.size();
			
			EIDOS_OUTSTREAM << "initializeRecombinationRate(";
			
			if (ratesSize > 1)
				EIDOS_OUTSTREAM << "c(";
			for (int interval_index = 0; interval_index < ratesSize; ++interval_index)
				EIDOS_OUTSTREAM << (interval_index == 0 ? "" : ", ") << chromosome_.recombination_rates_[interval_index];
			if (ratesSize > 1)
				EIDOS_OUTSTREAM << ")";
			
			if (endsSize > 0)
			{
				EIDOS_OUTSTREAM << ", ";
				
				if (endsSize > 1)
					EIDOS_OUTSTREAM << "c(";
				for (int interval_index = 0; interval_index < endsSize; ++interval_index)
					EIDOS_OUTSTREAM << (interval_index == 0 ? "" : ", ") << chromosome_.recombination_end_positions_[interval_index];
				if (endsSize > 1)
					EIDOS_OUTSTREAM << ")";
			}
			
			EIDOS_OUTSTREAM << ");" << endl;
		}
		
		num_recombination_rates++;
	}
	
	
	//
	//	*********************	initializeGeneConversion(numeric$ conversionFraction, numeric$ meanLength)
	//
	#pragma mark initializeGeneConversion()
	
	else if (p_function_name.compare(gStr_initializeGeneConversion) == 0)
	{
		if (num_gene_conversions > 0)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGeneConversion() may be called only once." << eidos_terminate();
		
		double gene_conversion_fraction = arg0_value->FloatAtIndex(0);
		double gene_conversion_avg_length = arg1_value->FloatAtIndex(0);
		
		if ((gene_conversion_fraction < 0.0) || (gene_conversion_fraction > 1.0))
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGeneConversion() conversionFraction must be between 0.0 and 1.0 (inclusive)." << eidos_terminate();
		if (gene_conversion_avg_length <= 0.0)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeGeneConversion() meanLength must be greater than 0.0." << eidos_terminate();
		
		chromosome_.gene_conversion_fraction_ = gene_conversion_fraction;
		chromosome_.gene_conversion_avg_length_ = gene_conversion_avg_length;
		
		if (DEBUG_INPUT)
			EIDOS_OUTSTREAM << "initializeGeneConversion(" << gene_conversion_fraction << ", " << gene_conversion_avg_length << ");" << endl;
		
		num_gene_conversions++;
	}
	
	
	//
	//	*********************	initializeMutationRate(numeric$ rate)
	//
#pragma mark initializeMutationRate()
	
	else if (p_function_name.compare(gStr_initializeMutationRate) == 0)
	{
		if (num_mutation_rates > 0)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeMutationRate() may be called only once." << eidos_terminate();
		
		double rate = arg0_value->FloatAtIndex(0);
		
		if (rate < 0.0)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeMutationRate() requires rate >= 0." << eidos_terminate();
		
		chromosome_.overall_mutation_rate_ = rate;
		
		if (DEBUG_INPUT)
			EIDOS_OUTSTREAM << "initializeMutationRate(" << chromosome_.overall_mutation_rate_ << ");" << endl;
		
		num_mutation_rates++;
	}
	
	
	//
	//	*********************	initializeSex(string$ chromosomeType, [numeric$ xDominanceCoeff])
	//
	#pragma mark initializeSex()
	
	else if (p_function_name.compare(gStr_initializeSex) == 0)
	{
		if (num_sex_declarations > 0)
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeSex() may be called only once." << eidos_terminate();
		
		string chromosome_type = arg0_value->StringAtIndex(0);
		
		if (chromosome_type.compare("A") == 0)
			modeled_chromosome_type_ = GenomeType::kAutosome;
		else if (chromosome_type.compare("X") == 0)
			modeled_chromosome_type_ = GenomeType::kXChromosome;
		else if (chromosome_type.compare("Y") == 0)
			modeled_chromosome_type_ = GenomeType::kYChromosome;
		else
			EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): initializeSex() requires a chromosomeType of \"A\", \"X\", or \"Y\"." << eidos_terminate();
		
		if (p_argument_count == 2)
		{
			if (modeled_chromosome_type_ == GenomeType::kXChromosome)
				x_chromosome_dominance_coeff_ = arg1_value->FloatAtIndex(0);
			else
				EIDOS_TERMINATION << "ERROR (RunInitializeCallbacks): xDominanceCoeff may be supplied to initializeSex() only for chromosomeType \"X\"." << eidos_terminate();
		}
		
		if (DEBUG_INPUT)
		{
			EIDOS_OUTSTREAM << "initializeSex(\"" << chromosome_type << "\"";
			
			if (p_argument_count == 2)
				EIDOS_OUTSTREAM << ", " << x_chromosome_dominance_coeff_;
			
			EIDOS_OUTSTREAM << ");" << endl;
		}
		
		sex_enabled_ = true;
		num_sex_declarations++;
	}
	
	// the initialize...() functions all return invisible NULL
	return gStaticEidosValueNULLInvisible;
}

std::vector<EidosFunctionSignature*> *SLiMSim::InjectedFunctionSignatures(void)
{
	if (generation_ == 0)
	{
		// Allocate our own EidosFunctionSignature objects; they cannot be statically allocated since they point to us
		if (!sim_0_signatures.size())
		{
			sim_0_signatures.push_back((new EidosFunctionSignature(gStr_initializeGenomicElement, EidosFunctionIdentifier::kDelegatedFunction, kValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S()->AddInt_S()->AddInt_S());
			sim_0_signatures.push_back((new EidosFunctionSignature(gStr_initializeGenomicElementType, EidosFunctionIdentifier::kDelegatedFunction, kValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S()->AddInt()->AddNumeric());
			sim_0_signatures.push_back((new EidosFunctionSignature(gStr_initializeMutationType, EidosFunctionIdentifier::kDelegatedFunction, kValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddInt_S()->AddNumeric_S()->AddString_S()->AddEllipsis());
			sim_0_signatures.push_back((new EidosFunctionSignature(gStr_initializeRecombinationRate, EidosFunctionIdentifier::kDelegatedFunction, kValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddNumeric()->AddInt_O());
			sim_0_signatures.push_back((new EidosFunctionSignature(gStr_initializeGeneConversion, EidosFunctionIdentifier::kDelegatedFunction, kValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddNumeric_S()->AddNumeric_S());
			sim_0_signatures.push_back((new EidosFunctionSignature(gStr_initializeMutationRate, EidosFunctionIdentifier::kDelegatedFunction, kValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddNumeric_S());
			sim_0_signatures.push_back((new EidosFunctionSignature(gStr_initializeSex, EidosFunctionIdentifier::kDelegatedFunction, kValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM"))->AddString_S()->AddNumeric_OS());
		}
		
		return &sim_0_signatures;
	}
	
	return nullptr;
}

void SLiMSim::InjectIntoInterpreter(EidosInterpreter &p_interpreter, SLiMEidosBlock *p_script_block)
{
	EidosSymbolTable &global_symbols = p_interpreter.GetSymbolTable();
	
	// Note that we can use InitializeConstantSymbolEntry() here only because the objects we are setting are guaranteed by
	// SLiM's design to live longer than the symbol table we are injecting into!  Every new execution of a script block
	// makes a new symbol table that receives a fresh injection.  Nothing that happens during the execution of a script
	// block can invalidate existing entries during the execution of the block; subpopulations do not get deleted immediately,
	// deregistered script blocks stick around until the end of the script execution, etc.  This is a very important stake!
	// We also need to guarantee here that the values we are setting will not change for the lifetime of the symbol table;
	// the objects referred to by the values may change, but the EidosValue objects themselves will not.  Be careful!
	
	// A constant for reference to the SLiMEidosBlock, self
	if (p_script_block && p_script_block->contains_self_)
		global_symbols.InitializeConstantSymbolEntry(p_script_block->CachedSymbolTableEntry());
	
	// Add signatures for functions we define – initialize...() functions only, right now
	if (generation_ == 0)
	{
		std::vector<EidosFunctionSignature*> *signatures = InjectedFunctionSignatures();
		
		if (signatures)
		{
			// construct a new map based on the built-in map, add our functions, and register it, which gives the pointer to the interpreter
			// this is slow, but it doesn't matter; if we start adding functions outside of initialize time, this will need to be revisited
			EidosFunctionMap *derived_function_map = new EidosFunctionMap(*EidosInterpreter::BuiltInFunctionMap());
			
			for (EidosFunctionSignature *signature : *signatures)
				derived_function_map->insert(EidosFunctionMapPair(signature->function_name_, signature));
			
			p_interpreter.RegisterFunctionMap(derived_function_map);
		}
	}
	
	// Inject for generations > 0 : no initialize...() functions, but global symbols
	if (generation_ != 0)
	{
		// A constant for reference to the simulation, sim
		if (!p_script_block || p_script_block->contains_sim_)
			global_symbols.InitializeConstantSymbolEntry(CachedSymbolTableEntry());
		
		// Add constants for our genomic element types, like g1, g2, ...
		if (!p_script_block || p_script_block->contains_gX_)
			for (auto getype_pair : genomic_element_types_)
				global_symbols.InitializeConstantSymbolEntry(getype_pair.second->CachedSymbolTableEntry());
		
		// Add constants for our mutation types, like m1, m2, ...
		if (!p_script_block || p_script_block->contains_mX_)
			for (auto mut_type_pair : mutation_types_)
				global_symbols.InitializeConstantSymbolEntry(mut_type_pair.second->CachedSymbolTableEntry());
		
		// Add constants for our subpopulations, like p1, p2, ...
		if (!p_script_block || p_script_block->contains_pX_)
			for (auto pop_pair : population_)
				global_symbols.InitializeConstantSymbolEntry(pop_pair.second->CachedSymbolTableEntry());
		
		// Add constants for our scripts, like s1, s2, ...
		if (!p_script_block || p_script_block->contains_sX_)
			for (SLiMEidosBlock *script_block : script_blocks_)
				if (script_block->block_id_ != -1)					// add symbols only for non-anonymous blocks
					global_symbols.InitializeConstantSymbolEntry(script_block->CachedScriptBlockSymbolTableEntry());
	}
}

const std::string *SLiMSim::ElementType(void) const
{
	return &gStr_SLiMSim;
}

std::vector<std::string> SLiMSim::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = EidosObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_chromosome);			// chromosome_
	constants.push_back(gStr_chromosomeType);		// modeled_chromosome_type_
	constants.push_back(gStr_genomicElementTypes);	// genomic_element_types_
	constants.push_back(gStr_mutations);			// population_.mutation_registry_
	constants.push_back(gStr_mutationTypes);		// mutation_types_
	constants.push_back(gStr_scriptBlocks);		// script_blocks_
	constants.push_back(gStr_sexEnabled);			// sex_enabled_
	constants.push_back(gStr_start);				// time_start_
	constants.push_back(gStr_subpopulations);		// population_
	constants.push_back(gStr_substitutions);		// population_.substitutions_
	
	return constants;
}

std::vector<std::string> SLiMSim::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = EidosObjectElement::ReadWriteMembers();
	
	variables.push_back(gStr_dominanceCoeffX);		// x_chromosome_dominance_coeff_; settable only when we're modeling sex chromosomes
	variables.push_back(gStr_generation);			// generation_
	variables.push_back(gStr_tag);					// tag_value_
	
	return variables;
}

bool SLiMSim::MemberIsReadOnly(EidosGlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_chromosome:
		case gID_chromosomeType:
		case gID_genomicElementTypes:
		case gID_mutations:
		case gID_mutationTypes:
		case gID_scriptBlocks:
		case gID_sexEnabled:
		case gID_subpopulations:
		case gID_substitutions:
			return true;
			
			// variables
		case gID_dominanceCoeffX:
		case gID_generation:
		case gID_tag:
			return false;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::MemberIsReadOnly(p_member_id);
	}
}

EidosValue *SLiMSim::GetValueForMember(EidosGlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
			// constants
		case gID_chromosome:
			return new EidosValue_Object_singleton_const(&chromosome_);
		case gID_chromosomeType:
		{
			switch (modeled_chromosome_type_)
			{
				case GenomeType::kAutosome:		return new EidosValue_String(gStr_Autosome);
				case GenomeType::kXChromosome:	return new EidosValue_String(gStr_X_chromosome);
				case GenomeType::kYChromosome:	return new EidosValue_String(gStr_Y_chromosome);
			}
		}
		case gID_genomicElementTypes:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (auto ge_type = genomic_element_types_.begin(); ge_type != genomic_element_types_.end(); ++ge_type)
				vec->PushElement(ge_type->second);
			
			return vec;
		}
		case gID_mutations:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			Genome &mutation_registry = population_.mutation_registry_;
			int mutation_count = mutation_registry.size();
			
			for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
				vec->PushElement(mutation_registry[mut_index]);
			
			return vec;
		}
		case gID_mutationTypes:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (auto mutation_type = mutation_types_.begin(); mutation_type != mutation_types_.end(); ++mutation_type)
				vec->PushElement(mutation_type->second);
			
			return vec;
		}
		case gID_scriptBlocks:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (auto script_block = script_blocks_.begin(); script_block != script_blocks_.end(); ++script_block)
				vec->PushElement(*script_block);
			
			return vec;
		}
		case gID_sexEnabled:
			return (sex_enabled_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_subpopulations:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (auto pop = population_.begin(); pop != population_.end(); ++pop)
				vec->PushElement(pop->second);
			
			return vec;
		}
		case gID_substitutions:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (auto sub_iter = population_.substitutions_.begin(); sub_iter != population_.substitutions_.end(); ++sub_iter)
				vec->PushElement(*sub_iter);
			
			return vec;
		}
			
			// variables
		case gID_dominanceCoeffX:
			return new EidosValue_Float_singleton_const(x_chromosome_dominance_coeff_);
		case gID_generation:
		{
			// We use external-temporary here because the value of generation_ can change, but it is permanent enough that
			// we can cache it – it can't change within the execution of a single statement, and if the value lives longer
			// than the context of a single statement that means it has been placed into a symbol table, and thus copied.
			if (!cached_value_generation_)
				cached_value_generation_ = (new EidosValue_Int_singleton_const(generation_))->SetExternalTemporary();
			return cached_value_generation_;
		}
		case gID_tag:
			return new EidosValue_Int_singleton_const(tag_value_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetValueForMember(p_member_id);
	}
}

void SLiMSim::SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
		case gID_generation:
		{
			TypeCheckValue(__func__, p_member_id, p_value, kValueMaskInt);
			
			int64_t value = p_value->IntAtIndex(0);
			RangeCheckValue(__func__, p_member_id, (value >= 1) && (value <= 1000000000000L));
			
			if (cached_value_generation_)
			{
				delete cached_value_generation_;
				cached_value_generation_ = nullptr;
			}
			
			generation_ = (int)value;
			return;
		}
			
		case gID_dominanceCoeffX:
		{
			if (!sex_enabled_ || (modeled_chromosome_type_ != GenomeType::kXChromosome))
				EIDOS_TERMINATION << "ERROR (SLiMSim::SetValueForMember): attempt to set member dominanceCoeffX when not simulating an X chromosome." << eidos_terminate();
			
			TypeCheckValue(__func__, p_member_id, p_value, kValueMaskInt | kValueMaskFloat);
			
			double value = p_value->FloatAtIndex(0);
			
			x_chromosome_dominance_coeff_ = value;
			return;
		}
			
		case gID_tag:
		{
			TypeCheckValue(__func__, p_member_id, p_value, kValueMaskInt);
			
			int64_t value = p_value->IntAtIndex(0);
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetValueForMember(p_member_id, p_value);
	}
}

std::vector<std::string> SLiMSim::Methods(void) const
{
	std::vector<std::string> methods = EidosObjectElement::Methods();
	
	methods.push_back(gStr_addSubpop);
	methods.push_back(gStr_addSubpopSplit);
	methods.push_back(gStr_deregisterScriptBlock);
	methods.push_back(gStr_mutationFrequencies);
	methods.push_back(gStr_outputFixedMutations);
	methods.push_back(gStr_outputFull);
	methods.push_back(gStr_outputMutations);
	methods.push_back(gStr_readFromPopulationFile);
	methods.push_back(gStr_registerScriptEvent);
	methods.push_back(gStr_registerScriptFitnessCallback);
	methods.push_back(gStr_registerScriptMateChoiceCallback);
	methods.push_back(gStr_registerScriptModifyChildCallback);
	
	return methods;
}

const EidosFunctionSignature *SLiMSim::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosFunctionSignature *addSubpopSig = nullptr;
	static EidosFunctionSignature *addSubpopSplitSig = nullptr;
	static EidosFunctionSignature *deregisterScriptBlockSig = nullptr;
	static EidosFunctionSignature *mutationFrequenciesSig = nullptr;
	static EidosFunctionSignature *outputFixedMutationsSig = nullptr;
	static EidosFunctionSignature *outputFullSig = nullptr;
	static EidosFunctionSignature *outputMutationsSig = nullptr;
	static EidosFunctionSignature *readFromPopulationFileSig = nullptr;
	static EidosFunctionSignature *registerScriptEventSig = nullptr;
	static EidosFunctionSignature *registerScriptFitnessCallbackSig = nullptr;
	static EidosFunctionSignature *registerScriptMateChoiceCallbackSig = nullptr;
	static EidosFunctionSignature *registerScriptModifyChildCallbackSig = nullptr;
	
	if (!addSubpopSig)
	{
		addSubpopSig = (new EidosFunctionSignature(gStr_addSubpop, EidosFunctionIdentifier::kNoFunction, kValueMaskObject))->SetInstanceMethod()->AddInt_S()->AddInt_S()->AddFloat_OS();
		addSubpopSplitSig = (new EidosFunctionSignature(gStr_addSubpopSplit, EidosFunctionIdentifier::kNoFunction, kValueMaskObject))->SetInstanceMethod()->AddInt_S()->AddInt_S()->AddObject_S()->AddFloat_OS();
		deregisterScriptBlockSig = (new EidosFunctionSignature(gStr_deregisterScriptBlock, EidosFunctionIdentifier::kNoFunction, kValueMaskNULL))->SetInstanceMethod()->AddObject();
		mutationFrequenciesSig = (new EidosFunctionSignature(gStr_mutationFrequencies, EidosFunctionIdentifier::kNoFunction, kValueMaskFloat))->SetInstanceMethod()->AddObject()->AddObject_O();
		outputFixedMutationsSig = (new EidosFunctionSignature(gStr_outputFixedMutations, EidosFunctionIdentifier::kNoFunction, kValueMaskNULL))->SetInstanceMethod();
		outputFullSig = (new EidosFunctionSignature(gStr_outputFull, EidosFunctionIdentifier::kNoFunction, kValueMaskNULL))->SetInstanceMethod()->AddString_OS();
		outputMutationsSig = (new EidosFunctionSignature(gStr_outputMutations, EidosFunctionIdentifier::kNoFunction, kValueMaskNULL))->SetInstanceMethod()->AddObject();
		readFromPopulationFileSig = (new EidosFunctionSignature(gStr_readFromPopulationFile, EidosFunctionIdentifier::kNoFunction, kValueMaskNULL))->SetInstanceMethod()->AddString_S();
		registerScriptEventSig = (new EidosFunctionSignature(gStr_registerScriptEvent, EidosFunctionIdentifier::kNoFunction, kValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_OS()->AddInt_OS();
		registerScriptFitnessCallbackSig = (new EidosFunctionSignature(gStr_registerScriptFitnessCallback, EidosFunctionIdentifier::kNoFunction, kValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_S()->AddInt_OS()->AddInt_OS()->AddInt_OS();
		registerScriptMateChoiceCallbackSig = (new EidosFunctionSignature(gStr_registerScriptMateChoiceCallback, EidosFunctionIdentifier::kNoFunction, kValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_OS()->AddInt_OS()->AddInt_OS();
		registerScriptModifyChildCallbackSig = (new EidosFunctionSignature(gStr_registerScriptModifyChildCallback, EidosFunctionIdentifier::kNoFunction, kValueMaskObject))->SetInstanceMethod()->AddInt_SN()->AddString_S()->AddInt_OS()->AddInt_OS()->AddInt_OS();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_addSubpop:
			return addSubpopSig;
		case gID_addSubpopSplit:
			return addSubpopSplitSig;
		case gID_deregisterScriptBlock:
			return deregisterScriptBlockSig;
		case gID_mutationFrequencies:
			return mutationFrequenciesSig;
		case gID_outputFixedMutations:
			return outputFixedMutationsSig;
		case gID_outputFull:
			return outputFullSig;
		case gID_outputMutations:
			return outputMutationsSig;
		case gID_readFromPopulationFile:
			return readFromPopulationFileSig;
		case gID_registerScriptEvent:
			return registerScriptEventSig;
		case gID_registerScriptFitnessCallback:
			return registerScriptFitnessCallbackSig;
		case gID_registerScriptMateChoiceCallback:
			return registerScriptMateChoiceCallbackSig;
		case gID_registerScriptModifyChildCallback:
			return registerScriptModifyChildCallbackSig;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SignatureForMethod(p_method_id);
	}
}

EidosValue *SLiMSim::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2] : nullptr);
	EidosValue *arg3_value = ((p_argument_count >= 4) ? p_arguments[3] : nullptr);
	EidosValue *arg4_value = ((p_argument_count >= 5) ? p_arguments[4] : nullptr);
	EidosValue *arg5_value = ((p_argument_count >= 6) ? p_arguments[5] : nullptr);
	
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	- (object$)addSubpop(integer$ subpopID, integer$ size, [float$ sexRatio])
			//
#pragma mark -addSubpop()
			
		case gID_addSubpop:
		{
			int subpop_id = (int)arg0_value->IntAtIndex(0);
			int subpop_size = (int)arg1_value->IntAtIndex(0);
			double sex_ratio = (arg2_value ? arg2_value->FloatAtIndex(0) : 0.5);		// 0.5 is the default whenever sex is enabled and a ratio is not given
			
			// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
			Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, sex_ratio);
			
			return new_subpop->CachedSymbolTableEntry()->second;
		}
			
			
			//
			//	*********************	- (object$)addSubpopSplit(integer$ subpopID, integer$ size, object$ sourceSubpop, [float$ sexRatio])
			//
#pragma mark -addSubpopSplit()
			
		case gID_addSubpopSplit:
		{
			int subpop_id = (int)arg0_value->IntAtIndex(0);
			int subpop_size = (int)arg1_value->IntAtIndex(0);
			Subpopulation *source_subpop = (Subpopulation *)(arg2_value->ElementAtIndex(0));
			double sex_ratio = (arg3_value ? arg3_value->FloatAtIndex(0) : 0.5);		// 0.5 is the default whenever sex is enabled and a ratio is not given
			
			// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
			Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, *source_subpop, subpop_size, sex_ratio);
			
			return new_subpop->CachedSymbolTableEntry()->second;
		}
			
			
			//
			//	*********************	- (void)deregisterScriptBlock(object scriptBlocks)
			//
#pragma mark -deregisterScriptBlock()
			
		case gID_deregisterScriptBlock:
		{
			int block_count = arg0_value->Count();
			
			// We just schedule the blocks for deregistration; we do not deregister them immediately, because that would leave stale pointers lying around
			for (int block_index = 0; block_index < block_count; ++block_index)
				scheduled_deregistrations_.push_back((SLiMEidosBlock *)(arg0_value->ElementAtIndex(block_index)));
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (float)mutationFrequencies(object subpops, [object mutations])
			//
#pragma mark -mutationFrequencies()
			
		case gID_mutationFrequencies:
		{
			int total_genome_count = 0;
			
			// The code below blows away the reference counts kept by Mutation, which must be valid at the end of each generation for
			// SLiM's internal machinery to work properly, so for simplicity we require that we're in the parental generation.
			if (population_.child_generation_valid)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): mutationFrequencies() may only be called when the parental generation is active (before offspring generation)." << eidos_terminate();
			
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
				for (const std::pair<const int,Subpopulation*> &subpop_pair : population_)
					subpops_to_tally.push_back(subpop_pair.second);
			}
			else
			{
				// requested subpops, so get them
				int requested_subpop_count = arg0_value->Count();
				
				if (requested_subpop_count)
				{
					if (((EidosValue_Object *)arg0_value)->ElementType() != &gStr_Subpopulation)
						EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): mutationFrequencies() requires that subpops have object element type Subpopulation." << eidos_terminate();
					
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
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			double denominator = 1.0 / total_genome_count;
			
			if (arg1_value)
			{
				// a vector of mutations was given, so loop through them and take their tallies
				int arg1_count = arg1_value->Count();
				
				if (arg1_count)
				{
					if (((EidosValue_Object *)arg1_value)->ElementType() != &gStr_Mutation)
						EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): mutationFrequencies() requires that mutations has object element type Mutation." << eidos_terminate();
					
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
			
		case gID_outputFixedMutations:
		{
			EIDOS_OUTSTREAM << "#OUT: " << generation_ << " F " << endl;
			EIDOS_OUTSTREAM << "Mutations:" << endl;
			
			std::vector<Substitution*> &subs = population_.substitutions_;
			
			for (int i = 0; i < subs.size(); i++)
			{
				EIDOS_OUTSTREAM << i;				// used to have a +1; switched to zero-based
				subs[i]->print(EIDOS_OUTSTREAM);
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
				EIDOS_OUTSTREAM << "#OUT: " << generation_ << " A" << endl;
				population_.PrintAll(EIDOS_OUTSTREAM);
			}
			else if (p_argument_count == 1)
			{
				string outfile_path = arg0_value->StringAtIndex(0);
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
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): outputFull() could not open "<< outfile_path << endl << eidos_terminate();
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (void)outputMutations(object mutations)
			//
#pragma mark -outputMutations()
			
		case gID_outputMutations:
		{
			// Extract all of the Mutation objects in mutations; would be nice if there was a simpler way to do this
			EidosValue_Object *mutations_object = (EidosValue_Object *)arg0_value;
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
						EIDOS_OUTSTREAM << "#OUT: " << generation_ << " T p" << subpop_pair.first << " ";
						polymorphism_pair.second.print(EIDOS_OUTSTREAM, polymorphism_pair.first, false /* no id */);
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
			string file_path = arg0_value->StringAtIndex(0);
			
			InitializePopulationFromFile(file_path.c_str());
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (object$)registerScriptEvent(integer$ id, string$ source, [integer$ start], [integer$ end])
			//
#pragma mark -registerScriptEvent()
			
		case gID_registerScriptEvent:
		{
			int script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
			string script_string = arg1_value->StringAtIndex(0);
			int start_generation = (arg2_value ? (int)arg2_value->IntAtIndex(0) : 1);
			int end_generation = (arg3_value ? (int)arg3_value->IntAtIndex(0) : INT_MAX);
			
			if (arg0_value->Type() != EidosValueType::kValueNULL)
			{
				script_id = (int)arg0_value->IntAtIndex(0);
				
				if (script_id < -1)
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptEvent() requires an id >= 0 (or -1, or NULL)." << endl << eidos_terminate();
			}
			
			if ((start_generation < 0) || (end_generation < 0) || (start_generation > end_generation))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires start <= end, and both values must be >= 0." << endl << eidos_terminate();
			
			SLiMEidosBlock *script_block = new SLiMEidosBlock(script_id, script_string, SLiMEidosBlockType::SLiMEidosEvent, start_generation, end_generation);
			
			script_blocks_.push_back(script_block);		// takes ownership form us
			scripts_changed_ = true;
			
			return script_block->CachedSymbolTableEntry()->second;
		}
			
			
			//
			//	*********************	- (object$)registerScriptFitnessCallback(integer$ id, string$ source, integer$ mutTypeID, [integer$ subpopID], [integer$ start], [integer$ end])
			//
#pragma mark -registerScriptFitnessCallback()
			
		case gID_registerScriptFitnessCallback:
		{
			int script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
			string script_string = arg1_value->StringAtIndex(0);
			int mut_type_id = (int)arg2_value->IntAtIndex(0);
			int subpop_id = -1;
			int start_generation = (arg4_value ? (int)arg4_value->IntAtIndex(0) : 1);
			int end_generation = (arg5_value ? (int)arg5_value->IntAtIndex(0) : INT_MAX);
			
			if (arg0_value->Type() != EidosValueType::kValueNULL)
			{
				script_id = (int)arg0_value->IntAtIndex(0);
				
				if (script_id < -1)
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an id >= 0 (or -1, or NULL)." << endl << eidos_terminate();
			}
			
			if (mut_type_id < 0)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires a mutTypeID >= 0." << endl << eidos_terminate();
			
			if (arg3_value && (arg3_value->Type() != EidosValueType::kValueNULL))
			{
				subpop_id = (int)arg3_value->IntAtIndex(0);
				
				if (subpop_id < -1)
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an subpopID >= 0 (or -1, or NULL)." << endl << eidos_terminate();
			}
			
			if ((start_generation < 0) || (end_generation < 0) || (start_generation > end_generation))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires start <= end, and both values must be >= 0." << endl << eidos_terminate();
			
			SLiMEidosBlock *script_block = new SLiMEidosBlock(script_id, script_string, SLiMEidosBlockType::SLiMEidosFitnessCallback, start_generation, end_generation);
			
			script_block->mutation_type_id_ = mut_type_id;
			script_block->subpopulation_id_ = subpop_id;
			
			script_blocks_.push_back(script_block);		// takes ownership form us
			scripts_changed_ = true;
			
			return script_block->CachedSymbolTableEntry()->second;
		}
			
			
			//
			//	*********************	- (object$)registerScriptMateChoiceCallback(integer$ id, string$ source, [integer$ subpopID], [integer$ start], [integer$ end])
			//	*********************	- (object$)registerScriptModifyChildCallback(integer$ id, string$ source, [integer$ subpopID], [integer$ start], [integer$ end])
			//
#pragma mark -registerScriptMateChoiceCallback()
#pragma mark -registerScriptModifyChildCallback()
			
		case gID_registerScriptMateChoiceCallback:
		case gID_registerScriptModifyChildCallback:
		{
			int script_id = -1;		// used if the arg0 is NULL, to indicate an anonymous block
			string script_string = arg1_value->StringAtIndex(0);
			int subpop_id = -1;
			int start_generation = (arg3_value ? (int)arg3_value->IntAtIndex(0) : 1);
			int end_generation = (arg4_value ? (int)arg4_value->IntAtIndex(0) : INT_MAX);
			
			if (arg0_value->Type() != EidosValueType::kValueNULL)
			{
				script_id = (int)arg0_value->IntAtIndex(0);
				
				if (script_id < -1)
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an id >= 0 (or -1, or NULL)." << endl << eidos_terminate();
			}
			
			if (arg2_value && (arg2_value->Type() != EidosValueType::kValueNULL))
			{
				subpop_id = (int)arg2_value->IntAtIndex(0);
				
				if (subpop_id < -1)
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires an subpopID >= 0 (or -1, or NULL)." << endl << eidos_terminate();
			}
			
			if ((start_generation < 0) || (end_generation < 0) || (start_generation > end_generation))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod): registerScriptFitnessCallback() requires start <= end, and both values must be >= 0." << endl << eidos_terminate();
			
			SLiMEidosBlockType block_type = ((p_method_id == gID_registerScriptMateChoiceCallback) ? SLiMEidosBlockType::SLiMEidosMateChoiceCallback : SLiMEidosBlockType::SLiMEidosModifyChildCallback);
			SLiMEidosBlock *script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_generation, end_generation);
			
			script_block->subpopulation_id_ = subpop_id;
			
			script_blocks_.push_back(script_block);		// takes ownership form us
			scripts_changed_ = true;
			
			return script_block->CachedSymbolTableEntry()->second;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}
































































