//
//  eidos_ast_node.h
//  Eidos
//
//  Created by Ben Haller on 7/27/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.


#ifndef __Eidos__eidos_ast_node__
#define __Eidos__eidos_ast_node__

#include <stdio.h>
#include <vector>

#include "eidos_token.h"
#include "eidos_value.h"
#include "eidos_call_signature.h"


class EidosASTNode;
class EidosInterpreter;


// EidosASTNodes must be allocated out of the global pool, for speed.  See eidos_object_pool.h.  When Eidos disposes of a node,
// it will assume that it was allocated from this pool, so its use is mandatory except for stack-allocated objects.
extern EidosObjectPool *gEidosASTNodePool;


// A typedef for a pointer to an EidosInterpreter evaluation method, cached for speed
typedef EidosValue_SP (EidosInterpreter::*EidosEvaluationMethod)(const EidosASTNode *p_node);


// A class representing a node in a parse tree for a script
class EidosASTNode
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
public:
	
	EidosToken *token_;													// normally not owned (owned by the Script's token stream) but:
	std::vector<EidosASTNode *> children_;								// OWNED POINTERS
	
	mutable EidosValue_SP cached_literal_value_;						// an optional pre-cached EidosValue for numbers, strings, and constant identifiers
	mutable EidosValue_SP cached_range_value_;							// an optional pre-cached EidosValue for constant range-operator expressions
	mutable EidosValue_SP cached_return_value_;							// an optional pre-cached EidosValue for constant return statements and constant-return blocks
	mutable EidosFunctionSignature_SP cached_signature_ = nullptr;		// a cached pointer to the function signature corresponding to the token
	mutable EidosEvaluationMethod cached_evaluator_ = nullptr;			// a pre-cached pointer to method to evaluate this node; shorthand for EvaluateNode()
	mutable EidosGlobalStringID cached_stringID_ = gEidosID_none;		// a pre-cached identifier for the token string, for fast property/method lookup
	
	uint8_t token_is_owned_ = false;									// if T, we own token_ because it is a virtual token that replaced a real token
	mutable uint8_t cached_for_references_index_ = true;				// pre-cached as true if the index variable is referenced at all in the loop
	mutable uint8_t cached_for_assigns_index_ = true;					// pre-cached as true if the index variable is assigned to in the loop
	mutable uint8_t cached_compound_assignment_ = false;				// pre-cached on assignment nodes if they are of the form "x=x+1" or "x=x-1" only
	
	mutable EidosTypeSpecifier typespec_;								// only valid for type-specifier nodes inside function declarations
	mutable bool hit_eof_in_tolerant_parse_ = false;					// only valid for compound statement nodes; used by the type-interpreter to handle scoping
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	mutable eidos_profile_t profile_total_ = 0;							// profiling clock for this node and its children; only set for some nodes
	EidosToken *full_range_end_token_ = nullptr;						// the ")" or "]" that ends the full range of tokens like "(", "[", for, if, and while
#endif
	
	EidosASTNode(const EidosASTNode&) = delete;							// no copying
	EidosASTNode& operator=(const EidosASTNode&) = delete;				// no copying
	EidosASTNode(void) = delete;										// no null construction
	
	// standard constructor; if p_token_is_owned, we own the token
	inline EidosASTNode(EidosToken *p_token, bool p_token_is_owned = false) : token_(p_token), token_is_owned_(p_token_is_owned) { }
	inline EidosASTNode(EidosToken *p_token, EidosASTNode *p_child_node) : token_(p_token)
	{
		this->AddChild(p_child_node);
	}
	
	~EidosASTNode(void);												// destructor
	
	void AddChild(EidosASTNode *p_child_node);							// takes ownership of the passed node
	void ReplaceTokenWithToken(EidosToken *p_token);					// used to fix virtual token to encompass their children; takes ownership
	
	void OptimizeTree(void) const;										// perform various (required) optimizations on the AST
	void _OptimizeConstants(void) const;								// cache EidosValues for constants and propagate constants upward
	void _OptimizeIdentifiers(void) const;								// cache function signatures, global strings for methods and properties, etc.
	void _OptimizeEvaluators(void) const;								// cache pointers to method for evaluation
	void _OptimizeFor(void) const;										// determine whether/how for-loop index variables need to be set up
	void _OptimizeForScan(const std::string &p_for_index_identifier, uint8_t *p_references, uint8_t *p_assigns) const;	// internal method
	void _OptimizeAssignments(void) const;								// detect and mark simple increment/decrement assignments on a variable
	
	bool HasCachedNumericValue(void) const;
	double CachedNumericValue(void) const;
	
	void PrintToken(std::ostream &p_outstream) const;
	void PrintTreeWithIndent(std::ostream &p_outstream, int p_indent) const;
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	void ZeroProfileTotals(void) const;
	eidos_profile_t ConvertProfileTotalsToSelfCounts(void) const;
	eidos_profile_t TotalOfSelfCounts(void) const;
	
	void FullUTF16Range(int32_t *p_start, int32_t *p_end) const;
#endif
};


#endif /* defined(__Eidos__eidos_ast_node__) */































































