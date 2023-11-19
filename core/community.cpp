//
//  community.cpp
//  SLiM
//
//  Created by Ben Haller on 2/28/2022.
//  Copyright (c) 2022-2023 Philipp Messer.  All rights reserved.
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


#include "community.h"
#include "species.h"
#include "slim_functions.h"
#include "eidos_test.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"
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
#include <map>
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


#pragma mark -
#pragma mark Community
#pragma mark -

Community::Community(void) : self_symbol_(gID_community, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Community_Class)))
{
	// BCH 3/16/2022: We used to allocate the Species object here, as the first thing we did.  In SLiM 4 there can
	// be multiple species and they can have names other than "sim", so we delay species creation until parse time.
	
	// set up the symbol tables we will use for global variables and constants; note that the global variables table
	// lives *above* the context constants table, which is fine since they cannot define the same symbol anyway
	// this satisfies Eidos, which expects the child of the intrinsic constants table to be the global variables table
	simulation_globals_ = new EidosSymbolTable(EidosSymbolTableType::kGlobalVariablesTable, gEidosConstantsSymbolTable);
	simulation_constants_ = new EidosSymbolTable(EidosSymbolTableType::kContextConstantsTable, simulation_globals_);
	
	// set up the function map with the base Eidos functions plus zero-gen functions, since we're in an initial state
	simulation_functions_ = *EidosInterpreter::BuiltInFunctionMap();
	AddZeroTickFunctionsToMap(simulation_functions_);
	AddSLiMFunctionsToMap(simulation_functions_);
	
	// reading from the input file is deferred to InitializeFromFile() to make raise-handling simpler - finish construction
}

Community::~Community(void)
{
	//EIDOS_ERRSTREAM << "Community::~Community" << std::endl;
	
	all_mutation_types_.clear();
	all_genomic_element_types_.clear();
	
	for (auto interaction_type : interaction_types_)
		delete interaction_type.second;
	interaction_types_.clear();
	
	for (Species *species : all_species_)
		delete species;
	
	delete simulation_globals_;
	simulation_globals_ = nullptr;
	
	delete simulation_constants_;
	simulation_constants_ = nullptr;
	
	simulation_functions_.clear();
	
	for (auto script_block : script_blocks_)
		delete script_block;
	script_blocks_.clear();
	
	// All the script blocks that refer to the script are now gone
	delete script_;
	script_ = nullptr;
}

void Community::InitializeRNGFromSeed(unsigned long int *p_override_seed_ptr)
{
	// track the random number seed given, if there is one
	unsigned long int rng_seed = (p_override_seed_ptr ? *p_override_seed_ptr : Eidos_GenerateRNGSeed());
	
	Eidos_SetRNGSeed(rng_seed);
	
	if (SLiM_verbosity_level >= 1)
		SLIM_OUTSTREAM << "// Initial random seed:\n" << rng_seed << "\n" << std::endl;
	
	// remember the original seed for .trees provenance
	original_seed_ = rng_seed;
}

void Community::InitializeFromFile(std::istream &p_infile)
{
	p_infile.clear();
	p_infile.seekg(0, std::fstream::beg);
	
	// Reset error position indicators used by SLiMgui
	ClearErrorPosition();
	
	// Read in the file; going through stringstream is fast...
	std::stringstream buffer;
	
	buffer << p_infile.rdbuf();
	
	// Tokenize and parse
	// BCH 5/1/2019: Note that this script_ variable may leak if tokenization/parsing raises below, because this method
	// is called while the Community constructor is still in progress, so the destructor is not called to clean up.  But
	// we can't actually clean up this variable, because it is used by SLiMAssertScriptRaise() to diagnose where the raise
	// occurred in the user's script; we'd have to redesign that code to fix this leak.  So be it.  It's not a large leak.
	script_ = new SLiMEidosScript(buffer.str());
	
	// Set up top-level error-reporting info
	gEidosErrorContext.currentScript = script_;
	gEidosErrorContext.executingRuntimeScript = false;
	
	script_->Tokenize();
	script_->ParseSLiMFileToAST();
	
	const EidosASTNode *root_node = script_->AST();
	
	// BCH 3/16/2022: The logic here used to be quite simple: loop over the parsed AST and make script blocks.  In SLiM 4
	// the top-level file structure is more complicated, because of species and ticks specifiers that can modify the
	// declared blocks.  Rather than making those part of the EidosASTNodes for the blocks themselves (which already have
	// a very complex internal structure), I have chosen to make them separate top-level nodes that modify the meaning
	// of the SLiMEidosBlock node that follows them.  That makes doing the parse, validating the file structure, creating
	// the species objects, and then creating the script blocks fairly complex.  That complexity is handled here.  Since
	// the logic here is rather complex, I have put in redundant checks to try to make sure nothing falls between the
	// cracks.  Errors that start with "(internal error)" should never be hit, as usual; those error cases ought to be
	// caught by earlier checks, unless I have missed a possible code path.
	
	// Assess the top-level structure and enforce semantics that can be enforced before knowing species names/declarations
	// Species are declared with initialize() callbacks of the form "species <identifier> initialize()"
	bool pending_species_spec = false, pending_ticks_spec = false;
	std::string pending_spec_species_name;
	std::vector<std::string> explicit_species_decl_names;		// names from "species <name> initialize()" declarations
	int implied_species_decl_count = 0;							// number of "initialize()" seen without "species <name>"
	
	for (EidosASTNode *script_block_node : root_node->children_)
	{
		if (script_block_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
		{
			// If we already have a pending specifier then we now have two specifiers in a row
			if (pending_species_spec)
				EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): a species specifier must be followed by a callback declaration." << EidosTerminate(script_block_node->token_);
			if (pending_ticks_spec)
				EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): a ticks specifier must be followed by an event declaration." << EidosTerminate(script_block_node->token_);
			
			if (script_block_node->children_.size() == 1)
			{
				pending_spec_species_name = script_block_node->children_[0]->token_->token_string_;
				
				if (script_block_node->token_->token_string_.compare(gStr_species) == 0)
				{
					//std::cout << "species specifier seen: " << pending_spec_species_name << std::endl;
					pending_species_spec = true;
					continue;
				}
				else if (script_block_node->token_->token_string_.compare(gStr_ticks) == 0)
				{
					//std::cout << "ticks specifier seen: " << pending_spec_species_name << std::endl;
					pending_ticks_spec = true;
					continue;
				}
			}
			
			EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): unexpected top-level token " << script_block_node->token_->token_string_ << "." << EidosTerminate(script_block_node->token_);
		}
		else
		{
			SLiMEidosBlockType type = SLiMEidosBlock::BlockTypeForRootNode(script_block_node);
			
			if (type == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
			{
				if (pending_species_spec || pending_ticks_spec)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): user-defined functions may not be preceded by a species or ticks specifier." << EidosTerminate(script_block_node->token_);
			}
			else if ((type == SLiMEidosBlockType::SLiMEidosEventFirst) || (type == SLiMEidosBlockType::SLiMEidosEventEarly) || (type == SLiMEidosBlockType::SLiMEidosEventLate))
			{
				if (pending_species_spec)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): event declarations may not be preceded by a species specifier; use a ticks specifier to designate an event as running only in the ticks when a particular species is active." << EidosTerminate(script_block_node->token_);
			}
			else if (type != SLiMEidosBlockType::SLiMEidosNoBlockType)	// callbacks
			{
				if (pending_ticks_spec)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): callback declarations may not be preceded by a ticks specifier; use a species specifier to designate a callback as being associated with a particular species." << EidosTerminate(script_block_node->token_);
				
				if (type == SLiMEidosBlockType::SLiMEidosInitializeCallback)
				{
					if (pending_species_spec)
					{
						// We have an explicit species declaration, "species <name> initialize()", so this is a multispecies model
						if (implied_species_decl_count > 0)
							EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): an initialize() callback without a species specifier has previously been seen, so this is a single-species script, and therefore species specifiers are illegal." << EidosTerminate(script_block_node->token_);
						
						// You can have multiple initialize() callbacks for a given species, but we want to tally the name just once; could use std::set instead but we want ordering
						// Note that `species all` is logged as a species name here; we will handle it separately below
						if (std::find(explicit_species_decl_names.begin(), explicit_species_decl_names.end(), pending_spec_species_name) == explicit_species_decl_names.end())
							explicit_species_decl_names.push_back(pending_spec_species_name);
					}
					else
					{
						// We have an implicit species declaration, "initialize()", so this is a single-species model
						if (explicit_species_decl_names.size() > 0)
							EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): an initialize() callback with a species specifier has previously been seen, so this is a multi-species script, and therefore species specifiers are required." << EidosTerminate(script_block_node->token_);
						
						implied_species_decl_count++;
					}
				}
				else if (type == SLiMEidosBlockType::SLiMEidosInteractionCallback)
				{
					if (pending_species_spec && (pending_spec_species_name != "all"))
						EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): interaction() callbacks must be declared with 'species all' in multispecies models; they are never species-specific." << EidosTerminate(script_block_node->children_[0]->token_);
				}
				else	// all other callback types
				{
					if (pending_species_spec && (pending_spec_species_name == "all"))
						EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): " << type << " callbacks may not be declared with 'species all'; they are always species-specific." << EidosTerminate(script_block_node->children_[0]->token_);
				}
			}
			
			//std::cout << "script block seen of type " << type << std::endl;
			
			pending_species_spec = false;
			pending_ticks_spec = false;
		}
	}
	
	// Create species objects for each declared species, or "sim" if there was only an implied species declaration
	if ((implied_species_decl_count > 0) && (explicit_species_decl_names.size() > 0))
		EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): (internal error) all initialize() callbacks must either (1) be preceded by a species specifier, for multi-species models, or (2) not be preceded by a species specifier, for single-species models." << EidosTerminate(nullptr);
	if ((implied_species_decl_count == 0) && (explicit_species_decl_names.size() == 0))
		EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): no initialize() callback found; at least one initialize() callback is required in all SLiM scripts." << EidosTerminate(nullptr);
	if ((explicit_species_decl_names.size() == 1) && (explicit_species_decl_names[0] == "all"))
		EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): no species-specific initialize() callback found; at least one species-specific initialize() callback is required in all SLiM scripts." << EidosTerminate(nullptr);
	
	if (implied_species_decl_count > 0)
	{
		// This is the single-species case; create a species named "sim"
		all_species_.push_back(new Species(*this, 0, gStr_sim));
		
		is_explicit_species_ = false;
	}
	else
	{
		// This is the multi-species case; create a species for each explicit declaration except `species all`
		int species_id = 0;
		
		for (std::string &species_name : explicit_species_decl_names)
			if (species_name != "all")
				all_species_.push_back(new Species(*this, species_id++, species_name));
		
		is_explicit_species_ = true;
	}
	
	// Extract SLiMEidosBlocks from the parse tree
	Species *last_species_spec = nullptr, *last_ticks_spec = nullptr;
	bool last_spec_is_ticks_all = false;	// "ticks all" is a special syntax; there is no species named "all" so it must be tracked with a separate flag
	bool last_spec_is_species_all = false;	// "species all" is a special syntax; there is no species named "all" so it must be tracked with a separate flag
	
	for (EidosASTNode *script_block_node : root_node->children_)
	{
		if ((script_block_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (script_block_node->children_.size() == 1))
		{
			// A "species <identifier>" or "ticks <identifier>" specification is present; remember what it specified
			EidosASTNode *child = script_block_node->children_[0];
			const std::string &species_name = child->token_->token_string_;
			bool species_is_all = (species_name.compare("all") == 0);
			Species *species = (species_is_all ? nullptr : SpeciesWithName(species_name));
			
			if (!species && !species_is_all)
				EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): undeclared species name " << species_name << "; species must be explicitly declared with a species <name> specifier on an initialize() block." << EidosTerminate(child->token_);
			
			if (script_block_node->token_->token_string_.compare(gStr_species) == 0)
			{
				if (!is_explicit_species_)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): no species have been explicitly declared, so species specifiers should not be used." << EidosTerminate(script_block_node->token_);
				
				last_species_spec = species;
				last_spec_is_species_all = species_is_all;
			}
			else if (script_block_node->token_->token_string_.compare(gStr_ticks) == 0)
			{
				if (!is_explicit_species_)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): no species have been explicitly declared, so ticks specifiers should not be used." << EidosTerminate(script_block_node->token_);
				
				last_ticks_spec = species;
				last_spec_is_ticks_all = species_is_all;
			}
		}
		else
		{
			SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_block_node);
			
			if (new_script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
			{
				// User-defined functions may not have a species or ticks specifier preceding them; this was already checked above
				if (last_species_spec || last_ticks_spec || last_spec_is_ticks_all || last_spec_is_species_all)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): (internal error) user-defined functions may not be preceded by a species or ticks specifier." << EidosTerminate(new_script_block->root_node_->token_);
			}
			else if ((new_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventFirst) ||
				(new_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventEarly) ||
				(new_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventLate))
			{
				// Events may have a ticks specifier, but not a species specifier, preceding them; this was already checked above
				if (last_species_spec || last_spec_is_species_all)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): (internal error) event declarations may not be preceded by a species specifier." << EidosTerminate(new_script_block->root_node_->token_);
				
				if (is_explicit_species_)
				{
					Species *block_ticks = last_ticks_spec;
					
					if (!block_ticks && !last_spec_is_ticks_all)
						EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): when species names have been explicitly declared (such as in multispecies models), every event must be preceded by a ticks specifier of the form 'ticks <species-name>'; if you want an event to run in every tick, specify 'ticks all'." << EidosTerminate(new_script_block->root_node_->token_);
					
					new_script_block->ticks_spec_ = block_ticks;	// nullptr for "ticks all"
				}
				else
				{
					new_script_block->ticks_spec_ = nullptr;
				}
			}
			else
			{
				// Callbacks of all types may not be preceded by a ticks specifier; this was already checked above
				if (last_ticks_spec || last_spec_is_ticks_all)
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): (internal error) callback declarations may not be preceded by a ticks specifier." << EidosTerminate(new_script_block->root_node_->token_);
				
				// Callbacks of all types may not be preceded by a species specifier in single-species models; this was already checked above
				if (!is_explicit_species_ && (last_species_spec || last_spec_is_species_all))
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): (internal error) callback declarations may not be preceded by a species specifier in single-species models." << EidosTerminate(new_script_block->root_node_->token_);
				
				// Callbacks of all types must be preceded by a species specifier in multispecies models
				if (is_explicit_species_ && !(last_species_spec || last_spec_is_species_all))
					EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): when species names have been explicitly declared (as in multispecies models), every callback must be preceded by a species specifier of the form 'species <species-name>'; for non-species-specific initialize() and interaction() callbacks, specify 'species all'." << EidosTerminate(new_script_block->root_node_->token_);
				
				Species *block_species = (is_explicit_species_ ? last_species_spec : all_species_[0]);	// nullptr for `species all`
				
				if (new_script_block->type_ == SLiMEidosBlockType::SLiMEidosInitializeCallback)
				{
					// In multispecies models, initialize() callbacks may be `species all` or `species name`; no action needed
				}
				else if (new_script_block->type_ == SLiMEidosBlockType::SLiMEidosInteractionCallback)
				{
					// interaction() callbacks must be "species all"; this was already checked above
					if (is_explicit_species_ && block_species)
						EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): (internal error) interaction() callbacks in multispecies models must be declared with 'species all'; they are never species-specific." << EidosTerminate(new_script_block->root_node_->token_);
					
					// In single-species models, the above default needs correction
					if (!is_explicit_species_)
						block_species = nullptr;
				}
				else
				{
					// Other callback types may not be `species all`; this was already checked above
					if (last_spec_is_species_all)
						EIDOS_TERMINATION << "ERROR (Community::InitializeFromFile): (internal error) " << new_script_block->type_ << " callbacks may not be declared with 'species all'; they are always species-specific." << EidosTerminate(new_script_block->root_node_->token_);
				}
				
				new_script_block->species_spec_ = block_species;
			}
			
			AddScriptBlock(new_script_block, nullptr, new_script_block->root_node_->children_[0]->token_);
			
			last_species_spec = nullptr;
			last_ticks_spec = nullptr;
			last_spec_is_ticks_all = false;
			last_spec_is_species_all = false;
		}
	}
	
	// Reset error position indicators used by SLiMgui
	ClearErrorPosition();
	
	// Zero out error-reporting info so raises elsewhere don't get attributed to this script
	gEidosErrorContext.currentScript = nullptr;
	gEidosErrorContext.executingRuntimeScript = false;
}

