//
//  eidos_ast_node.cpp
//  Eidos
//
//  Created by Ben Haller on 7/27/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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


#include "eidos_ast_node.h"
#include "eidos_interpreter.h"

#include "errno.h"
#include <string>
#include <algorithm>


// The global object pool for EidosASTNode, initialized in Eidos_WarmUp()
EidosObjectPool *gEidosASTNodePool = nullptr;


EidosASTNode::~EidosASTNode(void)
{
	for (auto child : children_)
	{
		// destroy children and return them to the pool; all children must be allocated out of gEidosASTNodePool!
		child->~EidosASTNode();
		gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(child));
	}
	
	if (token_is_owned_)
	{
		delete token_;
		token_ = nullptr;
	}
	
	if (argument_cache_)
	{
		delete argument_cache_;
		argument_cache_ = nullptr;
	}
}

void EidosASTNode::AddChild(EidosASTNode *p_child_node)
{
	children_.emplace_back(p_child_node);
}

void EidosASTNode::ReplaceTokenWithToken(EidosToken *p_token)
{
	// dispose of our previous token
	if (token_is_owned_)
	{
		delete token_;
		token_ = nullptr;
		token_is_owned_ = false;
	}
	
	// used to fix virtual token to encompass their children; takes ownership
	token_ = p_token;
	token_is_owned_ = true;
}

void EidosASTNode::OptimizeTree(void) const
{
	_OptimizeConstants();		// cache values for numeric and string constants, and for return statements and constant compound statements
	_OptimizeIdentifiers();		// cache unique IDs for identifiers using Eidos_GlobalStringIDForString()
	_OptimizeEvaluators();		// cache evaluator functions in cached_evaluator_ for fast node evaluation
	_OptimizeFor();				// cache information about for loops that allows them to be accelerated at runtime
	_OptimizeAssignments();		// cache information about assignments that allows simple increment/decrement assignments to be accelerated
}

void EidosASTNode::_OptimizeConstants(void) const
{
	// recurse down the tree; determine our children, then ourselves
	for (const EidosASTNode *child : children_)
		child->_OptimizeConstants();
	
	// now find constant expressions and make EidosValues for them
	EidosTokenType token_type = token_->token_type_;
	
	if (token_type == EidosTokenType::kTokenNumber)
	{
		try {
			cached_literal_value_ = EidosInterpreter::NumericValueForString(token_->token_string_, token_);
		}
		catch (...) {
			// if EidosInterpreter::NumericValueForString() raises, we just don't cache the value
		}
	}
	else if (token_type == EidosTokenType::kTokenString)
	{
		// This is taken from EidosInterpreter::Evaluate_String and needs to match exactly!
		cached_literal_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(token_->token_string_));
	}
	else if (token_type == EidosTokenType::kTokenIdentifier)
	{
		// Cache values for built-in constants; these can't be changed, so this should be safe, and
		// should be much faster than having to scan up through all the symbol tables recursively
		if (token_->token_string_ == gEidosStr_F)
			cached_literal_value_ = gStaticEidosValue_LogicalF;
		else if (token_->token_string_ == gEidosStr_T)
			cached_literal_value_ = gStaticEidosValue_LogicalT;
		else if (token_->token_string_ == gEidosStr_INF)
			cached_literal_value_ = gStaticEidosValue_FloatINF;
		else if (token_->token_string_ == gEidosStr_NAN)
			cached_literal_value_ = gStaticEidosValue_FloatNAN;
		else if (token_->token_string_ == gEidosStr_E)
			cached_literal_value_ = gStaticEidosValue_FloatE;
		else if (token_->token_string_ == gEidosStr_PI)
			cached_literal_value_ = gStaticEidosValue_FloatPI;
		else if (token_->token_string_ == gEidosStr_NULL)
			cached_literal_value_ = gStaticEidosValueNULL;
	}
	else if (token_type == EidosTokenType::kTokenReturn)
	{
		// A return statement can propagate a single constant value upward.  Note that this is not strictly true;
		// return statements have side effects on the flow of execution.  It would therefore be inappropriate for
		// their execution to be short-circuited in favor of a constant value in general; but that is not what
		// this optimization means.  Rather, it means that these nodes are saying "I've got just a constant value
		// inside me, so *if* nothing else is going on around me, I can be taken as equal to that constant."  We
		// honor that conditional statement by only checking for the cached constant in specific places.
		if (children_.size() == 1)
		{
			const EidosASTNode *child = children_[0];
			
			if (child->cached_literal_value_)
				cached_return_value_ = child->cached_literal_value_;
		}
	}
	else if (token_type == EidosTokenType::kTokenLBrace)
	{
		// This dovetails with the caching of returned values above, and the same caveats apply.  Basically, the idea
		// is that if a block consists of nothing but the return of a constant value, like "{ return 1.5; }", then
		// the block can declare that with cached_value_ and intelligent users of the block can avoid interpreting
		// the block.  Note that since blocks no longer evaluate to the value of their last statement, we now require
		// the child of the block to be an explicit return statement.
		if (children_.size() == 1)
		{
			const EidosASTNode *child = children_[0];
			
			if (child->cached_return_value_ && (child->token_->token_type_ == EidosTokenType::kTokenReturn))
				cached_return_value_ = child->cached_return_value_;
		}
	}
}

