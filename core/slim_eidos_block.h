//
//  slim_script_block.h
//  SLiM
//
//  Created by Ben Haller on 6/7/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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

/*
 
 The class SLiMEidosBlock represents one script block defined in SLiM's input file, or programmatically via the methods on SLiMSim.
 A SLiMEidosBlock knows the generation range in which it is to run, has a reference to its AST so it can be executed, and various
 other state.
 
 */

#ifndef __SLiM__slim_script_block__
#define __SLiM__slim_script_block__

#include "slim_global.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_functions.h"
#include "eidos_type_table.h"
#include "eidos_type_interpreter.h"


enum class SLiMEidosBlockType {
	SLiMEidosEventEarly = 0,
	SLiMEidosEventLate,
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
	SLiMEidosScript(const std::string &p_script_string);
	
	virtual ~SLiMEidosScript(void);												// destructor
	
	void ParseSLiMFileToAST(bool p_make_bad_nodes = false);						// generate AST from token stream for a SLiM input file ( slim_script_block* EOF )
	
	// Top-level parse methods for SLiM input files
	EidosASTNode *Parse_SLiMFile(void);
	EidosASTNode *Parse_SLiMEidosBlock(void);
	
	// A utility method for extracting the numeric component of an identifier like 'p2', 's3', 'm17', or 'g5'
	// This raises if the expected character prefix is not present, or if anything but numeric digits are present, or if the ID is out of range
	// The token-based API uses the token for error tracking if an exception is raised; the string-based API just calls eidos_terminate(), and
	// thus inherits whatever error-tracking token information might have been previously set.
	static bool StringIsIDWithPrefix(const std::string &p_identifier_string, char p_prefix_char);
	static slim_objectid_t ExtractIDFromStringWithPrefix(const std::string &p_identifier_string, char p_prefix_char, const EidosToken *p_blame_token);
	
	// Returns a string of the form "p1", "g7", "m17", etc., from the type character and object identifier
	static inline std::string IDStringWithPrefix(char p_type_char, slim_objectid_t p_object_id)
	{
		std::ostringstream idstring_stream;
		
		idstring_stream << p_type_char << p_object_id;
		
		return idstring_stream.str();
	}
};


extern EidosObjectClass *gSLiM_SLiMEidosBlock_Class;


class SLiMEidosBlock : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:

	EidosSymbolTableEntry self_symbol_;					// "self" : for fast setup of the symbol table
	EidosSymbolTableEntry script_block_symbol_;			// "sX" : for fast setup of the symbol table
	
public:
	
	SLiMEidosBlockType type_ = SLiMEidosBlockType::SLiMEidosEventEarly;
	
	slim_objectid_t block_id_ = -1;								// the id of the block; -1 if no id was assigned (anonymous block)
	EidosValue_SP cached_value_block_id_;						// a cached value for block_id_; reset() if that changes
	
	slim_generation_t start_generation_ = -1, end_generation_ = SLIM_MAX_GENERATION;		// the generation range to which the block is limited
	slim_objectid_t mutation_type_id_ = -1;						// -1 if not limited by this
	slim_objectid_t subpopulation_id_ = -1;						// -1 if not limited by this
	
	EidosScript *script_ = nullptr;								// OWNED: nullptr indicates that we are derived from the input file script
	const EidosASTNode *root_node_ = nullptr;					// NOT OWNED: the root node for the whole block, including its generation range and type nodes
	const EidosASTNode *compound_statement_node_ = nullptr;		// NOT OWNED: the node for the compound statement that constitutes the body of the block
	const EidosToken *identifier_token_ = nullptr;
	
	slim_usertag_t active_ = -1;								// the "active" property of the block: 0 if inactive, all other values are active
	slim_usertag_t tag_value_;									// a user-defined tag value
	
	// Flags indicating what identifiers this script block uses; identifiers that are not used do not need to be added.
	bool contains_wildcard_ = false;			// "apply", "executeLambda", "ls", "rm"; all other contains_ flags will be T if this is T
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
	SLiMEidosBlock(slim_objectid_t p_id, const std::string &p_script_string, SLiMEidosBlockType p_type, slim_generation_t p_start, slim_generation_t p_end);		// initialize from a programmatic script
	~SLiMEidosBlock(void);												// destructor
	
	// Tokenize and parse the script.  This should be called immediately after construction.  Raises on script errors.
	void TokenizeAndParse(void);
	
	// Scan the tree for optimization purposes, called by the constructors
	void _ScanNodeForIdentifiersUsed(const EidosASTNode *p_scan_node);
	void ScanTreeForIdentifiersUsed(void);
	
	//
	// Eidos support
	//
	EidosSymbolTableEntry &SelfSymbolTableEntry(void) { return self_symbol_; };
	EidosSymbolTableEntry &ScriptBlockSymbolTableEntry(void) { if (block_id_ != -1) return script_block_symbol_; else EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::ScriptBlockSymbolTableEntry): (internal error) no symbol table entry." << eidos_terminate(); };
	
	virtual const EidosObjectClass *Class(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


class SLiMTypeTable : public EidosTypeTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
public:
	
	SLiMTypeTable(const SLiMTypeTable&) = delete;										// no copying
	SLiMTypeTable& operator=(const SLiMTypeTable&) = delete;							// no copying
	explicit SLiMTypeTable(void);														// standard constructor
	virtual ~SLiMTypeTable(void);														// destructor
	
	// Test for containing a value for a symbol
	virtual bool ContainsSymbol(EidosGlobalStringID p_symbol_name) const;
	
	// Get the type for a symbol
	virtual EidosTypeSpecifier GetTypeForSymbol(EidosGlobalStringID p_symbol_name) const;
};


class SLiMTypeInterpreter : public EidosTypeInterpreter
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
public:
	
	SLiMTypeInterpreter(const SLiMTypeInterpreter&) = delete;					// no copying
	SLiMTypeInterpreter& operator=(const SLiMTypeInterpreter&) = delete;		// no copying
	SLiMTypeInterpreter(void) = delete;											// no null construction
	
	SLiMTypeInterpreter(const EidosScript &p_script, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, bool p_defines_only = false);			// we use the passed symbol table but do not own it
	SLiMTypeInterpreter(const EidosASTNode *p_root_node_, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, bool p_defines_only = false);		// we use the passed symbol table but do not own it
	
	virtual ~SLiMTypeInterpreter(void);													// destructor
	
	void _SetTypeForISArgumentOfClass(const EidosASTNode *p_arg_node, char p_symbol_prefix, const EidosObjectClass *p_type_class);
	
	virtual EidosTypeSpecifier _TypeEvaluate_FunctionCall_Internal(std::string const &p_function_name, const EidosFunctionSignature *p_function_signature, const EidosASTNode **const p_arguments, int p_argument_count);
	
	virtual EidosTypeSpecifier _TypeEvaluate_MethodCall_Internal(const EidosObjectClass *p_target, const EidosMethodSignature *p_method_signature, const EidosASTNode **const p_arguments, int p_argument_count);
};


#endif /* defined(__SLiM__slim_script_block__) */







































































