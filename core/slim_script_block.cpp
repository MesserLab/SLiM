//
//  slim_script_block.cpp
//  SLiM
//
//  Created by Ben Haller on 6/7/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#include "slim_script_block.h"
#include "script_interpreter.h"

#include "errno.h"


using std::endl;
using std::string;


SLiMScript::SLiMScript(const string &p_script_string, int p_start_index) : Script(p_script_string, p_start_index)
{
}

SLiMScript::~SLiMScript(void)
{
}

ScriptASTNode *SLiMScript::Parse_SLiMFile(void)
{
	ScriptToken *virtual_token = new ScriptToken(TokenType::kTokenSLiMFile, gStr_empty_string, 0, 0);
	ScriptASTNode *node = new ScriptASTNode(virtual_token, true);
	
	while (current_token_type_ != TokenType::kTokenEOF)
	{
		// We handle the grammar a bit differently than how it is printed in the railroad diagrams in the doc.
		// Parsing of the optional generation range is done in Parse_SLiMScriptBlock() since it ends up as children of that node.
		ScriptASTNode *script_block = Parse_SLiMScriptBlock();
		
		node->AddChild(script_block);
	}
	
	Match(TokenType::kTokenEOF, "SLiM file");
	
	return node;
}

ScriptASTNode *SLiMScript::Parse_SLiMScriptBlock(void)
{
	ScriptToken *virtual_token = new ScriptToken(TokenType::kTokenSLiMScriptBlock, gStr_empty_string, 0, 0);
	ScriptASTNode *slim_script_block_node = new ScriptASTNode(virtual_token, true);
	
	// We handle the grammar a bit differently than how it is printed in the railroad diagrams in the doc.
	// We parse the slim_script_info section here, as part of the script block.
	if (current_token_type_ == TokenType::kTokenString)
	{
		// a script identifier string is present; add it
		ScriptASTNode *script_id_node = Parse_Constant();
		
		slim_script_block_node->AddChild(script_id_node);
	}
	
	if (current_token_type_ == TokenType::kTokenNumber)
	{
		// A start generation is present; add it
		ScriptASTNode *start_generation_node = Parse_Constant();
		
		slim_script_block_node->AddChild(start_generation_node);
		
		if (current_token_type_ == TokenType::kTokenColon)
		{
			// An end generation is present; add it
			Match(TokenType::kTokenColon, "SLiM script block");
			
			if (current_token_type_ == TokenType::kTokenNumber)
			{
				ScriptASTNode *end_generation_node = Parse_Constant();
				
				slim_script_block_node->AddChild(end_generation_node);
			}
			else
			{
				SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_SLiMScriptBlock" << slim_terminate();
			}
		}
	}
	
	// Now we are to the point of parsing the actual slim_script_block
	if (current_token_type_ == TokenType::kTokenIdentifier)
	{
		if (current_token_->token_string_.compare(gStr_fitness) == 0)
		{
			ScriptASTNode *callback_info_node = new ScriptASTNode(current_token_);
			
			Match(TokenType::kTokenIdentifier, "SLiM fitness() callback");
			Match(TokenType::kTokenLParen, "SLiM fitness() callback");
			
			if (current_token_type_ == TokenType::kTokenNumber)
			{
				// A (required) mutation type id is present; add it
				ScriptASTNode *mutation_type_id_node = Parse_Constant();
				
				callback_info_node->AddChild(mutation_type_id_node);
			}
			else
			{
				SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_SLiMScriptBlock; a mutation type id is required in fitness() callback definitions" << slim_terminate();
			}
			
			if (current_token_type_ == TokenType::kTokenComma)
			{
				// A (optional) subpopulation id is present; add it
				Match(TokenType::kTokenComma, "SLiM fitness() callback");
				
				if (current_token_type_ == TokenType::kTokenNumber)
				{
					ScriptASTNode *subpopulation_id_node = Parse_Constant();
					
					callback_info_node->AddChild(subpopulation_id_node);
				}
				else
				{
					SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_SLiMScriptBlock; a subpopulation id is expected after a comma in fitness() callback definitions" << slim_terminate();
				}
			}
			
			Match(TokenType::kTokenRParen, "SLiM fitness() callback");
			
			slim_script_block_node->AddChild(callback_info_node);
		}
		else if (current_token_->token_string_.compare(gStr_mateChoice) == 0)
		{
			ScriptASTNode *callback_info_node = new ScriptASTNode(current_token_);
			
			Match(TokenType::kTokenIdentifier, "SLiM mateChoice() callback");
			Match(TokenType::kTokenLParen, "SLiM mateChoice() callback");
			
			// A (optional) subpopulation id is present; add it
			if (current_token_type_ == TokenType::kTokenNumber)
			{
				ScriptASTNode *subpopulation_id_node = Parse_Constant();
				
				callback_info_node->AddChild(subpopulation_id_node);
			}
			
			Match(TokenType::kTokenRParen, "SLiM mateChoice() callback");
			
			slim_script_block_node->AddChild(callback_info_node);
		}
		else if (current_token_->token_string_.compare(gStr_modifyChild) == 0)
		{
			ScriptASTNode *callback_info_node = new ScriptASTNode(current_token_);
			
			Match(TokenType::kTokenIdentifier, "SLiM modifyChild() callback");
			Match(TokenType::kTokenLParen, "SLiM modifyChild() callback");
			
			// A (optional) subpopulation id is present; add it
			if (current_token_type_ == TokenType::kTokenNumber)
			{
				ScriptASTNode *subpopulation_id_node = Parse_Constant();
				
				callback_info_node->AddChild(subpopulation_id_node);
			}
			
			Match(TokenType::kTokenRParen, "SLiM modifyChild() callback");
			
			slim_script_block_node->AddChild(callback_info_node);
		}
	}
	
	// Regardless of what happened above, all SLiMscript blocks end with a compound statement, which is the last child of the node
	ScriptASTNode *compound_statement_node = Parse_CompoundStatement();
	
	slim_script_block_node->AddChild(compound_statement_node);
	
	return slim_script_block_node;
}

