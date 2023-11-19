//
//  slim_script_block.cpp
//  SLiM
//
//  Created by Ben Haller on 6/7/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

#include "slim_eidos_block.h"
#include "eidos_interpreter.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"
#include "community.h"
#include "species.h"
#include "interaction_type.h"
#include "subpopulation.h"

#include "errno.h"
#include "string.h"
#include <string>
#include <algorithm>
#include <vector>


std::ostream& operator<<(std::ostream& p_out, SLiMEidosBlockType p_block_type)
{
	switch (p_block_type)
	{
		case SLiMEidosBlockType::SLiMEidosEventFirst:				p_out << "first()"; break;
		case SLiMEidosBlockType::SLiMEidosEventEarly:				p_out << "early()"; break;
		case SLiMEidosBlockType::SLiMEidosEventLate:				p_out << "late()"; break;
		case SLiMEidosBlockType::SLiMEidosInitializeCallback:		p_out << "initialize()"; break;
		case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	p_out << "mutationEffect()"; break;
		case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	p_out << "fitnessEffect()"; break;
		case SLiMEidosBlockType::SLiMEidosInteractionCallback:		p_out << "interaction()"; break;
		case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		p_out << "mateChoice()"; break;
		case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		p_out << "modifyChild()"; break;
		case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	p_out << "recombination()"; break;
		case SLiMEidosBlockType::SLiMEidosMutationCallback:			p_out << "mutation()"; break;
		case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			p_out << "survival()"; break;
		case SLiMEidosBlockType::SLiMEidosReproductionCallback:		p_out << "reproduction()"; break;
		case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		p_out << "function"; break;
		case SLiMEidosBlockType::SLiMEidosNoBlockType:				p_out << "NO BLOCK"; break;
	}
	
	return p_out;
}


//
//	SLiMEidosScript
//
#pragma mark -
#pragma mark SLiMEidosScript
#pragma mark -

SLiMEidosScript::SLiMEidosScript(const std::string &p_script_string) : EidosScript(p_script_string, 0)
{
}

SLiMEidosScript::~SLiMEidosScript(void)
{
}