void Community::ValidateScriptBlockCaches(void)
{
#if DEBUG_BLOCK_REG_DEREG
	std::cout << "Tick " << tick_ << ": ValidateScriptBlockCaches() called..." << std::endl;
#endif
	
	if (!script_block_types_cached_)
	{
		cached_first_events_.clear();
		cached_early_events_.clear();
		cached_late_events_.clear();
		cached_initialize_callbacks_.clear();
		cached_mutationEffect_callbacks_.clear();
		cached_fitnessEffect_callbacks_onetick_.clear();
		cached_fitnessEffect_callbacks_multitick_.clear();
		cached_interaction_callbacks_.clear();
		cached_matechoice_callbacks_.clear();
		cached_modifychild_callbacks_.clear();
		cached_recombination_callbacks_.clear();
		cached_mutation_callbacks_.clear();
		cached_survival_callbacks_.clear();
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
				case SLiMEidosBlockType::SLiMEidosEventFirst:				cached_first_events_.emplace_back(script_block);				break;
				case SLiMEidosBlockType::SLiMEidosEventEarly:				cached_early_events_.emplace_back(script_block);				break;
				case SLiMEidosBlockType::SLiMEidosEventLate:				cached_late_events_.emplace_back(script_block);					break;
				case SLiMEidosBlockType::SLiMEidosInitializeCallback:		cached_initialize_callbacks_.emplace_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	cached_mutationEffect_callbacks_.emplace_back(script_block);	break;
				case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:
				{
					// Note fitnessEffect() callbacks are not order-dependent, so we don't have to preserve their order
					// of declaration the way we do with other types of callbacks.  This allows us to be very efficient
					// in how we look them up, which is good since sometimes we have a very large number of them.
					// We put those that are registered for just a single tick in a separate vector, which we sort
					// by tick, allowing us to look them up quickly
					slim_tick_t start = script_block->start_tick_;
					slim_tick_t end = script_block->end_tick_;
					
					if (start == end)
					{
						cached_fitnessEffect_callbacks_onetick_.emplace(start, script_block);
					}
					else
					{
						cached_fitnessEffect_callbacks_multitick_.emplace_back(script_block);
					}
					break;
				}
				case SLiMEidosBlockType::SLiMEidosInteractionCallback:		cached_interaction_callbacks_.emplace_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		cached_matechoice_callbacks_.emplace_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		cached_modifychild_callbacks_.emplace_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	cached_recombination_callbacks_.emplace_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosMutationCallback:			cached_mutation_callbacks_.emplace_back(script_block);			break;
				case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			cached_survival_callbacks_.emplace_back(script_block);			break;
				case SLiMEidosBlockType::SLiMEidosReproductionCallback:		cached_reproduction_callbacks_.emplace_back(script_block);		break;
				case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		cached_userdef_functions_.emplace_back(script_block);			break;
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

std::vector<SLiMEidosBlock*> Community::ScriptBlocksMatching(slim_tick_t p_tick, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id, Species *p_species)
{
	if (!script_block_types_cached_)
		ValidateScriptBlockCaches();
	
	std::vector<SLiMEidosBlock*> *block_list = nullptr;
	
	switch (p_event_type)
	{
		case SLiMEidosBlockType::SLiMEidosEventFirst:				block_list = &cached_first_events_;							break;
		case SLiMEidosBlockType::SLiMEidosEventEarly:				block_list = &cached_early_events_;							break;
		case SLiMEidosBlockType::SLiMEidosEventLate:				block_list = &cached_late_events_;							break;
		case SLiMEidosBlockType::SLiMEidosInitializeCallback:		block_list = &cached_initialize_callbacks_;					break;
		case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	block_list = &cached_mutationEffect_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	block_list = &cached_fitnessEffect_callbacks_multitick_;	break;
		case SLiMEidosBlockType::SLiMEidosInteractionCallback:		block_list = &cached_interaction_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		block_list = &cached_matechoice_callbacks_;					break;
		case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		block_list = &cached_modifychild_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	block_list = &cached_recombination_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosMutationCallback:			block_list = &cached_mutation_callbacks_;					break;
		case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			block_list = &cached_survival_callbacks_;					break;
		case SLiMEidosBlockType::SLiMEidosReproductionCallback:		block_list = &cached_reproduction_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		block_list = &cached_userdef_functions_;					break;
		case SLiMEidosBlockType::SLiMEidosNoBlockType:				break;	// never hit
	}
	
	std::vector<SLiMEidosBlock*> matches;
	
	for (SLiMEidosBlock *script_block : *block_list)
	{
		// check that the tick is in range
		if ((script_block->start_tick_ > p_tick) || (script_block->end_tick_ < p_tick))
			continue;
		
		// check that the script type matches (event, callback, etc.) - now guaranteed by the caching mechanism
		//if (script_block->type_ != p_event_type)
		//	continue;
		
		// check that the mutation type id matches, if requested
		if (p_mutation_type_id != -1)
		{
			slim_objectid_t mutation_type_id = script_block->mutation_type_id_;
			
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
		
		// check that the species matches; this check is always on, nullptr means check that the species is nullptr
		if (p_species != script_block->species_spec_)
			continue;
		
		// OK, everything matches, so we want to return this script block
		matches.emplace_back(script_block);
	}
	
	// add in any single-tick fitnessEffect() callbacks
	if (p_event_type == SLiMEidosBlockType::SLiMEidosFitnessEffectCallback)
	{
		auto find_range = cached_fitnessEffect_callbacks_onetick_.equal_range(p_tick);
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
			
			// check that the species matches; this check is always on, nullptr means check that the species is nullptr
			if (p_species != script_block->species_spec_)
				continue;
			
			// OK, everything matches, so we want to return this script block
			matches.emplace_back(script_block);
		}
	}
	
	return matches;
}

std::vector<SLiMEidosBlock*> &Community::AllScriptBlocks()
{
	return script_blocks_;
}

std::vector<SLiMEidosBlock*> Community::AllScriptBlocksForSpecies(Species *p_species)
{
	std::vector<SLiMEidosBlock*> species_blocks;
	
	for (SLiMEidosBlock *block : script_blocks_)
		if (block->species_spec_ == p_species)
			species_blocks.push_back(block);
	
	return species_blocks;
}

void Community::OptimizeScriptBlock(SLiMEidosBlock *p_script_block)
{
	// The goal here is to look for specific structures in callbacks that we are able to optimize by short-circuiting
	// the callback interpretation entirely and replacing it with equivalent C++ code.  This is extremely messy, so
	// we're not going to do this for very many cases, but sometimes it is worth it.
	if (!p_script_block->has_cached_optimization_)
	{
		if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosFitnessEffectCallback)
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
		else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosMutationEffectCallback)
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
							
							if ((denominator_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (denominator_node->token_->token_string_ == "effect"))
							{
								// callback of the form { return A/effect; }
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

void Community::AddScriptBlock(SLiMEidosBlock *p_script_block, EidosInterpreter *p_interpreter, const EidosToken *p_error_token)
{
	script_blocks_.emplace_back(p_script_block);
	
	p_script_block->TokenizeAndParse();	// can raise
	
	// Check for the presence/absence of a species specifier, as required by the block type
	if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosNoBlockType)
	{
		EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) attempted add of a script block of type SLiMEidosNoBlockType." << EidosTerminate(p_error_token);
	}
	else if ((p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventFirst) ||
		(p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventEarly) ||
		(p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventLate) ||
		(p_script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction))
	{
		if (p_script_block->species_spec_)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for an event or user-defined function has a species set." << EidosTerminate(p_error_token);
	}
	else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosInitializeCallback)
	{
		// with explicit species, initialize() callbacks may be species-specific or not, both are allowed; without explicit species, they must be species-specific (to `sim`)
		if (!is_explicit_species_ && !p_script_block->species_spec_)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for an initialize() callback in a single-species model has no species set." << EidosTerminate(p_error_token);
	}
	else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosInteractionCallback)
	{
		// interaction() callbacks are always non-species-specific
		if (p_script_block->species_spec_)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for an interaction() callback has a species set." << EidosTerminate(p_error_token);
	}
	else if (!p_script_block->species_spec_)
	{
		EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for a callback has no species set." << EidosTerminate(p_error_token);
	}
	
	// SPECIES CONSISTENCY CHECK
	if (p_script_block->species_spec_)
	{
		bool species_has_initialized = (p_script_block->species_spec_->Cycle() >= 1);
		
		if (p_script_block->mutation_type_id_ >= 0)
		{
			// if the mutation type exists now, we check that it belongs to the specified species
			MutationType *muttype = MutationTypeWithID(p_script_block->mutation_type_id_);
			
			if (species_has_initialized && !muttype)
				EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): script block is specific to a mutation type id (" << p_script_block->mutation_type_id_ << ") that does not exist." << EidosTerminate(p_error_token);
			
			if (muttype && (&muttype->species_ != p_script_block->species_spec_))
				EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): script block is specific to a mutation type id (" << p_script_block->mutation_type_id_ << ") that belongs to a different species." << EidosTerminate(p_error_token);
		}
		
		if (p_script_block->subpopulation_id_ >= 0)
		{
			// if the subpopulation exists now, we check that it belongs to the specified species
			Subpopulation *subpop = SubpopulationWithID(p_script_block->subpopulation_id_);
			
			// cannot error out if the subpopulation does not exist, since subpopulations are dynamic
			
			if (subpop && (&subpop->species_ != p_script_block->species_spec_))
				EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): script block is specific to a subpopulation id (" << p_script_block->subpopulation_id_ << ") that belongs to a different species." << EidosTerminate(p_error_token);
		}
		
		if (p_script_block->interaction_type_id_ >= 0)
		{
			// interaction() callbacks may not have a specified species
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block with interaction_type_id_ set has a specified species." << EidosTerminate(p_error_token);
		}
		
		if (p_script_block->sex_specificity_ != IndividualSex::kUnspecified)
		{
			// if the species has been initialized, we check that it is sexual if necessary
			if (p_script_block->type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
				EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for a non-reproduction() callback has sex_specificity_ set." << EidosTerminate(p_error_token);
			
			if (species_has_initialized && !p_script_block->species_spec_->SexEnabled())
				EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for a reproduction() callback has sex_specificity_ set, but the specified species is not sexual." << EidosTerminate(p_error_token);
		}
	}
	else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosInteractionCallback)
	{
		// interaction() callbacks are weird; they are callbacks that are non-species-specific, so they must be checked separately
		if (p_script_block->mutation_type_id_ != -1)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for an interaction() callback has mutation_type_id_ set." << EidosTerminate(p_error_token);
		
		if (p_script_block->sex_specificity_ != IndividualSex::kUnspecified)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for an interaction() callback has sex_specificity_ set." << EidosTerminate(p_error_token);
	}
	else
	{
		// At this point we have an event, a user-defined function, or a non-species-specific initialize() callback, and no other specifier should be set
		if (p_script_block->mutation_type_id_ != -1)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for a non-callback or initialize() callback has mutation_type_id_ set." << EidosTerminate(p_error_token);
		
		if (p_script_block->subpopulation_id_ != -1)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for a non-callback or initialize() callback has subpopulation_id_ set." << EidosTerminate(p_error_token);
		
		if (p_script_block->interaction_type_id_ != -1)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for a non-callback or initialize() callback has interaction_type_id_ set." << EidosTerminate(p_error_token);
		
		if (p_script_block->sex_specificity_ != IndividualSex::kUnspecified)
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): (internal error) script block for a non-callback or initialize() callback has sex_specificity_ set." << EidosTerminate(p_error_token);
	}
	
	// The script block passed tokenization and parsing, so it is reasonably well-formed.  Now we check for cases we optimize.
	OptimizeScriptBlock(p_script_block);
	
	// Define the symbol for the script block, if any
	if (p_script_block->block_id_ != -1)
	{
		EidosSymbolTableEntry &symbol_entry = p_script_block->ScriptBlockSymbolTableEntry();
		EidosGlobalStringID symbol_id = symbol_entry.first;
		
		if ((simulation_constants_->ContainsSymbol(symbol_id)) || (p_interpreter && p_interpreter->SymbolTable().ContainsSymbol(symbol_id)))
			EIDOS_TERMINATION << "ERROR (Community::AddScriptBlock): script block symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate(p_error_token);
		
		simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	}
	
	// Notify the various interested parties that the script blocks have changed
	last_script_block_tick_cached_ = false;
	script_block_types_cached_ = false;
	scripts_changed_ = true;
	