void SLiMScript::ParseSLiMFileToAST(void)
{
	// delete the existing AST
	delete parse_root_;
	parse_root_ = nullptr;
	
	// set up parse state
	parse_index_ = 0;
	current_token_ = token_stream_.at(parse_index_);		// should always have at least an EOF
	current_token_type_ = current_token_->token_type_;
	
	// parse a new AST from our start token
	ScriptASTNode *tree = Parse_SLiMFile();
	
	tree->OptimizeTree();
	
	parse_root_ = tree;
	
	// if logging of the AST is requested, do that; always to cout, not to SLIM_OUTSTREAM
	if (gSLiMScriptLogAST)
	{
		std::cout << "AST : \n";
		this->PrintAST(std::cout);
	}
}



SLiMScriptBlock::SLiMScriptBlock(ScriptASTNode *p_root_node) : root_node_(p_root_node)
{
	const std::vector<ScriptASTNode *> &block_children = root_node_->children_;
	int child_index = 0, n_children = (int)block_children.size();
	
	// eat a string, for the script id, if present
	block_id_ = -1;	// the default unless it is set below
	
	if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == TokenType::kTokenString))
	{
		const string &id_string = block_children[child_index]->token_->token_string_;
		const char *id_cstr = id_string.c_str();
		int id_cstr_len = (int)strlen(id_cstr);
		
		if (*id_cstr != 's')
			SLIM_TERMINATION << "ERROR (SLiMScriptBlock::SLiMScriptBlock): the script block id must be a string that begins with \"s\"." << slim_terminate();
		for (int str_index = 1; str_index < id_cstr_len; ++str_index)
			if ((id_cstr[str_index] < '0') || (id_cstr[str_index] > '9'))
				SLIM_TERMINATION << "ERROR (SLiMScriptBlock::SLiMScriptBlock): the script block id after the \"s\" prefix must be a simple integer." << slim_terminate();
		
		if (id_cstr_len < 2)
			SLIM_TERMINATION << "ERROR (SLiMScriptBlock::SLiMScriptBlock): the script block id must have an integer identifier after the \"s\" prefix." << slim_terminate();
		
		errno = 0;
		long long_block_id = strtol(id_cstr + 1, NULL, 10);	// +1 to omit the leading "s"
		
		if (errno)
			SLIM_TERMINATION << "ERROR (SLiMScriptBlock::SLiMScriptBlock): the script block id " << id_string << " was not parseable." << slim_terminate();
		if (long_block_id > INT_MAX)
			SLIM_TERMINATION << "ERROR (SLiMScriptBlock::SLiMScriptBlock): the script block id " << id_string << " was out of range." << slim_terminate();
		
		block_id_ = (int)long_block_id;
		child_index++;
	}
	
	// eat a number, for the start generation, if present
	if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == TokenType::kTokenNumber))
	{
		start_generation_ = (int)ScriptInterpreter::IntForNumberToken(block_children[child_index]->token_);
		end_generation_ = start_generation_;	// if a start is given, the default end is the same as the start
		child_index++;
	}
	
	// eat a number, for the end generation, if present
	if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == TokenType::kTokenNumber))
	{
		end_generation_ = (int)ScriptInterpreter::IntForNumberToken(block_children[child_index]->token_);
		child_index++;
	}
	
	// eat the callback info node, if present
	if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ != TokenType::kTokenLBrace))
	{
		const ScriptASTNode *callback_node = block_children[child_index];
		const ScriptToken *callback_token = callback_node->token_;
		TokenType callback_type = callback_token->token_type_;
		const std::string &callback_name = callback_token->token_string_;
		const std::vector<ScriptASTNode *> &callback_children = callback_node->children_;
		int n_callback_children = (int)callback_children.size();
		
		if ((callback_type == TokenType::kTokenIdentifier) && (callback_name.compare(gStr_fitness) == 0))
		{
			if ((n_callback_children != 1) && (n_callback_children != 2))
				SLIM_TERMINATION << "ERROR (InitializeFromFile): fitness() callback needs 1 or 2 parameters" << slim_terminate();
			
			mutation_type_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[0]->token_);
			
			if (n_callback_children == 2)
				subpopulation_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[1]->token_);
			
			type_ = SLiMScriptBlockType::SLiMScriptFitnessCallback;
		}
		else if ((callback_type == TokenType::kTokenIdentifier) && (callback_name.compare(gStr_mateChoice) == 0))
		{
			if ((n_callback_children != 0) && (n_callback_children != 1))
				SLIM_TERMINATION << "ERROR (InitializeFromFile): mateChoice() callback needs 0 or 1 parameters" << slim_terminate();
			
			if (n_callback_children == 1)
				subpopulation_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[0]->token_);
			
			type_ = SLiMScriptBlockType::SLiMScriptMateChoiceCallback;
		}
		else if ((callback_type == TokenType::kTokenIdentifier) && (callback_name.compare(gStr_modifyChild) == 0))
		{
			if ((n_callback_children != 0) && (n_callback_children != 1))
				SLIM_TERMINATION << "ERROR (InitializeFromFile): modifyChild() callback needs 0 or 1 parameters" << slim_terminate();
			
			if (n_callback_children == 1)
				subpopulation_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[0]->token_);
			
			type_ = SLiMScriptBlockType::SLiMScriptModifyChildCallback;
		}
		else
		{
			SLIM_TERMINATION << "ERROR (InitializeFromFile): unknown callback type" << slim_terminate();
		}
		
		child_index++;
	}
	
	// eat the compound statement, which must be present
	if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == TokenType::kTokenLBrace))
	{
		compound_statement_node_ = block_children[child_index];
		child_index++;
	}
	
	if (!compound_statement_node_)
		SLIM_TERMINATION << "ERROR (InitializeFromFile): no compound statement found for SLiMScriptBlock" << slim_terminate();
	
	if (child_index != n_children)
		SLIM_TERMINATION << "ERROR (InitializeFromFile): unexpected node in SLiMScriptBlock" << slim_terminate();
	
	ScanTree();
}

