//
//  community.cpp
//  SLiM
//
//  Created by Ben Haller on 2/28/2022.
//  Copyright (c) 2022 Philipp Messer.  All rights reserved.
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


#pragma mark -
#pragma mark Community
#pragma mark -

Community::Community(std::istream &p_infile) : self_symbol_(gID_community, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Community_Class)))
{
	// Allocate our species; this will change to be done during/after the file is parsed, I suppose
	all_species_.push_back(new Species(*this, 0));
	
	// set up the symbol tables we will use for global variables and constants; note that the global variables table
	// lives *above* the context constants table, which is fine since they cannot define the same symbol anyway
	// this satisfies Eidos, which expects the child of the intrinsic constants table to be the global variables table
	simulation_globals_ = new EidosSymbolTable(EidosSymbolTableType::kGlobalVariablesTable, gEidosConstantsSymbolTable);
	simulation_constants_ = new EidosSymbolTable(EidosSymbolTableType::kContextConstantsTable, simulation_globals_);
	
	// set up the function map with the base Eidos functions plus zero-gen functions, since we're in an initial state
	simulation_functions_ = *EidosInterpreter::BuiltInFunctionMap();
	AddZeroTickFunctionsToMap(simulation_functions_);
	AddSLiMFunctionsToMap(simulation_functions_);
	
	// read all configuration information from the input file
	p_infile.clear();
	p_infile.seekg(0, std::fstream::beg);
	
	try {
		InitializeFromFile(p_infile);
	}
	catch (...) {
		// try to clean up what we've allocated so far
		delete simulation_globals_;
		simulation_globals_ = nullptr;
		
		delete simulation_constants_;
		simulation_constants_ = nullptr;
		
		simulation_functions_.clear();
		
		throw;
	}
}

Community::~Community(void)
{
	//EIDOS_ERRSTREAM << "Community::~Community" << std::endl;
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
	unsigned long int rng_seed = (p_override_seed_ptr ? *p_override_seed_ptr : Eidos_GenerateSeedFromPIDAndTime());
	
	Eidos_InitializeRNG();
	Eidos_SetRNGSeed(rng_seed);
	
	if (SLiM_verbosity_level >= 1)
		SLIM_OUTSTREAM << "// Initial random seed:\n" << rng_seed << "\n" << std::endl;
	
	// remember the original seed for .trees provenance
	original_seed_ = rng_seed;
}

