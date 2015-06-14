//
//  slim_script_block.h
//  SLiM
//
//  Created by Ben Haller on 6/7/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
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

/*
 
 The class SLiMScriptBlock represents one script block defined in SLiM's input file, or programmatically via the methods on SLiMSim.
 A SLiMScriptBlock knows the generation range in which it is to run, has a reference to its AST so it can be executed, and various
 other state.
 
 */

#ifndef __SLiM__slim_script_block__
#define __SLiM__slim_script_block__

#include "script.h"
#include "script_value.h"
#include "script_functions.h"


enum class SLiMScriptBlockType {
	SLiMScriptEvent = 0,
	SLiMScriptFitnessCallback,
	SLiMScriptMateChoiceCallback,
	SLiMScriptModifyChildCallback
};


class SLiMScriptBlock : public ScriptObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
public:
	
	SLiMScriptBlockType type_ = SLiMScriptBlockType::SLiMScriptEvent;
	
	int block_id_ = -1;											// the id of the block; -1 if no id was assigned (anonymous block)
	int start_generation_ = 1, end_generation_ = INT_MAX;		// the generation range to which the block is limited
	int mutation_type_id_ = -1;									// -1 if not limited by this
	int subpopulation_id_ = -1;									// -1 if not limited by this
	
	Script *script_ = nullptr;									// OWNED: nullptr indicates that we are derived from the input file script
	const ScriptASTNode *root_node_ = nullptr;					// NOT OWNED: the root node for the whole block, including its generation range and type nodes
	const ScriptASTNode *compound_statement_node_ = nullptr;	// NOT OWNED: the node for the compound statement that constitutes the body of the block
	
	int64_t active_ = -1;										// the "active" property of the block: 0 if inactive, all other values are active
	
	SLiMScriptBlock(const SLiMScriptBlock&) = delete;					// no copying
	SLiMScriptBlock& operator=(const SLiMScriptBlock&) = delete;		// no copying
	SLiMScriptBlock(void) = delete;										// no default constructor
	
	SLiMScriptBlock(ScriptASTNode *p_root_node);						// initialize from a SLiMScriptBlock root node from the input file
	SLiMScriptBlock(int p_id, std::string p_script_string, SLiMScriptBlockType p_type, int p_start, int p_end);		// initialize from a programmatic script
	~SLiMScriptBlock(void);												// destructor
	
	//
	// SLiMscript support
	//
	virtual std::string ElementType(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name);
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const FunctionSignature *SignatureForMethod(std::string const &p_method_name) const;
	virtual ScriptValue *ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_script_block__) */







































