SLiMScriptBlock::SLiMScriptBlock(int p_id, const std::string &p_script_string, SLiMScriptBlockType p_type, int p_start, int p_end)
	: block_id_(p_id), type_(p_type), start_generation_(p_start), end_generation_(p_end)
{
	script_ = new Script(p_script_string, 0);

	script_->Tokenize();
	script_->ParseInterpreterBlockToAST();
	
	root_node_ = script_->AST();
	
	if (root_node_->children_.size() != 1)
		SLIM_TERMINATION << "ERROR (SLiMScriptBlock::SLiMScriptBlock): script blocks must be compound statements." << slim_terminate();
	if (root_node_->children_[0]->token_->token_type_ != TokenType::kTokenLBrace)
		SLIM_TERMINATION << "ERROR (SLiMScriptBlock::SLiMScriptBlock): script blocks must be compound statements." << slim_terminate();
	
	compound_statement_node_ = root_node_->children_[0];
	
	ScanTree();
}

SLiMScriptBlock::~SLiMScriptBlock(void)
{
	delete script_;
	
	if (self_symbol_)
	{
		delete self_symbol_->second;
		delete self_symbol_;
	}
	if (script_block_symbol_)
	{
		delete script_block_symbol_->second;
		delete script_block_symbol_;
	}
	
	if (cached_value_block_id_)
		delete cached_value_block_id_;
}