void EidosASTNode::_OptimizeIdentifiers(void) const
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : children_)
		child->_OptimizeIdentifiers();
	
	if (token_->token_type_ == EidosTokenType::kTokenIdentifier)
	{
		const std::string &token_string = token_->token_string_;
		
		// if the identifier's name matches that of a global function, cache the function signature
		const EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
		
		if (function_map)
		{
			auto signature_iter = function_map->find(token_string);
			
			if (signature_iter != function_map->end())
				cached_signature_ = signature_iter->second;
		}
		
		// cache a uniqued ID for the identifier, allowing fast matching
		cached_stringID_ = Eidos_GlobalStringIDForString(token_string);
	}
}

void EidosASTNode::_OptimizeEvaluators(void) const
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : children_)
		child->_OptimizeEvaluators();
	
	EidosTokenType token_type = token_->token_type_;
	
	// The structure here avoids doing a break in the non-error case; a bit faster.
	switch (token_type)
	{
		case EidosTokenType::kTokenSemicolon:	cached_evaluator_ = &EidosInterpreter::Evaluate_NullStatement;		break;
		case EidosTokenType::kTokenColon:		cached_evaluator_ = &EidosInterpreter::Evaluate_RangeExpr;			break;
		case EidosTokenType::kTokenLBrace:		cached_evaluator_ = &EidosInterpreter::Evaluate_CompoundStatement;	break;
		case EidosTokenType::kTokenLParen:		cached_evaluator_ = &EidosInterpreter::Evaluate_Call;				break;
		case EidosTokenType::kTokenLBracket:	cached_evaluator_ = &EidosInterpreter::Evaluate_Subset;				break;
		case EidosTokenType::kTokenDot:			cached_evaluator_ = &EidosInterpreter::Evaluate_MemberRef;			break;
		case EidosTokenType::kTokenPlus:		cached_evaluator_ = &EidosInterpreter::Evaluate_Plus;				break;
		case EidosTokenType::kTokenMinus:		cached_evaluator_ = &EidosInterpreter::Evaluate_Minus;				break;
		case EidosTokenType::kTokenMod:			cached_evaluator_ = &EidosInterpreter::Evaluate_Mod;				break;
		case EidosTokenType::kTokenMult:		cached_evaluator_ = &EidosInterpreter::Evaluate_Mult;				break;
		case EidosTokenType::kTokenExp:			cached_evaluator_ = &EidosInterpreter::Evaluate_Exp;				break;
		case EidosTokenType::kTokenAnd:			cached_evaluator_ = &EidosInterpreter::Evaluate_And;				break;
		case EidosTokenType::kTokenOr:			cached_evaluator_ = &EidosInterpreter::Evaluate_Or;					break;
		case EidosTokenType::kTokenDiv:			cached_evaluator_ = &EidosInterpreter::Evaluate_Div;				break;
		case EidosTokenType::kTokenConditional:	cached_evaluator_ = &EidosInterpreter::Evaluate_Conditional;		break;
		case EidosTokenType::kTokenAssign:		cached_evaluator_ = &EidosInterpreter::Evaluate_Assign;				break;
		case EidosTokenType::kTokenEq:			cached_evaluator_ = &EidosInterpreter::Evaluate_Eq;					break;
		case EidosTokenType::kTokenLt:			cached_evaluator_ = &EidosInterpreter::Evaluate_Lt;					break;
		case EidosTokenType::kTokenLtEq:		cached_evaluator_ = &EidosInterpreter::Evaluate_LtEq;				break;
		case EidosTokenType::kTokenGt:			cached_evaluator_ = &EidosInterpreter::Evaluate_Gt;					break;
		case EidosTokenType::kTokenGtEq:		cached_evaluator_ = &EidosInterpreter::Evaluate_GtEq;				break;
		case EidosTokenType::kTokenNot:			cached_evaluator_ = &EidosInterpreter::Evaluate_Not;				break;
		case EidosTokenType::kTokenNotEq:		cached_evaluator_ = &EidosInterpreter::Evaluate_NotEq;				break;
		case EidosTokenType::kTokenNumber:		cached_evaluator_ = &EidosInterpreter::Evaluate_Number;				break;
		case EidosTokenType::kTokenString:		cached_evaluator_ = &EidosInterpreter::Evaluate_String;				break;
		case EidosTokenType::kTokenIdentifier:	cached_evaluator_ = &EidosInterpreter::Evaluate_Identifier;			break;
		case EidosTokenType::kTokenIf:			cached_evaluator_ = &EidosInterpreter::Evaluate_If;					break;
		case EidosTokenType::kTokenDo:			cached_evaluator_ = &EidosInterpreter::Evaluate_Do;					break;
		case EidosTokenType::kTokenWhile:		cached_evaluator_ = &EidosInterpreter::Evaluate_While;				break;
		case EidosTokenType::kTokenFor:			cached_evaluator_ = &EidosInterpreter::Evaluate_For;				break;
		case EidosTokenType::kTokenNext:		cached_evaluator_ = &EidosInterpreter::Evaluate_Next;				break;
		case EidosTokenType::kTokenBreak:		cached_evaluator_ = &EidosInterpreter::Evaluate_Break;				break;
		case EidosTokenType::kTokenReturn:		cached_evaluator_ = &EidosInterpreter::Evaluate_Return;				break;
		case EidosTokenType::kTokenFunction:	cached_evaluator_ = &EidosInterpreter::Evaluate_FunctionDecl;		break;
		default:
			// Node types with no known evaluator method just don't get an cached evaluator
			break;
	}
}

