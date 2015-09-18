//
//  eidos_ast_node.cpp
//  Eidos
//
//  Created by Ben Haller on 7/27/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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

using std::string;


EidosASTNode::EidosASTNode(EidosToken *p_token, bool p_token_is_owned) :
token_(p_token), token_is_owned_(p_token_is_owned)
{
}

EidosASTNode::EidosASTNode(EidosToken *p_token, EidosASTNode *p_child_node) :
token_(p_token)
{
	this->AddChild(p_child_node);
}

EidosASTNode::~EidosASTNode(void)
{
	for (auto child : children_)
		delete child;
	
	if (cached_value_)
	{
		if (cached_value_is_owned_)
			delete cached_value_;
		
		cached_value_ = nullptr;
	}
	
	if (token_is_owned_)
	{
		delete token_;
		token_ = nullptr;
	}
}

void EidosASTNode::AddChild(EidosASTNode *p_child_node)
{
	children_.push_back(p_child_node);
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
	_OptimizeConstants();
	_OptimizeIdentifiers();
	_OptimizeEvaluators();
	_OptimizeFor();
	_OptimizeAssignments();
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
		EidosValue *numeric_value = EidosInterpreter::NumericValueForString(token_->token_string_, token_);
		
		// OK, so this is a weird thing.  We don't call SetExternalPermanent(), because that guarantees that the object set will
		// live longer than the symbol table it might end up in, and we cannot make that guarantee here; the tree we are setting
		// this value in might be short-lived, whereas the symbol table might be long-lived (in an interactive interpreter
		// context, for example).  Instead, we call SetExternalTemporary().  Basically, we are pretending that we ourselves
		// (meaning the AST) are a symbol table, and that we "own" this object.  That will make the real symbol table make
		// its own copy of the object, rather than using ours.  It will also prevent the object from being regarded as a
		// temporary object and deleted by somebody, though.  So we get the best of both worlds; external ownership, but without
		// having to make the usual guarantee about the lifetime of the object.  The downside of this is that the value will
		// be copied if it is put into a symbol table, even though it is constant and could be shared in most circumstances.
		numeric_value->SetExternalTemporary();
		
		cached_value_ = numeric_value;
		cached_value_is_owned_ = true;
	}
	else if (token_type == EidosTokenType::kTokenString)
	{
		// This is taken from EidosInterpreter::Evaluate_String and needs to match exactly!
		EidosValue *result = new EidosValue_String_singleton(token_->token_string_);
		
		// See the above comment that begins "OK, so this is a weird thing".  It is still a weird thing down here, too.
		result->SetExternalTemporary();
		
		cached_value_ = result;
		cached_value_is_owned_ = true;
	}
	else if ((token_type == EidosTokenType::kTokenReturn) || (token_type == EidosTokenType::kTokenLBrace))
	{
		// These are node types which can propagate a single constant value upward.  Note that this is not strictly
		// true; both return and compound statements have side effects on the flow of execution.  It would therefore
		// be inappropriate for their execution to be short-circuited in favor of a constant value in general; but
		// that is not what this optimization means.  Rather, it means that these nodes are saying "I've got just a
		// constant value inside me, so *if* nothing else is going on around me, I can be taken as equal to that
		// constant."  We honor that conditional statement by only checking for the cached constant in specific places.
		if (children_.size() == 1)
		{
			const EidosASTNode *child = children_[0];
			EidosValue *cached_value = child->cached_value_;
			
			if (cached_value)
			{
				cached_value_ = cached_value;
				cached_value_is_owned_ = false;	// somebody below us owns the value
			}
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
		EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
		
		auto signature_iter = function_map->find(token_string);
		
		if (signature_iter != function_map->end())
			cached_signature_ = signature_iter->second;
		
		// if the identifier's name matches that of a property or method that we know about, cache an ID for it
		cached_stringID_ = EidosGlobalStringIDForString(token_string);
	}
	else if (token_->token_type_ == EidosTokenType::kTokenLParen)
	{
		// If we are a function call node, check that our first child, if it is a simple identifier, has cached a signature.
		// If we do not have children, or the first child is not an identifier, that is also problematic, but not our problem.
		// If a function identifier does not have a cached signature, it must at least have a cached string ID; this allows
		// initialize...() functions to pass our check, since they can't be set up before this check occurs.
		int children_count = (int)children_.size();
		
		if (children_count >= 1)
		{
			const EidosASTNode *first_child = children_[0];
			
			if (first_child->token_->token_type_ == EidosTokenType::kTokenIdentifier)
				if ((first_child->cached_signature_ == nullptr) && (first_child->cached_stringID_ == gEidosID_none))
					EIDOS_TERMINATION << "ERROR (EidosASTNode::_OptimizeIdentifiers): unrecognized function name \"" << first_child->token_->token_string_ << "\"." << eidos_terminate(first_child->token_);
		}
	}
	else if (token_->token_type_ == EidosTokenType::kTokenDot)
	{
		// If we are a dot-operator node, check that our second child has cached a string ID for the property or method being invoked.
		// If we do not have children, or the second child is not an identifier, that is also problematic, but not our problem.
		int children_count = (int)children_.size();
		
		if (children_count >= 2)
		{
			const EidosASTNode *second_child = children_[1];
			
			if (second_child->token_->token_type_ == EidosTokenType::kTokenIdentifier)
				if (second_child->cached_stringID_ == gEidosID_none)
					EIDOS_TERMINATION << "ERROR (EidosASTNode::_OptimizeIdentifiers): unrecognized property or method name \"" << second_child->token_->token_string_ << "\"." << eidos_terminate(second_child->token_);
		}
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
		case EidosTokenType::kTokenLParen:		cached_evaluator_ = &EidosInterpreter::Evaluate_FunctionCall;		break;
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
		default:
			// Node types with no known evaluator method just don't get an cached evaluator
			break;
	}
}