void SLiMScriptBlock::_ScanNodeForIdentifiersUsed(const ScriptASTNode *p_scan_node)
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : p_scan_node->children_)
		_ScanNodeForIdentifiersUsed(child);
	
	if (p_scan_node->token_->token_type_ == TokenType::kTokenIdentifier)
	{
		const std::string &token_string = p_scan_node->token_->token_string_;
		
		if (token_string.compare(gStr_executeLambda) == 0)		contains_wildcard_ = true;
		if (token_string.compare(gStr_globals) == 0)			contains_wildcard_ = true;
		
		// ***** If a new flag is added here, it must also be added to the list in SLiMScriptBlock::ScanTree!
		
		if (token_string.compare(gStr_T) == 0)					contains_T_ = true;
		if (token_string.compare(gStr_F) == 0)					contains_F_ = true;
		if (token_string.compare(gStr_NULL) == 0)				contains_NULL_ = true;
		if (token_string.compare(gStr_PI) == 0)					contains_PI_ = true;
		if (token_string.compare(gStr_E) == 0)					contains_E_ = true;
		if (token_string.compare(gStr_INF) == 0)				contains_INF_ = true;
		if (token_string.compare(gStr_NAN) == 0)				contains_NAN_ = true;
		
		// look for instance identifiers like p1, g1, m1, s1; the heuristic here is very dumb, but errs on the safe side
		if (token_string.length() >= 2)
		{
			char char2 = token_string[1];
			
			if ((char2 >= '0') && (char2 <= '9'))
			{
				char char1 = token_string[0];
				
				if (char1 == 'p')								contains_pX_ = true;
				if (char1 == 'g')								contains_gX_ = true;
				if (char1 == 'm')								contains_mX_ = true;
				if (char1 == 's')								contains_sX_ = true;
			}
		}
		
		if (token_string.compare(gStr_sim) == 0)				contains_sim_ = true;
		if (token_string.compare(gStr_self) == 0)				contains_self_ = true;
		
		if (token_string.compare(gStr_mut) == 0)				contains_mut_ = true;
		if (token_string.compare(gStr_relFitness) == 0)			contains_relFitness_ = true;
		if (token_string.compare(gStr_genome1) == 0)			contains_genome1_ = true;
		if (token_string.compare(gStr_genome2) == 0)			contains_genome2_ = true;
		if (token_string.compare(gStr_subpop) == 0)				contains_subpop_ = true;
		if (token_string.compare(gStr_homozygous) == 0)			contains_homozygous_ = true;
		if (token_string.compare(gStr_sourceSubpop) == 0)		contains_sourceSubpop_ = true;
		if (token_string.compare(gStr_weights) == 0)			contains_weights_ = true;
		if (token_string.compare(gStr_childGenome1) == 0)		contains_childGenome1_ = true;
		if (token_string.compare(gStr_childGenome2) == 0)		contains_childGenome2_ = true;
		if (token_string.compare(gStr_childIsFemale) == 0)		contains_childIsFemale_ = true;
		if (token_string.compare(gStr_parent1Genome1) == 0)		contains_parent1Genome1_ = true;
		if (token_string.compare(gStr_parent1Genome2) == 0)		contains_parent1Genome2_ = true;
		if (token_string.compare(gStr_isSelfing) == 0)			contains_isSelfing_ = true;
		if (token_string.compare(gStr_parent2Genome1) == 0)		contains_parent2Genome1_ = true;
		if (token_string.compare(gStr_parent2Genome2) == 0)		contains_parent2Genome2_ = true;
	}
}

void SLiMScriptBlock::ScanTree(void)
{
	_ScanNodeForIdentifiersUsed(compound_statement_node_);
	
	// If the script block contains a "wildcard" – an identifier that signifies that any other identifier could be accessed – then
	// we just set all of our "contains_" flags to T.  Any new flag that is added must be added here too!
	if (contains_wildcard_)
	{
		contains_T_ = true;
		contains_F_ = true;
		contains_NULL_ = true;
		contains_PI_ = true;
		contains_E_ = true;
		contains_INF_ = true;
		contains_NAN_ = true;
		contains_pX_ = true;
		contains_gX_ = true;
		contains_mX_ = true;
		contains_sX_ = true;
		contains_sim_ = true;
		contains_self_ = true;
		contains_mut_ = true;
		contains_relFitness_ = true;
		contains_genome1_ = true;
		contains_genome2_ = true;
		contains_subpop_ = true;
		contains_homozygous_ = true;
		contains_sourceSubpop_ = true;
		contains_weights_ = true;
		contains_childGenome1_ = true;
		contains_childGenome2_ = true;
		contains_childIsFemale_ = true;
		contains_parent1Genome1_ = true;
		contains_parent1Genome2_ = true;
		contains_isSelfing_ = true;
		contains_parent2Genome1_ = true;
		contains_parent2Genome2_ = true;
	}
}