#if DEBUG_BLOCK_REG_DEREG
	std::cout << "Tick " << tick_ << ": AddScriptBlock() just added a block, script_blocks_ is:" << std::endl;
	for (SLiMEidosBlock *script_block : script_blocks_)
	{
		std::cout << "      ";
		script_block->Print(std::cout);
		std::cout << std::endl;
	}
#endif
	
#ifdef SLIMGUI
	if (p_interpreter)		// not when initializing the community from script
	{
		gSLiMScheduling << "\t\tnew script block registered: ";
		p_script_block->PrintDeclaration(gSLiMScheduling, this);
		gSLiMScheduling << std::endl;
	}
#endif
}

void Community::DeregisterScheduledScriptBlocks(void)
{
	// If we have blocks scheduled for deregistration, we sweep through and deregister them at the end of each stage of each tick.
	// This happens at a time when script blocks are not executing, so that we're guaranteed not to leave hanging pointers that could
	// cause a crash; it also guarantees that script blocks are applied consistently across each cycle stage.  A single block
	// might be scheduled for deregistration more than once, but should only occur in script_blocks_ once, so we have to be careful
	// with our deallocations here; we deallocate a block only when we find it in script_blocks_.
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_deregistrations_.size())
	{
		std::cout << "Tick " << tick_ << ": DeregisterScheduledScriptBlocks() planning to remove:" << std::endl;
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
			std::cout << "Tick " << tick_ << ": DeregisterScheduledScriptBlocks() removing block:" << std::endl;
			std::cout << "   ";
			block_to_dereg->Print(std::cout);
			std::cout << std::endl;
#endif
			
			// Remove the symbol for it first
			if (block_to_dereg->block_id_ != -1)
				simulation_constants_->RemoveConstantForSymbol(block_to_dereg->ScriptBlockSymbolTableEntry().first);
			
			// Then remove it from our script block list and deallocate it
			script_blocks_.erase(script_block_position);
			last_script_block_tick_cached_ = false;
			script_block_types_cached_ = false;
			scripts_changed_ = true;
			delete block_to_dereg;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Community::DeregisterScheduledScriptBlocks): (internal error) couldn't find block for deregistration." << EidosTerminate();
		}
	}
	
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_deregistrations_.size())
	{
		std::cout << "Tick " << tick_ << ": DeregisterScheduledScriptBlocks() after removal:" << std::endl;
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

void Community::DeregisterScheduledInteractionBlocks(void)
{
	// Identical to DeregisterScheduledScriptBlocks() above, but for the interaction() dereg list; see deregisterScriptBlock()
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_interaction_deregs_.size())
	{
		std::cout << "Tick " << tick_ << ": DeregisterScheduledInteractionBlocks() planning to remove:" << std::endl;
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
			std::cout << "Tick " << tick_ << ": DeregisterScheduledInteractionBlocks() removing block:" << std::endl;
			std::cout << "   ";
			block_to_dereg->Print(std::cout);
			std::cout << std::endl;
#endif
			
			// Remove the symbol for it first
			if (block_to_dereg->block_id_ != -1)
				simulation_constants_->RemoveConstantForSymbol(block_to_dereg->ScriptBlockSymbolTableEntry().first);
			
			// Then remove it from our script block list and deallocate it
			script_blocks_.erase(script_block_position);
			last_script_block_tick_cached_ = false;
			script_block_types_cached_ = false;
			scripts_changed_ = true;
			delete block_to_dereg;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Community::DeregisterScheduledInteractionBlocks): (internal error) couldn't find block for deregistration." << EidosTerminate();
		}
	}
	
