//
//  script_interpreter.cpp
//  SLiM
//
//  Created by Ben Haller on 4/4/15.
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


#include "script_interpreter.h"
#include "script_functions.h"

#include <sstream>
#include "math.h"
#include "g_rng.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


// It is worth noting here that the memory management here leaks like a sieve.  I think it should only leak when exceptions are raised;
// but of course exceptions are raised a lot, especially in the interactive SLiMscribe environment.  The leaks are generally caused by a
// raise inside a call to EvaluateNode() that blows through the caller's context and fails to free ScriptValue pointer owned by the caller.
// It would be possible to fix them, of course; but since this is primarily a problem for SLiMscribe, I am not overly concerned about it
// at the moment – in SLiM itself, raises are fatal, so a leak at that point is irrelevant.  It also not completely clear what the best way
// is to fix the leaks.  Probably I ought to be using std::unique_ptr instead of naked pointers; but that's a C++11 feature, and not all of
// those are fully supported by all compilers yet, so if I started using them, it might make SLiM less portable.  My inclination is to hold
// off for now, let it leak, and perhaps migrate to std::unique_ptr later, rather than trying to protect every EvaluateNode() call.


bool TypeCheckAssignmentOfValueIntoValue(ScriptValue *base_value, ScriptValue *dest_value)
{
	ScriptValueType base_type = base_value->Type();
	ScriptValueType dest_type = dest_value->Type();
	bool base_is_object = (base_type == ScriptValueType::kValueObject);
	bool dest_is_object = (dest_type == ScriptValueType::kValueObject);
	
	if (base_is_object && dest_is_object)
	{
		// objects must match in their element type, or one or both must have no defined element type (due to being empty)
		string base_element_type = static_cast<ScriptValue_Object *>(base_value)->ElementType();
		string dest_element_type = static_cast<ScriptValue_Object *>(dest_value)->ElementType();
		bool base_is_typeless = (base_element_type.length() == 0);
		bool dest_is_typeless = (dest_element_type.length() == 0);
		
		if (base_is_typeless || dest_is_typeless)
			return true;
		
		return (base_element_type.compare(dest_element_type) == 0);
	}
	else if (base_is_object || dest_is_object)
	{
		// objects cannot be mixed with non-objects
		return false;
	}
	
	// identical types are always compatible, apart from object types handled above
	if (base_type == dest_type)
		return true;
	
	// NULL cannot be assigned into other things; this is a difference from R, because NULL cannot be represented as a value in other types
	// (it is its own type, not a value within types).
	if (base_type == ScriptValueType::kValueNULL)
		return false;
	
	// otherwise, we follow the promotion order defined in ScriptValueType
	return (dest_type > base_type);
}


//
//	ScriptInterpreter
//
#pragma mark ScriptInterpreter

ScriptInterpreter::ScriptInterpreter(const Script &p_script) : root_node_(p_script.AST())
{
	SharedInitialization();
}

ScriptInterpreter::ScriptInterpreter(const Script &p_script, SymbolTable *p_symbols) : root_node_(p_script.AST()), global_symbols_(p_symbols)
{
	SharedInitialization();
}

ScriptInterpreter::ScriptInterpreter(const ScriptASTNode *p_root_node_) : root_node_(p_root_node_)
{
	SharedInitialization();
}

ScriptInterpreter::ScriptInterpreter(const ScriptASTNode *p_root_node_, SymbolTable *p_symbols) : root_node_(p_root_node_), global_symbols_(p_symbols)
{
	SharedInitialization();
}

void ScriptInterpreter::SharedInitialization(void)
{
	if (!global_symbols_)
		global_symbols_ = new SymbolTable();
	
	RegisterFunctionMap(ScriptInterpreter::BuiltInFunctionMap());
	
	// Initialize the random number generator if and only if it has not already been initialized
	// If SLiM wants a different seed, it will enforce that; not our problem.
	if (!g_rng)
		InitializeRNGFromSeed(GenerateSeedFromPIDAndTime());
}

ScriptInterpreter::~ScriptInterpreter(void)
{
	delete global_symbols_;
	global_symbols_ = nullptr;
	
	if (function_map_ != ScriptInterpreter::BuiltInFunctionMap())
	{
		delete function_map_;
		function_map_ = nullptr;
	}
}

void ScriptInterpreter::SetShouldLogExecution(bool p_log)
{
	logging_execution_ = p_log;
}

bool ScriptInterpreter::ShouldLogExecution(void)
{
	return logging_execution_;
}

std::string ScriptInterpreter::ExecutionLog(void)
{
	return execution_log_.str();
}

std::string ScriptInterpreter::ExecutionOutput(void)
{
	return execution_output_.str();
}

SymbolTable &ScriptInterpreter::BorrowSymbolTable(void)
{
	return *global_symbols_;
}

SymbolTable *ScriptInterpreter::YieldSymbolTable(void)
{
	SymbolTable *return_value = global_symbols_;
	
	global_symbols_ = new SymbolTable();	// replace our symbol table with a fresh copy
	
	return return_value;
}

// the starting point for script blocks in SLiM simulations, which require braces
ScriptValue *ScriptInterpreter::EvaluateScriptBlock(void)
{
	if (logging_execution_)
	{
		execution_log_indent_ = 0;
		execution_log_ << IndentString(execution_log_indent_++) << "EvaluateScriptBlock() entered\n";
	}
	
	ScriptValue *result = EvaluateNode(root_node_);
	
	// if a next or break statement was hit and was not handled by a loop, throw an error
	if (next_statement_hit_ || break_statement_hit_)
	{
		if (result->IsTemporary()) delete result;
		
		SLIM_TERMINATION << "ERROR (EvaluateScriptBlock): statement \"" << (next_statement_hit_ ? "next" : "break") << "\" encountered with no enclosing loop." << slim_terminate();
	}
	
	// handle a return statement; we're at the top level, so there's not much to do
	if (return_statement_hit_)
		return_statement_hit_ = false;
	
	// EvaluateScriptBlock() does not send the result of execution to the output stream; EvaluateInterpreterBlock() does,
	// because it is for interactive use, but EvaluateScriptBlock() is for use in SLiM itself, and so interactive output
	// is undesirable.  Script that wants to generate output can always use print().
	
	/*if (!result->Invisible())
	{
		auto position = execution_output_.tellp();
		execution_output_ << *result;
		
		// ScriptValue does not put an endl on the stream, so if it emitted any output, add an endl
		if (position != execution_output_.tellp())
			execution_output_ << endl;
	}*/
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "EvaluateScriptBlock() : return == " << *result << "\n";
	
	// if requested, send the full trace to std:cout
	if (gSLiMScriptLogEvaluation)
		std::cout << (execution_log_.str());
	
	return result;
}

