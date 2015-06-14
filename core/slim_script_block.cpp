//
//  slim_script_block.cpp
//  SLiM
//
//  Created by Ben Haller on 6/7/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#include "slim_script_block.h"

#include "errno.h"


using std::endl;
using std::string;


SLiMScriptBlock::SLiMScriptBlock(ScriptASTNode *p_root_node) : root_node_(p_root_node)
{
	const std::vector<ScriptASTNode *> &block_children = root_node_->children_;
	int child_index = 0, n_children = (int)block_children.size();
	
	// eat a string, for the script id, if present
	block_id_ = -1;	// the default unless it is set below
	
	if ((child_index < n_children) && (block_children[child_index]->token_->token_type_ == TokenType::kTokenString))
	{
		string id_string = block_children[child_index]->token_->token_string_;
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
		long long_block_id = strtol(id_cstr + 1, NULL, 10);
		
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
		const std::vector<ScriptASTNode *> &callback_children = callback_node->children_;
		int n_callback_children = (int)callback_children.size();
		
		if (callback_type == TokenType::kTokenFitness)
		{
			if ((n_callback_children != 1) && (n_callback_children != 2))
				SLIM_TERMINATION << "ERROR (InitializeFromFile): fitness() callback needs 1 or 2 parameters" << slim_terminate();
			
			mutation_type_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[0]->token_);
			
			if (n_callback_children == 2)
				subpopulation_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[1]->token_);
		}
		else if (callback_type == TokenType::kTokenMateChoice)
		{
			if ((n_callback_children != 0) && (n_callback_children != 1))
				SLIM_TERMINATION << "ERROR (InitializeFromFile): mateChoice() callback needs 0 or 1 parameters" << slim_terminate();
			
			if (n_callback_children == 1)
				subpopulation_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[0]->token_);
		}
		else if (callback_type == TokenType::kTokenModifyChild)
		{
			if ((n_callback_children != 0) && (n_callback_children != 1))
				SLIM_TERMINATION << "ERROR (InitializeFromFile): modifyChild() callback needs 0 or 1 parameters" << slim_terminate();
			
			if (n_callback_children == 1)
				subpopulation_id_ = (int)ScriptInterpreter::IntForNumberToken(callback_children[0]->token_);
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
}

SLiMScriptBlock::SLiMScriptBlock(int p_id, std::string p_script_string, SLiMScriptBlockType p_type, int p_start, int p_end)
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
}

SLiMScriptBlock::~SLiMScriptBlock(void)
{
	delete script_;
}


//
// SLiMscript support
//
std::string SLiMScriptBlock::ElementType(void) const
{
	return "SLiMScriptBlock";
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
	
	constants.push_back("id");			// block_id_
	constants.push_back("start");		// start_generation_
	constants.push_back("end");			// end_generation_
	constants.push_back("type");		// type_
	constants.push_back("source");		// source_
	
	return constants;
}

std::vector<std::string> SLiMScriptBlock::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back("active");		// active_
	
	return variables;
}

ScriptValue *SLiMScriptBlock::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("id") == 0)
		return new ScriptValue_Int(block_id_);
	if (p_member_name.compare("start") == 0)
		return new ScriptValue_Int(start_generation_);
	if (p_member_name.compare("end") == 0)
		return new ScriptValue_Int(end_generation_);
	if (p_member_name.compare("type") == 0)
	{
		switch (type_)
		{
			case SLiMScriptBlockType::SLiMScriptEvent:					return new ScriptValue_String("event");
			case SLiMScriptBlockType::SLiMScriptFitnessCallback:		return new ScriptValue_String("fitness");
			case SLiMScriptBlockType::SLiMScriptMateChoiceCallback:		return new ScriptValue_String("mateChoice");
			case SLiMScriptBlockType::SLiMScriptModifyChildCallback:	return new ScriptValue_String("modifyChild");
		}
	}
	if (p_member_name.compare("source") == 0)
		return new ScriptValue_String(compound_statement_node_->token_->token_string_);
	
	// variables
	if (p_member_name.compare("active") == 0)
		return new ScriptValue_Int(active_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void SLiMScriptBlock::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	if (p_member_name.compare("active") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		active_ = p_value->IntAtIndex(0);
		
		return;
	}
	
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> SLiMScriptBlock::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	return methods;
}

const FunctionSignature *SLiMScriptBlock::SignatureForMethod(std::string const &p_method_name) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *SLiMScriptBlock::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}





























