void EidosASTNode::_OptimizeForScan(const std::string &p_for_index_identifier, uint8_t *p_references, uint8_t *p_assigns) const
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : children_)
		child->_OptimizeForScan(p_for_index_identifier, p_references, p_assigns);
	
	EidosTokenType token_type = token_->token_type_;
	
	if (token_type == EidosTokenType::kTokenIdentifier)
	{
		// if the identifier occurs anywhere in the subtree, that is a reference
		if (token_->token_string_.compare(p_for_index_identifier) == 0)
			*p_references = true;
	}
	else if (children_.size() >= 1)
	{
		if (token_type == EidosTokenType::kTokenAssign)
		{
			// if the identifier occurs anywhere on the left-hand side of an assignment, that is an assignment (overbroad, but whatever)
			EidosASTNode *lvalue_node = children_[0];
			uint8_t references = false, assigns = false;
			
			lvalue_node->_OptimizeForScan(p_for_index_identifier, &references, &assigns);
			
			if (references)
				*p_assigns = true;
		}
		else if (token_type == EidosTokenType::kTokenFor)
		{
			// for loops assign into their index variable, so they are like an assignment statement
			EidosASTNode *identifier_child = children_[0];
			
			if (identifier_child->token_->token_type_ == EidosTokenType::kTokenIdentifier)
			{
				if (identifier_child->token_->token_string_.compare(p_for_index_identifier) == 0)
					*p_assigns = true;
			}
		}
		else if (token_type == EidosTokenType::kTokenLParen)
		{
			// certain functions are unpredictable and must be assumed to reference and/or assign
			EidosASTNode *function_name_node = children_[0];
			EidosTokenType function_name_token_type = function_name_node->token_->token_type_;
			
			if (function_name_token_type == EidosTokenType::kTokenIdentifier)	// is it a function call, not a method call?
			{
				const std::string &function_name = function_name_node->token_->token_string_;
				
				if ((function_name.compare(gEidosStr_apply) == 0) || (function_name.compare(gEidosStr_sapply) == 0) || (function_name.compare(gEidosStr_executeLambda) == 0) || (function_name.compare(gEidosStr__executeLambda_OUTER) == 0) || (function_name.compare(gEidosStr_doCall) == 0) || (function_name.compare(gEidosStr_rm) == 0))
				{
					*p_references = true;
					*p_assigns = true;
				}
				else if (function_name.compare(gEidosStr_ls) == 0)
				{
					*p_references = true;
				}
			}
		}
	}
}