#if DEBUG_BLOCK_REG_DEREG
	if (scheduled_interaction_deregs_.size())
	{
		std::cout << "Tick " << tick_ << ": DeregisterScheduledInteractionBlocks() after removal:" << std::endl;
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

void Community::ExecuteFunctionDefinitionBlock(SLiMEidosBlock *p_script_block)
{
	EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &SymbolTable());
	EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
	
	EidosInterpreter interpreter(p_script_block->root_node_->children_[0], client_symbols, simulation_functions_, this, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
	
	try
	{
		// Interpret the script; the result from the interpretation is not used for anything
		EidosValue_SP result = interpreter.EvaluateInternalBlock(p_script_block->script_);
	}
	catch (...)
	{
		throw;
	}
}

bool Community::SubpopulationIDInUse(slim_objectid_t p_subpop_id)
{
	// This method returns whether a subpop ID is conceptually "in use": whether it is being used, has ever
	// been used, or is reserved for use in some way by, by any SLiM species or by any tree sequence.
	
	// First check our own data structures; we now do not allow reuse of subpop ids, even disjoint in time
	for (Species *species : all_species_)
		if (species->subpop_ids_.count(p_subpop_id))
			return true;
	
	// Then have each species check for a conflict with its tree-sequence population table
	for (Species *species : all_species_)
		if (species->_SubpopulationIDInUse(p_subpop_id))
			return true;
	
	return false;
}

bool Community::SubpopulationNameInUse(const std::string &p_subpop_name)
{
	// This method returns whether a subpop name is conceptually "in use": whether it is being used, has ever
	// been used, or is reserved for use in some way by, by any SLiM species or by any tree sequence.
	
	// First check our own data structures; we now do not allow reuse of subpop names, even disjoint in time
	for (Species *species : all_species_)
		if (species->subpop_names_.count(p_subpop_name))
			return true;
	
	// The tree-sequence population table does not keep names for populations, so no conflicts can occur
	
	return false;
}

Subpopulation *Community::SubpopulationWithID(slim_objectid_t p_subpop_id)
{
	for (Species *species : all_species_)
	{
		Subpopulation *found_subpop = species->SubpopulationWithID(p_subpop_id);
		
		if (found_subpop)
			return found_subpop;
	}
	
	return nullptr;
}

MutationType *Community::MutationTypeWithID(slim_objectid_t p_muttype_id)
{
	for (Species *species : all_species_)
	{
		MutationType *found_muttype = species->MutationTypeWithID(p_muttype_id);
		
		if (found_muttype)
			return found_muttype;
	}
	
	return nullptr;
}

GenomicElementType *Community::GenomicElementTypeWithID(slim_objectid_t p_getype_id)
{
	for (Species *species : all_species_)
	{
		GenomicElementType *found_getype = species->GenomicElementTypeWithID(p_getype_id);
		
		if (found_getype)
			return found_getype;
	}
	
	return nullptr;
}

SLiMEidosBlock *Community::ScriptBlockWithID(slim_objectid_t p_script_block_id)
{
	for (SLiMEidosBlock *block : script_blocks_)
		if (block->block_id_ == p_script_block_id)
			return block;
	
	return nullptr;
}

Species *Community::SpeciesWithID(slim_objectid_t p_species_id)
{
	// Species IDs are just indices into all_species_
	if ((p_species_id < 0) || (p_species_id >= (slim_objectid_t)all_species_.size()))
		return nullptr;
	
	return all_species_[p_species_id];
}

Species *Community::SpeciesWithName(const std::string &species_name)
{
	for (Species *species : all_species_)
	{
		if (species->name_ == species_name)
			return species;
	}
	
	return nullptr;
}

void Community::InvalidateInteractionsForSpecies(Species *p_invalid_species)
{
	for (auto iter : interaction_types_)
		iter.second->InvalidateForSpecies(p_invalid_species);
}

void Community::InvalidateInteractionsForSubpopulation(Subpopulation *p_invalid_subpop)
{
	for (auto iter : interaction_types_)
		iter.second->InvalidateForSubpopulation(p_invalid_subpop);
}

Species *Community::SpeciesForIndividualsVector(Individual **individuals, int value_count)
{
	if (value_count == 0)
		return nullptr;
	
	Species *consensus_species = &individuals[0]->subpopulation_->species_;
	
	if (consensus_species->community_.all_species_.size() == 1)	// with only one species, all objects must be in this species
		return consensus_species;
	
	for (int value_index = 1; value_index < value_count; ++value_index)
	{
		Species *species = &individuals[value_index]->subpopulation_->species_;
		
		if (species != consensus_species)
			return nullptr;
	}
	
	return consensus_species;
}

Species *Community::SpeciesForIndividuals(EidosValue *value)
{
	if (value->Type() != EidosValueType::kValueObject)
		EIDOS_TERMINATION << "ERROR (Community::SpeciesForIndividuals): (internal error) value is not of type object." << EidosTerminate();
	
	EidosValue_Object *object_value = (EidosValue_Object *)value;
	
	int value_count = object_value->Count();
	
	if (value_count == 0)	// allow an empty vector that is not of class Individual, to allow object() to pass our checks
		return nullptr;
	
	if (object_value->Class() != gSLiM_Individual_Class)
		EIDOS_TERMINATION << "ERROR (Community::SpeciesForIndividuals): (internal error) value is not of class Individual." << EidosTerminate();
	
	if (value_count == 1)
		return &((Individual *)object_value->ObjectElementAtIndex(0, nullptr))->subpopulation_->species_;
	
	EidosValue_Object_vector *object_vector_value = (EidosValue_Object_vector *)object_value;
	Individual **individuals = (Individual **)object_vector_value->data();
	
	return Community::SpeciesForIndividualsVector(individuals, value_count);
}

Species *Community::SpeciesForGenomesVector(Genome **genomes, int value_count)
{
	if (value_count == 0)
		return nullptr;
	
	Species *consensus_species = &genomes[0]->OwningIndividual()->subpopulation_->species_;
	
	if (consensus_species->community_.all_species_.size() == 1)	// with only one species, all objects must be in this species
		return consensus_species;
	
	for (int value_index = 1; value_index < value_count; ++value_index)
	{
		Species *species = &genomes[value_index]->OwningIndividual()->subpopulation_->species_;
		
		if (species != consensus_species)
			return nullptr;
	}
	
	return consensus_species;
}

Species *Community::SpeciesForGenomes(EidosValue *value)
{
	if (value->Type() != EidosValueType::kValueObject)
		EIDOS_TERMINATION << "ERROR (Community::SpeciesForGenomes): (internal error) value is not of type object." << EidosTerminate();
	
	EidosValue_Object *object_value = (EidosValue_Object *)value;
	
	int value_count = object_value->Count();
	
	if (value_count == 0)	// allow an empty vector that is not of class Individual, to allow object() to pass our checks
		return nullptr;
	
	if (object_value->Class() != gSLiM_Genome_Class)
		EIDOS_TERMINATION << "ERROR (Community::SpeciesForGenomes): (internal error) value is not of class Genome." << EidosTerminate();
	
	if (value_count == 1)
		return &((Genome *)object_value->ObjectElementAtIndex(0, nullptr))->OwningIndividual()->subpopulation_->species_;
	
	EidosValue_Object_vector *object_vector_value = (EidosValue_Object_vector *)object_value;
	Genome **genomes = (Genome **)object_vector_value->data();
	
	return Community::SpeciesForGenomesVector(genomes, value_count);
}

Species *Community::SpeciesForMutationsVector(Mutation **mutations, int value_count)
{
	if (value_count == 0)
		return nullptr;
	
	Species *consensus_species = &mutations[0]->mutation_type_ptr_->species_;
	
	if (consensus_species->community_.all_species_.size() == 1)	// with only one species, all objects must be in this species
		return consensus_species;
	
	for (int value_index = 1; value_index < value_count; ++value_index)
	{
		Species *species = &mutations[value_index]->mutation_type_ptr_->species_;
		
		if (species != consensus_species)
			return nullptr;
	}
	
	return consensus_species;
}

Species *Community::SpeciesForMutations(EidosValue *value)
{
	if (value->Type() != EidosValueType::kValueObject)
		EIDOS_TERMINATION << "ERROR (Community::SpeciesForMutations): (internal error) value is not of type object." << EidosTerminate();
	
	EidosValue_Object *object_value = (EidosValue_Object *)value;
	
	int value_count = object_value->Count();
	
	if (value_count == 0)	// allow an empty vector that is not of class Individual, to allow object() to pass our checks
		return nullptr;
	
	if (object_value->Class() != gSLiM_Mutation_Class)
		EIDOS_TERMINATION << "ERROR (Community::SpeciesForMutations): (internal error) value is not of class Mutation." << EidosTerminate();
	
	if (value_count == 1)
		return &((Mutation *)object_value->ObjectElementAtIndex(0, nullptr))->mutation_type_ptr_->species_;
	
	EidosValue_Object_vector *object_vector_value = (EidosValue_Object_vector *)object_value;
	Mutation **mutations = (Mutation **)object_vector_value->data();
	
	return Community::SpeciesForMutationsVector(mutations, value_count);
}

slim_tick_t Community::FirstTick(void)
{
	slim_tick_t first_tick = SLIM_MAX_TICK + 1;
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	
	// Figure out our first tick; it is the earliest tick in which an Eidos event is set up to run,
	// since an Eidos event that adds a subpopulation is necessary to get things started
	for (auto script_block : script_blocks)
		if (((script_block->type_ == SLiMEidosBlockType::SLiMEidosEventFirst) || (script_block->type_ == SLiMEidosBlockType::SLiMEidosEventEarly) || (script_block->type_ == SLiMEidosBlockType::SLiMEidosEventLate))
			&& (script_block->start_tick_ < first_tick) && (script_block->start_tick_ > 0))
			first_tick = script_block->start_tick_;
	
	return first_tick;
}

slim_tick_t Community::EstimatedLastTick(void)
{
	// return our cached value if we have one
	if (last_script_block_tick_cached_)
		return last_script_block_tick_;
	
	// otherwise, fill the cache
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	slim_tick_t last_tick = 1;
	
	// The estimate is derived from the last tick in which an Eidos block is registered.
	// Any block type works, since the simulation could plausibly be stopped within a callback.
	// However, blocks that do not specify an end tick don't count.
	for (auto script_block : script_blocks)
		if ((script_block->end_tick_ > last_tick) && (script_block->end_tick_ != SLIM_MAX_TICK + 1))
			last_tick = script_block->end_tick_;
	
	last_script_block_tick_ = last_tick;
	last_script_block_tick_cached_ = true;
	
	return last_script_block_tick_;
}

void Community::SetModelType(SLiMModelType p_new_type)
{
	if (model_type_set_)
		EIDOS_TERMINATION << "ERROR (Community::SetModelType): (internal error) the model has already been declared." << EidosTerminate();
	
	model_type_set_ = true;
	model_type_ = p_new_type;
	
	// propagate the model type decision downward to ensure consistency
	for (Species *species : all_species_)
	{
		species->model_type_ = model_type_;
		species->population_.model_type_ = model_type_;
	}
}

void Community::SetTick(slim_tick_t p_new_tick)
{
	tick_ = p_new_tick;
	
	// The tree sequence tick increments when generating offspring occurs, not at the ends of ticks as delineated by SLiM.
	// This prevents the tree sequence code from seeing two "generations" with the same value for the tick counter.
	if (((model_type_ == SLiMModelType::kModelTypeWF) && (CycleStage() < SLiMCycleStage::kWFStage2GenerateOffspring)) ||
		((model_type_ == SLiMModelType::kModelTypeNonWF) && (CycleStage() < SLiMCycleStage::kNonWFStage1GenerateOffspring)))
		tree_seq_tick_ = tick_ - 1;
	else
		tree_seq_tick_ = tick_;
	
	tree_seq_tick_offset_ = 0;
}

// This function is called by both SLiM and SLiMgui to run a tick.  In SLiM, it simply calls _RunOneTick(),
// with no exception handling; in that scenario exceptions should not be thrown, since EidosTerminate() will log an
// error and then call exit().  In SLiMgui, EidosTerminate() will raise an exception, and it will be caught right
// here and converted to an "invalid simulation" state (simulation_valid_ == false), which will be noticed by SLiMgui
// and will cause error reporting to occur based upon the error-tracking variables set.
bool Community::RunOneTick(void)
{
#ifdef SLIMGUI
	if (simulation_valid_)
	{
		try
		{
#endif
			return _RunOneTick();
#ifdef SLIMGUI
		}
		catch (...)
		{
			simulation_valid_ = false;
			
			// In the event of a raise, we clear gEidosCurrentScript, which is not normally part of the error-
			// reporting state, but is used only to inform EidosTerminate() about the current script at the point
			// when a raise occurs.  We don't want raises after RunOneTick() returns to be attributed to us,
			// so we clear the script pointer.  We do NOT clear any of the error-reporting state, since it will
			// be used by higher levels to select the error in the GUI.
			gEidosErrorContext.currentScript = nullptr;
			return false;
		}
	}
	
	gEidosErrorContext.currentScript = nullptr;
#endif
	
	return false;
}

// This function is called only by the SLiM self-testing machinery.  It has no exception handling; raises will
// blow through to the catch block in the test harness so that they can be handled there.
bool Community::_RunOneTick(void)
{
	// ******************************************************************
	//
	// Stage 0: Pre-cycle bookkeeping
	//
	cycle_stage_ = SLiMCycleStage::kStagePreCycle;
	
	// Define the current script around each cycle execution, for error reporting
	gEidosErrorContext.currentScript = script_;
	gEidosErrorContext.executingRuntimeScript = false;
	
	// Activate all species at the beginning of the tick, according their modulo/phase
	if (tick_ == 0)
	{
#ifdef SLIMGUI
		gSLiMScheduling << "# initialize() callbacks executing:" << std::endl;
#endif
		
		for (Species *species : all_species_)
			species->SetActive(true);
	}
	else
	{
		for (Species *species : all_species_)
		{
			slim_tick_t phase = species->TickPhase();
			
			if (tick_ >= phase)
			{
				slim_tick_t modulo = species->TickModulo();
				
				if ((modulo == 1) || ((tick_ - phase) % modulo == 0))
				{
					species->SetActive(true);
					continue;
				}
			}
			
			species->SetActive(false);
		}
		
		
#ifdef SLIMGUI
		gSLiMScheduling << "# tick " << tick_ << ": ";
		bool first_species = true;
		
		for (Species *species : all_species_)
		{
			if (!first_species)
				gSLiMScheduling << ", ";
			
			if (species->Active())
				gSLiMScheduling << "species " << species->name_ << " active (cycle " << species->cycle_ << ")";
			else
				gSLiMScheduling << "species " << species->name_ << " INACTIVE";
			
			first_species = false;
		}
		
		gSLiMScheduling << std::endl;
#endif
	}
	
	// Activate registered script blocks at the beginning of the tick, unless the block's species/ticks specifier refers to an inactive species
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
	{
		if ((!script_block->species_spec_ || script_block->species_spec_->Active()) && (!script_block->ticks_spec_ || script_block->ticks_spec_->Active()))
		{
			script_block->block_active_ = -1;			// block is active this tick
		}
		else
		{
			script_block->block_active_ = 0;			// block is inactive this tick
			
			// Check for deactivation causing a block not to execute at all; we consider this an error since it is almost certainly not what the user wants...
			if ((script_block->start_tick_ == script_block->end_tick_) && (script_block->start_tick_ == tick_))
				EIDOS_TERMINATION << "ERROR (Community::_RunOneTick): A script block that is scheduled to execute only in a single tick (tick " << tick_ << ") was deactivated in that tick due to a 'species' or 'ticks' specifier in its declaration; the script block will thus not execute at all." << EidosTerminate(script_block->identifier_token_);
		}
	}
	
	// Execute either initialize() callbacks (for tick 0) or the full cycle
	if (tick_ == 0)
	{
		AllSpecies_RunInitializeCallbacks();
		CheckLongTermBoundary();
		return true;
	}
	else
	{
		for (Species *species : all_species_)
			if (species->Active())
				species->PrepareForCycle();
		
		// Non-zero ticks are handled by separate functions for WF and nonWF models
		if (model_type_ == SLiMModelType::kModelTypeWF)
			return _RunOneTickWF();
		else
			return _RunOneTickNonWF();
	}
}

void Community::AllSpecies_RunInitializeCallbacks(void)
{
	// The zero tick is handled here by shared code, since it is the same for WF and nonWF models
	
	// execute user-defined function blocks first; no need to profile this, it's just the definitions not the executions
	std::vector<SLiMEidosBlock*> function_blocks = ScriptBlocksMatching(-1, SLiMEidosBlockType::SLiMEidosUserDefinedFunction, -1, -1, -1, nullptr);
	
	for (auto script_block : function_blocks)
		ExecuteFunctionDefinitionBlock(script_block);
	
	if (SLiM_verbosity_level >= 1)
		SLIM_OUTSTREAM << "// RunInitializeCallbacks():" << std::endl;
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	// execute non-species-specific (`species all`) initialize() callbacks first
	active_species_ = nullptr;
	RunInitializeCallbacks();
	
	// execute initialize() callbacks for each species, in species declaration order
	for (Species *species : all_species_)
	{
		active_species_ = species;
		active_species_->RunInitializeCallbacks();
		active_species_ = nullptr;
	}
	
	DeregisterScheduledScriptBlocks();
	
	// compile results from initialization into our overall state
	for (Species *species : all_species_)
	{
		const std::map<slim_objectid_t,MutationType*> &muttypes = species->MutationTypes();
		const std::map<slim_objectid_t,GenomicElementType*> &getypes = species->GenomicElementTypes();
		
		all_mutation_types_.insert(muttypes.begin(), muttypes.end());
		all_genomic_element_types_.insert(getypes.begin(), getypes.end());
	}
	
	// set up global symbols for all species, and for ourselves
	for (Species *species : all_species_)
		simulation_constants_->InitializeConstantSymbolEntry(species->SymbolTableEntry());
	simulation_constants_->InitializeConstantSymbolEntry(SymbolTableEntry());
	
	// we're done with the initialization tick, so remove the zero-tick functions
	RemoveZeroTickFunctionsFromMap(simulation_functions_);
	
	// determine the first tick and emit our start log
	tick_start_ = FirstTick();	// SLIM_MAX_TICK + 1 if it can't find a first block
	
	if (tick_start_ == SLIM_MAX_TICK + 1)
		EIDOS_TERMINATION << "ERROR (Community::AllSpecies_RunInitializeCallbacks): No Eidos event found to start the simulation." << EidosTerminate();
	
	if (SLiM_verbosity_level >= 1)
		SLIM_OUTSTREAM << "\n// Starting run at tick <start>:\n" << tick_start_ << " " << "\n" << std::endl;
	
	// start at the beginning; note that tree_seq_tick_ will not equal tick_ until after reproduction
	SetTick(tick_start_);
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(profile_stage_totals_[0]);
#endif
	
	// Zero out error-reporting info so raises elsewhere don't get attributed to this script
	gEidosErrorContext.currentScript = nullptr;
	gEidosErrorContext.executingRuntimeScript = false;
	
#if (SLIMPROFILING == 1)
	// PROFILING
	if (gEidosProfilingClientCount)
		CollectSLiMguiMemoryUsageProfileInfo();
#endif
}

void Community::RunInitializeCallbacks(void)
{
	// zero out the initialization check counts
	num_interaction_types_ = 0;
	num_modeltype_declarations_ = 0;
	
	// execute `species all` initialize() callbacks, which should always have a tick of 0 set
	std::vector<SLiMEidosBlock*> init_blocks = ScriptBlocksMatching(0, SLiMEidosBlockType::SLiMEidosInitializeCallback, -1, -1, -1, nullptr);
	
	for (auto script_block : init_blocks)
		ExecuteEidosEvent(script_block);
	
	// check for complete initialization
	
	// In multispecies models, we are responsible for finalizing the model type decision at the end of our initialization
	// In single-species models, the Species will do this after its init instead; see Species::RunInitializeCallbacks().
	if (is_explicit_species_)
	{
		// We default to WF, but here we explicitly declare our model type so everybody knows the default was not changed
		// This cements the choice of WF if a `species all` callback does not declare a model type explicitly
		if (num_modeltype_declarations_ == 0)
			SetModelType(SLiMModelType::kModelTypeWF);
	}
}

// execute a script event in the population; the script is assumed to be due to trigger
void Community::ExecuteEidosEvent(SLiMEidosBlock *p_script_block)
{
	if (!p_script_block->block_active_)
		return;
	
#ifndef DEBUG_POINTS_ENABLED
#error "DEBUG_POINTS_ENABLED is not defined; include eidos_globals.h"
#endif
#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	EidosDebugPointIndent indenter;
	
	{
		EidosInterpreterDebugPointsSet *debug_points = DebugPoints();
		EidosToken *decl_token = p_script_block->root_node_->token_;
		
		if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
			(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
		{
			SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG ";
			
			if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventFirst)
				SLIM_ERRSTREAM << "first()";
			else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventEarly)
				SLIM_ERRSTREAM << "early()";
			else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventLate)
				SLIM_ERRSTREAM << "late()";
			else if (p_script_block->type_ == SLiMEidosBlockType::SLiMEidosInitializeCallback)
				SLIM_ERRSTREAM << "initialize()";
			else
				SLIM_ERRSTREAM << "???";
			
			if (p_script_block->block_id_ != -1)
				SLIM_ERRSTREAM << " s" << p_script_block->block_id_;
			
			SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << DebugPointInfo() << ")" << std::endl;
			indenter.indent();
		}
	}