void Community::InitializeFromFile(std::istream &p_infile)
{
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
	
	// Extract SLiMEidosBlocks from the parse tree
	const EidosASTNode *root_node = script_->AST();
	
	for (EidosASTNode *script_block_node : root_node->children_)
	{
		SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_block_node);
		
		AddScriptBlock(new_script_block, nullptr, new_script_block->root_node_->children_[0]->token_);
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
		cached_fitness_callbacks_.clear();
		cached_fitnessglobal_callbacks_onetick_.clear();
		cached_fitnessglobal_callbacks_multitick_.clear();
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
				case SLiMEidosBlockType::SLiMEidosFitnessCallback:			cached_fitness_callbacks_.emplace_back(script_block);			break;
				case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:
				{
					// Global fitness callbacks are not order-dependent, so we don't have to preserve their order
					// of declaration the way we do with other types of callbacks.  This allows us to be very efficient
					// in how we look them up, which is good since sometimes we have a very large number of them.
					// We put those that are registered for just a single tick in a separate vector, which we sort
					// by tick, allowing us to look them up quickly
					slim_tick_t start = script_block->start_tick_;
					slim_tick_t end = script_block->end_tick_;
					
					if (start == end)
					{
						cached_fitnessglobal_callbacks_onetick_.emplace(start, script_block);
					}
					else
					{
						cached_fitnessglobal_callbacks_multitick_.emplace_back(script_block);
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

std::vector<SLiMEidosBlock*> Community::ScriptBlocksMatching(slim_tick_t p_tick, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id)
{
	if (!script_block_types_cached_)
		ValidateScriptBlockCaches();
	
	std::vector<SLiMEidosBlock*> *block_list = nullptr;
	
	switch (p_event_type)
	{
		case SLiMEidosBlockType::SLiMEidosEventFirst:				block_list = &cached_first_events_;						break;
		case SLiMEidosBlockType::SLiMEidosEventEarly:				block_list = &cached_early_events_;						break;
		case SLiMEidosBlockType::SLiMEidosEventLate:				block_list = &cached_late_events_;						break;
		case SLiMEidosBlockType::SLiMEidosInitializeCallback:		block_list = &cached_initialize_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosFitnessCallback:			block_list = &cached_fitness_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:	block_list = &cached_fitnessglobal_callbacks_multitick_;	break;
		case SLiMEidosBlockType::SLiMEidosInteractionCallback:		block_list = &cached_interaction_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		block_list = &cached_matechoice_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		block_list = &cached_modifychild_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	block_list = &cached_recombination_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosMutationCallback:			block_list = &cached_mutation_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			block_list = &cached_survival_callbacks_;				break;
		case SLiMEidosBlockType::SLiMEidosReproductionCallback:		block_list = &cached_reproduction_callbacks_;			break;
		case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		block_list = &cached_userdef_functions_;				break;
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
		// this is now a bit tricky, with the NULL mut-type option, indicated by -2.  The rules now are:
		//
		// * if -2 is requested, -2 callbacks are all you get
		// * if anything other than -2 is requested (including -1), -2 callbacks will not be returned
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
	
	// add in any single-tick global fitness callbacks
	if (p_event_type == SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)
	{
		auto find_range = cached_fitnessglobal_callbacks_onetick_.equal_range(p_tick);
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

std::vector<SLiMEidosBlock*> &Community::AllScriptBlocks()
{
	return script_blocks_;
}

std::vector<SLiMEidosBlock*> &Community::AllScriptBlocksForSpecies(Species *p_species)
{
#warning needs to filter down to blocks owned by one species
	return script_blocks_;
}

void Community::OptimizeScriptBlock(SLiMEidosBlock *p_script_block)
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

void Community::AddScriptBlock(SLiMEidosBlock *p_script_block, EidosInterpreter *p_interpreter, const EidosToken *p_error_token)
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
}

void Community::DeregisterScheduledScriptBlocks(void)
{
	// If we have blocks scheduled for deregistration, we sweep through and deregister them at the end of each stage of each tick.
	// This happens at a time when script blocks are not executing, so that we're guaranteed not to leave hanging pointers that could
	// cause a crash; it also guarantees that script blocks are applied consistently across each generation stage.  A single block
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

InteractionType *Community::InteractionTypeWithID(slim_objectid_t p_inttype_id)
{
	for (Species *species : all_species_)
	{
		InteractionType *found_inttype = species->InteractionTypeWithID(p_inttype_id);
		
		if (found_inttype)
			return found_inttype;
	}
	
	return nullptr;
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
	if (model_type_explicit_ && (model_type_ != p_new_type))
		EIDOS_TERMINATION << "ERROR (Community::SetModelType): the model has already been declared as a different type; all species in a community must use the same model type.  This error can occur if one species explicitly declares the nonWF model type and another species fails to declare the nonWF model type (and thus defaults to WF)." << EidosTerminate();
	
	model_type_explicit_ = true;
	model_type_ = p_new_type;
	
	// We do not propagate the model type downward to the species/population/subpopulation level here
	// This method can only be called during species initialization, and the species propagates the model type
}

void Community::SetTick(slim_tick_t p_new_tick)
{
	tick_ = p_new_tick;
	
	// The tree sequence tick increments when offspring generation occurs, not at the ends of ticks as delineated by SLiM.
	// This prevents the tree sequence code from seeing two "generations" with the same value for the tick counter.
	if (((model_type_ == SLiMModelType::kModelTypeWF) && (GenerationStage() < SLiMGenerationStage::kWFStage2GenerateOffspring)) ||
		((model_type_ == SLiMModelType::kModelTypeNonWF) && (GenerationStage() < SLiMGenerationStage::kNonWFStage1GenerateOffspring)))
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
#if DEBUG_MUTATION_RUNS
	{
		slim_refcount_t total_genome_count = 0, tally_mutrun_ref_count = 0, total_mutrun_count = 0;
		
		{
			// do a bulk operation to tally up mutation run and genome counts
			int64_t operation_id = ++gSLiM_MutationRun_OperationID;
			
			for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			{
				Subpopulation *subpop = subpop_pair.second;
				
				slim_popsize_t subpop_genome_count = subpop->CurrentGenomeCount();
				std::vector<Genome *> &subpop_genomes = subpop->CurrentGenomes();
				
				for (slim_popsize_t i = 0; i < subpop_genome_count; i++)							// child genomes
				{
					Genome &genome = *subpop_genomes[i];
					
					if (!genome.IsNull())
					{
						subpop_genomes[i]->TallyGenomeReferences(&tally_mutrun_ref_count, &total_mutrun_count, operation_id);
						total_genome_count++;
					}
				}
			}
		}
		
		int64_t external_buffer_use_count = 0, external_buffer_capacity_tally = 0, external_buffer_count_tally = 0;
		
		{
			// do a bulk operation to tally up mutation run external buffer usage efficiency
			int64_t operation_id = ++gSLiM_MutationRun_OperationID;
			
			for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			{
				Subpopulation *subpop = subpop_pair.second;
				
				slim_popsize_t subpop_genome_count = subpop->CurrentGenomeCount();
				std::vector<Genome *> &subpop_genomes = subpop->CurrentGenomes();
				
				for (slim_popsize_t i = 0; i < subpop_genome_count; i++)							// child genomes
				{
					Genome &genome = *subpop_genomes[i];
					
					if (!genome.IsNull())
					{
						subpop_genomes[i]->TallyBufferUsage(&external_buffer_use_count, &external_buffer_capacity_tally, &external_buffer_count_tally, operation_id);
					}
				}
			}
		}
		
		int mutruns_per_genome = TheChromosome().mutrun_count_;
		
		std::cerr << "#DEBUG_MUTATION_RUNS: tick " << tick_ << ": " << std::endl;
		std::cerr << "    " << gSLiM_ActiveMutrunCount << " active, " << gSLiM_FreeMutrunCount << " freed, " << total_mutrun_count << " reachable (from " << total_genome_count << " genomes, " << (total_genome_count * mutruns_per_genome) << " mutrun refs); " << gSLiM_AllocatedMutrunCount << " mutruns allocated this tick, " << gSLiM_UnfreedMutrunCount << " unfreed, " << gSLiM_ConstructedMutrunCount << " constructed." << std::endl;
		std::cerr << "    " << gSLiM_MutationsBufferCount << " mutindex buffers (" << gSLiM_MutationsBufferBytes << " bytes); " << gSLiM_MutationsBufferNewCount << " new buffers created, " << gSLiM_MutationsBufferReallocCount << " realloced, " << gSLiM_MutationsBufferFreedCount << " freed" << std::endl;
		std::cerr << "    of " << total_mutrun_count << " reachable mutation runs, " << external_buffer_use_count << " use external buffers, with space efficiency " << (external_buffer_count_tally / (double)external_buffer_capacity_tally) << " (" << (external_buffer_capacity_tally / (double)external_buffer_use_count) << " mean capacity, " << (external_buffer_count_tally / (double)external_buffer_use_count) << " mean used)" << std::endl;
		gSLiM_AllocatedMutrunCount = 0;
		gSLiM_UnfreedMutrunCount = 0;
		gSLiM_ConstructedMutrunCount = 0;
		gSLiM_MutationsBufferNewCount = 0;
		gSLiM_MutationsBufferReallocCount = 0;
		gSLiM_MutationsBufferFreedCount = 0;
	}
#endif
	
	// ******************************************************************
	//
	// Stage 0: Pre-generation bookkeeping
	//
	generation_stage_ = SLiMGenerationStage::kStagePreGeneration;
	
	// Define the current script around each generation execution, for error reporting
	gEidosErrorContext.currentScript = script_;
	gEidosErrorContext.executingRuntimeScript = false;
	
	// Activate all registered script blocks at the beginning of the generation
	std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
		script_block->active_ = -1;
	
	if (tick_ == 0)
	{
		AllSpecies_RunInitializeCallbacks();
		return true;
	}
	else
	{
		for (Species *species : all_species_)
			species->PrepareForGenerationCycle();
		
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
	std::vector<SLiMEidosBlock*> function_blocks = ScriptBlocksMatching(-1, SLiMEidosBlockType::SLiMEidosUserDefinedFunction, -1, -1, -1);
	
	for (auto script_block : function_blocks)
		ExecuteFunctionDefinitionBlock(script_block);
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	// execute initialize() callbacks for each species, in species declaration order
	for (Species *species : all_species_)
	{
		active_species_ = species;
		active_species_->RunInitializeCallbacks();
		active_species_ = nullptr;
		
	}
	
	DeregisterScheduledScriptBlocks();
	
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
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(profile_stage_totals_[0]);
#endif
	
	// Zero out error-reporting info so raises elsewhere don't get attributed to this script
	gEidosErrorContext.currentScript = nullptr;
	gEidosErrorContext.executingRuntimeScript = false;
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	if (gEidosProfilingClientCount)
		CollectSLiMguiMemoryUsageProfileInfo();
#endif
}

// execute a script event in the population; the script is assumed to be due to trigger
void Community::ExecuteEidosEvent(SLiMEidosBlock *p_script_block)
{
	if (!p_script_block->active_)
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
	
	SLiMEidosBlockType old_executing_block_type = executing_block_type_;
	executing_block_type_ = p_script_block->type_;
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
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
			EIDOS_TERMINATION << "ERROR (Population::ExecuteScript): " << p_script_block->type_ << " callbacks must not return a value; use a \"return;\" statement to explicitly return void if desired." << EidosTerminate(p_script_block->identifier_token_);
	}
	catch (...)
	{
		throw;
	}
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(profile_callback_totals_[(int)executing_block_type_]);
#endif
	
	executing_block_type_ = old_executing_block_type;
}

void Community::AllSpecies_CheckIndividualIntegrity(void)
{
#if DEBUG
	// Check the integrity of all the information in the individuals and genomes of the parental population
	for (Species *species : all_species_)
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : species->population_.subpops_)
			subpop_pair.second->CheckIndividualIntegrity();
#endif
}

//
//		_RunOneTickWF() : runs all the stages for one generation of a WF model
//
bool Community::_RunOneTickWF(void)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	if (gEidosProfilingClientCount)
		for (Species *species : all_species_)
			species->CollectSLiMguiMutationProfileInfo();
#endif
#endif
	
	// ******************************************************************
	//
	// Stage 0: Execute first() script events for the current generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kWFStage0ExecuteFirstScripts;
		std::vector<SLiMEidosBlock*> first_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventFirst, -1, -1, -1);
		
		for (auto script_block : first_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[1]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	// ******************************************************************
	//
	// Stage 1: Execute early() script events for the current generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kWFStage1ExecuteEarlyScripts;
		std::vector<SLiMEidosBlock*> early_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventEarly, -1, -1, -1);
		
		for (auto script_block : early_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[2]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 2: Generate offspring: evolve all subpopulations
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		for (Species *species : all_species_)
			species->CheckMutationStackPolicy();
		
		generation_stage_ = SLiMGenerationStage::kWFStage2GenerateOffspring;
		
		// increment the tree-sequence tick immediately, since we are now going to make a new generation of individuals
		tree_seq_tick_++;
		tree_seq_tick_offset_ = 0;
		// note that tick_ is incremented later!
		
		// first all species generate offspring
		for (Species *species : all_species_)
			species->WF_GenerateOffspring();
		
		// then all species switch generations; this prevents access to the child generation of one species while another is still generating offspring
		for (Species *species : all_species_)
			species->WF_SwitchToChildGeneration();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[3]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		
		for (Species *species : all_species_)
			species->MaintainMutationRegistry();
		
		// Invalidate interactions, now that the generation they were valid for is disappearing
		for (Species *species : all_species_)
			species->InvalidateAllInteractions();
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[4]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		
		for (Species *species : all_species_)
			species->WF_SwapGenerations();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[5]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		std::vector<SLiMEidosBlock*> late_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventLate, -1, -1, -1);
		
		for (auto script_block : late_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[6]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		
		for (Species *species : all_species_)
			species->RecalculateFitness();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Maintain our mutation run experiments; we want this overhead to appear within the stage 6 profile
		for (Species *species : all_species_)
			species->FinishMutationRunExperimentTiming();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
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
	
	
	// ******************************************************************
	//
	// Stage 7: Advance the tick counter and do end-generation tasks
	//
	{
		generation_stage_ = SLiMGenerationStage::kWFStage7AdvanceGenerationCounter;
		
#ifdef SLIMGUI
		// re-tally for SLiMgui; this should avoid doing any new work if no mutations have been added or removed since the last tally
		// it is needed, though, so that if the user added/removed mutations in a late() event SLiMgui displays correctly
		// NOTE that this means tallies may be different in SLiMgui than in slim!  I *think* this will never be visible to the
		// user's model, because if they ask for mutation counts/frequences a call to TallyMutationReferences() will be made at that
		// point anyway to synchronize; but in slim's code itself, not in Eidos, the tallies can definitely differ!  Beware!
		for (Species *species : all_species_)
			species->population_.TallyMutationReferences(nullptr, false);
#endif
		
		for (Species *species : all_species_)
			species->MaintainTreeSequence();
		
		// LogFile output
		for (LogFile *log_file : log_file_registry_)
			log_file->TickEndCallout();
		
		// Advance the tick and generation counters (note that tree_seq_tick_ was incremented earlier!)
		tick_++;
		for (Species *species : all_species_)
			species->AdvanceGenerationCounter();
		
		// Use a special generation stage for the interstitial space between ticks, when Eidos console input runs
		generation_stage_ = SLiMGenerationStage::kStagePostGeneration;
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosErrorContext.currentScript = nullptr;
		gEidosErrorContext.executingRuntimeScript = false;
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
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
//		_RunOneTickNonWF() : runs all the stages for one generation of a nonWF model
//
bool Community::_RunOneTickNonWF(void)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	if (gEidosProfilingClientCount)
		for (Species *species : all_species_)
			species->CollectSLiMguiMutationProfileInfo();
#endif
#endif
	
	// ******************************************************************
	//
	// Stage 0: Execute first() script events for the current generation
	//
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage0ExecuteFirstScripts;
		std::vector<SLiMEidosBlock*> first_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventFirst, -1, -1, -1);
		
		for (auto script_block : first_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[1]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
#endif
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		for (Species *species : all_species_)
			species->CheckMutationStackPolicy();
		
		generation_stage_ = SLiMGenerationStage::kNonWFStage1GenerateOffspring;
		
		for (Species *species : all_species_)
			species->nonWF_GenerateOffspring();
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[2]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		std::vector<SLiMEidosBlock*> early_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventEarly, -1, -1, -1);
		
		for (auto script_block : early_blocks)
			ExecuteEidosEvent(script_block);
		
		// dispose of any freed subpops; we do this before fitness calculation so tallies are correct
		for (Species *species : all_species_)
			species->population_.PurgeRemovedSubpopulations();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[3]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		
		for (Species *species : all_species_)
			species->RecalculateFitness();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Invalidate interactions, now that the generation they were valid for is disappearing
		for (Species *species : all_species_)
			species->InvalidateAllInteractions();
		
		// Deregister any interaction() callbacks that have been scheduled for deregistration, since it is now safe to do so
		DeregisterScheduledInteractionBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[4]);
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
		
		for (Species *species : all_species_)
			species->nonWF_ViabilitySurvival();
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[5]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		
		for (Species *species : all_species_)
			species->MaintainMutationRegistry();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[6]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
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
		std::vector<SLiMEidosBlock*> late_blocks = ScriptBlocksMatching(tick_, SLiMEidosBlockType::SLiMEidosEventLate, -1, -1, -1);
		
		for (auto script_block : late_blocks)
			ExecuteEidosEvent(script_block);
		
		// the stage is done, so deregister script blocks as requested
		DeregisterScheduledScriptBlocks();
		
		// Maintain our mutation run experiments; we want this overhead to appear within the stage 6 profile
		for (Species *species : all_species_)
			species->FinishMutationRunExperimentTiming();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(profile_stage_totals_[7]);
#endif
	}
	
	AllSpecies_CheckIndividualIntegrity();
	
	
	// ******************************************************************
	//
	// Stage 7: Advance the tick counter and do end-generation tasks
	//
	{
		generation_stage_ = SLiMGenerationStage::kNonWFStage7AdvanceGenerationCounter;
		
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
		// user's model, because if they ask for mutation counts/frequences a call to TallyMutationReferences() will be made at that
		// point anyway to synchronize; but in slim's code itself, not in Eidos, the tallies can definitely differ!  Beware!
		for (Species *species : all_species_)
			species->population_.TallyMutationReferences(nullptr, false);
#endif
		
		for (Species *species : all_species_)
			species->MaintainTreeSequence();
		
		// LogFile output
		for (LogFile *log_file : log_file_registry_)
			log_file->TickEndCallout();
		
		// Advance the tick counter (note that tree_seq_tick_ is incremented after first() events in the next tick!)
		tick_++;
		for (Species *species : all_species_)
			species->AdvanceGenerationCounter();
		
		for (Species *species : all_species_)
			for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : species->population_.subpops_)
				subpop_pair.second->IncrementIndividualAges();
		
		// Use a special generation stage for the interstitial space between ticks, when Eidos console input runs
		generation_stage_ = SLiMGenerationStage::kStagePostGeneration;
		
		// Zero out error-reporting info so raises elsewhere don't get attributed to this script
		gEidosErrorContext.currentScript = nullptr;
		gEidosErrorContext.executingRuntimeScript = false;
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
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
	
	// MutationRun global buffers
	p_usage->mutationRunUnusedPoolSpace = sizeof(MutationRun) * MutationRun::s_freed_mutation_runs_.size();
	
	for (MutationRun *mutrun : MutationRun::s_freed_mutation_runs_)
	{
		p_usage->mutationRunUnusedPoolBuffers += mutrun->MemoryUsageForMutationIndexBuffers();
		p_usage->mutationRunUnusedPoolBuffers += mutrun->MemoryUsageForNonneutralCaches();
	}
	
	// Eidos usage
	p_usage->eidosASTNodePool = gEidosASTNodePool->MemoryUsageForAllNodes();
	p_usage->eidosSymbolTablePool = MemoryUsageForSymbolTables(p_current_symbols);
	p_usage->eidosValuePool = gEidosValuePool->MemoryUsageForAllNodes();
	
	// Total
	SumUpMemoryUsage_Community(*p_usage);
}

#if defined(SLIMGUI) && (SLIMPROFILING == 1)
// PROFILING
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
	
	SLIM_ERRSTREAM << "// ********** Turning on tree-sequence recording with crosschecks (-TSXC)." << std::endl << std::endl;
}

void Community::AllSpecies_TSF_Enable(void)
{
    // This is called by command-line slim if a -TSF command-line option is supplied; the point of this is to allow
    // tree-sequence recording to be turned on, with mutation recording but without runtime crosschecks, with a simple
    // command-line flag, so that my existing test suite can be tested with tree-seq easily.  -TSF is not public.
	for (Species *species : all_species_)
		species->TSF_Enable();
    
    SLIM_ERRSTREAM << "// ********** Turning on tree-sequence recording without crosschecks (-TSF)." << std::endl << std::endl;
}































