void EidosASTNode::_OptimizeFor(void) const
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : children_)
		child->_OptimizeFor();
	
	EidosTokenType token_type = token_->token_type_;
	
	if ((token_type == EidosTokenType::kTokenFor) && (children_.size() == 3))
	{
		// This node is a for loop node.  We want to determine whether any node under this node:
		//	1. is unpredictable (executeLambda, _executeLambda_OUTER, apply, sapply, rm, ls)
		//	2. references our index variable
		//	3. assigns to our index variable
		EidosASTNode *identifier_child = children_[0];
		EidosASTNode *statement_child = children_[2];
		
		if (identifier_child->token_->token_type_ == EidosTokenType::kTokenIdentifier)
		{
			cached_for_references_index_ = false;
			cached_for_assigns_index_ = false;
			
			statement_child->_OptimizeForScan(identifier_child->token_->token_string_, &cached_for_references_index_, &cached_for_assigns_index_);
		}
	}
}

void EidosASTNode::_OptimizeAssignments(void) const
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : children_)
		child->_OptimizeAssignments();
	
	EidosTokenType token_type = token_->token_type_;
	
	if ((token_type == EidosTokenType::kTokenAssign) && (children_.size() == 2))
	{
		// We have an assignment node with two children...
		EidosASTNode *child0 = children_[0];
		
		if (child0->token_->token_type_ == EidosTokenType::kTokenIdentifier)
		{
			// ...the lvalue is a simple identifier...
			EidosASTNode *child1 = children_[1];
			EidosTokenType child1_token_type = child1->token_->token_type_;
			
			if ((child1_token_type == EidosTokenType::kTokenPlus) || (child1_token_type == EidosTokenType::kTokenMinus) || (child1_token_type == EidosTokenType::kTokenDiv) || (child1_token_type == EidosTokenType::kTokenMod) || (child1_token_type == EidosTokenType::kTokenMult) || (child1_token_type == EidosTokenType::kTokenExp))
			{
				// ... the rvalue uses an eligible operator...
				if (child1->children_.size() == 2)
				{
					// ... the rvalue has two children...
					EidosASTNode *left_operand = child1->children_[0];
					
					if (left_operand->token_->token_type_ == EidosTokenType::kTokenIdentifier)
					{
						// ... the left operand is an identifier...
						if (left_operand->token_->token_string_.compare(child0->token_->token_string_) == 0)
						{
							// ... the left and right identifiers are the same...
							EidosASTNode *right_operand = child1->children_[1];
							
							if ((right_operand->token_->token_type_ == EidosTokenType::kTokenNumber) && (right_operand->cached_literal_value_))
							{
								// ... and the right operand is a constant number with a cached value...
								
								// we have a simple increment/decrement by one, so we mark that in the tree for Evaluate_Assign() to handle super fast
								cached_compound_assignment_ = true;
							}
						}
					}
				}
			}
		}
	}
}

bool EidosASTNode::HasCachedNumericValue(void) const
{
	if ((token_->token_type_ == EidosTokenType::kTokenNumber) && cached_literal_value_ && (cached_literal_value_->Count() == 1))
		return true;
	
	if ((token_->token_type_ == EidosTokenType::kTokenMinus) && (children_.size() == 1))
	{
		const EidosASTNode *minus_child = children_[0];
		
		if ((minus_child->token_->token_type_ == EidosTokenType::kTokenNumber) && minus_child->cached_literal_value_ && (minus_child->cached_literal_value_->Count() == 1))
			return true;
	}
	
	return false;
}

double EidosASTNode::CachedNumericValue(void) const
{
	if ((token_->token_type_ == EidosTokenType::kTokenNumber) && cached_literal_value_ && (cached_literal_value_->Count() == 1))
		return cached_literal_value_->FloatAtIndex(0, nullptr);
	
	if ((token_->token_type_ == EidosTokenType::kTokenMinus) && (children_.size() == 1))
	{
		const EidosASTNode *minus_child = children_[0];
		
		if ((minus_child->token_->token_type_ == EidosTokenType::kTokenNumber) && minus_child->cached_literal_value_ && (minus_child->cached_literal_value_->Count() == 1))
			return -minus_child->cached_literal_value_->FloatAtIndex(0, nullptr);
	}
	
	EIDOS_TERMINATION << "ERROR (EidosASTNode::CachedNumericValue): (internal error) no cached numeric value" << EidosTerminate(nullptr);
}