#endif
	
#ifdef SLIMGUI
	if ((p_script_block->type_ == SLiMEidosBlockType::SLiMEidosInitializeCallback) ||
		(p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventFirst) ||
		(p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventEarly) ||
		(p_script_block->type_ == SLiMEidosBlockType::SLiMEidosEventLate))
	{
		// These four types of script blocks log out to the scheduling stream when executed in SLiMgui
		gSLiMScheduling << "\tevent: ";
		p_script_block->PrintDeclaration(gSLiMScheduling, this);
		gSLiMScheduling << std::endl;
	}
#endif
	
	SLiMEidosBlockType old_executing_block_type = executing_block_type_;
	executing_block_type_ = p_script_block->type_;
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &SymbolTable());
	EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
	EidosFunctionMap &function_map = FunctionMap();
	
	EidosInterpreter interpreter(p_script_block->compound_statement_node_, client_symbols, function_map, this, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
	
	if (p_script_block->contains_self_)
		callback_symbols.InitializeConstantSymbolEntry(p_script_block->SelfSymbolTableEntry());		// define "self"
	
	try
	{
		// Interpret the script; the result from the interpretation is not used for anything and must be void
		EidosValue_SP result = interpreter.EvaluateInternalBlock(p_script_block->script_);
		
		if (result->Type() != EidosValueType::kValueVOID)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteEidosEvent): " << p_script_block->type_ << " callbacks must not return a value; use a \"return;\" statement to explicitly return void if desired." << EidosTerminate(p_script_block->identifier_token_);
	}
	catch (...)
	{
		throw;
	}
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(profile_callback_totals_[(int)executing_block_type_]);
#endif
	
	executing_block_type_ = old_executing_block_type;
}