EidosASTNode *SLiMEidosScript::Parse_SLiMFile(void)
{
	EidosToken *virtual_token = new EidosToken(EidosTokenType::kTokenContextFile, gEidosStr_empty_string, 0, 0, 0, 0, -1);
	
	EidosASTNode *node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(virtual_token, true);
	
	try
	{
		// We handle the grammar a bit differently than how it is printed in the railroad diagrams in the doc.
		// Parsing of the optional tick range is done in Parse_SLiMEidosBlock() since it ends up as children of that node.
		while (current_token_type_ != EidosTokenType::kTokenEOF)
		{
			// For multispecies, we now look at the current token and handle it specially if it is "species" or "ticks".
			if ((current_token_type_ == EidosTokenType::kTokenIdentifier) && (current_token_->token_string_.compare(gStr_species) == 0))
				node->AddChild(Parse_SpeciesSpecifier());
			else if ((current_token_type_ == EidosTokenType::kTokenIdentifier) && (current_token_->token_string_.compare(gStr_ticks) == 0))
				node->AddChild(Parse_TicksSpecifier());
			else
				node->AddChild(Parse_SLiMEidosBlock());
		}
		
		Match(EidosTokenType::kTokenEOF, "SLiM file");
	}
	catch (...)
	{
		// destroy the parse root and return it to the pool; the tree must be allocated out of gEidosASTNodePool!
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *SLiMEidosScript::Parse_SpeciesSpecifier(void)
{
	EidosASTNode *node = nullptr, *species_name;
	
	try {
		// This parses "species identifier" specifiers, creating a node with the species name as its child
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenIdentifier, "species specifier");
		
		species_name = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		node->AddChild(species_name);
		
#if (SLIMPROFILING == 1)
		// PROFILING
		node->full_range_end_token_ = current_token_;
#endif
		
		Match(EidosTokenType::kTokenIdentifier, "species specifier");
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *SLiMEidosScript::Parse_TicksSpecifier(void)
{
	EidosASTNode *node = nullptr, *species_name;
	
	try {
		// This parses "ticks identifier" specifiers, creating a node with the species name as its child
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenIdentifier, "ticks specifier");
		
		species_name = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		node->AddChild(species_name);
		
#if (SLIMPROFILING == 1)
		// PROFILING
		node->full_range_end_token_ = current_token_;
#endif
		
		Match(EidosTokenType::kTokenIdentifier, "ticks specifier");
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *SLiMEidosScript::Parse_SLiMEidosBlock(void)
{
	EidosToken *virtual_token = new EidosToken(EidosTokenType::kTokenContextEidosBlock, gEidosStr_empty_string, 0, 0, 0, 0, -1);
	
	EidosASTNode *slim_script_block_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(virtual_token, true);
	
	// We handle the grammar a bit differently than how it is printed in the railroad diagrams in the doc.
	// We parse the slim_script_info section here, as part of the script block.
	try
	{
		// Keep track of the beginning of the script block, to patch virtual_token below...
		const int32_t token_start = current_token_->token_start_;
		const int32_t token_UTF16_start = current_token_->token_UTF16_start_;
		const int32_t token_line = current_token_->token_line_;	// we use the line of our starting token
		EidosASTNode *compound_statement_node = nullptr;
		
		if (current_token_type_ == EidosTokenType::kTokenFunction)
		{
			// Starting in SLiM 2.5, the user can declare their own functions at the top level in the SLiM file.
			// Since the SLiM input file is not an Eidos interpreter block, we have to handle that ourselves.
			EidosASTNode *function_node = Parse_FunctionDecl();
			
			if (function_node->children_.size() == 4)
			{
				compound_statement_node = function_node->children_[3];	// for the virtual token range below
				
				slim_script_block_node->AddChild(function_node);
			}
		}
		else
		{
			// The first element is an optional script identifier like s1; we check here that an identifier matches the
			// pattern sX before eating it, since an identifier here could also be a callback tag like "mutationEffect".
			if ((current_token_type_ == EidosTokenType::kTokenIdentifier) && SLiMEidosScript::StringIsIDWithPrefix(current_token_->token_string_, 's'))
			{
				// a script identifier like s1 is present; add it
				slim_script_block_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
				
				Match(EidosTokenType::kTokenIdentifier, "SLiM script block");
			}
			
			// Next comes an optional tick X, or a tick range X:Y, X:, or :Y (a lone : is not legal).
			// We don't parse this as if the : were an operator, since we have to allow for a missing start or end;
			// for this reason, we make the : into a node of its own, with no children, so X:Y, X:, and :Y are distinct.
			// SLiMEidosBlock::SLiMEidosBlock(EidosASTNode *p_root_node) handles this anomalous tree structure.
			if (current_token_type_ == EidosTokenType::kTokenNumber)
			{
				// A start tick is present; add it
				slim_script_block_node->AddChild(Parse_Constant());
				
				// If a colon is present, we have a range, although it could be just X:
				if (current_token_type_ == EidosTokenType::kTokenColon)
				{
					slim_script_block_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
					Match(EidosTokenType::kTokenColon, "SLiM script block");
					
					// If an end tick is present, add it
					if (current_token_type_ == EidosTokenType::kTokenNumber)
						slim_script_block_node->AddChild(Parse_Constant());
				}
			}
			else if (current_token_type_ == EidosTokenType::kTokenColon)
			{
				// The tick range starts with a colon; first eat that
				slim_script_block_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
				Match(EidosTokenType::kTokenColon, "SLiM script block");
				
				// In this situation, we must have an end tick; a lone colon is not a legal tick specifier
				if (current_token_type_ == EidosTokenType::kTokenNumber)
					slim_script_block_node->AddChild(Parse_Constant());
				else
				{
					if (!parse_make_bad_nodes_)
						EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; expected an integer for the tick range end." << EidosTerminate(current_token_);
					
					// Introduce a bad node, since we're being error-tolerant
					slim_script_block_node->AddChild(Parse_Constant());
				}
			}
			
			// Now we are to the point of parsing the actual slim_script_block
			if (current_token_type_ == EidosTokenType::kTokenIdentifier)
			{
				if (current_token_->token_string_.compare(gStr_first) == 0)
				{
					slim_script_block_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM first() event");
					Match(EidosTokenType::kTokenLParen, "SLiM first() event");
					Match(EidosTokenType::kTokenRParen, "SLiM first() event");
				}
				else if (current_token_->token_string_.compare(gStr_early) == 0)
				{
					slim_script_block_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM early() event");
					Match(EidosTokenType::kTokenLParen, "SLiM early() event");
					Match(EidosTokenType::kTokenRParen, "SLiM early() event");
				}
				else if (current_token_->token_string_.compare(gStr_late) == 0)
				{
					slim_script_block_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM late() event");
					Match(EidosTokenType::kTokenLParen, "SLiM late() event");
					Match(EidosTokenType::kTokenRParen, "SLiM late() event");
				}
				else if (current_token_->token_string_.compare(gStr_initialize) == 0)
				{
					slim_script_block_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM initialize() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM initialize() callback");
					Match(EidosTokenType::kTokenRParen, "SLiM initialize() callback");
				}
				else if (current_token_->token_string_.compare(gStr_fitnessEffect) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM fitnessEffect() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM fitnessEffect() callback");
					
					// A (optional) subpopulation id is present; add it
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM fitnessEffect() callback");
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM fitnessEffect() callback");
				}
				else if (current_token_->token_string_.compare(gStr_mutationEffect) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM mutationEffect() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM mutationEffect() callback");
					
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						// A (required) mutation type id is present; add it
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM mutationEffect() callback");
					}
					else
					{
						if (!parse_make_bad_nodes_)
							EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; a mutation type id is required in mutationEffect() callback definitions." << EidosTerminate(current_token_);
						
						// Make a placeholder bad node, to be error-tolerant
						EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, 0, 0, 0, 0, -1);
						EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
						callback_info_node->AddChild(bad_node);
					}
					
					if (current_token_type_ == EidosTokenType::kTokenComma)
					{
						// A (optional) subpopulation id is present; add it
						Match(EidosTokenType::kTokenComma, "SLiM mutationEffect() callback");
						
						if (current_token_type_ == EidosTokenType::kTokenIdentifier)
						{
							callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
							
							Match(EidosTokenType::kTokenIdentifier, "SLiM mutationEffect() callback");
						}
						else
						{
							if (!parse_make_bad_nodes_)
								EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; subpopulation id expected." << EidosTerminate(current_token_);
							
							// Make a placeholder bad node, to be error-tolerant
							EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, 0, 0, 0, 0, -1);
							EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
							callback_info_node->AddChild(bad_node);
						}
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM mutationEffect() callback");
				}
				else if (current_token_->token_string_.compare(gStr_mutation) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM mutation() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM mutation() callback");
					
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						// A (optional) mutation type id (or NULL) is present; add it
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM mutation() callback");
					
						if (current_token_type_ == EidosTokenType::kTokenComma)
						{
							// A (optional) subpopulation id is present; add it
							Match(EidosTokenType::kTokenComma, "SLiM mutation() callback");
							
							if (current_token_type_ == EidosTokenType::kTokenIdentifier)
							{
								callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
								
								Match(EidosTokenType::kTokenIdentifier, "SLiM mutation() callback");
							}
							else
							{
								if (!parse_make_bad_nodes_)
									EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; subpopulation id expected." << EidosTerminate(current_token_);
								
								// Make a placeholder bad node, to be error-tolerant
								EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, 0, 0, 0, 0, -1);
								EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
								callback_info_node->AddChild(bad_node);
							}
						}
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM mutation() callback");
				}
				else if (current_token_->token_string_.compare(gStr_interaction) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM interaction() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM interaction() callback");
					
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						// A (required) interaction type id is present; add it
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM interaction() callback");
					}
					else
					{
						if (!parse_make_bad_nodes_)
							EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; an interaction type id is required in interaction() callback definitions." << EidosTerminate(current_token_);
						
						// Make a placeholder bad node, to be error-tolerant
						EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, 0, 0, 0, 0, -1);
						EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
						callback_info_node->AddChild(bad_node);
					}
					
					if (current_token_type_ == EidosTokenType::kTokenComma)
					{
						// A (optional) subpopulation id is present; add it
						Match(EidosTokenType::kTokenComma, "SLiM interaction() callback");
						
						if (current_token_type_ == EidosTokenType::kTokenIdentifier)
						{
							callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
							
							Match(EidosTokenType::kTokenIdentifier, "SLiM interaction() callback");
						}
						else
						{
							if (!parse_make_bad_nodes_)
								EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; subpopulation id expected." << EidosTerminate(current_token_);
							
							// Make a placeholder bad node, to be error-tolerant
							EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, 0, 0, 0, 0, -1);
							EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
							callback_info_node->AddChild(bad_node);
						}
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM interaction() callback");
				}
				else if (current_token_->token_string_.compare(gStr_mateChoice) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM mateChoice() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM mateChoice() callback");
					
					// A (optional) subpopulation id is present; add it
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM mateChoice() callback");
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM mateChoice() callback");
				}
				else if (current_token_->token_string_.compare(gStr_modifyChild) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM modifyChild() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM modifyChild() callback");
					
					// A (optional) subpopulation id is present; add it
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM modifyChild() callback");
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM modifyChild() callback");
				}
				else if (current_token_->token_string_.compare(gStr_recombination) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM recombination() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM recombination() callback");
					
					// A (optional) subpopulation id is present; add it
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM recombination() callback");
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM recombination() callback");
				}
				else if (current_token_->token_string_.compare(gStr_survival) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM survival() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM survival() callback");
					
					// A (optional) subpopulation id is present; add it
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM survival() callback");
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM survival() callback");
				}
				else if (current_token_->token_string_.compare(gStr_reproduction) == 0)
				{
					EidosASTNode *callback_info_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
					slim_script_block_node->AddChild(callback_info_node);
					
					Match(EidosTokenType::kTokenIdentifier, "SLiM reproduction() callback");
					Match(EidosTokenType::kTokenLParen, "SLiM reproduction() callback");
					
					// A (optional) subpopulation id (or NULL) is present; add it
					if (current_token_type_ == EidosTokenType::kTokenIdentifier)
					{
						callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
						
						Match(EidosTokenType::kTokenIdentifier, "SLiM reproduction() callback");
						
						if (current_token_type_ == EidosTokenType::kTokenComma)
						{
							// A (optional) sex string (or NULL) is present; add it
							Match(EidosTokenType::kTokenComma, "SLiM reproduction() callback");
							
							if (current_token_type_ == EidosTokenType::kTokenString)
							{
								callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
								
								Match(EidosTokenType::kTokenString, "SLiM reproduction() callback");
							}
							else if (current_token_type_ == EidosTokenType::kTokenIdentifier)
							{
								callback_info_node->AddChild(new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_));
								
								Match(EidosTokenType::kTokenIdentifier, "SLiM reproduction() callback");
							}
							else
							{
								if (!parse_make_bad_nodes_)
									EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; sex of 'M' or 'F' expected." << EidosTerminate(current_token_);
								
								// Make a placeholder bad node, to be error-tolerant
								EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, 0, 0, 0, 0, -1);
								EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
								callback_info_node->AddChild(bad_node);
							}
						}
					}
					
					Match(EidosTokenType::kTokenRParen, "SLiM reproduction() callback");
				}
				else
				{
					if (!parse_make_bad_nodes_)
						EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected identifier " << *current_token_ << "; expected an event declaration (first, early, late), a callback declaration (initialize, fitnessEffect, interaction, mateChoice, modifyChild, mutation, mutationEffect, recombination, reproduction, or survival), or a function declaration." << EidosTerminate(current_token_);
					
					// Consume the stray identifier, to be error-tolerant
					Consume();
				}
			}
			else
			{
				if (!parse_make_bad_nodes_)
					EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): unexpected token " << *current_token_ << "; expected an event declaration (first, early, late), a callback declaration (initialize, fitnessEffect, interaction, mateChoice, modifyChild, mutation, mutationEffect, recombination, reproduction, or survival), or a function declaration.  Note that early() is no longer a default script block type that may be omitted; it must now be specified explicitly." << EidosTerminate(current_token_);
				
				// Consume the stray identifier, to be error-tolerant
				Consume();
			}
			
			// Regardless of what happened above, all Eidos blocks end with a compound statement, which is the last child of the node
			compound_statement_node = Parse_CompoundStatement();
			
			slim_script_block_node->AddChild(compound_statement_node);
		}
		
		// Patch virtual_token to contain the range from beginning to end of the script block
		if (compound_statement_node)
		{
			const int32_t token_end = compound_statement_node->token_->token_end_;
			const int32_t token_UTF16_end = compound_statement_node->token_->token_UTF16_end_;
			
			std::string &&token_string = script_string_.substr(token_start, token_end - token_start + 1);
			
			slim_script_block_node->ReplaceTokenWithToken(new EidosToken(slim_script_block_node->token_->token_type_, token_string, token_start, token_end, token_UTF16_start, token_UTF16_end, token_line));
		}
		else if (!parse_make_bad_nodes_)
			EIDOS_TERMINATION << "ERROR (SLiMEidosScript::Parse_SLiMEidosBlock): (internal error) missing compound_statement_node" << EidosTerminate(current_token_);

	}
	catch (...)
	{
		if (slim_script_block_node)
		{
			slim_script_block_node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(slim_script_block_node));
		}
		
		throw;
	}
	
	return slim_script_block_node;
}