void EidosASTNode::PrintToken(std::ostream &p_outstream) const
{
	// We want to print some tokens differently when they are in the context of an AST, for readability
	switch (token_->token_type_)
	{
		case EidosTokenType::kTokenLBrace:		p_outstream << "BLOCK";				break;
		case EidosTokenType::kTokenSemicolon:	p_outstream << "NULL_STATEMENT";	break;
		case EidosTokenType::kTokenLParen:		p_outstream << "CALL";				break;
		case EidosTokenType::kTokenLBracket:	p_outstream << "SUBSET";			break;
		//case EidosTokenType::kTokenComma:		p_outstream << "ARG_LIST";			break;		// no longer used in the AST
		default:								p_outstream << *token_;				break;
	}
}

void EidosASTNode::PrintTreeWithIndent(std::ostream &p_outstream, int p_indent) const
{
	// If we are indented, start a new line and indent
	if (p_indent > 0)
	{
		p_outstream << "\n  ";
		
		for (int i = 0; i < p_indent - 1; ++i)
			p_outstream << "  ";
	}
	
	if (children_.size() == 0)
	{
		// If we are a leaf, just print our token
		PrintToken(p_outstream);
	}
	else
	{
		// Determine whether we have only leaves as children
		bool childWithChildren = false;
		
		for (auto child : children_)
		{
			if (child->children_.size() > 0)
			{
				childWithChildren = true;
				break;
			}
		}
		
		if (childWithChildren)
		{
			// If we have non-leaf children, then print them with an incremented indent
			p_outstream << "(";
			PrintToken(p_outstream);
			
			for (auto child : children_)
				child->PrintTreeWithIndent(p_outstream, p_indent + 1);
			
			// and then outdent and show our end paren
			p_outstream << "\n";
			
			if (p_indent > 0)
			{
				p_outstream << "  ";
				
				for (int i = 0; i < p_indent - 1; ++i)
					p_outstream << "  ";
			}
			
			p_outstream << ")";
		}
		else
		{
			// If we have only leaves as children, then print everything on one line, for compactness
			p_outstream << "(";
			PrintToken(p_outstream);
			
			for (auto child : children_)
			{
				p_outstream << " ";
				child->PrintToken(p_outstream);
			}
			
			p_outstream << ")";
		}
	}
}

#if defined(SLIMGUI) && (SLIMPROFILING == 1)
// PROFILING

void EidosASTNode::ZeroProfileTotals(void) const
{
	// recurse down the tree; zero our children, then ourselves
	for (const EidosASTNode *child : children_)
		child->ZeroProfileTotals();
	
	profile_total_ = 0;
}

eidos_profile_t EidosASTNode::ConvertProfileTotalsToSelfCounts(void) const
{
	// convert profile counts in the tree to self counts, excluding time spent in children
	if (profile_total_)
	{
		// Nodes with a non-zero count return their count as their total, and exclude their children
		eidos_profile_t result = profile_total_;
		
		for (const EidosASTNode *child : children_)
			profile_total_ -= child->ConvertProfileTotalsToSelfCounts();
		
		return result;
	}
	else
	{
		// Nodes with a zero count have a zero self count, and report the total of their children
		eidos_profile_t result = 0;
		
		for (const EidosASTNode *child : children_)
			result += child->ConvertProfileTotalsToSelfCounts();
		
		return result;
	}
}

eidos_profile_t EidosASTNode::TotalOfSelfCounts(void) const
{
	eidos_profile_t total = profile_total_;
	
	for (const EidosASTNode *child : children_)
		total += child->TotalOfSelfCounts();
	
	return total;
}

void EidosASTNode::FullUTF16Range(int32_t *p_start, int32_t *p_end) const
{
	int32_t start = token_->token_UTF16_start_;
	int32_t end = token_->token_UTF16_end_;
	
	if (full_range_end_token_)
	{
		// If we have an end token, that defines our range end
		end = std::max(end, full_range_end_token_->token_UTF16_end_);
		
		// We still need to scan our children for our range start, however
		for (const EidosASTNode *child : children_)
		{
			int32_t child_start = 0, child_end = 0;
			
			child->FullUTF16Range(&child_start, &child_end);
			
			start = std::min(start, child_start);
		}
	}
	else
	{
		// Otherwise, incorporate the ranges of our children
		for (const EidosASTNode *child : children_)
		{
			int32_t child_start = 0, child_end = 0;
			
			child->FullUTF16Range(&child_start, &child_end);
			
			start = std::min(start, child_start);
			end = std::max(end, child_end);
		}
	}
	
	*p_start = start;
	*p_end = end;
}

#endif	// defined(SLIMGUI) && (SLIMPROFILING == 1)
































