void Community::AllSpecies_CheckIntegrity(void)
{
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (Species *species : all_species_)
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : species->population_.subpops_)
			subpop_pair.second->CheckIndividualIntegrity();
#endif
	
#if DEBUG
	// Check for species consistency across all of the objects in each species
	for (size_t species_index = 0; species_index < all_species_.size(); ++species_index)
	{
		Species *species = all_species_[species_index];
		
		if (&species->community_ != this)
			EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) species->community_ mismatch." << EidosTerminate();
		
		if (species->model_type_ != model_type_)
			EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) species->model_type_ mismatch." << EidosTerminate();
		
		if (species->species_id_ != (int)species_index)
			EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) species->species_id_ mismatch." << EidosTerminate();
		
		if (&species->TheChromosome().species_ != species)
			EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) species->TheChromosome().species_ mismatch." << EidosTerminate();
		
		Population &population = species->population_;
		const std::map<slim_objectid_t,MutationType*> &muttypes = species->MutationTypes();
		const std::map<slim_objectid_t,GenomicElementType*> &getypes = species->GenomicElementTypes();
		
		if (&population.species_ != species)
			EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) population.species_ mismatch." << EidosTerminate();
		
		for (auto const &subpop_iter : population.subpops_)
			if (&subpop_iter.second->species_ != species)
				EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) subpopulation->species_ mismatch." << EidosTerminate();
		
		for (auto const &muttype_iter : muttypes)
			if (&muttype_iter.second->species_ != species)
				EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) muttype->species_ mismatch." << EidosTerminate();
		
		for (auto const &getype_iter : getypes)
			if (&getype_iter.second->species_ != species)
				EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) getype->species_ mismatch." << EidosTerminate();
	}
#endif
	
#if DEBUG
	// Check the integrity of the mutation registry; all MutationIndex values should be in range
	for (Species *species : all_species_)
	{
		int registry_size;
		const MutationIndex *registry = species->population_.MutationRegistry(&registry_size);
		std::vector<MutationIndex> indices;
		
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			MutationIndex mutation_index = registry[registry_index];
			
			if ((mutation_index < 0) || (mutation_index >= gSLiM_Mutation_Block_Capacity))
				EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) mutation index " << mutation_index << " out of the mutation block." << EidosTerminate();
			
			indices.push_back(mutation_index);
		}
		
		size_t original_size = indices.size();
		
		std::sort(indices.begin(), indices.end());
		indices.resize(static_cast<size_t>(std::distance(indices.begin(), std::unique(indices.begin(), indices.end()))));
		
		if (indices.size() != original_size)
			EIDOS_TERMINATION << "ERROR (Community::AllSpecies_CheckIntegrity): (internal error) duplicate mutation index in the mutation registry (size difference " << (original_size - indices.size()) << ")." << EidosTerminate();
	}
#endif
}

void Community::AllSpecies_PurgeRemovedObjects(void)
{
	// Purge removed subpopulations and killed individuals in all subpopulations.  This doesn't have
	// to happen at any particular frequency, really, but it frees up memory, and it also allows
	// frequency/count tallying to use MutationRun refcounts to run faster, so we do it after every
	// stage of the tick cycle in nonWF models.  In WF models, individuals can't be killed so that
	// is a non-issue, and removal of subpops is generally infrequent, so we purge removed subpops
	// with PurgeRemovedSubpopulations() only in Population::SwapGenerations().
	for (Species *species : all_species_)
	{
		species->population_.PurgeRemovedSubpopulations();
		species->EmptyGraveyard();
	}
}

//
//		_RunOneTickWF() : runs all the stages for one cycle of a WF model
//
bool Community::_RunOneTickWF(void)
{
#if (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	if (gEidosProfilingClientCount)
		for (Species *species : all_species_)
			species->CollectMutationProfileInfo();
#endif
#endif
	
	// ******************************************************************
	//
	// Stage 0: Execute first() script events for the current cycle
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kWFStage0ExecuteFirstScripts;
		std::vector<SLiMEidosBlock*> first_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventFirst, -1, -1, -1, nullptr);
		
		for (auto script_block : first_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[1]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_CheckIntegrity();
	
	// ******************************************************************
	//
	// Stage 1: Execute early() script events for the current cycle
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kWFStage1ExecuteEarlyScripts;
		std::vector<SLiMEidosBlock*> early_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventEarly, -1, -1, -1, nullptr);
		
		for (auto script_block : early_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[2]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 2: Generate offspring: evolve all subpopulations
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		for (Species *species : all_species_)
			species->CheckMutationStackPolicy();
		
		cycle_stage_ = SLiMCycleStage::kWFStage2GenerateOffspring;
		
		// increment the tree-sequence tick immediately, since we are now going to make a new generation of individuals
		tree_seq_tick_++;
		tree_seq_tick_offset_ = 0;
		// note that tick_ is incremented later!
		
		// first all species generate offspring
		for (Species *species : all_species_)
			if (species->Active())
			{
				executing_species_ = species;
				
#ifdef SLIMGUI
				if (is_explicit_species_)
					gSLiMScheduling << "\toffspring generation: species " << species->name_ << std::endl;
#endif
				species->WF_GenerateOffspring();
				species->has_recalculated_fitness_ = false;
				
				executing_species_ = nullptr;
			}
		
		// then all species switch generations; this prevents access to the child generation of one species while another is still generating offspring
		for (Species *species : all_species_)
			if (species->Active())
				species->WF_SwitchToChildGeneration();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[3]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 3: Remove fixed mutations and associated tasks
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kWFStage3RemoveFixedMutations;
		
		for (Species *species : all_species_)
			if (species->Active())
				species->MaintainMutationRegistry();
		
		// Invalidate interactions, now that the generation they were valid for is disappearing
		for (Species *species : all_species_)
			if (species->Active())
				InvalidateInteractionsForSpecies(species);
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[4]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 4: Swap generations
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kWFStage4SwapGenerations;
		
		for (Species *species : all_species_)
			if (species->Active())
				species->WF_SwapGenerations();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[5]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 5: Execute late() script events for the current cycle
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kWFStage5ExecuteLateScripts;
		std::vector<SLiMEidosBlock*> late_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventLate, -1, -1, -1, nullptr);
		
		for (auto script_block : late_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[6]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 6: Calculate fitness values for the new parental generation
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kWFStage6CalculateFitness;
		
		for (Species *species : all_species_)
			if (species->Active())
			{
				executing_species_ = species;
				
#ifdef SLIMGUI
				if (is_explicit_species_)
					gSLiMScheduling << "\tfitness recalculation: species " << species->name_ << std::endl;
#endif
				species->RecalculateFitness();
				
				executing_species_ = nullptr;
			}
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Maintain our mutation run experiments; we want this overhead to appear within the stage 6 profile
		for (Species *species : all_species_)
			if (species->Active())
				species->FinishMutationRunExperimentTiming();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[7]);