void SLiMEidosScript::ParseSLiMFileToAST(bool p_make_bad_nodes)
{
	// destroy the parse root and return it to the pool; the tree must be allocated out of gEidosASTNodePool!
	if (parse_root_)
	{
		parse_root_->~EidosASTNode();
		gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(parse_root_));
		parse_root_ = nullptr;
	}
	
	// set up parse state
	parse_index_ = 0;
	current_token_ = &token_stream_.at(parse_index_);		// should always have at least an EOF
	current_token_type_ = current_token_->token_type_;
	parse_make_bad_nodes_ = p_make_bad_nodes;
	
	// parse a new AST from our start token
	parse_root_ = Parse_SLiMFile();
	
	parse_root_->OptimizeTree();
	
	// if logging of the AST is requested, do that
	if (gEidosLogAST)
	{
		std::cout << "AST : \n";
		this->PrintAST(std::cout);
	}
	
	parse_make_bad_nodes_ = false;
}

bool SLiMEidosScript::StringIsIDWithPrefix(const std::string &p_identifier_string, char p_prefix_char)
{
	const char *id_cstr = p_identifier_string.c_str();
	size_t id_cstr_len = strlen(id_cstr);
	
	// the criteria here are pretty loose, because we want SLiMEidosScript::ExtractIDFromStringWithPrefix to be
	// called and generate a raise if the string appears to be intended to be an ID but is malformed
	// however, we don't want to end up here with just any string that starts with the prefix character...
	
	// if the prefix character is not present, then it's not a match
	if ((id_cstr_len < 1) || (*id_cstr != p_prefix_char))
		return false;
	
	// if there is at least one character following the prefix, those characters must all be numeric
	if (id_cstr_len > 1)
	{
		for (unsigned int str_index = 1; str_index < id_cstr_len; ++str_index)
			if ((id_cstr[str_index] < '0') || (id_cstr[str_index] > '9'))
				return false;
	}
	
	return true;
}

slim_objectid_t SLiMEidosScript::ExtractIDFromStringWithPrefix(const std::string &p_identifier_string, char p_prefix_char, const EidosToken *p_blame_token)
{
	const char *id_cstr = p_identifier_string.c_str();
	size_t id_cstr_len = strlen(id_cstr);
	
	if ((id_cstr_len < 1) || (*id_cstr != p_prefix_char))
		EIDOS_TERMINATION << "ERROR (SLiMEidosScript::ExtractIDFromStringWithPrefix): an identifier prefix '" << p_prefix_char << "' was expected." << EidosTerminate(p_blame_token);
	
	for (unsigned int str_index = 1; str_index < id_cstr_len; ++str_index)
		if ((id_cstr[str_index] < '0') || (id_cstr[str_index] > '9'))
			EIDOS_TERMINATION << "ERROR (SLiMEidosScript::ExtractIDFromStringWithPrefix): the id after the '" << p_prefix_char << "' prefix must be a simple integer." << EidosTerminate(p_blame_token);
	
	if (id_cstr_len < 2)
		EIDOS_TERMINATION << "ERROR (SLiMEidosScript::ExtractIDFromStringWithPrefix): an integer id was expected after the '" << p_prefix_char << "' prefix." << EidosTerminate(p_blame_token);
	
	errno = 0;
	char *end_scan_char = nullptr;
	int64_t long_block_id = strtoll(id_cstr + 1, &end_scan_char, 10);	// +1 to omit the prefix character
	
	if (errno || (end_scan_char == id_cstr + 1))
		EIDOS_TERMINATION << "ERROR (SLiMEidosScript::ExtractIDFromStringWithPrefix): the identifier " << p_identifier_string << " was not parseable." << EidosTerminate(p_blame_token);
	
	if ((long_block_id < 0) || (long_block_id > SLIM_MAX_ID_VALUE))
		EIDOS_TERMINATION << "ERROR (SLiMEidosScript::ExtractIDFromStringWithPrefix): the identifier " << p_identifier_string << " was out of range." << EidosTerminate(p_blame_token);
	
	return static_cast<slim_objectid_t>(long_block_id);		// range check is above, with a better message than SLiMCastToObjectidTypeOrRaise()
}


//
//	SLiMEidosBlock
//
#pragma mark -
#pragma mark SLiMEidosBlock
#pragma mark -

SLiMEidosBlockType SLiMEidosBlock::BlockTypeForRootNode(EidosASTNode *p_root_node)
{
	// Get the block type for a node without actually constructing the block.  This is parallel to the constructor code below,
	// and the two must be maintained in parallel when new callback types, etc., are added.  Note that we don't do any bounds-
	// or error-checking here; we just need to know the *intended* block type, if we can figure it out.
	const std::vector<EidosASTNode *> &block_children = p_root_node->children_;
	int child_index = 0, n_children = (int)block_children.size();
	
	if ((n_children == 1) && (block_children[child_index]->token_->token_type_ == EidosTokenType::kTokenFunction))
	{
		return SLiMEidosBlockType::SLiMEidosUserDefinedFunction;
	}
	else
	{
		// eat a string, for the script id, if present; an identifier token must follow the sX format to be taken as an id here, as in the parse code
		if (child_index < n_children)
		{
			EidosToken *script_id_token = block_children[child_index]->token_;
			
			if ((script_id_token->token_type_ == EidosTokenType::kTokenIdentifier) && SLiMEidosScript::StringIsIDWithPrefix(script_id_token->token_string_, 's'))
				child_index++;
		}
		
		// eat the optional tick range, which could be X, X:Y, X:, or :Y
		if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == EidosTokenType::kTokenNumber))
				child_index++;
		if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == EidosTokenType::kTokenColon))
				child_index++;
		if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == EidosTokenType::kTokenNumber))
				child_index++;
		
		// eat the callback info node, if present
		if (child_index < n_children)
		{
			const EidosASTNode *callback_node = block_children[child_index];
			const EidosToken *callback_token = callback_node->token_;
			
			if (callback_token->token_type_ == EidosTokenType::kTokenIdentifier)
			{
				const std::string &callback_name = callback_token->token_string_;
				
				if (callback_name.compare(gStr_first) == 0)
					return SLiMEidosBlockType::SLiMEidosEventFirst;
				else if (callback_name.compare(gStr_early) == 0)
					return SLiMEidosBlockType::SLiMEidosEventEarly;
				else if (callback_name.compare(gStr_late) == 0)
					return SLiMEidosBlockType::SLiMEidosEventLate;
				else if (callback_name.compare(gStr_initialize) == 0)
					return SLiMEidosBlockType::SLiMEidosInitializeCallback;
				else if (callback_name.compare(gStr_fitnessEffect) == 0)
					return SLiMEidosBlockType::SLiMEidosFitnessEffectCallback;
				else if (callback_name.compare(gStr_mutationEffect) == 0)
					return SLiMEidosBlockType::SLiMEidosMutationEffectCallback;
				else if (callback_name.compare(gStr_mutation) == 0)
					return SLiMEidosBlockType::SLiMEidosMutationCallback;
				else if (callback_name.compare(gStr_interaction) == 0)
					return SLiMEidosBlockType::SLiMEidosInteractionCallback;
				else if (callback_name.compare(gStr_mateChoice) == 0)
					return SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
				else if (callback_name.compare(gStr_modifyChild) == 0)
					return SLiMEidosBlockType::SLiMEidosModifyChildCallback;
				else if (callback_name.compare(gStr_recombination) == 0)
					return SLiMEidosBlockType::SLiMEidosRecombinationCallback;
				else if (callback_name.compare(gStr_survival) == 0)
					return SLiMEidosBlockType::SLiMEidosSurvivalCallback;
				else if (callback_name.compare(gStr_reproduction) == 0)
					return SLiMEidosBlockType::SLiMEidosReproductionCallback;
			}
		}
	}
	
	return SLiMEidosBlockType::SLiMEidosNoBlockType;
}

