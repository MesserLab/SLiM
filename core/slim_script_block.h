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
	
private:

	SymbolTableEntry *self_symbol_ = nullptr;					// OWNED POINTER: SymbolTableEntry object for fast setup of the symbol table
	SymbolTableEntry *script_block_symbol_ = nullptr;			// OWNED POINTER: SymbolTableEntry object for fast setup of the symbol table
	
public:
	
	SLiMScriptBlockType type_ = SLiMScriptBlockType::SLiMScriptEvent;
	
	int block_id_ = -1;											// the id of the block; -1 if no id was assigned (anonymous block)
	ScriptValue *cached_value_block_id_ = nullptr;				// OWNED POINTER: a cached value for block_id_; delete and nil if that changes
	
	int start_generation_ = 1, end_generation_ = INT_MAX;		// the generation range to which the block is limited
	int mutation_type_id_ = -1;									// -1 if not limited by this
	int subpopulation_id_ = -1;									// -1 if not limited by this
	
	Script *script_ = nullptr;									// OWNED: nullptr indicates that we are derived from the input file script
	const ScriptASTNode *root_node_ = nullptr;					// NOT OWNED: the root node for the whole block, including its generation range and type nodes
	const ScriptASTNode *compound_statement_node_ = nullptr;	// NOT OWNED: the node for the compound statement that constitutes the body of the block
	
	int64_t active_ = -1;										// the "active" property of the block: 0 if inactive, all other values are active
	
	// Flags indicating what identifiers this script block uses; identifiers that are not used do not need to be added.
	bool contains_wildcard_ = false;			// "executeLambda", "globals"
	bool contains_T_ = false;					// "T"
	bool contains_F_ = false;					// "F"
	bool contains_NULL_ = false;				// "NULL"
	bool contains_PI_ = false;					// "PI"
	bool contains_E_ = false;					// "E"
	bool contains_INF_ = false;					// "INF"
	bool contains_NAN_ = false;					// "NAN"
	bool contains_pX_ = false;					// any subpop identifier like p1, p2...
	bool contains_gX_ = false;					// any genomic element type identifier like g1, g2...
	bool contains_mX_ = false;					// any mutation type identifier like m1, m2...
	bool contains_sX_ = false;					// any script identifier like s1, s2...
	bool contains_sim_ = false;					// "sim"
	bool contains_self_ = false;				// "self"
	bool contains_mut_ = false;					// "mut" (fitness callback parameter)
	bool contains_relFitness_ = false;			// "relFitness" (fitness callback parameter)
	bool contains_genome1_ = false;				// "genome1" (fitness/mateChoice callback parameter)
	bool contains_genome2_ = false;				// "genome2" (fitness/mateChoice callback parameter)
	bool contains_subpop_ = false;				// "subpop" (fitness/mateChoice/modifyChild callback parameter)
	bool contains_homozygous_ = false;			// "homozygous" (fitness callback parameter)
	bool contains_sourceSubpop_ = false;		// "sourceSubpop" (mateChoice/modifyChild callback parameter)
	bool contains_weights_ = false;				// "weights" (mateChoice callback parameter)
	bool contains_childGenome1_ = false;		// "childGenome1" (modifyChild callback parameter)
	bool contains_childGenome2_ = false;		// "childGenome2" (modifyChild callback parameter)
	bool contains_childIsFemale_ = false;		// "childIsFemale" (modifyChild callback parameter)
	bool contains_parent1Genome1_ = false;		// "parent1Genome1" (modifyChild callback parameter)
	bool contains_parent1Genome2_ = false;		// "parent1Genome2" (modifyChild callback parameter)
	bool contains_isSelfing_ = false;			// "isSelfing" (modifyChild callback parameter)
	bool contains_parent2Genome1_ = false;		// "parent2Genome1" (modifyChild callback parameter)
	bool contains_parent2Genome2_ = false;		// "parent2Genome2" (modifyChild callback parameter)
	
	SLiMScriptBlock(const SLiMScriptBlock&) = delete;					// no copying
	SLiMScriptBlock& operator=(const SLiMScriptBlock&) = delete;		// no copying
	SLiMScriptBlock(void) = delete;										// no default constructor
	
	SLiMScriptBlock(ScriptASTNode *p_root_node);						// initialize from a SLiMScriptBlock root node from the input file
	SLiMScriptBlock(int p_id, std::string p_script_string, SLiMScriptBlockType p_type, int p_start, int p_end);		// initialize from a programmatic script
	~SLiMScriptBlock(void);												// destructor
	
	// Scan the tree for optimization purposes, called by the constructors
	void _ScanNodeForIdentifiers(const ScriptASTNode *p_scan_node);
	void _ScanNodeForConstants(const ScriptASTNode *p_scan_node);
	void ScanTree(void);
	
	//
	// SLiMscript support
	//
	void GenerateCachedSymbolTableEntry(void);
	inline SymbolTableEntry *CachedSymbolTableEntry(void) { if (!self_symbol_) GenerateCachedSymbolTableEntry(); return self_symbol_; };
	void GenerateCachedScriptBlockSymbolTableEntry(void);
	inline SymbolTableEntry *CachedScriptBlockSymbolTableEntry(void)
		{ if (!script_block_symbol_) GenerateCachedScriptBlockSymbolTableEntry(); return script_block_symbol_; };
	
	virtual std::string ElementType(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name);
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const FunctionSignature *SignatureForMethod(std::string const &p_method_name) const;
	virtual ScriptValue *ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_script_block__) */







































































