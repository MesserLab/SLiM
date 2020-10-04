//
//  slim_script_block.h
//  SLiM
//
//  Created by Ben Haller on 6/7/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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

#include "slim_globals.h"
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
	SLiMEidosFitnessGlobalCallback,
	SLiMEidosInteractionCallback,
	SLiMEidosMateChoiceCallback,
	SLiMEidosModifyChildCallback,
	SLiMEidosRecombinationCallback,
	SLiMEidosMutationCallback,
	SLiMEidosReproductionCallback,
	
	SLiMEidosUserDefinedFunction,
	
	SLiMEidosNoBlockType = -1		// not used as a type, only used to indicate "no type"
};

std::ostream& operator<<(std::ostream& p_out, SLiMEidosBlockType p_block_type);


#pragma mark -
#pragma mark SLiMEidosScript
#pragma mark -

class SLiMEidosScript : public EidosScript
{
public:
	SLiMEidosScript(const SLiMEidosScript&) = delete;							// no copying
	SLiMEidosScript& operator=(const SLiMEidosScript&) = delete;				// no copying
	SLiMEidosScript(void) = delete;												// no null construction
	explicit SLiMEidosScript(const std::string &p_script_string);
	
	virtual ~SLiMEidosScript(void);
	
	void ParseSLiMFileToAST(bool p_make_bad_nodes = false);						// generate AST from token stream for a SLiM input file ( slim_script_block* EOF )
	
	// Top-level parse methods for SLiM input files
	EidosASTNode *Parse_SLiMFile(void);
	EidosASTNode *Parse_SLiMEidosBlock(void);
	
	// A utility method for extracting the numeric component of an identifier like 'p2', 's3', 'm17', or 'g5'
	// This raises if the expected character prefix is not present, or if anything but numeric digits are present, or if the ID is out of range
	// The token-based API uses the token for error tracking if an exception is raised; the string-based API just calls EidosTerminate(), and
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


#pragma mark -
#pragma mark SLiMEidosBlock
#pragma mark -

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
	
	slim_generation_t start_generation_ = -1, end_generation_ = SLIM_MAX_GENERATION + 1;		// the generation range to which the block is limited
	slim_objectid_t mutation_type_id_ = -1;						// -1 if not limited by this; -2 indicates a NULL mutation-type id
	slim_objectid_t subpopulation_id_ = -1;						// -1 if not limited by this
	slim_objectid_t interaction_type_id_ = -1;					// -1 if not limited by this
	IndividualSex sex_specificity_ = IndividualSex::kUnspecified;	// IndividualSex::kUnspecified if not limited by this
	
	EidosScript *script_ = nullptr;								// OWNED: nullptr indicates that we are derived from the input file script
	const EidosASTNode *root_node_ = nullptr;					// NOT OWNED: the root node for the whole block, including its generation range and type nodes
	const EidosASTNode *compound_statement_node_ = nullptr;		// NOT OWNED: the node for the compound statement that constitutes the body of the block
	const EidosToken *identifier_token_ = nullptr;
	
	slim_usertag_t active_ = -1;								// the "active" property of the block: 0 if inactive, all other values are active
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;			// a user-defined tag value
	
	// Flags indicating what identifiers this script block uses; identifiers that are not used do not need to be added.
	bool contains_wildcard_ = false;			// "apply", "sapply", "executeLambda", "_executeLambda_OUTER", "ls", "rm"; all other contains_ flags will be T if this is T
	bool contains_self_ = false;				// "self"
	bool contains_mut_ = false;					// "mut" (fitness/mutation callback parameter)
	bool contains_relFitness_ = false;			// "relFitness" (fitness callback parameter)
	bool contains_individual_ = false;			// "individual" (fitness/mateChoice/recombination/reproduction callback parameter)
	bool contains_element_ = false;				// "element" (mutation callback parameter)
	bool contains_genome_ = false;				// "genome" (mutation callback parameter)
	bool contains_genome1_ = false;				// "genome1" (fitness/mateChoice/recombination/reproduction callback parameter)
	bool contains_genome2_ = false;				// "genome2" (fitness/mateChoice/recombination/reproduction callback parameter)
	bool contains_subpop_ = false;				// "subpop" (fitness/interaction/mateChoice/modifyChild/recombination/reproduction/mutation callback parameter)
	bool contains_homozygous_ = false;			// "homozygous" (fitness callback parameter)
	bool contains_sourceSubpop_ = false;		// "sourceSubpop" (mateChoice/modifyChild callback parameter)
	bool contains_weights_ = false;				// "weights" (mateChoice callback parameter)
	bool contains_child_ = false;				// "child" (modifyChild callback parameter)
	bool contains_childGenome1_ = false;		// "childGenome1" (modifyChild callback parameter)
	bool contains_childGenome2_ = false;		// "childGenome2" (modifyChild callback parameter)
	bool contains_childIsFemale_ = false;		// "childIsFemale" (modifyChild callback parameter)
	bool contains_parent_ = false;				// "parent" (mutation callback parameter)
	bool contains_parent1_ = false;				// "parent1" (modifyChild callback parameter)
	bool contains_parent1Genome1_ = false;		// "parent1Genome1" (modifyChild callback parameter)
	bool contains_parent1Genome2_ = false;		// "parent1Genome2" (modifyChild callback parameter)
	bool contains_isCloning_ = false;			// "isCloning" (modifyChild callback parameter)
	bool contains_isSelfing_ = false;			// "isSelfing" (modifyChild callback parameter)
	bool contains_parent2_ = false;				// "parent2" (modifyChild callback parameter)
	bool contains_parent2Genome1_ = false;		// "parent2Genome1" (modifyChild callback parameter)
	bool contains_parent2Genome2_ = false;		// "parent2Genome2" (modifyChild callback parameter)
	bool contains_breakpoints_ = false;			// "breakpoints" (recombination callback parameter)
	bool contains_distance_ = false;			// "distance" (interaction callback parameter)
	bool contains_strength_ = false;			// "strength" (interaction callback parameter)
	bool contains_receiver_ = false;			// "receiver" (interaction callback parameter)
	bool contains_exerter_ = false;				// "exerter" (interaction callback parameter)
	bool contains_originalNuc_ = false;			// "originalNuc" (mutation callback parameter)
	