// the starting point for script blocks in SLiMscript, which does not require braces
ScriptValue *ScriptInterpreter::EvaluateInterpreterBlock(void)
{
	if (logging_execution_)
	{
		execution_log_indent_ = 0;
		execution_log_ << IndentString(execution_log_indent_++) << "EvaluateInterpreterBlock() entered\n";
	}
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	for (ScriptASTNode *child_node : root_node_->children_)
	{
		if (result->IsTemporary()) delete result;
		
		result = EvaluateNode(child_node);
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
		{
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (EvaluateInterpreterBlock): statement \"" << (next_statement_hit_ ? "next" : "break") << "\" encountered with no enclosing loop." << slim_terminate();
		}
		
		// send the result of the block to our output stream
		if (!result->Invisible())
		{
			auto position = execution_output_.tellp();
			execution_output_ << *result;
			
			// ScriptValue does not put an endl on the stream, so if it emitted any output, add an endl
			if (position != execution_output_.tellp())
				execution_output_ << endl;
		}
		
		// handle a return statement; we're at the top level, so there's not much to do except stop execution
		if (return_statement_hit_)
		{
			return_statement_hit_ = false;
			break;
		}
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "EvaluateInterpreterBlock() : return == " << *result << "\n";
	
	// if requested, send the full trace to std:cout
	if (gSLiMScriptLogEvaluation)
		std::cout << (execution_log_.str());
	
	return result;
}

// A subscript has been encountered as the top-level operation on the left-hand side of an assignment – x[5] = y, x.foo[5] = y, or more
// complex cases like x[3:10].foo[2:5][1:2] = y.  The job of this function is to determine the identity of the symbol host (x, x, and
// x[3:10], respectively), the name of the member within the symbol host (none, foo, and foo, respectively), and the indices of the final
// subscript operation (5, 5, and {3,4}, respectively), and return them to the caller, who will assign into those subscripts.  Note that
// complex cases work only because of several other aspects of SLiMscript.  Notably, subscripting of an object creates a new object, but
// the new object refers to the same elements as the parent object, by pointer; this means that x[5].foo = y works, because x[5] refers to
// the same element, by pointer, as x does.  If SLiMscript did not have these shared-value semantics, assignment would be much trickier,
// since SLiMscript cannot use a symbol table to store values, in general (since many values accessible through script are stored in
// private representations kept by external classes in SLiM).  In other words, assignment relies upon the fact that a temporary object
// constructed by Evaluate_Node() refers to the same underlying element objects as the original source of the elements does, and thus
// assigning into the temporary also assigns into the original.
void ScriptInterpreter::_ProcessSubscriptAssignment(ScriptValue **p_base_value_ptr, string *p_member_name_ptr, vector<int> *p_indices_ptr, const ScriptASTNode *p_parent_node)
{
	// The left operand is the thing we're subscripting.  If it is an identifier or a dot operator, then we are the deepest (i.e. first)
	// subscript operation, and we can resolve the symbol host, set up a vector of indices, and return.  If it is a subscript, we recurse.
	TokenType token_type = p_parent_node->token_->token_type_;
	
	switch (token_type)
	{
		case TokenType::kTokenLBracket:
		{
			if (p_parent_node->children_.size() != 2)
				SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): internal error (expected 2 children for '[' node)." << slim_terminate();
			
			ScriptASTNode *left_operand = p_parent_node->children_[0];
			ScriptASTNode *right_operand = p_parent_node->children_[1];
			
			vector<int> base_indices;
			
			// Recurse to find the symbol host and member name that we are ultimately subscripting off of
			_ProcessSubscriptAssignment(p_base_value_ptr, p_member_name_ptr, &base_indices, left_operand);
			
			// Find out which indices we're supposed to use within our base vector
			ScriptValue *second_child_value = EvaluateNode(right_operand);
			ScriptValueType second_child_type = second_child_value->Type();
			
			if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat) && (second_child_type != ScriptValueType::kValueLogical) && (second_child_type != ScriptValueType::kValueNULL))
			{
				if (second_child_value->IsTemporary()) delete second_child_value;
				
				SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): index operand type " << second_child_type << " is not supported by the '[]' operator." << slim_terminate();
			}
			
			int second_child_count = second_child_value->Count();
			
			if (second_child_type == ScriptValueType::kValueLogical)
			{
				// A logical vector must exactly match in length; if it does, it selects corresponding indices from base_indices
				if (second_child_count != base_indices.size())
				{
					if (second_child_value->IsTemporary()) delete second_child_value;
					
					SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << slim_terminate();
				}
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					bool bool_value = second_child_value->LogicalAtIndex(value_idx);
					
					if (bool_value)
						p_indices_ptr->push_back(base_indices[value_idx]);
				}
			}
			else if ((second_child_type == ScriptValueType::kValueInt) || (second_child_type == ScriptValueType::kValueFloat))
			{
				// A numeric vector can be of any length; each number selects the index at that index in base_indices
				int base_indices_count =  (int)base_indices.size();
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					int64_t index_value = second_child_value->IntAtIndex(value_idx);
					
					if ((index_value < 0) || (index_value >= base_indices_count))
					{
						if (second_child_value->IsTemporary()) delete second_child_value;
						
						SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): out-of-range index " << index_value << " used with the '[]' operator." << slim_terminate();
					}
					else
						p_indices_ptr->push_back(base_indices[index_value]);
				}
			}
			else if (second_child_type == ScriptValueType::kValueNULL)
			{
				// A NULL index selects no values; this will likely cause a raise downstream, but that is not our problem, it's legal syntax
				base_indices.clear();
				*p_indices_ptr = base_indices;
			}
			
			break;
		}
		case TokenType::kTokenDot:
		{
			if (p_parent_node->children_.size() != 2)
				SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): internal error (expected 2 children for '.' node)." << slim_terminate();
			
			ScriptASTNode *left_operand = p_parent_node->children_[0];
			ScriptASTNode *right_operand = p_parent_node->children_[1];
			
			ScriptValue *first_child_value = EvaluateNode(left_operand);
			ScriptValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != ScriptValueType::kValueObject)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): operand type " << first_child_type << " is not supported by the '.' operator." << slim_terminate();
			}
			
			if (right_operand->token_->token_type_ != TokenType::kTokenIdentifier)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): the '.' operator for x.y requires operand y to be an identifier." << slim_terminate();
			}
			
			*p_base_value_ptr = first_child_value;
			*p_member_name_ptr = right_operand->token_->token_string_;
			
			int number_of_elements = first_child_value->Count();	// member operations are guaranteed to produce one value per element
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->push_back(element_idx);
			
			break;
		}
		case TokenType::kTokenIdentifier:
		{
			if (p_parent_node->children_.size() != 0)
				SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): internal error (expected 0 children for identifier node)." << slim_terminate();
			
			ScriptValue *identifier_value = global_symbols_->GetValueForSymbol(p_parent_node->token_->token_string_);
			*p_base_value_ptr = identifier_value;
			
			int number_of_elements = identifier_value->Count();	// this value is already defined, so this is fast
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->push_back(element_idx);
			
			break;
		}
		default:
			SLIM_TERMINATION << "ERROR (_ProcessSubscriptAssignment): Unexpected node token type " << token_type << "; lvalue required." << slim_terminate();
			break;
	}
}

void ScriptInterpreter::_AssignRValueToLValue(ScriptValue *rvalue, const ScriptASTNode *p_lvalue_node)
{
	TokenType token_type = p_lvalue_node->token_->token_type_;
	
	if (logging_execution_)
	{
		execution_log_ << IndentString(execution_log_indent_) << "_AssignRValueToLValue() : lvalue token ";
		p_lvalue_node->PrintToken(execution_log_);
		execution_log_ << "\n";
	}
	
	switch (token_type)
	{
		case TokenType::kTokenLBracket:
		{
			if (p_lvalue_node->children_.size() != 2)
				SLIM_TERMINATION << "ERROR (_AssignRValueToLValue): internal error (expected 2 children for '[' node)." << slim_terminate();
			
			ScriptValue *base_value;
			string member_name;
			vector<int> indices;
			
			_ProcessSubscriptAssignment(&base_value, &member_name, &indices, p_lvalue_node);
			
			int index_count = (int)indices.size();
			int rvalue_count = rvalue->Count();
			
			if (rvalue_count == 1)
			{
				if (member_name.length() == 0)
				{
					if (!TypeCheckAssignmentOfValueIntoValue(rvalue, base_value))
						SLIM_TERMINATION << "ERROR (ScriptInterpreter::_AssignRValueToLValue): type mismatch in assignment." << slim_terminate();
					
					// we have a multiplex assignment of one value to (maybe) more than one index in a symbol host: x[5:10] = 10
					for (int value_idx = 0; value_idx < index_count; value_idx++)
						base_value->SetValueAtIndex(indices[value_idx], rvalue);
				}
				else
				{
					// we have a multiplex assignment of one value to (maybe) more than one index in a member of a symbol host: x.foo[5:10] = 10
					// here we use the guarantee that the member operator returns one result per element, and that elements following sharing semantics,
					// to rearrange this assignment from host.member[indices] = rvalue to host[indices].member = rvalue; this must be equivalent!
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						ScriptValue *temp_lvalue = base_value->GetValueAtIndex(indices[value_idx]);
						
						if (temp_lvalue->Type() != ScriptValueType::kValueObject)
							SLIM_TERMINATION << "ERROR (ScriptInterpreter::_AssignRValueToLValue): internal error: dot operator used with non-object value." << slim_terminate();
						
						static_cast<ScriptValue_Object *>(temp_lvalue)->SetValueForMemberOfElements(member_name, rvalue);
						
						delete temp_lvalue;
					}
				}
			}
			else if (index_count == rvalue_count)
			{
				if (member_name.length() == 0)
				{
					if (!TypeCheckAssignmentOfValueIntoValue(rvalue, base_value))
						SLIM_TERMINATION << "ERROR (ScriptInterpreter::_AssignRValueToLValue): type mismatch in assignment." << slim_terminate();
					
					// we have a one-to-one assignment of values to indices in a symbol host: x[5:10] = 5:10
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						ScriptValue *temp_rvalue = rvalue->GetValueAtIndex(value_idx);
						
						base_value->SetValueAtIndex(indices[value_idx], temp_rvalue);
						delete temp_rvalue;
					}
				}
				else
				{
					// we have a one-to-one assignment of values to indices in a member of a symbol host: x.foo[5:10] = 5:10
					// as above, we rearrange this assignment from host.member[indices1] = rvalue[indices2] to host[indices1].member = rvalue[indices2]
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						ScriptValue *temp_lvalue = base_value->GetValueAtIndex(indices[value_idx]);
						ScriptValue *temp_rvalue = rvalue->GetValueAtIndex(value_idx);
						
						if (temp_lvalue->Type() != ScriptValueType::kValueObject)
							SLIM_TERMINATION << "ERROR (ScriptInterpreter::_AssignRValueToLValue): internal error: dot operator used with non-object value." << slim_terminate();
						
						static_cast<ScriptValue_Object *>(temp_lvalue)->SetValueForMemberOfElements(member_name, temp_rvalue);
						
						delete temp_lvalue;
						delete temp_rvalue;
					}
				}
			}
			else
			{
				SLIM_TERMINATION << "ERROR (_AssignRValueToLValue): assignment to a subscript requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << slim_terminate();
			}
			
			break;
		}
		case TokenType::kTokenDot:
		{
			if (p_lvalue_node->children_.size() != 2)
				SLIM_TERMINATION << "ERROR (_AssignRValueToLValue): internal error (expected 2 children for '.' node)." << slim_terminate();
			
			ScriptValue *first_child_value = EvaluateNode(p_lvalue_node->children_[0]);
			ScriptValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != ScriptValueType::kValueObject)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				SLIM_TERMINATION << "ERROR (_AssignRValueToLValue): operand type " << first_child_type << " is not supported by the '.' operator." << slim_terminate();
			}
			
			ScriptASTNode *second_child_node = p_lvalue_node->children_[1];
			
			if (second_child_node->token_->token_type_ != TokenType::kTokenIdentifier)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				SLIM_TERMINATION << "ERROR (_AssignRValueToLValue): the '.' operator for x.y requires operand y to be an identifier." << slim_terminate();
			}
			
			// OK, we have <object type>.<identifier>; we can work with that
			static_cast<ScriptValue_Object *>(first_child_value)->SetValueForMemberOfElements(second_child_node->token_->token_string_, rvalue);
			break;
		}
		case TokenType::kTokenIdentifier:
		{
			if (p_lvalue_node->children_.size() != 0)
				SLIM_TERMINATION << "ERROR (_AssignRValueToLValue): internal error (expected 0 children for identifier node)." << slim_terminate();
			
			// Simple identifier; the symbol host is the global symbol table, at least for now
			global_symbols_->SetValueForSymbol(p_lvalue_node->token_->token_string_, rvalue);
			break;
		}
		default:
			SLIM_TERMINATION << "ERROR (_AssignRValueToLValue): Unexpected node token type " << token_type << "; lvalue required." << slim_terminate();
			break;
	}
}