void EidosASTNode::_OptimizeForScan(const std::string &p_for_index_identifier, bool *p_references, bool *p_assigns) const
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
	else if (token_type == EidosTokenType::kTokenAssign)
	{
		// if the identifier occurs anywhere on the left-hand side of an assignment, that is an assignment (overbroad, but whatever)
		EidosASTNode *lvalue_node = children_[0];
		bool references = false, assigns = false;
		
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
			const string &function_name = function_name_node->token_->token_string_;
			
			if ((function_name.compare(gEidosStr_apply) == 0) || (function_name.compare(gEidosStr_executeLambda) == 0) || (function_name.compare(gEidosStr_rm) == 0))
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

void EidosASTNode::_OptimizeFor(void) const
{
	// recurse down the tree; determine our children, then ourselves
	for (auto child : children_)
		child->_OptimizeFor();
	
	EidosTokenType token_type = token_->token_type_;
	
	if ((token_type == EidosTokenType::kTokenFor) && (children_.size() == 3))
	{
		// This node is a for loop node.  We want to determine whether any node under this node:
		//	1. is unpredictable (executeLambda, apply, rm, ls)
		//	2. references our index variable
		//	3. assigns to our index variable
		EidosASTNode *identifier_child = children_[0];
		EidosASTNode *statement_child = children_[2];
		
		if (identifier_child->token_->token_type_ == EidosTokenType::kTokenIdentifier)
		{
			cached_for_references_index = false;
			cached_for_assigns_index = false;
			
			statement_child->_OptimizeForScan(identifier_child->token_->token_string_, &cached_for_references_index, &cached_for_assigns_index);
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
							
							if ((right_operand->token_->token_type_ == EidosTokenType::kTokenNumber) && (right_operand->cached_value_))
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

void EidosASTNode::PrintToken(std::ostream &p_outstream) const
{
	// We want to print some tokens differently when they are in the context of an AST, for readability
	switch (token_->token_type_)
	{
		case EidosTokenType::kTokenLBrace:		p_outstream << "BLOCK";				break;
		case EidosTokenType::kTokenSemicolon:	p_outstream << "NULL_STATEMENT";	break;
		case EidosTokenType::kTokenLParen:		p_outstream << "CALL";				break;
		case EidosTokenType::kTokenLBracket:	p_outstream << "SUBSET";			break;
		case EidosTokenType::kTokenComma:		p_outstream << "ARG_LIST";			break;
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

































