SLiMEidosBlock::SLiMEidosBlock(EidosASTNode *p_root_node) :
	self_symbol_(gID_self, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMEidosBlock_Class))),
	script_block_symbol_(gEidosID_none, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMEidosBlock_Class))),
	root_node_(p_root_node), user_script_line_offset_(p_root_node->token_->token_line_)
{
	// NOTE: SLiMEidosBlock::BlockTypeForRootNode() above must be maintained in parallel with this method!
	const std::vector<EidosASTNode *> &block_children = root_node_->children_;
	int child_index = 0, n_children = (int)block_children.size();
	
	block_id_ = -1;	// the default unless it is set below
	
	if ((n_children == 1) && (block_children[child_index]->token_->token_type_ == EidosTokenType::kTokenFunction))
	{
		EidosASTNode *function_decl_node = block_children[child_index];
		
		if (function_decl_node->children_.size() == 4)
		{
			compound_statement_node_ = function_decl_node->children_[3];
			type_ = SLiMEidosBlockType::SLiMEidosUserDefinedFunction;
			
			child_index++;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): (internal error) unexpected child count in user-defined function declaration." << EidosTerminate(function_decl_node->token_);
		}
	}
	else
	{
		// eat a string, for the script id, if present; an identifier token must follow the sX format to be taken as an id here, as in the parse code
		if (child_index < n_children)
		{
			EidosToken *script_id_token = block_children[child_index]->token_;
			
			if ((script_id_token->token_type_ == EidosTokenType::kTokenIdentifier) && SLiMEidosScript::StringIsIDWithPrefix(script_id_token->token_string_, 's'))
			{
				block_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(script_id_token->token_string_, 's', script_id_token);
				child_index++;
				
				// fix ID string for our symbol
				std::string new_symbol_string = SLiMEidosScript::IDStringWithPrefix('s', block_id_);
				script_block_symbol_.first = EidosStringRegistry::GlobalStringIDForString(new_symbol_string);
			}
		}
		
		// eat the optional tick range, which could be X, X:Y, X:, or :Y
		// we don't need to syntax-check here since the parse already did
		if (child_index < n_children)
		{
			EidosToken *start_tick_token = block_children[child_index]->token_;
			
			if (start_tick_token->token_type_ == EidosTokenType::kTokenNumber)
			{
				int64_t long_start = EidosInterpreter::NonnegativeIntegerForString(start_tick_token->token_string_, start_tick_token);
				
				// We do our own range checking here so that we can highlight the bad token
				if ((long_start < 1) || (long_start > SLIM_MAX_TICK))
					EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): the start tick " << start_tick_token->token_string_ << " is out of range." << EidosTerminate(start_tick_token);
				
				start_tick_ = SLiMCastToTickTypeOrRaise(long_start);
				end_tick_ = start_tick_;			// if a start is given, the default end is the same as the start
				child_index++;
			}
		}
		
		if (child_index < n_children)
		{
			EidosToken *colon_token = block_children[child_index]->token_;
			
			// we don't need to do much here except fix the end tick in case none is supplied, as in X:
			if (colon_token->token_type_ == EidosTokenType::kTokenColon)
			{
				end_tick_ = SLIM_MAX_TICK + 1;	// marker value for "no endpoint specified"; illegal for the user to specify this as a literal
				child_index++;
			}
		}
		
		if (child_index < n_children)
		{
			EidosToken *end_tick_token = block_children[child_index]->token_;
			
			if (end_tick_token->token_type_ == EidosTokenType::kTokenNumber)
			{
				int64_t long_end = EidosInterpreter::NonnegativeIntegerForString(end_tick_token->token_string_, end_tick_token);
				
				// We do our own range checking here so that we can highlight the bad token
				if ((long_end < 1) || (long_end > SLIM_MAX_TICK))
					EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): the end tick " << end_tick_token->token_string_ << " is out of range." << EidosTerminate(end_tick_token);
				if (long_end < start_tick_)
					EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): the end tick " << end_tick_token->token_string_ << " is less than the start tick." << EidosTerminate(end_tick_token);
				
				end_tick_ = SLiMCastToTickTypeOrRaise(long_end);
				child_index++;
			}
		}
		
		// eat the callback info node, if present
		if (child_index < n_children)
		{
			const EidosASTNode *callback_node = block_children[child_index];
			const EidosToken *callback_token = callback_node->token_;
			
			if (callback_token->token_type_ != EidosTokenType::kTokenLBrace)
			{
				EidosTokenType callback_type = callback_token->token_type_;
				const std::string &callback_name = callback_token->token_string_;
				const std::vector<EidosASTNode *> &callback_children = callback_node->children_;
				int n_callback_children = (int)callback_children.size();
				
				identifier_token_ = callback_token;	// remember our identifier token for easy access later
				
				if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_first) == 0))
				{
					if (n_callback_children != 0)
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): first() event needs 0 parameters." << EidosTerminate(callback_token);
					
					type_ = SLiMEidosBlockType::SLiMEidosEventFirst;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_early) == 0))
				{
					if (n_callback_children != 0)
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): early() event needs 0 parameters." << EidosTerminate(callback_token);
					
					type_ = SLiMEidosBlockType::SLiMEidosEventEarly;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_late) == 0))
				{
					if (n_callback_children != 0)
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): late() event needs 0 parameters." << EidosTerminate(callback_token);
					
					type_ = SLiMEidosBlockType::SLiMEidosEventLate;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_initialize) == 0))
				{
					if (n_callback_children != 0)
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): initialize() callback needs 0 parameters." << EidosTerminate(callback_token);
					
					if ((start_tick_ != -1) || (end_tick_ != SLIM_MAX_TICK + 1))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): a tick range cannot be specified for an initialize() callback." << EidosTerminate(callback_token);
					
					start_tick_ = 0;
					end_tick_ = 0;
					type_ = SLiMEidosBlockType::SLiMEidosInitializeCallback;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_fitnessEffect) == 0))
				{
					if ((n_callback_children != 0) && (n_callback_children != 1))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): fitnessEffect() callback needs 0 or 1 parameter." << EidosTerminate(callback_token);
					
					type_ = SLiMEidosBlockType::SLiMEidosFitnessEffectCallback;
					
					if (n_callback_children == 1)
					{
						EidosToken *subpop_id_token = callback_children[0]->token_;
						
						subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_mutationEffect) == 0))
				{
					if ((n_callback_children != 1) && (n_callback_children != 2))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): mutationEffect() callback needs 1 or 2 parameters." << EidosTerminate(callback_token);
					
					EidosToken *mutation_type_id_token = callback_children[0]->token_;
					
					mutation_type_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(mutation_type_id_token->token_string_, 'm', mutation_type_id_token);
					type_ = SLiMEidosBlockType::SLiMEidosMutationEffectCallback;
					
					if (n_callback_children == 2)
					{
						EidosToken *subpop_id_token = callback_children[1]->token_;
						
						subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_mutation) == 0))
				{
					if ((n_callback_children != 0) && (n_callback_children != 1) && (n_callback_children != 2))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): mutation() callback needs 0, 1, or 2 parameters." << EidosTerminate(callback_token);
					
					if (n_callback_children >= 1)
					{
						EidosToken *mutation_type_id_token = callback_children[0]->token_;
						
						if (mutation_type_id_token->token_string_ == gEidosStr_NULL)
							mutation_type_id_ = -1;	// special placeholder that indicates a NULL mutation type identifier
						else
							mutation_type_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(mutation_type_id_token->token_string_, 'm', mutation_type_id_token);
						
						if (n_callback_children == 2)
						{
							EidosToken *subpop_id_token = callback_children[1]->token_;
							
							subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
						}
					}
					
					type_ = SLiMEidosBlockType::SLiMEidosMutationCallback;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_interaction) == 0))
				{
					if ((n_callback_children != 1) && (n_callback_children != 2))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): interaction() callback needs 1 or 2 parameters." << EidosTerminate(callback_token);
					
					EidosToken *interaction_type_id_token = callback_children[0]->token_;
					
					interaction_type_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(interaction_type_id_token->token_string_, 'i', interaction_type_id_token);
					
					if (n_callback_children == 2)
					{
						EidosToken *subpop_id_token = callback_children[1]->token_;
						
						subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
					
					type_ = SLiMEidosBlockType::SLiMEidosInteractionCallback;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_mateChoice) == 0))
				{
					if ((n_callback_children != 0) && (n_callback_children != 1))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): mateChoice() callback needs 0 or 1 parameters." << EidosTerminate(callback_token);
					
					if (n_callback_children == 1)
					{
						EidosToken *subpop_id_token = callback_children[0]->token_;
						
						subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
					
					type_ = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_modifyChild) == 0))
				{
					if ((n_callback_children != 0) && (n_callback_children != 1))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): modifyChild() callback needs 0 or 1 parameters." << EidosTerminate(callback_token);
					
					if (n_callback_children == 1)
					{
						EidosToken *subpop_id_token = callback_children[0]->token_;
						
						subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
					
					type_ = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_recombination) == 0))
				{
					if ((n_callback_children != 0) && (n_callback_children != 1))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): recombination() callback needs 0 or 1 parameters." << EidosTerminate(callback_token);
					
					if (n_callback_children == 1)
					{
						EidosToken *subpop_id_token = callback_children[0]->token_;
						
						subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
					
					type_ = SLiMEidosBlockType::SLiMEidosRecombinationCallback;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_survival) == 0))
				{
					if ((n_callback_children != 0) && (n_callback_children != 1))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): survival() callback needs 0 or 1 parameters." << EidosTerminate(callback_token);
					
					if (n_callback_children == 1)
					{
						EidosToken *subpop_id_token = callback_children[0]->token_;
						
						subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
					
					type_ = SLiMEidosBlockType::SLiMEidosSurvivalCallback;
				}
				else if ((callback_type == EidosTokenType::kTokenIdentifier) && (callback_name.compare(gStr_reproduction) == 0))
				{
					if ((n_callback_children != 0) && (n_callback_children != 1) && (n_callback_children != 2))
						EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): reproduction() callback needs 0, 1, or 2 parameters." << EidosTerminate(callback_token);
					
					if (n_callback_children >= 1)
					{
						EidosToken *subpop_id_token = callback_children[0]->token_;
						
						if (subpop_id_token->token_string_ == gEidosStr_NULL)
							subpopulation_id_ = -1;		// not limited to one subpopulation
						else
							subpopulation_id_ = SLiMEidosScript::ExtractIDFromStringWithPrefix(subpop_id_token->token_string_, 'p', subpop_id_token);
					}
					
					if (n_callback_children >= 2)
					{
						EidosToken *sex_token = callback_children[1]->token_;
						
						if ((sex_token->token_type_ == EidosTokenType::kTokenIdentifier) && (sex_token->token_string_ == gEidosStr_NULL))
							sex_specificity_ = IndividualSex::kUnspecified;		// not limited by sex
						else if ((sex_token->token_type_ == EidosTokenType::kTokenString) && (sex_token->token_string_ == "M"))
							sex_specificity_ = IndividualSex::kMale;
						else if ((sex_token->token_type_ == EidosTokenType::kTokenString) && (sex_token->token_string_ == "F"))
							sex_specificity_ = IndividualSex::kFemale;
						else
							EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): reproduction() callback needs a value for sex of 'M', 'F', or NULL." << EidosTerminate(callback_token);
					}
					
					type_ = SLiMEidosBlockType::SLiMEidosReproductionCallback;
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): unknown callback type." << EidosTerminate(callback_token);
				}
				
				child_index++;
			}
		}
		
		// eat the compound statement, which must be present
		if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == EidosTokenType::kTokenLBrace))
		{
			compound_statement_node_ = block_children[child_index];
			child_index++;
		}
	}
	
	if (!compound_statement_node_)
		EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): no compound statement found for SLiMEidosBlock." << EidosTerminate(child_index > 0 ? block_children[child_index - 1]->token_ : nullptr);
	
	if (child_index != n_children)
		EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SLiMEidosBlock): unexpected node in SLiMEidosBlock." << EidosTerminate(block_children[child_index]->token_);
	
	ScanTreeForIdentifiersUsed();
}