ScriptValue *ScriptInterpreter::EvaluateNode(const ScriptASTNode *p_node)
{
	TokenType token_type = p_node->token_->token_type_;
	ScriptValue *result = nullptr;
	
	if (logging_execution_)
	{
		execution_log_ << IndentString(execution_log_indent_) << "EvaluateNode() : token ";
		p_node->PrintToken(execution_log_);
		execution_log_ << "\n";
	}
	
	switch (token_type)
	{
		case TokenType::kTokenSemicolon:	result = Evaluate_NullStatement(p_node);		break;
		case TokenType::kTokenColon:		result = Evaluate_RangeExpr(p_node);			break;
		case TokenType::kTokenLBrace:		result = Evaluate_CompoundStatement(p_node);	break;
		case TokenType::kTokenLParen:		result = Evaluate_FunctionCall(p_node);			break;
		case TokenType::kTokenLBracket:		result = Evaluate_Subset(p_node);				break;
		case TokenType::kTokenDot:			result = Evaluate_MemberRef(p_node);			break;
		case TokenType::kTokenPlus:			result = Evaluate_Plus(p_node);					break;
		case TokenType::kTokenMinus:		result = Evaluate_Minus(p_node);				break;
		case TokenType::kTokenMod:			result = Evaluate_Mod(p_node);					break;
		case TokenType::kTokenMult:			result = Evaluate_Mult(p_node);					break;
		case TokenType::kTokenExp:			result = Evaluate_Exp(p_node);					break;
		case TokenType::kTokenAnd:			result = Evaluate_And(p_node);					break;
		case TokenType::kTokenOr:			result = Evaluate_Or(p_node);					break;
		case TokenType::kTokenDiv:			result = Evaluate_Div(p_node);					break;
		case TokenType::kTokenAssign:		result = Evaluate_Assign(p_node);				break;
		case TokenType::kTokenEq:			result = Evaluate_Eq(p_node);					break;
		case TokenType::kTokenLt:			result = Evaluate_Lt(p_node);					break;
		case TokenType::kTokenLtEq:			result = Evaluate_LtEq(p_node);					break;
		case TokenType::kTokenGt:			result = Evaluate_Gt(p_node);					break;
		case TokenType::kTokenGtEq:			result = Evaluate_GtEq(p_node);					break;
		case TokenType::kTokenNot:			result = Evaluate_Not(p_node);					break;
		case TokenType::kTokenNotEq:		result = Evaluate_NotEq(p_node);				break;
		case TokenType::kTokenNumber:		result = Evaluate_Number(p_node);				break;
		case TokenType::kTokenString:		result = Evaluate_String(p_node);				break;
		case TokenType::kTokenIdentifier:	result = Evaluate_Identifier(p_node);			break;
		case TokenType::kTokenIf:			result = Evaluate_If(p_node);					break;
		case TokenType::kTokenDo:			result = Evaluate_Do(p_node);					break;
		case TokenType::kTokenWhile:		result = Evaluate_While(p_node);				break;
		case TokenType::kTokenFor:			result = Evaluate_For(p_node);					break;
		case TokenType::kTokenNext:			result = Evaluate_Next(p_node);					break;
		case TokenType::kTokenBreak:		result = Evaluate_Break(p_node);				break;
		case TokenType::kTokenReturn:		result = Evaluate_Return(p_node);				break;
		default:
			SLIM_TERMINATION << "ERROR (EvaluateNode): Unexpected node token type " << token_type << "." << slim_terminate();
			break;
	}
	
	if (!result)
		SLIM_TERMINATION << "ERROR (EvaluateNode): nullptr returned from evaluation of token type " << token_type << "." << slim_terminate();
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_NullStatement(const ScriptASTNode *p_node)
{
#pragma unused(p_node)
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_NullStatement() entered\n";
	
	if (p_node->children_.size() != 0)
		SLIM_TERMINATION << "ERROR (Evaluate_NullStatement): internal error (expected 0 children)." << slim_terminate();
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_NullStatement() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_CompoundStatement(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_CompoundStatement() entered\n";
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	for (ScriptASTNode *child_node : p_node->children_)
	{
		if (result->IsTemporary()) delete result;
		
		result = EvaluateNode(child_node);
		
		// a next, break, or return makes us exit immediately, out to the (presumably enclosing) loop evaluator
		if (next_statement_hit_ || break_statement_hit_ || return_statement_hit_)
			break;
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_CompoundStatement() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_RangeExpr(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_RangeExpr() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	bool too_wide = false;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): operand type " << first_child_type << " is not supported by the ':' operator." << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): operand type " << second_child_type << " is not supported by the ':' operator." << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != 1) || (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): operands of the ':' operator must have size() == 1." << slim_terminate();
	}
	
	// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
	bool underflow = false;
	
	if ((first_child_type == ScriptValueType::kValueInt) && (second_child_type == ScriptValueType::kValueInt))
	{
		int64_t first_int = first_child_value->IntAtIndex(0);
		int64_t second_int = second_child_value->IntAtIndex(0);
		
		ScriptValue_Int *int_result = new ScriptValue_Int();
		
		if (first_int <= second_int)
		{
			if (second_int - first_int >= 100000)
				too_wide = true;
			else
				for (int64_t range_index = first_int; range_index <= second_int; ++range_index)
					int_result->PushInt(range_index);
		}
		else
		{
			if (first_int - second_int >= 100000)
				too_wide = true;
			else
				for (int64_t range_index = first_int; range_index >= second_int; --range_index)
					int_result->PushInt(range_index);
		}
		
		result = int_result;
	}
	else
	{
		double first_float = first_child_value->FloatAtIndex(0);
		double second_float = second_child_value->FloatAtIndex(0);
		
		ScriptValue_Float *float_result = new ScriptValue_Float();
		
		if (first_float <= second_float)
		{
			if (second_float - first_float >= 100000)
				too_wide = true;
			else
				for (double range_index = first_float; range_index <= second_float; )
				{
					float_result->PushFloat(range_index);
					
					// be careful not to hang due to underflow
					double next_index = range_index + 1.0;
					
					if (next_index == range_index)
					{
						underflow = true;
						break;
					}
					
					range_index = next_index;
				}
		}
		else
		{
			if (first_float - second_float >= 100000)
				too_wide = true;
			else
				for (double range_index = first_float; range_index >= second_float; )
				{
					float_result->PushFloat(range_index);
					
					// be careful not to hang due to underflow
					double next_index = range_index - 1.0;
					
					if (next_index == range_index)
					{
						underflow = true;
						break;
					}
					
					range_index = next_index;
				}
		}
		
		result = float_result;
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (underflow)
	{
		if (result->IsTemporary()) delete result;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): the floating-point range could not be constructed due to underflow." << slim_terminate();
	}
	if (too_wide)
	{
		if (result->IsTemporary()) delete result;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): a range with more than 100000 entries cannot be constructed." << slim_terminate();
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_RangeExpr() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_FunctionCall(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_FunctionCall() entered\n";
	
	ScriptValue *result = nullptr;
	
	// We do not evaluate the function name node (our first child) to get a function object; there is no such type in
	// SLiMScript for now.  Instead, we extract the identifier name directly from the node and work with it.  If the
	// node is an identifier, it is a function call; if it is a dot operator, it is a method call; other constructs
	// are illegal since expressions cannot evaluate to function objects, there being no function objects in SLiMscript.
	ScriptASTNode *function_name_node = p_node->children_[0];
	TokenType function_name_token_type = function_name_node->token_->token_type_;
	
	string function_name;
	ScriptValue_Object *method_object = nullptr;
	
	if (function_name_token_type == TokenType::kTokenIdentifier)
	{
		// OK, we have <identifier>(...); that's a well-formed function call
		function_name = function_name_node->token_->token_string_;
	}
	else if (function_name_token_type == TokenType::kTokenDot)
	{
		if (function_name_node->children_.size() != 2)
			SLIM_TERMINATION << "ERROR (Evaluate_FunctionCall): internal error (expected 2 children for '.' node)." << slim_terminate();
		
		ScriptValue *first_child_value = EvaluateNode(function_name_node->children_[0]);
		ScriptValueType first_child_type = first_child_value->Type();
		
		if (first_child_type != ScriptValueType::kValueObject)
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_FunctionCall): operand type " << first_child_type << " is not supported by the '.' operator." << slim_terminate();
		}
		
		ScriptASTNode *second_child_node = function_name_node->children_[1];
		
		if (second_child_node->token_->token_type_ != TokenType::kTokenIdentifier)
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_FunctionCall): the '.' operator for x.y requires operand y to be an identifier." << slim_terminate();
		}
		
		// OK, we have <object type>.<identifier>(...); that's a well-formed method call
		function_name = second_child_node->token_->token_string_;
		method_object = static_cast<ScriptValue_Object *>(first_child_value);	// guaranteed by the Type() call above
	}
	else
	{
		SLIM_TERMINATION << "ERROR (Evaluate_FunctionCall): type " << function_name_token_type << " is not supported by the '()' operator (illegal operand for a function call operation)." << slim_terminate();
	}
	
	// Evaluate all arguments; note this occurs before the function call itself is evaluated at all
	vector<ScriptValue*> arguments;
	
	for (auto child_iter = p_node->children_.begin() + 1; child_iter != p_node->children_.end(); ++child_iter)
	{
		ScriptASTNode *child = *child_iter;
		
		if (child->token_->token_type_ == TokenType::kTokenComma)
		{
			// a child with token type kTokenComma is an argument list node; we need to take its children and evaluate them
			for (auto arg_list_iter = child->children_.begin(); arg_list_iter != child->children_.end(); ++arg_list_iter)
				arguments.push_back(EvaluateNode(*arg_list_iter));
		}
		else
		{
			// all other children get evaluated, and the results added to the arguments vector
			arguments.push_back(EvaluateNode(child));
		}
	}
	
	// We offload the actual work to ExecuteMethodCall() / ExecuteFunctionCall() to keep things simple here
	if (method_object)
		result = ExecuteMethodCall(method_object, function_name, arguments, execution_output_);
	else
		result = ExecuteFunctionCall(function_name, arguments, execution_output_);
	
	// And now we can free the arguments
	for (auto arg_iter = arguments.begin(); arg_iter != arguments.end(); ++arg_iter)
	{
		ScriptValue *arg = *arg_iter;
		
		if (arg->IsTemporary()) delete arg;
	}
	
	// And if it was a method call, we can free the method object now, too
	if (method_object && method_object->IsTemporary()) delete method_object;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_FunctionCall() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Subset(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Subset() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Subset): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if (first_child_type == ScriptValueType::kValueNULL)
	{
		// Any subscript of NULL returns NULL
		result = new ScriptValue_NULL();
		
		if (first_child_value->IsTemporary()) delete first_child_value;
	}
	else
	{
		ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
		ScriptValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat) && (second_child_type != ScriptValueType::kValueLogical) && (second_child_type != ScriptValueType::kValueNULL))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Subset): index operand type " << second_child_type << " is not supported by the '[]' operator." << slim_terminate();
		}
		
		// OK, we can definitely do this subset, so allocate the result value based on the type of the first operand
		result = first_child_value->NewMatchingType();
		
		// Any subscript using NULL returns an empty vector
		if (first_child_type != ScriptValueType::kValueNULL)
		{
			int first_child_count = first_child_value->Count();
			int second_child_count = second_child_value->Count();
			
			if (second_child_type == ScriptValueType::kValueLogical)
			{
				// Subsetting with a logical vector means the vectors must match in length; indices with a T value will be taken
				if (first_child_count != second_child_count)
				{
					if (first_child_value->IsTemporary()) delete first_child_value;
					if (second_child_value->IsTemporary()) delete second_child_value;
					if (result->IsTemporary()) delete result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Subset): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << slim_terminate();
				}
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					bool bool_value = second_child_value->LogicalAtIndex(value_idx);
					
					if (bool_value)
						result->PushValueFromIndexOfScriptValue(value_idx, first_child_value);
				}
			}
			else
			{
				// Subsetting with a int/float vector can use a vector of any length; the specific indices referenced will be taken
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					int64_t index_value = second_child_value->IntAtIndex(value_idx);
					
					if ((index_value < 0) || (index_value >= first_child_count))
					{
						if (first_child_value->IsTemporary()) delete first_child_value;
						if (second_child_value->IsTemporary()) delete second_child_value;
						if (result->IsTemporary()) delete result;
						
						SLIM_TERMINATION << "ERROR (Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << slim_terminate();
					}
					else
						result->PushValueFromIndexOfScriptValue((int)index_value, first_child_value);
				}
			}
		}
		
		// Free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Subset() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_MemberRef(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_MemberRef() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_MemberRef): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if (first_child_type != ScriptValueType::kValueObject)
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_MemberRef): operand type " << first_child_type << " is not supported by the '.' operator." << slim_terminate();
	}
	
	ScriptASTNode *second_child_node = p_node->children_[1];
	
	if (second_child_node->token_->token_type_ != TokenType::kTokenIdentifier)
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_MemberRef): the '.' operator for x.y requires operand y to be an identifier." << slim_terminate();
	}
	
	string member_name = second_child_node->token_->token_string_;
	result = static_cast<ScriptValue_Object *>(first_child_value)->GetValueForMemberOfElements(member_name);
	
	// free our operand
	if (first_child_value->IsTemporary()) delete first_child_value;
	
	// check result; this should never happen, since GetValueForMember should check
	if (!result)
		SLIM_TERMINATION << "ERROR (Evaluate_MemberRef): undefined member " << member_name << "." << slim_terminate();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_MemberRef() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Plus(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Plus() entered\n";
	
	if ((p_node->children_.size() != 1) && (p_node->children_.size() != 2))
		SLIM_TERMINATION << "ERROR (Evaluate_Plus): internal error (expected 1 or 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if (p_node->children_.size() == 1)
	{
		// unary plus is a no-op, but legal only for numeric types
		if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Plus): operand type " << first_child_type << " is not supported by the unary '+' operator." << slim_terminate();
		}
		
		result = first_child_value;
		
		// don't free our operand, because result points to it
	}
	else
	{
		// binary plus is legal either between two numeric types, or between a string and any other operand
		ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
		ScriptValueType second_child_type = second_child_value->Type();
		
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
		
		if ((first_child_type == ScriptValueType::kValueString) || (second_child_type == ScriptValueType::kValueString))
		{
			// If either operand is a string, then we are doing string concatenation, with promotion to strings if needed
			ScriptValue_String *string_result = new ScriptValue_String();
			
			if (first_child_count == second_child_count)
			{
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					string_result->PushString(first_child_value->StringAtIndex(value_index) + second_child_value->StringAtIndex(value_index));
			}
			else if (first_child_count == 1)
			{
				string singleton_int = first_child_value->StringAtIndex(0);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					string_result->PushString(singleton_int + second_child_value->StringAtIndex(value_index));
			}
			else if (second_child_count == 1)
			{
				string singleton_int = second_child_value->StringAtIndex(0);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					string_result->PushString(first_child_value->StringAtIndex(value_index) + singleton_int);
			}
			
			result = string_result;
		}
		else if ((first_child_type == ScriptValueType::kValueInt) && (second_child_type == ScriptValueType::kValueInt))
		{
			ScriptValue_Int *int_result = new ScriptValue_Int();
			
			if (first_child_count == second_child_count)
			{
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					int_result->PushInt(first_child_value->IntAtIndex(value_index) + second_child_value->IntAtIndex(value_index));
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					int_result->PushInt(singleton_int + second_child_value->IntAtIndex(value_index));
			}
			else if (second_child_count == 1)
			{
				int64_t singleton_int = second_child_value->IntAtIndex(0);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					int_result->PushInt(first_child_value->IntAtIndex(value_index) + singleton_int);
			}
			
			result = int_result;
		}
		else
		{
			if (((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat)) || ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat)))
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				if (second_child_value->IsTemporary()) delete second_child_value;
				
				SLIM_TERMINATION << "ERROR (Evaluate_Plus): the combination of operand types " << first_child_type << " and " << second_child_type << " is not supported by the binary '+' operator." << slim_terminate();
			}
			
			ScriptValue_Float *float_result = new ScriptValue_Float();
			
			if (first_child_count == second_child_count)
			{
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index) + second_child_value->FloatAtIndex(value_index));
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					float_result->PushFloat(singleton_float + second_child_value->FloatAtIndex(value_index));
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index) + singleton_float);
			}
			
			result = float_result;
		}
		
		// free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Plus() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Minus(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Minus() entered\n";
	
	if ((p_node->children_.size() != 1) && (p_node->children_.size() != 2))
		SLIM_TERMINATION << "ERROR (Evaluate_Minus): internal error (expected 1 or 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Minus): operand type " << first_child_type << " is not supported by the '-' operator." << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	
	if (p_node->children_.size() == 1)
	{
		// unary minus
		if (first_child_type == ScriptValueType::kValueInt)
		{
			ScriptValue_Int *int_result = new ScriptValue_Int();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				int_result->PushInt(-first_child_value->IntAtIndex(value_index));
			
			result = int_result;
		}
		else
		{
			ScriptValue_Float *float_result = new ScriptValue_Float();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(-first_child_value->FloatAtIndex(value_index));
			
			result = float_result;
		}
		
		// free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
	}
	else
	{
		// binary minus
		ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
		ScriptValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Minus): operand type " << second_child_type << " is not supported by the '-' operator." << slim_terminate();
		}
		
		int second_child_count = second_child_value->Count();
		
		if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Minus): the '-' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
		
		if ((first_child_type == ScriptValueType::kValueInt) && (second_child_type == ScriptValueType::kValueInt))
		{
			ScriptValue_Int *int_result = new ScriptValue_Int();
			
			if (first_child_count == second_child_count)
			{
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					int_result->PushInt(first_child_value->IntAtIndex(value_index) - second_child_value->IntAtIndex(value_index));
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					int_result->PushInt(singleton_int - second_child_value->IntAtIndex(value_index));
			}
			else if (second_child_count == 1)
			{
				int64_t singleton_int = second_child_value->IntAtIndex(0);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					int_result->PushInt(first_child_value->IntAtIndex(value_index) - singleton_int);
			}
			
			result = int_result;
		}
		else
		{
			ScriptValue_Float *float_result = new ScriptValue_Float();
			
			if (first_child_count == second_child_count)
			{
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index) - second_child_value->FloatAtIndex(value_index));
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					float_result->PushFloat(singleton_float - second_child_value->FloatAtIndex(value_index));
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index) - singleton_float);
			}
			
			result = float_result;
		}
		
		// free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Minus() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Mod(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Mod() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): operand type " << first_child_type << " is not supported by the '%' operator." << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): operand type " << second_child_type << " is not supported by the '%' operator." << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): the '%' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
	}
	
	/*
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, they will need to do float division and then use floor() and asInteger().  Alternatively, we could provide a separate
	// operator for integer division, as R does.  However, the definition of integer division is tricky and contested; best to let people do their own.
	// This decision applies also to modulo, for consistency.
	
	if ((first_child_type == ScriptValueType::kValueInt) && (second_child_type == ScriptValueType::kValueInt))
	{
		// integer modulo has to check for division by zero and throw, otherwise we crash
		ScriptValue_Int *int_result = new ScriptValue_Int();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int64_t divisor = second_child_value->IntAtIndex(value_index);
				
				if (divisor == 0)
				{
					if (first_child_value->IsTemporary()) delete first_child_value;
					if (second_child_value->IsTemporary()) delete second_child_value;
					if (int_result->IsTemporary()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Mod): integer modulo by zero." << slim_terminate();
				}
				
				int_result->PushInt(first_child_value->IntAtIndex(value_index) % divisor);
			}
		}
		else if (first_child_count == 1)
		{
			int64_t singleton_int = first_child_value->IntAtIndex(0);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int64_t divisor = second_child_value->IntAtIndex(value_index);
				
				if (divisor == 0)
				{
					if (first_child_value->IsTemporary()) delete first_child_value;
					if (second_child_value->IsTemporary()) delete second_child_value;
					if (int_result->IsTemporary()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Mod): integer modulo by zero." << slim_terminate();
				}
				
				int_result->PushInt(singleton_int % divisor);
			}
		}
		else if (second_child_count == 1)
		{
			int64_t singleton_int = second_child_value->IntAtIndex(0);
			
			if (singleton_int == 0)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				if (second_child_value->IsTemporary()) delete second_child_value;
				if (int_result->IsTemporary()) delete int_result;
				
				SLIM_TERMINATION << "ERROR (Evaluate_Mod): integer modulo by zero." << slim_terminate();
			}
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				int_result->PushInt(first_child_value->IntAtIndex(value_index) % singleton_int);
		}
		
		result = int_result;
	}
	else
	*/
	{
		// floating-point modulo by zero is safe; it will produce an NaN, following IEEE as implemented by C++
		ScriptValue_Float *float_result = new ScriptValue_Float();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(fmod(first_child_value->FloatAtIndex(value_index), second_child_value->FloatAtIndex(value_index)));
		}
		else if (first_child_count == 1)
		{
			double singleton_float = first_child_value->FloatAtIndex(0);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->PushFloat(fmod(singleton_float, second_child_value->FloatAtIndex(value_index)));
		}
		else if (second_child_count == 1)
		{
			double singleton_float = second_child_value->FloatAtIndex(0);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(fmod(first_child_value->FloatAtIndex(value_index), singleton_float));
		}
		
		result = float_result;
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Mod() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Mult(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Mult() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): operand type " << first_child_type << " is not supported by the '*' operator." << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): operand type " << second_child_type << " is not supported by the '*' operator." << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if (first_child_count == second_child_count)
	{
		// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
		if ((first_child_type == ScriptValueType::kValueInt) && (second_child_type == ScriptValueType::kValueInt))
		{
			ScriptValue_Int *int_result = new ScriptValue_Int();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				int_result->PushInt(first_child_value->IntAtIndex(value_index) * second_child_value->IntAtIndex(value_index));
			
			result = int_result;
		}
		else
		{
			ScriptValue_Float *float_result = new ScriptValue_Float();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(first_child_value->FloatAtIndex(value_index) * second_child_value->FloatAtIndex(value_index));
			
			result = float_result;
		}
	}
	else if ((first_child_count == 1) || (second_child_count == 1))
	{
		ScriptValue *one_count_child = ((first_child_count == 1) ? first_child_value : second_child_value);
		ScriptValue *any_count_child = ((first_child_count == 1) ? second_child_value : first_child_value);
		int any_count = ((first_child_count == 1) ? second_child_count : first_child_count);
		
		// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
		if ((first_child_type == ScriptValueType::kValueInt) && (second_child_type == ScriptValueType::kValueInt))
		{
			int64_t singleton_int = one_count_child->IntAtIndex(0);
			ScriptValue_Int *int_result = new ScriptValue_Int();
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				int_result->PushInt(any_count_child->IntAtIndex(value_index) * singleton_int);
			
			result = int_result;
		}
		else
		{
			double singleton_float = one_count_child->FloatAtIndex(0);
			ScriptValue_Float *float_result = new ScriptValue_Float();
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				float_result->PushFloat(any_count_child->FloatAtIndex(value_index) * singleton_float);
			
			result = float_result;
		}
	}
	else
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): the '*' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Mult() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Div(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Div() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Div): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Div): operand type " << first_child_type << " is not supported by the '/' operator." << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Div): operand type " << second_child_type << " is not supported by the '/' operator." << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Div): the '/' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
	}
	
	/*
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, they will need to do float division and then use floor() and asInteger().  Alternatively, we could provide a separate
	// operator for integer division, as R does.  However, the definition of integer division is tricky and contested; best to let people do their own.
	 
	if ((first_child_type == ScriptValueType::kValueInt) && (second_child_type == ScriptValueType::kValueInt))
	{
		// integer division has to check for division by zero and throw, otherwise we crash
		ScriptValue_Int *int_result = new ScriptValue_Int();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int64_t divisor = second_child_value->IntAtIndex(value_index);
				
				if (divisor == 0)
				{
					if (first_child_value->IsTemporary()) delete first_child_value;
					if (second_child_value->IsTemporary()) delete second_child_value;
					if (int_result->IsTemporary()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Div): integer divide by zero." << slim_terminate();
				}
				
				int_result->PushInt(first_child_value->IntAtIndex(value_index) / divisor);
			}
		}
		else if (first_child_count == 1)
		{
			int64_t singleton_int = first_child_value->IntAtIndex(0);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int64_t divisor = second_child_value->IntAtIndex(value_index);
				
				if (divisor == 0)
				{
					if (first_child_value->IsTemporary()) delete first_child_value;
					if (second_child_value->IsTemporary()) delete second_child_value;
					if (int_result->IsTemporary()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Div): integer divide by zero." << slim_terminate();
				}
				
				int_result->PushInt(singleton_int / divisor);
			}
		}
		else if (second_child_count == 1)
		{
			int64_t singleton_int = second_child_value->IntAtIndex(0);
			
			if (singleton_int == 0)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				if (second_child_value->IsTemporary()) delete second_child_value;
				if (int_result->IsTemporary()) delete int_result;
				
				SLIM_TERMINATION << "ERROR (Evaluate_Div): integer divide by zero." << slim_terminate();
			}
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				int_result->PushInt(first_child_value->IntAtIndex(value_index) / singleton_int);
		}
		
		result = int_result;
	}
	else
	*/
	{
		// floating-point division by zero is safe; it will produce an infinity, following IEEE as implemented by C++
		ScriptValue_Float *float_result = new ScriptValue_Float();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(first_child_value->FloatAtIndex(value_index) / second_child_value->FloatAtIndex(value_index));
		}
		else if (first_child_count == 1)
		{
			double singleton_float = first_child_value->FloatAtIndex(0);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->PushFloat(singleton_float / second_child_value->FloatAtIndex(value_index));
		}
		else if (second_child_count == 1)
		{
			double singleton_float = second_child_value->FloatAtIndex(0);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(first_child_value->FloatAtIndex(value_index) / singleton_float);
		}
		
		result = float_result;
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Div() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Exp(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Exp() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): operand type " << first_child_type << " is not supported by the '^' operator." << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): operand type " << second_child_type << " is not supported by the '^' operator." << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): the '^' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
	}
	
	// Exponentiation always produces a float result; the user can cast back to integer if they really want
	ScriptValue_Float *result = new ScriptValue_Float();
	
	if (first_child_count == second_child_count)
	{
		for (int value_index = 0; value_index < first_child_count; ++value_index)
			result->PushFloat(pow(first_child_value->FloatAtIndex(value_index), second_child_value->FloatAtIndex(value_index)));
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0);
		
		for (int value_index = 0; value_index < second_child_count; ++value_index)
			result->PushFloat(pow(singleton_float, second_child_value->FloatAtIndex(value_index)));
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0);
		
		for (int value_index = 0; value_index < first_child_count; ++value_index)
			result->PushFloat(pow(first_child_value->FloatAtIndex(value_index), singleton_float));
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Exp() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_And(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_And() entered\n";
	
	if (p_node->children_.size() < 2)
		SLIM_TERMINATION << "ERROR (Evaluate_And): internal error (expected 2+ children)." << slim_terminate();
	
	ScriptValue_Logical *result = nullptr;
	int result_count = 0;
	
	for (ScriptASTNode *child_node : p_node->children_)
	{
		ScriptValue *child_result = EvaluateNode(child_node);
		ScriptValueType child_type = child_result->Type();
		
		if ((child_type != ScriptValueType::kValueLogical) && (child_type != ScriptValueType::kValueString) && (child_type != ScriptValueType::kValueInt) && (child_type != ScriptValueType::kValueFloat))
		{
			if (child_result->IsTemporary()) delete child_result;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_And): operand type " << child_type << " is not supported by the '&' operator." << slim_terminate();
		}
		
		int child_count = child_result->Count();
		
		if (!result)
		{
			// if this is our first operand, we just clone it and move on to the next operand
			result = new ScriptValue_Logical();
			result_count = child_count;
			
			for (int value_index = 0; value_index < child_count; ++value_index)
				result->PushLogical(child_result->LogicalAtIndex(value_index));
			
			continue;
		}
		else
		{
			// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
			if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
			{
				if (child_result->IsTemporary()) delete child_result;
				if (result->IsTemporary()) delete result;
				
				SLIM_TERMINATION << "ERROR (Evaluate_And): operands to the '&' operator are not compatible in size()." << slim_terminate();
			}
			
			if (child_count == 1)
			{
				// if child_bool is T, it has no effect on result; if it is F, it turns result to all F
				bool child_bool = child_result->LogicalAtIndex(0);
				
				if (!child_bool)
					for (int value_index = 0; value_index < result_count; ++value_index)
						result->SetLogicalAtIndex(value_index, false);
			}
			else if (result_count == 1)
			{
				// we had a one-length result vector, but now we need to upscale it to match child_result
				bool result_bool = result->LogicalAtIndex(0);
				
				if (result->IsTemporary()) delete result;
				result = new ScriptValue_Logical();
				result_count = child_count;
				
				if (result_bool)
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->PushLogical(child_result->LogicalAtIndex(value_index));
				else
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->PushLogical(false);
			}
			else
			{
				// result and child_result are both != 1 length, so we match them one to one, and if child_result is F we turn result to F
				for (int value_index = 0; value_index < result_count; ++value_index)
					if (!child_result->LogicalAtIndex(value_index))
						result->SetLogicalAtIndex(value_index, false);
			}
		}
		
		// free our operand
		if (child_result->IsTemporary()) delete child_result;
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_And() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Or(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Or() entered\n";
	
	if (p_node->children_.size() < 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Or): internal error (expected 2+ children)." << slim_terminate();
	
	ScriptValue_Logical *result = nullptr;
	int result_count = 0;
	
	for (ScriptASTNode *child_node : p_node->children_)
	{
		ScriptValue *child_result = EvaluateNode(child_node);
		ScriptValueType child_type = child_result->Type();
		
		if ((child_type != ScriptValueType::kValueLogical) && (child_type != ScriptValueType::kValueString) && (child_type != ScriptValueType::kValueInt) && (child_type != ScriptValueType::kValueFloat))
		{
			if (child_result->IsTemporary()) delete child_result;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Or): operand type " << child_type << " is not supported by the '|' operator." << slim_terminate();
		}
		
		int child_count = child_result->Count();
		
		if (!result)
		{
			// if this is our first operand, we just clone it and move on to the next operand
			result = new ScriptValue_Logical();
			result_count = child_count;
			
			for (int value_index = 0; value_index < child_count; ++value_index)
				result->PushLogical(child_result->LogicalAtIndex(value_index));
		}
		else
		{
			// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
			if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
			{
				if (child_result->IsTemporary()) delete child_result;
				if (result->IsTemporary()) delete result;
				
				SLIM_TERMINATION << "ERROR (Evaluate_Or): operands to the '|' operator are not compatible in size()." << slim_terminate();
			}
			
			if (child_count == 1)
			{
				// if child_bool is F, it has no effect on result; if it is T, it turns result to all T
				bool child_bool = child_result->LogicalAtIndex(0);
				
				if (child_bool)
					for (int value_index = 0; value_index < result_count; ++value_index)
						result->SetLogicalAtIndex(value_index, true);
			}
			else if (result_count == 1)
			{
				// we had a one-length result vector, but now we need to upscale it to match child_result
				bool result_bool = result->LogicalAtIndex(0);
				
				if (result->IsTemporary()) delete result;
				result = new ScriptValue_Logical();
				result_count = child_count;
				
				if (result_bool)
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->PushLogical(true);
				else
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->PushLogical(child_result->LogicalAtIndex(value_index));
			}
			else
			{
				// result and child_result are both != 1 length, so we match them one to one, and if child_result is T we turn result to T
				for (int value_index = 0; value_index < result_count; ++value_index)
					if (child_result->LogicalAtIndex(value_index))
						result->SetLogicalAtIndex(value_index, true);
			}
		}
		
		// free our operand
		if (child_result->IsTemporary()) delete child_result;
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Or() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Not(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Not() entered\n";
	
	if (p_node->children_.size() != 1)
		SLIM_TERMINATION << "ERROR (Evaluate_Not): internal error (expected 1 child)." << slim_terminate();
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueLogical) && (first_child_type != ScriptValueType::kValueString) && (first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Not): operand type " << first_child_type << " is not supported by the '!' operator." << slim_terminate();
	}
	
	ScriptValue_Logical *result = new ScriptValue_Logical();
	
	int first_child_count = first_child_value->Count();
	
	for (int value_index = 0; value_index < first_child_count; ++value_index)
		result->PushLogical(!first_child_value->LogicalAtIndex(value_index));
	
	// free our operand
	if (first_child_value->IsTemporary()) delete first_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Not() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Assign(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Assign() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Assign): internal error (expected 2 children)." << slim_terminate();
	
	ScriptASTNode *lvalue_node = p_node->children_[0];
	ScriptValue *rvalue = EvaluateNode(p_node->children_[1]);
	
	_AssignRValueToLValue(rvalue, lvalue_node);		// disposes of rvalue somehow
	
	// by design, assignment does not yield a usable value; instead it produces NULL – this prevents the error "if (x = 3) ..."
	// since the condition is NULL and will raise; the loss of legitimate uses of "if (x = 3)" seems a small price to pay
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	// free our operand
	if (rvalue->IsTemporary()) delete rvalue;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Assign() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Eq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Eq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Eq): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue_Logical *result = new ScriptValue_Logical;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	// if either operand is NULL (including if both are), we return logical(0)
	if ((first_child_type != ScriptValueType::kValueNULL) && (second_child_type != ScriptValueType::kValueNULL))
	{
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, value_index);
				
				result->PushLogical(compare_result == 0);
			}
		}
		else if (first_child_count == 1)
		{
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, 0, second_child_value, value_index);
				
				result->PushLogical(compare_result == 0);
			}
		}
		else if (second_child_count == 1)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, 0);
				
				result->PushLogical(compare_result == 0);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Eq): the '==' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Eq() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Lt(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Lt() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Lt): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue_Logical *result = new ScriptValue_Logical;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == ScriptValueType::kValueObject) || (second_child_type == ScriptValueType::kValueObject))
		SLIM_TERMINATION << "ERROR (Evaluate_Lt): the '<' operator cannot be used with type object." << slim_terminate();
	
	// if either operand is NULL (including if both are), we return logical(0)
	if ((first_child_type != ScriptValueType::kValueNULL) && (second_child_type != ScriptValueType::kValueNULL))
	{
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, value_index);
				
				result->PushLogical(compare_result == -1);
			}
		}
		else if (first_child_count == 1)
		{
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, 0, second_child_value, value_index);
				
				result->PushLogical(compare_result == -1);
			}
		}
		else if (second_child_count == 1)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, 0);
				
				result->PushLogical(compare_result == -1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Lt): the '<' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Lt() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_LtEq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_LtEq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_LtEq): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue_Logical *result = new ScriptValue_Logical;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == ScriptValueType::kValueObject) || (second_child_type == ScriptValueType::kValueObject))
		SLIM_TERMINATION << "ERROR (Evaluate_Lt): the '<=' operator cannot be used with type object." << slim_terminate();
	
	// if either operand is NULL (including if both are), we return logical(0)
	if ((first_child_type != ScriptValueType::kValueNULL) && (second_child_type != ScriptValueType::kValueNULL))
	{
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, value_index);
				
				result->PushLogical(compare_result != 1);
			}
		}
		else if (first_child_count == 1)
		{
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, 0, second_child_value, value_index);
				
				result->PushLogical(compare_result != 1);
			}
		}
		else if (second_child_count == 1)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, 0);
				
				result->PushLogical(compare_result != 1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_LtEq): the '<=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_LtEq() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Gt(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Gt() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Gt): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue_Logical *result = new ScriptValue_Logical;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == ScriptValueType::kValueObject) || (second_child_type == ScriptValueType::kValueObject))
		SLIM_TERMINATION << "ERROR (Evaluate_Lt): the '>' operator cannot be used with type object." << slim_terminate();
	
	// if either operand is NULL (including if both are), we return logical(0)
	if ((first_child_type != ScriptValueType::kValueNULL) && (second_child_type != ScriptValueType::kValueNULL))
	{
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, value_index);
				
				result->PushLogical(compare_result == 1);
			}
		}
		else if (first_child_count == 1)
		{
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, 0, second_child_value, value_index);
				
				result->PushLogical(compare_result == 1);
			}
		}
		else if (second_child_count == 1)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, 0);
				
				result->PushLogical(compare_result == 1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Gt): the '>' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Gt() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_GtEq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_GtEq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_GtEq): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue_Logical *result = new ScriptValue_Logical;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == ScriptValueType::kValueObject) || (second_child_type == ScriptValueType::kValueObject))
		SLIM_TERMINATION << "ERROR (Evaluate_Lt): the '>=' operator cannot be used with type object." << slim_terminate();
	
	// if either operand is NULL (including if both are), we return logical(0)
	if ((first_child_type != ScriptValueType::kValueNULL) && (second_child_type != ScriptValueType::kValueNULL))
	{
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, value_index);
				
				result->PushLogical(compare_result != -1);
			}
		}
		else if (first_child_count == 1)
		{
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, 0, second_child_value, value_index);
				
				result->PushLogical(compare_result != -1);
			}
		}
		else if (second_child_count == 1)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, 0);
				
				result->PushLogical(compare_result != -1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_GtEq): the '>=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_GtEq() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_NotEq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_NotEq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_NotEq): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue_Logical *result = new ScriptValue_Logical;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	// if either operand is NULL (including if both are), we return logical(0)
	if ((first_child_type != ScriptValueType::kValueNULL) && (second_child_type != ScriptValueType::kValueNULL))
	{
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, value_index);
				
				result->PushLogical(compare_result != 0);
			}
		}
		else if (first_child_count == 1)
		{
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, 0, second_child_value, value_index);
				
				result->PushLogical(compare_result != 0);
			}
		}
		else if (second_child_count == 1)
		{
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareScriptValues(first_child_value, value_index, second_child_value, 0);
				
				result->PushLogical(compare_result != 0);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			if (result->IsTemporary()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_NotEq): the '!=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << slim_terminate();
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_NotEq() : return == " << *result << "\n";
	
	return result;
}