#endif
		
#ifdef SLIMGUI
		// Let SLiMgui survey the population for mean fitness and such, if it is our target
		// We do this outside of profiling and mutation run experiments, since SLiMgui overhead should not affect those
		for (Species *species : all_species_)
			species->population_.SurveyPopulation();
#endif
	}
	
	CheckLongTermBoundary();
	
	
	// ******************************************************************
	//
	// Stage 7: Advance the tick counter and do end-cycle tasks
	//
	{
		cycle_stage_ = SLiMCycleStage::kWFStage7AdvanceTickCounter;
		
#ifdef SLIMGUI
		// re-tally for SLiMgui; this should avoid doing any new work if no mutations have been added or removed since the last tally
		// it is needed, though, so that if the user added/removed mutations in a late() event SLiMgui displays correctly
		// NOTE that this means tallies may be different in SLiMgui than in slim!  I *think* this will never be visible to the
		// user's model, because if they ask for mutation counts/frequences a call to TallyMutationReferences...() will be made at that
		// point anyway to synchronize; but in slim's code itself, not in Eidos, the tallies can definitely differ!  Beware!
		for (Species *species : all_species_)
			if (species->HasGenetics())
				species->population_.TallyMutationReferencesAcrossPopulation(false);
#endif
		
		for (Species *species : all_species_)
			if (species->Active())
				species->MaintainTreeSequence();
		
		// LogFile output
		for (LogFile *log_file : log_file_registry_)
			log_file->TickEndCallout();
		
		// Advance the tick and cycle counters (note that tree_seq_tick_ was incremented earlier!)
		tick_++;
		for (Species *species : all_species_)
			if (species->Active())
				species->AdvanceCycleCounter();
		
		// Use a special cycle stage for the interstitial space between ticks, when Eidos console input runs
		cycle_stage_ = SLiMCycleStage::kStagePostCycle;
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosErrorContext.currentScript = nullptr;
		gEidosErrorContext.executingRuntimeScript = false;
		
#if (SLIMPROFILING == 1)
		// PROFILING
		if (gEidosProfilingClientCount)
			CollectSLiMguiMemoryUsageProfileInfo();
#endif
		
		// Decide whether the simulation is over.  We need to call EstimatedLastTick() every time; we can't
		// cache it, because it can change based upon changes in script registration / deregistration.
		bool result;
		
		if (sim_declared_finished_)
			result = false;
		else
			result = (tick_ <= EstimatedLastTick());
		
		if (!result)
			SimulationHasFinished();
		
		return result;
	}
}

//
//		_RunOneTickNonWF() : runs all the stages for one cycle of a nonWF model
//
bool Community::_RunOneTickNonWF(void)
{
#if (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	if (gEidosProfilingClientCount)
		for (Species *species : all_species_)
			species->CollectMutationProfileInfo();
#endif
#endif
	
	// ******************************************************************
	//
	// Stage 0: Execute first() script events for the current cycle
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kNonWFStage0ExecuteFirstScripts;
		std::vector<SLiMEidosBlock*> first_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventFirst, -1, -1, -1, nullptr);
		
		for (auto script_block : first_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[1]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_PurgeRemovedObjects();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 1: Generate offspring: call reproduction() callbacks
	//
	{
		// increment the tree-seq tick at the start of reproduction; note that in first() events it is one less than tick_!
		tree_seq_tick_++;
		tree_seq_tick_offset_ = 0;

#if defined(SLIMGUI)
		// zero out offspring counts used for SLiMgui's display
		for (Species *species : all_species_)
		{
			if (species->species_active_)
			{
				for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : species->population_.subpops_)
				{
					Subpopulation *subpop = subpop_pair.second;
					
					subpop->gui_offspring_cloned_M_ = 0;
					subpop->gui_offspring_cloned_F_ = 0;
					subpop->gui_offspring_selfed_ = 0;
					subpop->gui_offspring_crossed_ = 0;
					subpop->gui_offspring_empty_ = 0;
				}
				
				// zero out migration counts used for SLiMgui's display
				for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : species->population_.subpops_)
				{
					Subpopulation *subpop = subpop_pair.second;
					
					subpop->gui_premigration_size_ = subpop->parent_subpop_size_;
					subpop->gui_migrants_.clear();
				}
			}
		}
#endif
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		for (Species *species : all_species_)
			species->CheckMutationStackPolicy();
		
		cycle_stage_ = SLiMCycleStage::kNonWFStage1GenerateOffspring;
		
		// BCH 28 September 2022: Offspring generation in nonWF models is now done in two passes: first all species generate
		// their offspring, then all species merge their offspring.  This allows multispecies interactions to remain valid
		// through the whole reproduction process.  In effect, reproduction is now kind of two separate tick cycle stages,
		// but this is not emphasized since it only makes a difference to multispecies models; conceptually it is one stage.
		for (Species *species : all_species_)
			if (species->Active())
			{
				executing_species_ = species;
				
#ifdef SLIMGUI
				if (is_explicit_species_)
					gSLiMScheduling << "\toffspring generation: species " << species->name_ << std::endl;
#endif
				species->nonWF_GenerateOffspring();
				
				executing_species_ = nullptr;
			}
		
		for (Species *species : all_species_)
			if (species->Active())
			{
				executing_species_ = species;
				
#ifdef SLIMGUI
				if (is_explicit_species_)
					gSLiMScheduling << "\tmerge offspring: species " << species->name_ << std::endl;
#endif
				species->nonWF_MergeOffspring();
				species->has_recalculated_fitness_ = false;
				
				executing_species_ = nullptr;
			}
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[2]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_PurgeRemovedObjects();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 2: Execute early() script events for the current cycle
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts;
		std::vector<SLiMEidosBlock*> early_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventEarly, -1, -1, -1, nullptr);
		
		for (auto script_block : early_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[3]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_PurgeRemovedObjects();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 3: Calculate fitness values for the new population
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kNonWFStage3CalculateFitness;
		
		for (Species *species : all_species_)
			if (species->Active())
			{
				executing_species_ = species;
				
#ifdef SLIMGUI
				if (is_explicit_species_)
					gSLiMScheduling << "\tfitness recalculation: species " << species->name_ << std::endl;
#endif
				species->RecalculateFitness();
				
				executing_species_ = nullptr;
			}
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Invalidate interactions, now that the cycle they were valid for is disappearing
		for (Species *species : all_species_)
			if (species->Active())
				InvalidateInteractionsForSpecies(species);
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[4]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_PurgeRemovedObjects();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 4: Viability/survival selection
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kNonWFStage4SurvivalSelection;
		
		for (Species *species : all_species_)
			if (species->Active())
			{
				executing_species_ = species;
				
#ifdef SLIMGUI
				if (is_explicit_species_)
					gSLiMScheduling << "\tviability/survival: species " << species->name_ << std::endl;
#endif
				species->nonWF_ViabilitySurvival();
				
				executing_species_ = nullptr;
			}
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[5]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_PurgeRemovedObjects();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 5: Remove fixed mutations and associated tasks
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kNonWFStage5RemoveFixedMutations;
		
		for (Species *species : all_species_)
			if (species->Active())
				species->MaintainMutationRegistry();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[6]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_PurgeRemovedObjects();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 6: Execute late() script events for the current cycle
	//
	{
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		cycle_stage_ = SLiMCycleStage::kNonWFStage6ExecuteLateScripts;
		std::vector<SLiMEidosBlock*> late_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventLate, -1, -1, -1, nullptr);
		
		for (auto script_block : late_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Maintain our mutation run experiments; we want this overhead to appear within the stage 6 profile
		for (Species *species : all_species_)
			if (species->Active())
				species->FinishMutationRunExperimentTiming();
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[7]);
#endif
	}
	
	CheckLongTermBoundary();
	AllSpecies_PurgeRemovedObjects();
	AllSpecies_CheckIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 7: Advance the tick counter and do end-cycle tasks
	//
	{
		cycle_stage_ = SLiMCycleStage::kNonWFStage7AdvanceTickCounter;
		
#ifdef SLIMGUI
		// Let SLiMgui survey the population for mean fitness and such, if it is our target
		// We do this outside of profiling and mutation run experiments, since SLiMgui overhead should not affect those
		for (Species *species : all_species_)
			species->population_.SurveyPopulation();
#endif
		
#ifdef SLIMGUI
		// re-tally for SLiMgui; this should avoid doing any new work if no mutations have been added or removed since the last tally
		// it is needed, though, so that if the user added/removed mutations in a late() event SLiMgui displays correctly
		// NOTE that this means tallies may be different in SLiMgui than in slim!  I *think* this will never be visible to the
		// user's model, because if they ask for mutation counts/frequences a call to TallyMutationReferences...() will be made at that
		// point anyway to synchronize; but in slim's code itself, not in Eidos, the tallies can definitely differ!  Beware!
		for (Species *species : all_species_)
			if (species->HasGenetics())
				species->population_.TallyMutationReferencesAcrossPopulation(false);
#endif
		
		for (Species *species : all_species_)
			if (species->Active())
				species->MaintainTreeSequence();
		
		// LogFile output
		for (LogFile *log_file : log_file_registry_)
			log_file->TickEndCallout();
		
		// Advance the tick counter (note that tree_seq_tick_ is incremented after first() events in the next tick!)
		tick_++;
		for (Species *species : all_species_)
			if (species->Active())
				species->AdvanceCycleCounter();
		
		for (Species *species : all_species_)
		{
			if (species->Active())
				for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : species->population_.subpops_)
					subpop_pair.second->IncrementIndividualAges();
		}
		
		// Use a special cycle stage for the interstitial space between ticks, when Eidos console input runs
		cycle_stage_ = SLiMCycleStage::kStagePostCycle;
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosErrorContext.currentScript = nullptr;
		gEidosErrorContext.executingRuntimeScript = false;
		