SLiMEidosBlock::SLiMEidosBlock(slim_objectid_t p_id, const std::string &p_script_string, int32_t p_user_script_line_offset, SLiMEidosBlockType p_type, slim_tick_t p_start, slim_tick_t p_end, Species *p_species_spec, Species *p_ticks_spec) :
	self_symbol_(gID_self, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMEidosBlock_Class))),
	script_block_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('s', p_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_SLiMEidosBlock_Class))),
	type_(p_type), block_id_(p_id), start_tick_(p_start), end_tick_(p_end), species_spec_(p_species_spec), ticks_spec_(p_ticks_spec), user_script_line_offset_(p_user_script_line_offset)
{
	script_ = new EidosScript(p_script_string, p_user_script_line_offset);
	// the caller should now call TokenizeAndParse() to complete initialization
}

SLiMEidosBlock::~SLiMEidosBlock(void)
{
	delete script_;
}

void SLiMEidosBlock::TokenizeAndParse(void)
{
	// This should be called on script-based SLiMEidosBlocks immediately after construction;
	// it is separated out from the constructor for simplicity because it may raise.
	if (script_)
	{
		script_->Tokenize();
		script_->ParseInterpreterBlockToAST(false);
		
		root_node_ = script_->AST();
		
		if (root_node_->children_.size() != 1)
			EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::TokenizeAndParse): script blocks must be compound statements." << EidosTerminate();
		if (root_node_->children_[0]->token_->token_type_ != EidosTokenType::kTokenLBrace)
			EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::TokenizeAndParse): script blocks must be compound statements." << EidosTerminate();
		
		compound_statement_node_ = root_node_->children_[0];
		
		ScanTreeForIdentifiersUsed();
	}
}