// A utility static method for getting an int for a number node outside of a ScriptInterpreter session
int64_t ScriptInterpreter::IntForNumberToken(const ScriptToken *p_token)
{
	if (p_token->token_type_ != TokenType::kTokenNumber)
		SLIM_TERMINATION << "ERROR (IntForNumberToken): internal error (expected kTokenNumber)." << slim_terminate();
	
	string number_string = p_token->token_string_;
	
	// This needs to use the same criteria as Evaluate_Number() below; it raises if the number is a float.
	if ((number_string.find('.') != string::npos) || (number_string.find('-') != string::npos))
	{
		SLIM_TERMINATION << "ERROR (IntForNumberToken): an integer is required." << slim_terminate();
		return 0;
	}
	else if ((number_string.find('e') != string::npos) || (number_string.find('E') != string::npos))
		return static_cast<int64_t>(strtod(number_string.c_str(), nullptr));			// has an exponent
	else
		return strtoll(number_string.c_str(), NULL, 10);								// plain integer
}

ScriptValue *ScriptInterpreter::Evaluate_Number(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Number() entered\n";
	
	if (p_node->children_.size() > 0)
		SLIM_TERMINATION << "ERROR (Evaluate_Number): internal error (expected 0 children)." << slim_terminate();
	
	string number_string = p_node->token_->token_string_;
	
	// At this point, we have to decide whether to instantiate an int or a float.  If it has a decimal point or
	// a minus sign in it (which would be in the exponent), we'll make a float.  Otherwise, we'll make an int.
	// This might need revision in future; 1.2e3 could be an int, for example.  However, it is an ambiguity in
	// the syntax that will never be terribly comfortable; it's the price we pay for wanting ints to be
	// expressable using scientific notation.
	ScriptValue *result = nullptr;
	
	if ((number_string.find('.') != string::npos) || (number_string.find('-') != string::npos))
		result = new ScriptValue_Float(strtod(number_string.c_str(), nullptr));							// requires a float
	else if ((number_string.find('e') != string::npos) || (number_string.find('E') != string::npos))
		result = new ScriptValue_Int(static_cast<int64_t>(strtod(number_string.c_str(), nullptr)));			// has an exponent
	else
		result = new ScriptValue_Int(strtoll(number_string.c_str(), nullptr, 10));						// plain integer
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Number() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_String(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_String() entered\n";
	
	if (p_node->children_.size() > 0)
		SLIM_TERMINATION << "ERROR (Evaluate_String): internal error (expected 0 children)." << slim_terminate();
	
	ScriptValue *result = new ScriptValue_String(p_node->token_->token_string_);
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_String() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Identifier(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Identifier() entered\n";
	
	if (p_node->children_.size() > 0)
		SLIM_TERMINATION << "ERROR (Evaluate_Identifier): internal error (expected 0 children)." << slim_terminate();
	
	string identifier_string = p_node->token_->token_string_;
	ScriptValue *result = global_symbols_->GetValueForSymbol(identifier_string);
	
	// check result; this should never happen, since GetValueForSymbol should check
	if (!result)
		SLIM_TERMINATION << "ERROR (Evaluate_Identifier): undefined identifier " << identifier_string << "." << slim_terminate();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Identifier() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_If(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_If() entered\n";
	
	if ((p_node->children_.size() != 2) && (p_node->children_.size() != 3))
		SLIM_TERMINATION << "ERROR (Evaluate_If): internal error (expected 2 or 3 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptASTNode *condition_node = p_node->children_[0];
	ScriptValue *condition_result = EvaluateNode(condition_node);
	
	if (condition_result->Count() == 1)
	{
		bool condition_bool = condition_result->LogicalAtIndex(0);
		
		if (condition_bool)
		{
			ScriptASTNode *true_node = p_node->children_[1];
			result = EvaluateNode(true_node);
		}
		else if (p_node->children_.size() == 3)		// has an 'else' node
		{
			ScriptASTNode *false_node = p_node->children_[2];
			result = EvaluateNode(false_node);
		}
		else										// no 'else' node, so the result is NULL
		{
			result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
		}
	}
	else
	{
		if (condition_result->IsTemporary()) delete condition_result;
		
		SLIM_TERMINATION << "ERROR (Evaluate_If): condition has size() != 1." << slim_terminate();
	}
	
	// free our operands
	if (condition_result->IsTemporary()) delete condition_result;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_If() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Do(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Do() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Do): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	do
	{
		// execute the do...while loop's statement by evaluating its node; evaluation values get thrown away
		ScriptValue *statement_value = EvaluateNode(p_node->children_[0]);
		
		// if a return statement has occurred, we pass the return value outward
		if (return_statement_hit_)
		{
			result = statement_value;
			break;
		}
		
		// otherwise, discard the return value
		if (statement_value->IsTemporary()) delete statement_value;
		
		// handle next and break statements
		if (next_statement_hit_)
			next_statement_hit_ = false;		// this is all we need to do; the rest of the function of "next" was done by Evaluate_CompoundStatement()
		
		if (break_statement_hit_)
		{
			break_statement_hit_ = false;
			break;							// break statements, on the other hand, get handled additionally by a break from our loop here
		}
		
		// test the loop condition
		ScriptASTNode *condition_node = p_node->children_[1];
		ScriptValue *condition_result = EvaluateNode(condition_node);
		
		if (condition_result->Count() == 1)
		{
			bool condition_bool = condition_result->LogicalAtIndex(0);
			
			if (condition_result->IsTemporary()) delete condition_result;
			
			if (!condition_bool)
				break;
		}
		else
		{
			if (condition_result->IsTemporary()) delete condition_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Do): condition has size() != 1." << slim_terminate();
		}
	}
	while (true);
	
	if (!result)
		result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Do() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_While(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_While() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_While): internal error (expected 2 children)." << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	while (true)
	{
		// test the loop condition
		ScriptASTNode *condition_node = p_node->children_[0];
		ScriptValue *condition_result = EvaluateNode(condition_node);
		
		if (condition_result->Count() == 1)
		{
			bool condition_bool = condition_result->LogicalAtIndex(0);
			
			if (condition_result->IsTemporary()) delete condition_result;
			
			if (!condition_bool)
				break;
		}
		else
		{
			if (condition_result->IsTemporary()) delete condition_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_While): condition has size() != 1." << slim_terminate();
		}
		
		// execute the while loop's statement by evaluating its node; evaluation values get thrown away
		ScriptValue *statement_value = EvaluateNode(p_node->children_[1]);
		
		// if a return statement has occurred, we pass the return value outward
		if (return_statement_hit_)
		{
			result = statement_value;
			break;
		}
		
		// otherwise, discard the return value
		if (statement_value->IsTemporary()) delete statement_value;
		
		// handle next and break statements
		if (next_statement_hit_)
			next_statement_hit_ = false;		// this is all we need to do; the rest of the function of "next" was done by Evaluate_CompoundStatement()
		
		if (break_statement_hit_)
		{
			break_statement_hit_ = false;
			break;							// break statements, on the other hand, get handled additionally by a break from our loop here
		}
	}
	
	if (!result)
		result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_While() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_For(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_For() entered\n";
	
	if (p_node->children_.size() != 3)
		SLIM_TERMINATION << "ERROR (Evaluate_For): internal error (expected 3 children)." << slim_terminate();
	
	ScriptASTNode *identifier_child = p_node->children_[0];
	
	// an lvalue is needed to assign into; for right now, we require an identifier, although that isn't quite right
	// since we should also be able to assign into a subscript, a member of a class, etc.; we need a concept of lvalue references
	if (identifier_child->token_->token_type_ != TokenType::kTokenIdentifier)
		SLIM_TERMINATION << "ERROR (Evaluate_For): the 'for' keyword requires an identifier for its left operand." << slim_terminate();
	
	string identifier_name = identifier_child->token_->token_string_;
	ScriptValue *range_value = EvaluateNode(p_node->children_[1]);
	int range_count = range_value->Count();
	ScriptValue *result = nullptr;
	
	for (int range_index = 0; range_index < range_count; ++range_index)
	{
		// set the index variable to the range value and then throw the range value away
		ScriptValue *range_value_at_index = range_value->GetValueAtIndex(range_index);
		
		global_symbols_->SetValueForSymbol(identifier_name, range_value_at_index);
		
		if (range_value_at_index->IsTemporary()) delete range_value_at_index;
		
		// execute the for loop's statement by evaluating its node; evaluation values get thrown away
		ScriptValue *statement_value = EvaluateNode(p_node->children_[2]);
		
		// if a return statement has occurred, we pass the return value outward
		if (return_statement_hit_)
		{
			result = statement_value;
			break;
		}
		
		// otherwise, discard the return value
		if (statement_value->IsTemporary()) delete statement_value;
		
		// handle next and break statements
		if (next_statement_hit_)
			next_statement_hit_ = false;		// this is all we need to do; the rest of the function of "next" was done by Evaluate_CompoundStatement()
		
		if (break_statement_hit_)
		{
			break_statement_hit_ = false;
			break;							// break statements, on the other hand, get handled additionally by a break from our loop here
		}
	}
	
	if (!result)
		result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	// free our range operand
	if (range_value->IsTemporary()) delete range_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_For() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Next(const ScriptASTNode *p_node)
{
#pragma unused(p_node)
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Next() entered\n";
	
	if (p_node->children_.size() != 0)
		SLIM_TERMINATION << "ERROR (Evaluate_Next): internal error (expected 0 children)." << slim_terminate();
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	next_statement_hit_ = true;
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Next() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Break(const ScriptASTNode *p_node)
{
#pragma unused(p_node)
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Break() entered\n";
	
	if (p_node->children_.size() != 0)
		SLIM_TERMINATION << "ERROR (Evaluate_Break): internal error (expected 0 children)." << slim_terminate();
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	break_statement_hit_ = true;
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Break() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Return(const ScriptASTNode *p_node)
{
#pragma unused(p_node)
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Return() entered\n";
	
	if (p_node->children_.size() > 1)
		SLIM_TERMINATION << "ERROR (Evaluate_Return): internal error (expected 0 or 1 children)." << slim_terminate();
	
	// set a flag in the interpreter, which will be seen by the eval methods and will cause them to return up to the top-level block immediately
	return_statement_hit_ = true;
	
	ScriptValue *result = nullptr;
	
	if (p_node->children_.size() == 0)
		result = ScriptValue_NULL::ScriptValue_NULL_Invisible();	// default return value
	else
		result = EvaluateNode(p_node->children_[0]);
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Return() : return == " << *result << "\n";
	
	return result;
}





























