	// Special-case optimizations for particular common callback types.  If a callback can be substituted by C++ code,
	// has_cached_optimization_ will be true and the flags and values below will indicate exactly how to do so.
	bool has_cached_optimization_ = false;
	bool has_cached_opt_dnorm1_ = false;
	bool has_cached_opt_reciprocal = false;
	double cached_opt_A_ = 0.0;
	double cached_opt_B_ = 0.0;
	double cached_opt_C_ = 0.0;
	double cached_opt_D_ = 0.0;
	
	
	SLiMEidosBlock(const SLiMEidosBlock&) = delete;					// no copying
	SLiMEidosBlock& operator=(const SLiMEidosBlock&) = delete;		// no copying
	SLiMEidosBlock(void) = delete;									// no default constructor
	
	explicit SLiMEidosBlock(EidosASTNode *p_root_node);				// initialize from a SLiMEidosBlock root node from the input file
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
	inline EidosSymbolTableEntry &SelfSymbolTableEntry(void) { return self_symbol_; };
	EidosSymbolTableEntry &ScriptBlockSymbolTableEntry(void) { if (block_id_ != -1) return script_block_symbol_; else EIDOS_TERMINATION << "ERROR (SLiMEidosBlock::ScriptBlockSymbolTableEntry): (internal error) no symbol table entry." << EidosTerminate(); };
	
	virtual const EidosObjectClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
};


#pragma mark -
#pragma mark SLiMTypeTable
#pragma mark -

class SLiMTypeTable : public EidosTypeTable
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
public:
	
	SLiMTypeTable(const SLiMTypeTable&) = delete;										// no copying
	SLiMTypeTable& operator=(const SLiMTypeTable&) = delete;							// no copying
	explicit SLiMTypeTable(void);														// standard constructor
	virtual ~SLiMTypeTable(void) override;
	
	// Test for containing a value for a symbol
	virtual bool ContainsSymbol(EidosGlobalStringID p_symbol_name) const override;
	
	// Get the type for a symbol
	virtual EidosTypeSpecifier GetTypeForSymbol(EidosGlobalStringID p_symbol_name) const override;
};


#pragma mark -
#pragma mark SLiMTypeInterpreter
#pragma mark -

class SLiMTypeInterpreter : public EidosTypeInterpreter
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
public:
	
	SLiMTypeInterpreter(const SLiMTypeInterpreter&) = delete;					// no copying
	SLiMTypeInterpreter& operator=(const SLiMTypeInterpreter&) = delete;		// no copying
	SLiMTypeInterpreter(void) = delete;											// no null construction
	
	SLiMTypeInterpreter(const EidosScript &p_script, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only = false);			// we use the passed symbol table but do not own it
	SLiMTypeInterpreter(const EidosASTNode *p_root_node_, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only = false);		// we use the passed symbol table but do not own it
	
	virtual ~SLiMTypeInterpreter(void) override;
	
	void _SetTypeForISArgumentOfClass(const EidosASTNode *p_arg_node, char p_symbol_prefix, const EidosObjectClass *p_type_class);
	
	virtual EidosTypeSpecifier _TypeEvaluate_FunctionCall_Internal(std::string const &p_function_name, const EidosFunctionSignature *p_function_signature, const std::vector<EidosASTNode *> &p_arguments) override;
	
	virtual EidosTypeSpecifier _TypeEvaluate_MethodCall_Internal(const EidosObjectClass *p_target, const EidosMethodSignature *p_method_signature, const std::vector<EidosASTNode *> &p_arguments) override;
};


#endif /* defined(__SLiM__slim_script_block__) */







































