void SLiMEidosBlock::_ScanNodeForIdentifiersUsed(const EidosASTNode *p_scan_node)
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : p_scan_node->children_)
		_ScanNodeForIdentifiersUsed(child);
	
	if (p_scan_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
	{
		const std::string &token_string = p_scan_node->token_->token_string_;
		
		if (token_string.compare(gEidosStr_apply) == 0)						contains_wildcard_ = true;
		if (token_string.compare(gEidosStr_sapply) == 0)					contains_wildcard_ = true;
		if (token_string.compare(gEidosStr_doCall) == 0)					contains_wildcard_ = true;
		if (token_string.compare(gEidosStr_executeLambda) == 0)				contains_wildcard_ = true;
		if (token_string.compare(gEidosStr__executeLambda_OUTER) == 0)		contains_wildcard_ = true;
		if (token_string.compare(gEidosStr_ls) == 0)						contains_wildcard_ = true;
		if (token_string.compare(gEidosStr_rm) == 0)						contains_wildcard_ = true;
		
		if (token_string.compare(gStr_self) == 0)				contains_self_ = true;
		
		if (token_string.compare(gStr_mut) == 0)				contains_mut_ = true;
		if (token_string.compare(gStr_effect) == 0)				contains_effect_ = true;
		if (token_string.compare(gStr_individual) == 0)			contains_individual_ = true;
		if (token_string.compare(gStr_element) == 0)			contains_element_ = true;
		if (token_string.compare(gStr_genome) == 0)				contains_genome_ = true;
		if (token_string.compare(gStr_genome1) == 0)			contains_genome1_ = true;
		if (token_string.compare(gStr_genome2) == 0)			contains_genome2_ = true;
		if (token_string.compare(gStr_subpop) == 0)				contains_subpop_ = true;
		if (token_string.compare(gStr_homozygous) == 0)			contains_homozygous_ = true;
		if (token_string.compare(gStr_sourceSubpop) == 0)		contains_sourceSubpop_ = true;
		if (token_string.compare(gEidosStr_weights) == 0)		contains_weights_ = true;
		if (token_string.compare(gStr_child) == 0)				contains_child_ = true;
		if (token_string.compare(gStr_parent) == 0)				contains_parent_ = true;
		if (token_string.compare(gStr_parent1) == 0)			contains_parent1_ = true;
		if (token_string.compare(gStr_isCloning) == 0)			contains_isCloning_ = true;
		if (token_string.compare(gStr_isSelfing) == 0)			contains_isSelfing_ = true;
		if (token_string.compare(gStr_parent2) == 0)			contains_parent2_ = true;
		if (token_string.compare(gStr_breakpoints) == 0)		contains_breakpoints_ = true;
		if (token_string.compare(gStr_distance) == 0)			contains_distance_ = true;
		if (token_string.compare(gStr_strength) == 0)			contains_strength_ = true;
		if (token_string.compare(gStr_receiver) == 0)			contains_receiver_ = true;
		if (token_string.compare(gStr_exerter) == 0)			contains_exerter_ = true;
		if (token_string.compare(gStr_originalNuc) == 0)		contains_originalNuc_ = true;
		if (token_string.compare(gStr_surviving) == 0)			contains_surviving_ = true;
		if (token_string.compare(gStr_fitness) == 0)			contains_fitness_ = true;
		if (token_string.compare(gStr_draw) == 0)				contains_draw_ = true;
	}
}

void SLiMEidosBlock::ScanTreeForIdentifiersUsed(void)
{
	_ScanNodeForIdentifiersUsed(compound_statement_node_);
	
	// If the script block contains a "wildcard" – an identifier that signifies that any other identifier could be accessed – then
	// we just set all of our "contains_" flags to T.  Any new flag that is added must be added here too!
	if (contains_wildcard_)
	{
		contains_self_ = true;
		contains_mut_ = true;
		contains_effect_ = true;
		contains_individual_ = true;
		contains_element_ = true;
		contains_genome_ = true;
		contains_genome1_ = true;
		contains_genome2_ = true;
		contains_subpop_ = true;
		contains_homozygous_ = true;
		contains_sourceSubpop_ = true;
		contains_weights_ = true;
		contains_child_ = true;
		contains_parent_ = true;
		contains_parent1_ = true;
		contains_isCloning_ = true;
		contains_isSelfing_ = true;
		contains_parent2_ = true;
		contains_breakpoints_ = true;
		contains_distance_ = true;
		contains_strength_ = true;
		contains_receiver_ = true;
		contains_exerter_ = true;
		contains_originalNuc_ = true;
		contains_surviving_ = true;
		contains_fitness_ = true;
		contains_draw_ = true;
	}
}

#ifdef SLIMGUI
// used by SLiMgui to generate the scheduling log's output
void SLiMEidosBlock::PrintDeclaration(std::ostream& p_out, Community *p_community)
{
	if (p_community->is_explicit_species_)
	{
		if ((type_ == SLiMEidosBlockType::SLiMEidosEventFirst) ||
			(type_ == SLiMEidosBlockType::SLiMEidosEventEarly) ||
			(type_ == SLiMEidosBlockType::SLiMEidosEventLate))
		{
			// events have ticks specifiers
			if (ticks_spec_ == nullptr)
				gSLiMScheduling << "ticks all ";
			else
				gSLiMScheduling << "ticks " << ticks_spec_->name_ << " ";
		}
		else if (type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
		{
			// callbacks have species specifiers
			if (species_spec_ == nullptr)
				gSLiMScheduling << "species all ";
			else
				gSLiMScheduling << "species " << species_spec_->name_ << " ";
		}
	}
	
	if (block_id_ != -1)
		gSLiMScheduling << "s" << block_id_ << " ";
	
	if (type_ != SLiMEidosBlockType::SLiMEidosInitializeCallback)
	{
		if (start_tick_ != -1)
			gSLiMScheduling << start_tick_;
		if (end_tick_ != start_tick_)
		{
			if ((start_tick_ != -1) || (end_tick_ != SLIM_MAX_TICK + 1))
				gSLiMScheduling << ":";
			if (end_tick_ != SLIM_MAX_TICK + 1)
				gSLiMScheduling << end_tick_;
		}
		if ((start_tick_ != -1) || (end_tick_ != SLIM_MAX_TICK + 1))
			gSLiMScheduling << " ";
	}
	
	switch (type_)
	{
		case SLiMEidosBlockType::SLiMEidosEventFirst:				p_out << "first()"; break;
		case SLiMEidosBlockType::SLiMEidosEventEarly:				p_out << "early()"; break;
		case SLiMEidosBlockType::SLiMEidosEventLate:				p_out << "late()"; break;
		case SLiMEidosBlockType::SLiMEidosInitializeCallback:		p_out << "initialize()"; break;
		case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	p_out << "fitnessEffect()"; break;
		case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		p_out << "function"; break;
		case SLiMEidosBlockType::SLiMEidosNoBlockType:				p_out << "NO BLOCK"; break;
			
		case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:
		{
			// mutationEffect(<mutTypeId> [, <subpopId>])
			p_out << "mutationEffect(m" << mutation_type_id_;
			if (subpopulation_id_ != -1)
				p_out << ", p" << subpopulation_id_;
			p_out << ")";
			break;
		}
			
		case SLiMEidosBlockType::SLiMEidosInteractionCallback:
		{
			// interaction(<intTypeId> [, <subpopId>])
			p_out << "interaction(i" << interaction_type_id_;
			if (subpopulation_id_ != -1)
				p_out << ", p" << subpopulation_id_;
			p_out << ")";
			break;
		}
			
		case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:
		{
			// mateChoice([<subpopId>])
			p_out << "mateChoice(";
			if (subpopulation_id_ != -1)
				p_out << "p" << subpopulation_id_;
			p_out << ")";
			break;
		}
			
		case SLiMEidosBlockType::SLiMEidosModifyChildCallback:
		{
			// modifyChild([<subpopId>])
			p_out << "modifyChild(";
			if (subpopulation_id_ != -1)
				p_out << "p" << subpopulation_id_;
			p_out << ")";
			break;
		}
			
		case SLiMEidosBlockType::SLiMEidosRecombinationCallback:
		{
			// recombination([<subpopId>])
			p_out << "recombination(";
			if (subpopulation_id_ != -1)
				p_out << "p" << subpopulation_id_;
			p_out << ")";
			break;
		}
			
		case SLiMEidosBlockType::SLiMEidosMutationCallback:
		{
			// mutation([<mutTypeId> [, <subpopId>]])
			p_out << "mutation(";
			if (mutation_type_id_ != -1)
				p_out << "m" << mutation_type_id_;
			else if (subpopulation_id_ != -1)
				p_out << "NULL";
			if (subpopulation_id_ != -1)
				p_out << ", p" << subpopulation_id_;
			p_out << ")";
			break;
		}
			
		case SLiMEidosBlockType::SLiMEidosSurvivalCallback:
		{
			// survival([<subpopId>])
			p_out << "survival(";
			if (subpopulation_id_ != -1)
				p_out << "p" << subpopulation_id_;
			p_out << ")";
			break;
		}
			
		case SLiMEidosBlockType::SLiMEidosReproductionCallback:
		{
			// reproduction([<subpopId> [, <sex>]])
			p_out << "reproduction(";
			if (subpopulation_id_ != -1)
				p_out << "p" << subpopulation_id_;
			else if (sex_specificity_ != IndividualSex::kUnspecified)
				p_out << "NULL";
			if (sex_specificity_ != IndividualSex::kUnspecified)
				p_out << ", \"" << sex_specificity_ << "\"";
			p_out << ")";
			break;
		}
	}
	
	int32_t token_line = root_node_->token_->token_line_;
	
	if (token_line != -1)
		gSLiMScheduling << " [line " << (token_line + 1) << "]";
}
#endif


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *SLiMEidosBlock::Class(void) const
{
	return gSLiM_SLiMEidosBlock_Class;
}

void SLiMEidosBlock::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName() << "<";
	
	if (start_tick_ > 0)
	{
		p_ostream << start_tick_;
		
		if (end_tick_ != start_tick_)
			p_ostream << ":" << end_tick_;
		
		p_ostream << " : ";
	}
	
	switch (type_)
	{
		case SLiMEidosBlockType::SLiMEidosEventFirst:				p_ostream << gStr_first; break;
		case SLiMEidosBlockType::SLiMEidosEventEarly:				p_ostream << gStr_early; break;
		case SLiMEidosBlockType::SLiMEidosEventLate:				p_ostream << gStr_late; break;
		case SLiMEidosBlockType::SLiMEidosInitializeCallback:		p_ostream << gStr_initialize; break;
		case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	p_ostream << gStr_mutationEffect; break;
		case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	p_ostream << gStr_fitnessEffect; break;
		case SLiMEidosBlockType::SLiMEidosInteractionCallback:		p_ostream << gStr_interaction; break;
		case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		p_ostream << gStr_mateChoice; break;
		case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		p_ostream << gStr_modifyChild; break;
		case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	p_ostream << gStr_recombination; break;
		case SLiMEidosBlockType::SLiMEidosMutationCallback:			p_ostream << gStr_mutation; break;
		case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			p_ostream << gStr_survival; break;
		case SLiMEidosBlockType::SLiMEidosReproductionCallback:		p_ostream << gStr_reproduction; break;
		case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		p_ostream << gEidosStr_function; break;
		case SLiMEidosBlockType::SLiMEidosNoBlockType: 				break;	// never hit
	}
	
	p_ostream << ">";
}