//
// SLiMscript support
//

void SLiMScriptBlock::GenerateCachedSymbolTableEntry(void)
{
	self_symbol_ = new SymbolTableEntry(gStr_self, (new ScriptValue_Object_singleton_const(this))->SetExternallyOwned());
}

void SLiMScriptBlock::GenerateCachedScriptBlockSymbolTableEntry(void)
{
	if (block_id_ == -1)
		SLIM_TERMINATION << "ERROR (SLiMScriptBlock::GenerateCachedSymbolTableEntry): internal error: cached symbol table entries for anonymous script blocks are not supported." << slim_terminate();
	
	std::ostringstream script_stream;
	
	script_stream << "s" << block_id_;
	
	script_block_symbol_ = new SymbolTableEntry(script_stream.str(), (new ScriptValue_Object_singleton_const(this))->SetExternallyOwned());
}

const std::string SLiMScriptBlock::ElementType(void) const
{
	return gStr_SLiMScriptBlock;
}

void SLiMScriptBlock::Print(std::ostream &p_ostream) const
{
	p_ostream << ElementType() << "<" << start_generation_;
	
	if (end_generation_ != start_generation_)
		p_ostream << ":" << end_generation_;
	
	switch (type_)
	{
		case SLiMScriptBlockType::SLiMScriptEvent:					break;
		case SLiMScriptBlockType::SLiMScriptFitnessCallback:		p_ostream << " : fitness"; break;
		case SLiMScriptBlockType::SLiMScriptMateChoiceCallback:		p_ostream << " : mateChoice"; break;
		case SLiMScriptBlockType::SLiMScriptModifyChildCallback:	p_ostream << " : modifyChild"; break;
	}
	
	p_ostream << ">";
}

std::vector<std::string> SLiMScriptBlock::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_id);			// block_id_
	constants.push_back(gStr_start);		// start_generation_
	constants.push_back(gStr_end);			// end_generation_
	constants.push_back(gStr_type);		// type_
	constants.push_back(gStr_source);		// source_
	
	return constants;
}

std::vector<std::string> SLiMScriptBlock::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back(gStr_active);		// active_
	
	return variables;
}

bool SLiMScriptBlock::MemberIsReadOnly(GlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_id:
		case gID_start:
		case gID_end:
		case gID_type:
		case gID_source:
			return true;
			
			// variables
		case gID_active:
			return false;
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::MemberIsReadOnly(p_member_id);
	}
}

ScriptValue *SLiMScriptBlock::GetValueForMember(GlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
			// constants
		case gID_id:
		{
			if (!cached_value_block_id_)
				cached_value_block_id_ = (new ScriptValue_Int_singleton_const(block_id_))->SetExternallyOwned();
			return cached_value_block_id_;
		}
		case gID_start:
			return new ScriptValue_Int_singleton_const(start_generation_);
		case gID_end:
			return new ScriptValue_Int_singleton_const(end_generation_);
		case gID_type:
		{
			switch (type_)
			{
				case SLiMScriptBlockType::SLiMScriptEvent:					return new ScriptValue_String(gStr_event);
				case SLiMScriptBlockType::SLiMScriptFitnessCallback:		return new ScriptValue_String(gStr_fitness);
				case SLiMScriptBlockType::SLiMScriptMateChoiceCallback:		return new ScriptValue_String(gStr_mateChoice);
				case SLiMScriptBlockType::SLiMScriptModifyChildCallback:	return new ScriptValue_String(gStr_modifyChild);
			}
		}
		case gID_source:
			return new ScriptValue_String(compound_statement_node_->token_->token_string_);
			
			// variables
		case gID_active:
			return new ScriptValue_Int_singleton_const(active_);
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::GetValueForMember(p_member_id);
	}
}

void SLiMScriptBlock::SetValueForMember(GlobalStringID p_member_id, ScriptValue *p_value)
{
	if (p_member_id == gID_active)
	{
		TypeCheckValue(__func__, p_member_id, p_value, kScriptValueMaskInt);
		
		active_ = p_value->IntAtIndex(0);
		
		return;
	}
	
	// all others, including gID_none
	else
		return ScriptObjectElement::SetValueForMember(p_member_id, p_value);
}

std::vector<std::string> SLiMScriptBlock::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	return methods;
}

const FunctionSignature *SLiMScriptBlock::SignatureForMethod(GlobalStringID p_method_id) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_id);
}

ScriptValue *SLiMScriptBlock::ExecuteMethod(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}





























































