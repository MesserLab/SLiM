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
 
 The class SLiMEidosBlock represents one script block defined in SLiM's input file, or programmatically via the methods on SLiMSim.
 A SLiMEidosBlock knows the generation range in which it is to run, has a reference to its AST so it can be executed, and various
 other state.
 
 */

#ifndef __SLiM__slim_script_block__
#define __SLiM__slim_script_block__

#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_functions.h"


enum class SLiMEidosBlockType {
	SLiMEidosEvent = 0,
	SLiMEidosInitializeCallback,
	SLiMEidosFitnessCallback,
	SLiMEidosMateChoiceCallback,
	SLiMEidosModifyChildCallback
};


class SLiMEidosScript : public EidosScript
{
public:
	SLiMEidosScript(const SLiMEidosScript&) = delete;							// no copying
	SLiMEidosScript& operator=(const SLiMEidosScript&) = delete;				// no copying
	SLiMEidosScript(void) = delete;												// no null construction
	SLiMEidosScript(const std::string &p_script_string, int p_start_index);
	
	virtual ~SLiMEidosScript(void);												// destructor
	
	void ParseSLiMFileToAST(void);									// generate AST from token stream for a SLiM input file ( slim_script_block* EOF )
	
	// Top-level parse methods for SLiM input files
	EidosASTNode *Parse_SLiMFile(void);
	EidosASTNode *Parse_SLiMEidosBlock(void);
	
	// A utility method for extracting the numeric component of an identifier like 'p2', 's3', 'm17', or 'g5'
	// This raises if the expected character prefix is not present, or if anything but numeric digits are present, or if the ID is out of range
	static bool StringIsIDWithPrefix(const std::string &p_identifier_string, char p_prefix_char);
	static int ExtractIDFromStringWithPrefix(const std::string &p_identifier_string, char p_prefix_char);
};


class SLiMEidosBlock : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:

	EidosSymbolTableEntry *self_symbol_ = nullptr;					// OWNED POINTER: EidosSymbolTableEntry object for fast setup of the symbol table
	EidosSymbolTableEntry *script_block_symbol_ = nullptr;			// OWNED POINTER: EidosSymbolTableEntry object for fast setup of the symbol table
	
public:
	
	SLiMEidosBlockType type_ = SLiMEidosBlockType::SLiMEidosEvent;
	
	int block_id_ = -1;											// the id of the block; -1 if no id was assigned (anonymous block)
	EidosValue *cached_value_block_id_ = nullptr;				// OWNED POINTER: a cached value for block_id_; delete and nil if that changes
	
	int start_generation_ = -1, end_generation_ = INT_MAX;		// the generation range to which the block is limited
	int mutation_type_id_ = -1;									// -1 if not limited by this
	int subpopulation_id_ = -1;									// -1 if not limited by this
	
	EidosScript *script_ = nullptr;								// OWNED: nullptr indicates that we are derived from the input file script
	const EidosASTNode *root_node_ = nullptr;					// NOT OWNED: the root node for the whole block, including its generation range and type nodes
	const EidosASTNode *compound_statement_node_ = nullptr;	// NOT OWNED: the node for the compound statement that constitutes the body of the block
	
	int64_t active_ = -1;										// the "active" property of the block: 0 if inactive, all other values are active
	
	int64_t tag_value_;											// a user-defined tag value
	
	// Flags indicating what identifiers this script block uses; identifiers that are not used do not need to be added.
	bool contains_wildcard_ = false;			// "executeLambda", "globals"; all other contains_ flags will be T if this is T
	EidosSymbolUsageParamBlock eidos_contains_;	// flags passed to Eidos regarding symbol usage
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
	bool contains_isCloning_ = false;			// "isCloning" (modifyChild callback parameter)
	bool contains_isSelfing_ = false;			// "isSelfing" (modifyChild callback parameter)
	bool contains_parent2Genome1_ = false;		// "parent2Genome1" (modifyChild callback parameter)
	bool contains_parent2Genome2_ = false;		// "parent2Genome2" (modifyChild callback parameter)
	
	SLiMEidosBlock(const SLiMEidosBlock&) = delete;					// no copying
	SLiMEidosBlock& operator=(const SLiMEidosBlock&) = delete;		// no copying
	SLiMEidosBlock(void) = delete;										// no default constructor
	
	SLiMEidosBlock(EidosASTNode *p_root_node);						// initialize from a SLiMEidosBlock root node from the input file
	SLiMEidosBlock(int p_id, const std::string &p_script_string, SLiMEidosBlockType p_type, int p_start, int p_end);		// initialize from a programmatic script
	~SLiMEidosBlock(void);												// destructor
	
	// Scan the tree for optimization purposes, called by the constructors
	void _ScanNodeForIdentifiersUsed(const EidosASTNode *p_scan_node);
	void ScanTree(void);
	
	//
	// Eidos support
	//
	void GenerateCachedSymbolTableEntry(void);
	inline EidosSymbolTableEntry *CachedSymbolTableEntry(void) { if (!self_symbol_) GenerateCachedSymbolTableEntry(); return self_symbol_; };
	void GenerateCachedScriptBlockSymbolTableEntry(void);
	inline EidosSymbolTableEntry *CachedScriptBlockSymbolTableEntry(void)
		{ if (!script_block_symbol_) GenerateCachedScriptBlockSymbolTableEntry(); return script_block_symbol_; };
	
	virtual const std::string *ElementType(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual bool MemberIsReadOnly(EidosGlobalStringID p_member_id) const;
	virtual EidosValue *GetValueForMember(EidosGlobalStringID p_member_id);
	virtual void SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue *ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_script_block__) */







































