EidosValue_SP SLiMEidosBlock::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:
		{
			if (!cached_value_block_id_)
				cached_value_block_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(block_id_));
			return cached_value_block_id_;
		}
		case gEidosID_start:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(start_tick_));
		case gEidosID_end:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(end_tick_));
		case gEidosID_type:
		{
			switch (type_)
			{
				case SLiMEidosBlockType::SLiMEidosEventFirst:				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_first));
				case SLiMEidosBlockType::SLiMEidosEventEarly:				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_early));
				case SLiMEidosBlockType::SLiMEidosEventLate:				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_late));
				case SLiMEidosBlockType::SLiMEidosInitializeCallback:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_initialize));
				case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_mutationEffect));
				case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_fitnessEffect));
				case SLiMEidosBlockType::SLiMEidosInteractionCallback:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_interaction));
				case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_mateChoice));
				case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_modifyChild));
				case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_recombination));
				case SLiMEidosBlockType::SLiMEidosMutationCallback:			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_mutation));
				case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_survival));
				case SLiMEidosBlockType::SLiMEidosReproductionCallback:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_reproduction));
				case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_function));
				case SLiMEidosBlockType::SLiMEidosNoBlockType:				return gStaticEidosValue_StringAsterisk;	// never hit
			}
			EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::GetProperty): (internal error) unrecognized value for type_." << EidosTerminate();
		}
		case gEidosID_source:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(compound_statement_node_->token_->token_string_));
		case gID_speciesSpec:
		{
			// With no species spec, we return an empty object vector of class Species; this is allowed since this is a read-only property
			if (species_spec_)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(species_spec_, gSLiM_Species_Class));
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Species_Class));
		}
		case gID_ticksSpec:
		{
			// With no ticks spec, we return an empty object vector of class Species; this is allowed since this is a read-only property
			if (ticks_spec_)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(ticks_spec_, gSLiM_Species_Class));
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Species_Class));
		}
			
			// variables
		case gID_active:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(block_active_));
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::GetProperty): property tag accessed on script block before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void SLiMEidosBlock::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gID_active:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			// cannot activate a block if it has been deactivated by its association with an inactive species
			if (value && ((species_spec_ && !species_spec_->Active()) || (ticks_spec_ && !ticks_spec_->Active())))
				EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::SetProperty): property active cannot be used to activate a block that is inactive because of a 'species' or 'ticks' specifier in its declaration, or because it was deactivated by a call to skipTick()." << EidosTerminate();
			
			block_active_ = value;
			
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
			return super::SetProperty(p_property_id, p_value);
	}
}


//
//	SLiMEidosBlock_Class
//
#pragma mark -
#pragma mark SLiMEidosBlock_Class
#pragma mark -

EidosClass *gSLiM_SLiMEidosBlock_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *SLiMEidosBlock_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("SLiMEidosBlock_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_active,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_start,		true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_end,		true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_type,		true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_source,	true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_speciesSpec,		true,	kEidosValueMaskObject, gSLiM_Species_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_ticksSpec,		true,	kEidosValueMaskObject, gSLiM_Species_Class)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}


//
//	SLiMTypeTable
//
#pragma mark -
#pragma mark SLiMTypeTable
#pragma mark -

SLiMTypeTable::SLiMTypeTable(void) : EidosTypeTable()
{
}

SLiMTypeTable::~SLiMTypeTable(void)
{
}

bool SLiMTypeTable::ContainsSymbol(EidosGlobalStringID p_symbol_name) const
{
	bool has_symbol = EidosTypeTable::ContainsSymbol(p_symbol_name);
	
	if (!has_symbol)
	{
		// If our superclass is not aware of the symbol, then we want to pretend it exists if it follows
		// one of the standard naming patterns pX, gX, mX, or sX; this lets the user complete off of
		// those roots even if the simulation is not aware of the existence of the variable.  See also
		// eidosConsoleWindowController:tokenStringIsSpecialIdentifier:
		const std::string &token_string = EidosStringRegistry::StringForGlobalStringID(p_symbol_name);
		int len = (int)token_string.length();
		
		if (len >= 2)
		{
			char first_ch = token_string[0];
			
			if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm') || (first_ch == 's') || (first_ch == 'i'))
			{
				for (int ch_index = 1; ch_index < len; ++ch_index)
				{
					char idx_ch = token_string[ch_index];
					
					if ((idx_ch < '0') || (idx_ch > '9'))
						return false;
				}
				
				return true;
			}
		}
		
		return false;
	}
	
	return has_symbol;
}

EidosTypeSpecifier SLiMTypeTable::GetTypeForSymbol(EidosGlobalStringID p_symbol_name) const
{
	EidosTypeSpecifier symbol_type = EidosTypeTable::GetTypeForSymbol(p_symbol_name);
	
	if (symbol_type.type_mask == kEidosValueMaskNone)
	{
		// If our superclass is not aware of the symbol, then we want to pretend it exists if it follows
		// one of the standard naming patterns pX, gX, mX, or sX; this lets the user complete off of
		// those roots even if the simulation is not aware of the existence of the variable.  See also
		// eidosConsoleWindowController:tokenStringIsSpecialIdentifier:
		const std::string &token_string = EidosStringRegistry::StringForGlobalStringID(p_symbol_name);
		int len = (int)token_string.length();
		
		if (len >= 2)
		{
			char first_ch = token_string[0];
			
			if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm') || (first_ch == 's') || (first_ch == 'i'))
			{
				for (int ch_index = 1; ch_index < len; ++ch_index)
				{
					char idx_ch = token_string[ch_index];
					
					if ((idx_ch < '0') || (idx_ch > '9'))
						return symbol_type;
				}
				
				switch(first_ch)
				{
					case 'p': return EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class};
					case 'g': return EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class};
					case 'm': return EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_MutationType_Class};
					case 's': return EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class};
					case 'i': return EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_InteractionType_Class};
					default: break;	// never hit, given the if above
				}
			}
		}
		
		return symbol_type;
	}
	
	return symbol_type;
}