#if (SLIMPROFILING == 1)
		// PROFILING
		if (gEidosProfilingClientCount)
			CollectSLiMguiMemoryUsageProfileInfo();
#endif
		
		// Decide whether the simulation is over.  We need to call EstimatedLastTick() every time; we can't
		// cache it, because it can change based upon changes in script registration / deregistration.
		bool result;
		
		if (sim_declared_finished_)
			result = false;
		else
			result = (tick_ <= EstimatedLastTick());
		
		if (!result)
			SimulationHasFinished();
		
		return result;
	}
}

void Community::SimulationHasFinished(void)
{
	// This is an opportunity for final calculation/output when a simulation finishes
	for (Species *species : all_species_)
		species->SimulationHasFinished();
}

void Community::TabulateSLiMMemoryUsage_Community(SLiMMemoryUsage_Community *p_usage, EidosSymbolTable *p_current_symbols)
{
	EIDOS_BZERO(p_usage, sizeof(SLiMMemoryUsage_Community));
	
	// Community usage
	p_usage->communityObjects_count = 1;
	p_usage->communityObjects = p_usage->communityObjects_count * sizeof(Community);
	
	// Mutation global buffers
	p_usage->mutationRefcountBuffer = SLiMMemoryUsageForMutationRefcounts();
	p_usage->mutationUnusedPoolSpace = SLiMMemoryUsageForFreeMutations();		// note that in SLiMgui everybody shares this
	
	// InteractionType
	{
		p_usage->interactionTypeObjects_count = interaction_types_.size();
		p_usage->interactionTypeObjects = sizeof(InteractionType) * p_usage->interactionTypeObjects_count;
		
		for (auto iter : interaction_types_)
		{
			p_usage->interactionTypeKDTrees += iter.second->MemoryUsageForKDTrees();
			p_usage->interactionTypePositionCaches += iter.second->MemoryUsageForPositions();
		}
		
		p_usage->interactionTypeSparseVectorPool += InteractionType::MemoryUsageForSparseVectorPool();
	}
	
	// Eidos usage
	p_usage->eidosASTNodePool = gEidosASTNodePool->MemoryUsageForAllNodes();
	p_usage->eidosSymbolTablePool = MemoryUsageForSymbolTables(p_current_symbols);
	p_usage->eidosValuePool = gEidosValuePool->MemoryUsageForAllNodes();
	
	for (auto const &filebuf_pair : gEidosBufferedZipAppendData)
		p_usage->fileBuffers += filebuf_pair.second.capacity();
	
	// Total
	SumUpMemoryUsage_Community(*p_usage);
}

#if (SLIMPROFILING == 1)
// PROFILING
void Community::StartProfiling(void)
{
	gEidosProfilingClientCount++;
	
	// prepare for profiling by measuring profile block overhead and lag
	Eidos_PrepareForProfiling();
	
	// initialize counters
	profile_elapsed_CPU_clock = 0;
	profile_elapsed_wall_clock = 0;
	profile_start_tick = Tick();
	
	// call this first, purely for its side effect of emptying out any pending profile counts
	// note that the accumulators governed by this method get zeroed out down below
	for (Species *focal_species : all_species_)
		focal_species->CollectMutationProfileInfo();
	
	// zero out profile counts for cycle stages
	for (int i = 0; i < 9; ++i)
		profile_stage_totals_[i] = 0;
	
	// zero out profile counts for callback types (note SLiMEidosUserDefinedFunction is excluded; that is not a category we profile)
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventFirst)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventEarly)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventLate)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInitializeCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMutationEffectCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessEffectCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInteractionCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosModifyChildCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosRecombinationCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMutationCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosReproductionCallback)] = 0;
	profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosSurvivalCallback)] = 0;
	
	// zero out profile counts for script blocks; dynamic scripts will be zeroed on construction
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
		if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)	// exclude user-defined functions; not user-visible as blocks
			script_block->root_node_->ZeroProfileTotals();
	
	// zero out profile counts for all user-defined functions
	EidosFunctionMap &function_map = FunctionMap();
	
	for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
	{
		const EidosFunctionSignature *signature = functionPairIter->second.get();
		
		if (signature->body_script_ && signature->user_defined_)
			signature->body_script_->AST()->ZeroProfileTotals();
	}
	
#if SLIM_USE_NONNEUTRAL_CACHES
	// zero out mutation run metrics that are collected by CollectMutationProfileInfo()
	for (Species *focal_species : all_species_)
	{
		focal_species->profile_mutcount_history_.clear();
		focal_species->profile_nonneutral_regime_history_.clear();
		focal_species->profile_mutation_total_usage_ = 0;
		focal_species->profile_nonneutral_mutation_total_ = 0;
		focal_species->profile_mutrun_total_usage_ = 0;
		focal_species->profile_unique_mutrun_total_ = 0;
		focal_species->profile_mutrun_nonneutral_recache_total_ = 0;
		focal_species->profile_max_mutation_index_ = 0;
	}
#endif
	
	// zero out memory usage metrics
	EIDOS_BZERO(&profile_last_memory_usage_Community, sizeof(SLiMMemoryUsage_Community));
	EIDOS_BZERO(&profile_total_memory_usage_Community, sizeof(SLiMMemoryUsage_Community));
	EIDOS_BZERO(&profile_last_memory_usage_AllSpecies, sizeof(SLiMMemoryUsage_Species));
	EIDOS_BZERO(&profile_total_memory_usage_AllSpecies, sizeof(SLiMMemoryUsage_Species));
	total_memory_tallies_ = 0;
	
	time(&profile_start_date);
	profile_start_clock = std::chrono::steady_clock::now();
}

void Community::StopProfiling(void)
{
	time(&profile_end_date);
	profile_end_clock = std::chrono::steady_clock::now();
	profile_end_tick = Tick();
	
	gEidosProfilingClientCount--;
}

void Community::CollectSLiMguiMemoryUsageProfileInfo(void)
{
	// Gather the data
	EIDOS_BZERO(&profile_last_memory_usage_AllSpecies, sizeof(SLiMMemoryUsage_Species));
	
	TabulateSLiMMemoryUsage_Community(&profile_last_memory_usage_Community, nullptr);
	
	for (Species *species : all_species_)
	{
		species->TabulateSLiMMemoryUsage_Species(&species->profile_last_memory_usage_Species);
		
		// Add this tick's usage for this species into its single-species accumulator
		AccumulateMemoryUsageIntoTotal_Species(species->profile_last_memory_usage_Species, species->profile_total_memory_usage_Species);
		
		// Add this tick's usage for this species into this tick's overall species accumulator
		AccumulateMemoryUsageIntoTotal_Species(species->profile_last_memory_usage_Species, profile_last_memory_usage_AllSpecies);
	}
	
	// Add this tick's data into our top-level accumulators
	AccumulateMemoryUsageIntoTotal_Community(profile_last_memory_usage_Community, profile_total_memory_usage_Community);
	AccumulateMemoryUsageIntoTotal_Species(profile_last_memory_usage_AllSpecies, profile_total_memory_usage_AllSpecies);
	
	// Increment our accumulator count; we divide by this to get averages
	total_memory_tallies_++;
}
#endif

#ifdef SLIMGUI
void Community::FileWriteNotification(const std::string &p_file_path, std::vector<std::string> &&p_lines, bool p_append)
{
	auto buffer_iter = std::find(file_write_paths_.begin(), file_write_paths_.end(), p_file_path);
	
	if (buffer_iter == file_write_paths_.end())
	{
		// No existing buffer for this path, so make a new one; this does not mean the file is new!
		file_write_paths_.emplace_back(p_file_path);
		file_write_buffers_.emplace_back(std::move(p_lines));
		file_write_appends_.emplace_back(p_append);
	}
	else
	{
		// Use the existing buffer for this path
		size_t buffer_index = std::distance(file_write_paths_.begin(), buffer_iter);
		std::vector<std::string> &buffer = file_write_buffers_[buffer_index];
		
		if (!p_append)
			buffer.clear();
		
		for (std::string &line : p_lines)
			buffer.emplace_back(std::move(line));
		
		// Note the append flag; the vector here is always parallel to the main vector
		file_write_appends_[buffer_index] = p_append;
	}
}
#endif

void Community::AllSpecies_TSXC_Enable(void)
{
	// This is called by command-line slim if a -TSXC command-line option is supplied; the point of this is to allow
	// tree-sequence recording to be turned on, with mutation recording and runtime crosschecks, with a simple
	// command-line flag, so that my existing test suite can be crosschecked easily.  The -TSXC flag is not public.
	for (Species *species : all_species_)
		species->TSXC_Enable();
	
	if (SLiM_verbosity_level >= 1)
		SLIM_ERRSTREAM << "// ********** Turning on tree-sequence recording with crosschecks (-TSXC)." << std::endl << std::endl;
}

void Community::AllSpecies_TSF_Enable(void)
{
    // This is called by command-line slim if a -TSF command-line option is supplied; the point of this is to allow
    // tree-sequence recording to be turned on, with mutation recording but without runtime crosschecks, with a simple
    // command-line flag, so that my existing test suite can be tested with tree-seq easily.  -TSF is not public.
	for (Species *species : all_species_)
		species->TSF_Enable();
    
	if (SLiM_verbosity_level >= 1)
		SLIM_ERRSTREAM << "// ********** Turning on tree-sequence recording without crosschecks (-TSF)." << std::endl << std::endl;
}































