//
//	SLiMTypeInterpreter
//
#pragma mark -
#pragma mark SLiMTypeInterpreter
#pragma mark -

SLiMTypeInterpreter::SLiMTypeInterpreter(const EidosScript &p_script, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types)
	: EidosTypeInterpreter(p_script, p_symbols, p_functions, p_call_types)
{
}

SLiMTypeInterpreter::SLiMTypeInterpreter(const EidosASTNode *p_root_node_, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types)
	: EidosTypeInterpreter(p_root_node_, p_symbols, p_functions, p_call_types)
{
}

SLiMTypeInterpreter::~SLiMTypeInterpreter(void)
{
}

void SLiMTypeInterpreter::_SetTypeForISArgumentOfClass(const EidosASTNode *p_arg_node, char p_symbol_prefix, const EidosClass *p_type_class)
{
	if (p_arg_node)
	{
		const EidosToken *arg_token = p_arg_node->token_;
		
		if (arg_token->token_type_ == EidosTokenType::kTokenString)
		{
			// The argument can be a string, in which case it must start with p_symbol_prefix and then have 1+ numeric characters
			const std::string &constant_name = arg_token->token_string_;
			
			if ((constant_name.length() >= 2) && (constant_name[0] == p_symbol_prefix))
			{
				bool all_numeric = true;
				
				for (size_t idx = 1; idx < constant_name.length(); ++idx)
					if (!isdigit(constant_name[idx]))
						all_numeric = false;
				
				if (all_numeric)
				{
					EidosGlobalStringID constant_id = EidosStringRegistry::GlobalStringIDForString(constant_name);
					
					global_symbols_->SetTypeForSymbol(constant_id, EidosTypeSpecifier{kEidosValueMaskObject, p_type_class});
				}
			}
		}
		else if (arg_token->token_type_ == EidosTokenType::kTokenNumber)
		{
			// The argument can be numeric, in which case it must have a cached int value that is singleton and within bounds
			EidosValue *cached_value = p_arg_node->cached_literal_value_.get();
			
			if (cached_value && (cached_value->Type() == EidosValueType::kValueInt) && (cached_value->IsSingleton()))
			{
				int64_t cached_int = cached_value->IntAtIndex(0, nullptr);
				
				if ((cached_int >= 0) && (cached_int <= SLIM_MAX_ID_VALUE))
				{
					EidosGlobalStringID constant_id = EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix(p_symbol_prefix, static_cast<slim_objectid_t>(cached_int)));
					
					global_symbols_->SetTypeForSymbol(constant_id, EidosTypeSpecifier{kEidosValueMaskObject, p_type_class});
				}
			}
		}
	}
}

EidosTypeSpecifier SLiMTypeInterpreter::_TypeEvaluate_FunctionCall_Internal(std::string const &p_function_name, const EidosFunctionSignature *p_function_signature, const std::vector<EidosASTNode *> &p_arguments)
{
	// call super; this should always be called, since it type-evaluates all arguments as a side effect
	EidosTypeSpecifier ret = EidosTypeInterpreter::_TypeEvaluate_FunctionCall_Internal(p_function_name, p_function_signature, p_arguments);
	
	// Create any symbols defined as a side effect of this call, which happens after argument type-evaluation.
	// In figuring this stuff out, we need to be careful about the fact that the p_arguments vector can contain nullptr
	// values if there were missing arguments, etc.; we try to be error-tolerant, so we allow cases that would raise
	// in EidosInterpreter.  _SetTypeForISArgumentOfClass() is safe to call with nullptr.
	int argument_count = (int)p_arguments.size();
	
	if ((p_function_name == "initializeGenomicElementType") && (argument_count >= 1))
	{
		_SetTypeForISArgumentOfClass(p_arguments[0], 'g', gSLiM_GenomicElementType_Class);
	}
	else if (((p_function_name == "initializeMutationType") || (p_function_name == "initializeMutationTypeNuc")) && (argument_count >= 1))
	{
		_SetTypeForISArgumentOfClass(p_arguments[0], 'm', gSLiM_MutationType_Class);
	}
	else if ((p_function_name == "initializeInteractionType") && (argument_count >= 1))
	{
		_SetTypeForISArgumentOfClass(p_arguments[0], 'i', gSLiM_InteractionType_Class);
	}
	
	return ret;
}

EidosTypeSpecifier SLiMTypeInterpreter::_TypeEvaluate_MethodCall_Internal(const EidosClass *p_target, const EidosMethodSignature *p_method_signature, const std::vector<EidosASTNode *> &p_arguments)
{
	// call super; this should always be called, since it type-evaluates all arguments as a side effect
	EidosTypeSpecifier ret = EidosTypeInterpreter::_TypeEvaluate_MethodCall_Internal(p_target, p_method_signature, p_arguments);
	
	// Create any symbols defined as a side effect of this call, which happens after argument type-evaluation.
	// In figuring this stuff out, we need to be careful about the fact that the p_arguments vector can contain nullptr
	// values if there were missing arguments, etc.; we try to be error-tolerant, so we allow cases that would raise
	// in EidosInterpreter.  _SetTypeForISArgumentOfClass() is safe to call with nullptr.
	if (p_method_signature)
	{
		if (p_target == gSLiM_Community_Class)
		{
			int argument_count = (int)p_arguments.size();
			
			const std::string &function_name = p_method_signature->call_name_;
			
			if (((function_name == "registerFirstEvent") ||
				 (function_name == "registerEarlyEvent") ||
				 (function_name == "registerInteractionCallback") ||
				 (function_name == "registerLateEvent") ||
				 (function_name == "rescheduleScriptBlock")) && (argument_count >= 1))
			{
				_SetTypeForISArgumentOfClass(p_arguments[0], 's', gSLiM_SLiMEidosBlock_Class);
			}
		}
		else if (p_target == gSLiM_Species_Class)
		{
			int argument_count = (int)p_arguments.size();
			
			const std::string &function_name = p_method_signature->call_name_;
			
			if (((function_name == "addSubpop") || (function_name == "addSubpopSplit")) && (argument_count >= 1))
			{
				_SetTypeForISArgumentOfClass(p_arguments[0], 'p', gSLiM_Subpopulation_Class);
			}
			if (((function_name == "registerFitnessEffectCallback") ||
				 (function_name == "registerMutationEffectCallback") ||
				 (function_name == "registerMateChoiceCallback") ||
				 (function_name == "registerModifyChildCallback") ||
				 (function_name == "registerRecombinationCallback") ||
				 (function_name == "registerMutationCallback") ||
				 (function_name == "registerSurvivalCallback") ||
				 (function_name == "registerReproductionCallback")) && (argument_count >= 1))
			{
				_SetTypeForISArgumentOfClass(p_arguments[0], 's', gSLiM_SLiMEidosBlock_Class);
			}
		}
	}
	
	return ret;
}

























































