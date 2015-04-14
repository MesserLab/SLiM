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


//
//	ScriptInterpreter
//
#pragma mark ScriptInterpreter

ScriptInterpreter::ScriptInterpreter(const Script &p_script) : script_(p_script)
{
	global_symbols_ = new SymbolTable();
}

ScriptInterpreter::ScriptInterpreter(const Script &p_script, SymbolTable *p_symbols) : script_(p_script), global_symbols_(p_symbols)
{
	if (!global_symbols_)
		global_symbols_ = new SymbolTable();
}

ScriptInterpreter::~ScriptInterpreter(void)
{
	delete global_symbols_;
	global_symbols_ = nullptr;
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

const SymbolTable &ScriptInterpreter::BorrowSymbolTable(void)
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
	
	ScriptValue *result = EvaluateNode(script_.AST());
	
	// if a next or break statement was hit and was not handled by a loop, throw an error
	if (next_statement_hit_ || break_statement_hit_)
	{
		if (!result->InSymbolTable()) delete result;
		
		SLIM_TERMINATION << "ERROR (EvaluateScriptBlock): statement \"" << (next_statement_hit_ ? "next" : "break") << "\" encountered with no enclosing loop." << endl << slim_terminate();
	}
	
	// send the result of execution to our output stream
	if (!result->Invisible())
	{
		auto position = execution_output_.tellp();
		execution_output_ << *result;
		
		// ScriptValue does not put an endl on the stream, so if it emitted any output, add an endl
		if (position != execution_output_.tellp())
			execution_output_ << endl;
	}
	
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
	
	const ScriptASTNode *p_node = script_.AST();
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	for (ScriptASTNode *child_node : p_node->children_)
	{
		if (!result->InSymbolTable()) delete result;
		
		result = EvaluateNode(child_node);
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
		{
			if (!result->InSymbolTable()) delete result;
			
			SLIM_TERMINATION << "ERROR (EvaluateInterpreterBlock): statement \"" << (next_statement_hit_ ? "next" : "break") << "\" encountered with no enclosing loop." << endl << slim_terminate();
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
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "EvaluateInterpreterBlock() : return == " << *result << "\n";
	
	// if requested, send the full trace to std:cout
	if (gSLiMScriptLogEvaluation)
		std::cout << (execution_log_.str());
	
	return result;
}

// This is a special version of EvaluateNode() used specifically for getting an lvalue reference without evaluating it.
// For example, to perform "x = 5", we don't want to fetch the current value of x; instead we want to figure out what
// symbol table "x" lives in, and then tell that symbol table to set identifier "x" to 5.  So we need to evaluate the
// left-hand side of the expression with the goal of getting a symbol table reference, not a value.  This would be
// simpler if we just had a "symbol" object that contained the value; then we could treat lvalues and rvalues in the
// same way.  But we don't; instead we have particular subclasses of ScriptValue, and we even have C++ objects that
// are wrapped by proxy ScriptValue objects, so we have to handle lvalues differently from rvalues.  I think.
LValueReference *ScriptInterpreter::Evaluate_LValueReference(const ScriptASTNode *p_node)
{
	TokenType token_type = p_node->token_->token_type_;
	
	if (logging_execution_)
	{
		execution_log_ << IndentString(execution_log_indent_) << "Evaluate_LValueReference() : token ";
		p_node->PrintToken(execution_log_);
		execution_log_ << "\n";
	}
	
	switch (token_type)
	{
		case TokenType::kTokenLBracket:
		{
			if (p_node->children_.size() != 2)
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): internal error (expected 2 children for '[' node)." << endl << slim_terminate();
			
			ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
			ScriptValueType first_child_type = first_child_value->Type();
			
			if (first_child_type == ScriptValueType::kValueNULL)
			{
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): operand type " << first_child_type << " is not supported by the '[]' operator." << endl << slim_terminate();
			}
			
			ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
			ScriptValueType second_child_type = second_child_value->Type();
			
			if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat) && (second_child_type != ScriptValueType::kValueLogical) && (second_child_type != ScriptValueType::kValueNULL))
			{
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				if (!second_child_value->InSymbolTable()) delete second_child_value;
				
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): index operand type " << second_child_type << " is not supported by the '[]' operator." << endl << slim_terminate();
			}
			
			int second_child_count = second_child_value->Count();
			
			if (second_child_count != 1)
			{
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				if (!second_child_value->InSymbolTable()) delete second_child_value;
				
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): index operand for the '[]' operator must have size() == 1." << endl << slim_terminate();
			}
			
			// OK, we have <legal type>[<single numeric>]; we can work with that
			return new LValueSubscriptReference(first_child_value, (int)second_child_value->IntAtIndex(0));
		}
		case TokenType::kTokenDot:
		{
			if (p_node->children_.size() != 2)
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): internal error (expected 2 children for '.' node)." << endl << slim_terminate();
			
			ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
			ScriptValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != ScriptValueType::kValueProxy)
			{
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): operand type " << first_child_type << " is not supported by the '.' operator." << endl << slim_terminate();
			}
			
			ScriptASTNode *second_child_node = p_node->children_[1];
			
			if (second_child_node->token_->token_type_ != TokenType::kTokenIdentifier)
			{
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): the '.' operator for x.y requires operand y to be an identifier." << endl << slim_terminate();
			}
			
			// OK, we have <proxy type>.<identifier>; we can work with that
			return new LValueMemberReference(first_child_value, second_child_node->token_->token_string_);
		}
		case TokenType::kTokenIdentifier:
		{
			if (p_node->children_.size() != 0)
				SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): internal error (expected 0 children for identifier node)." << endl << slim_terminate();
			
			// Simple identifier; the symbol host is the global symbol table, at least for now
			return new LValueMemberReference(global_symbols_, p_node->token_->token_string_);
		}
		default:
			SLIM_TERMINATION << "ERROR (Evaluate_LValueReference): Unexpected node token type " << token_type << "; lvalue required." << endl << slim_terminate();
			break;
	}
	
	return nullptr;
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
		default:
			SLIM_TERMINATION << "ERROR (EvaluateNode): Unexpected node token type " << token_type << "." << endl << slim_terminate();
			break;
	}
	
	if (!result)
		SLIM_TERMINATION << "ERROR (EvaluateNode): nullptr returned from evaluation of token type " << token_type << "." << endl << slim_terminate();
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_NullStatement(const ScriptASTNode *p_node)
{
#pragma unused(p_node)
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_NullStatement() entered\n";
	
	if (p_node->children_.size() != 0)
		SLIM_TERMINATION << "ERROR (Evaluate_NullStatement): internal error (expected 0 children)." << endl << slim_terminate();
	
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
		if (!result->InSymbolTable()) delete result;
		
		result = EvaluateNode(child_node);
		
		// a next or break makes us exit immediately, out to the (presumably enclosing) loop evaluator
		if (next_statement_hit_ || break_statement_hit_)
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
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	bool too_wide = false;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): operand type " << first_child_type << " is not supported by the ':' operator." << endl << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): operand type " << second_child_type << " is not supported by the ':' operator." << endl << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != 1) || (second_child_count != 1))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): operands of the ':' operator must have size() == 1." << endl << slim_terminate();
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
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (underflow)
	{
		if (!result->InSymbolTable()) delete result;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): the floating-point range could not be constructed due to underflow." << endl << slim_terminate();
	}
	if (too_wide)
	{
		if (!result->InSymbolTable()) delete result;
		
		SLIM_TERMINATION << "ERROR (Evaluate_RangeExpr): a range with more than 100000 entries cannot be constructed." << endl << slim_terminate();
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
	// node is not an identifier, then at present it is illegal; functions are only globals, not methods, for now.
	ScriptASTNode *function_name_node = p_node->children_[0];
	TokenType function_name_token_type = function_name_node->token_->token_type_;
	
	if (function_name_token_type == TokenType::kTokenIdentifier)
	{
		string function_name = function_name_node->token_->token_string_;
		vector<ScriptValue*> arguments;
		
		// Evaluate all arguments; note this occurs before the function call itself is evaluated at all
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
		
		// We offload the actual work to ExecuteFunctionCall() to keep things simple here
		result = ExecuteFunctionCall(function_name, arguments, execution_output_, *this);
		
		// And now we can free the arguments
		for (auto arg_iter = arguments.begin(); arg_iter != arguments.end(); ++arg_iter)
		{
			ScriptValue *arg = *arg_iter;
			
			if (!arg->InSymbolTable()) delete arg;
		}
	}
	else
	{
		SLIM_TERMINATION << "ERROR (Evaluate_FunctionCall): type " << function_name_token_type << " is not supported by the '()' operator (illegal operand for a function call operation)." << endl << slim_terminate();
	}
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_FunctionCall() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Subset(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Subset() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Subset): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if (first_child_type == ScriptValueType::kValueNULL)
	{
		// Any subscript of NULL returns NULL
		result = new ScriptValue_NULL();
		
		if (!first_child_value->InSymbolTable()) delete first_child_value;
	}
	else
	{
		ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
		ScriptValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat) && (second_child_type != ScriptValueType::kValueLogical) && (second_child_type != ScriptValueType::kValueNULL))
		{
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Subset): index operand type " << second_child_type << " is not supported by the '[]' operator." << endl << slim_terminate();
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
					if (!first_child_value->InSymbolTable()) delete first_child_value;
					if (!second_child_value->InSymbolTable()) delete second_child_value;
					if (!result->InSymbolTable()) delete result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Subset): the '[]' operator requires that both operands have the same size()." << endl << slim_terminate();
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
						if (!first_child_value->InSymbolTable()) delete first_child_value;
						if (!second_child_value->InSymbolTable()) delete second_child_value;
						if (!result->InSymbolTable()) delete result;
						
						SLIM_TERMINATION << "ERROR (Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << endl << slim_terminate();
					}
					else
						result->PushValueFromIndexOfScriptValue((int)index_value, first_child_value);
				}
			}
		}
		
		// Free our operands
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
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
		SLIM_TERMINATION << "ERROR (Evaluate_MemberRef): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if (first_child_type != ScriptValueType::kValueProxy)
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_MemberRef): operand type " << first_child_type << " is not supported by the '.' operator." << endl << slim_terminate();
	}
	
	ScriptASTNode *second_child_node = p_node->children_[1];
	
	if (second_child_node->token_->token_type_ != TokenType::kTokenIdentifier)
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_MemberRef): the '.' operator for x.y requires operand y to be an identifier." << endl << slim_terminate();
	}
	
	result = first_child_value->GetValueForMember(second_child_node->token_->token_string_);
	
	// free our operand
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_MemberRef() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Plus(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Plus() entered\n";
	
	if ((p_node->children_.size() != 1) && (p_node->children_.size() != 2))
		SLIM_TERMINATION << "ERROR (Evaluate_Plus): internal error (expected 1 or 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if (p_node->children_.size() == 1)
	{
		// unary plus is a no-op, but legal only for numeric types
		if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
		{
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Plus): operand type " << first_child_type << " is not supported by the unary '+' operator." << endl << slim_terminate();
		}
		
		result = first_child_value;
		
		// free our operands
		if (!first_child_value->InSymbolTable()) delete first_child_value;
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
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
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
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				if (!second_child_value->InSymbolTable()) delete second_child_value;
				
				SLIM_TERMINATION << "ERROR (Evaluate_Plus): the combination of operand types " << first_child_type << " and " << second_child_type << " is not supported by the binary '+' operator." << endl << slim_terminate();
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
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
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
		SLIM_TERMINATION << "ERROR (Evaluate_Minus): internal error (expected 1 or 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Minus): operand type " << first_child_type << " is not supported by the '-' operator." << endl << slim_terminate();
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
		if (!first_child_value->InSymbolTable()) delete first_child_value;
	}
	else
	{
		// binary minus
		ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
		ScriptValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
		{
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Minus): operand type " << second_child_type << " is not supported by the '-' operator." << endl << slim_terminate();
		}
		
		int second_child_count = second_child_value->Count();
		
		if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
		{
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Minus): the '-' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
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
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
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
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): operand type " << first_child_type << " is not supported by the '%' operator." << endl << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): operand type " << second_child_type << " is not supported by the '%' operator." << endl << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mod): the '%' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
	}
	
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
					if (!first_child_value->InSymbolTable()) delete first_child_value;
					if (!second_child_value->InSymbolTable()) delete second_child_value;
					if (!int_result->InSymbolTable()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Mod): integer modulo by zero." << endl << slim_terminate();
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
					if (!first_child_value->InSymbolTable()) delete first_child_value;
					if (!second_child_value->InSymbolTable()) delete second_child_value;
					if (!int_result->InSymbolTable()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Mod): integer modulo by zero." << endl << slim_terminate();
				}
				
				int_result->PushInt(singleton_int % divisor);
			}
		}
		else if (second_child_count == 1)
		{
			int64_t singleton_int = second_child_value->IntAtIndex(0);
			
			if (singleton_int == 0)
			{
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				if (!second_child_value->InSymbolTable()) delete second_child_value;
				if (!int_result->InSymbolTable()) delete int_result;
				
				SLIM_TERMINATION << "ERROR (Evaluate_Mod): integer modulo by zero." << endl << slim_terminate();
			}
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				int_result->PushInt(first_child_value->IntAtIndex(value_index) % singleton_int);
		}
		
		result = int_result;
	}
	else
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
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Mod() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Mult(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Mult() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): operand type " << first_child_type << " is not supported by the '*' operator." << endl << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): operand type " << second_child_type << " is not supported by the '*' operator." << endl << slim_terminate();
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
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Mult): the '*' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
	}
	
	// free our operands
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Mult() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Div(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Div() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Div): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptValue *result = nullptr;
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Div): operand type " << first_child_type << " is not supported by the '/' operator." << endl << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Div): operand type " << second_child_type << " is not supported by the '/' operator." << endl << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Div): the '/' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
	}
	
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
					if (!first_child_value->InSymbolTable()) delete first_child_value;
					if (!second_child_value->InSymbolTable()) delete second_child_value;
					if (!int_result->InSymbolTable()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Div): integer divide by zero." << endl << slim_terminate();
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
					if (!first_child_value->InSymbolTable()) delete first_child_value;
					if (!second_child_value->InSymbolTable()) delete second_child_value;
					if (!int_result->InSymbolTable()) delete int_result;
					
					SLIM_TERMINATION << "ERROR (Evaluate_Div): integer divide by zero." << endl << slim_terminate();
				}
				
				int_result->PushInt(singleton_int / divisor);
			}
		}
		else if (second_child_count == 1)
		{
			int64_t singleton_int = second_child_value->IntAtIndex(0);
			
			if (singleton_int == 0)
			{
				if (!first_child_value->InSymbolTable()) delete first_child_value;
				if (!second_child_value->InSymbolTable()) delete second_child_value;
				if (!int_result->InSymbolTable()) delete int_result;
				
				SLIM_TERMINATION << "ERROR (Evaluate_Div): integer divide by zero." << endl << slim_terminate();
			}
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				int_result->PushInt(first_child_value->IntAtIndex(value_index) / singleton_int);
		}
		
		result = int_result;
	}
	else
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
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Div() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Exp(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Exp() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValue *second_child_value = EvaluateNode(p_node->children_[1]);
	
	ScriptValueType first_child_type = first_child_value->Type();
	ScriptValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): operand type " << first_child_type << " is not supported by the '^' operator." << endl << slim_terminate();
	}
	
	if ((second_child_type != ScriptValueType::kValueInt) && (second_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): operand type " << second_child_type << " is not supported by the '^' operator." << endl << slim_terminate();
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		if (!second_child_value->InSymbolTable()) delete second_child_value;
		SLIM_TERMINATION << "ERROR (Evaluate_Exp): the '^' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
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
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Exp() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_And(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_And() entered\n";
	
	if (p_node->children_.size() < 2)
		SLIM_TERMINATION << "ERROR (Evaluate_And): internal error (expected 2+ children)." << endl << slim_terminate();
	
	bool result_bool = true;
	
	for (ScriptASTNode *child_node : p_node->children_)
	{
		ScriptValue *child_result = EvaluateNode(child_node);
		ScriptValueType child_type = child_result->Type();
		
		if ((child_type != ScriptValueType::kValueLogical) && (child_type != ScriptValueType::kValueString) && (child_type != ScriptValueType::kValueInt) && (child_type != ScriptValueType::kValueFloat))
		{
			if (!child_result->InSymbolTable()) delete child_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_And): operand type " << child_type << " is not supported by the '&&' operator." << endl << slim_terminate();
		}
		
		if (child_result->Count() == 1)
		{
			bool child_bool = child_result->LogicalAtIndex(0);
			
			if (!child_bool)
			{
				result_bool = false;
				break;
			}
		}
		else
		{
			if (!child_result->InSymbolTable()) delete child_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_And): the '&&' operator requires operands of size() == 1." << endl << slim_terminate();
		}
		
		// free our operand
		if (!child_result->InSymbolTable()) delete child_result;
	}
	
	ScriptValue *result = new ScriptValue_Logical(result_bool);
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_And() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Or(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Or() entered\n";
	
	if (p_node->children_.size() < 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Or): internal error (expected 2+ children)." << endl << slim_terminate();
	
	bool result_bool = false;
	
	for (ScriptASTNode *child_node : p_node->children_)
	{
		ScriptValue *child_result = EvaluateNode(child_node);
		ScriptValueType child_type = child_result->Type();
		
		if ((child_type != ScriptValueType::kValueLogical) && (child_type != ScriptValueType::kValueString) && (child_type != ScriptValueType::kValueInt) && (child_type != ScriptValueType::kValueFloat))
		{
			if (!child_result->InSymbolTable()) delete child_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Or): operand type " << child_type << " is not supported by the '||' operator." << endl << slim_terminate();
		}
		
		if (child_result->Count() == 1)
		{
			bool child_bool = child_result->LogicalAtIndex(0);
			
			if (child_bool)
			{
				result_bool = true;
				break;
			}
		}
		else
		{
			if (!child_result->InSymbolTable()) delete child_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Or): the '||' operator requires operands of size() == 1." << endl << slim_terminate();
		}
		
		// free our operand
		if (!child_result->InSymbolTable()) delete child_result;
	}
	
	ScriptValue *result = new ScriptValue_Logical(result_bool);
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Or() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Not(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Not() entered\n";
	
	if (p_node->children_.size() != 1)
		SLIM_TERMINATION << "ERROR (Evaluate_Not): internal error (expected 1 child)." << endl << slim_terminate();
	
	ScriptValue *first_child_value = EvaluateNode(p_node->children_[0]);
	ScriptValueType first_child_type = first_child_value->Type();
	
	if ((first_child_type != ScriptValueType::kValueLogical) && (first_child_type != ScriptValueType::kValueString) && (first_child_type != ScriptValueType::kValueInt) && (first_child_type != ScriptValueType::kValueFloat))
	{
		if (!first_child_value->InSymbolTable()) delete first_child_value;
		
		SLIM_TERMINATION << "ERROR (Evaluate_Not): operand type " << first_child_type << " is not supported by the '!' operator." << endl << slim_terminate();
	}
	
	ScriptValue_Logical *result = new ScriptValue_Logical();
	
	int first_child_count = first_child_value->Count();
	
	for (int value_index = 0; value_index < first_child_count; ++value_index)
		result->PushLogical(!first_child_value->LogicalAtIndex(value_index));
	
	// free our operand
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Not() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Assign(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Assign() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Assign): internal error (expected 2 children)." << endl << slim_terminate();
	
	ScriptASTNode *lvalue_child = p_node->children_[0];
	
	// an lvalue is needed to assign into; for right now, we require an identifier, although that isn't quite right
	// since we should also be able to assign into a subscript, a member of a class, etc.; we need a concept of lvalue references
	LValueReference *lvalue_ref = Evaluate_LValueReference(lvalue_child);
	if (!lvalue_ref)
		SLIM_TERMINATION << "ERROR (Evaluate_Assign): the '=' operator requires an lvalue for its left operand." << endl << slim_terminate();
	
	ScriptValue *r_value = EvaluateNode(p_node->children_[1]);
	
	// replace the existing value or set a new value
	lvalue_ref->SetLValueToValue(r_value);
	
	// by design, assignment does not yield a usable value; instead it produces NULL – this prevents the error "if (x = 3) ..."
	// since the condition is NULL and will raise; the loss of legitimate uses of "if (x = 3)" seems a small price to pay
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	// free our operands; note r_value is now owned by whoever lvalue_ref gave it to, so don't free it here!
	delete lvalue_ref;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Assign() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Eq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Eq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Eq): internal error (expected 2 children)." << endl << slim_terminate();
	
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
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			if (!result->InSymbolTable()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Eq): the '==' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
		}
	}
	
	// free our operands
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Eq() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Lt(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Lt() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Lt): internal error (expected 2 children)." << endl << slim_terminate();
	
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
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			if (!result->InSymbolTable()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Lt): the '<' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
		}
	}
	
	// free our operands
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Lt() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_LtEq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_LtEq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_LtEq): internal error (expected 2 children)." << endl << slim_terminate();
	
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
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			if (!result->InSymbolTable()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_LtEq): the '<=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
		}
	}
	
	// free our operands
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_LtEq() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Gt(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Gt() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Gt): internal error (expected 2 children)." << endl << slim_terminate();
	
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
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			if (!result->InSymbolTable()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Gt): the '>' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
		}
	}
	
	// free our operands
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Gt() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_GtEq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_GtEq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_GtEq): internal error (expected 2 children)." << endl << slim_terminate();
	
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
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			if (!result->InSymbolTable()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_GtEq): the '>=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
		}
	}
	
	// free our operands
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_GtEq() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_NotEq(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_NotEq() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_NotEq): internal error (expected 2 children)." << endl << slim_terminate();
	
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
			if (!first_child_value->InSymbolTable()) delete first_child_value;
			if (!second_child_value->InSymbolTable()) delete second_child_value;
			if (!result->InSymbolTable()) delete result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_NotEq): the '!=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << endl << slim_terminate();
		}
	}
	
	// free our operands
	if (!first_child_value->InSymbolTable()) delete first_child_value;
	if (!second_child_value->InSymbolTable()) delete second_child_value;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_NotEq() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Number(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Number() entered\n";
	
	if (p_node->children_.size() > 0)
		SLIM_TERMINATION << "ERROR (Evaluate_Number): internal error (expected 0 children)." << endl << slim_terminate();
	
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
		result = new ScriptValue_Int(static_cast<int>(strtod(number_string.c_str(), nullptr)));			// has an exponent
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
		SLIM_TERMINATION << "ERROR (Evaluate_String): internal error (expected 0 children)." << endl << slim_terminate();
	
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
		SLIM_TERMINATION << "ERROR (Evaluate_Identifier): internal error (expected 0 children)." << endl << slim_terminate();
	
	string identifier_string = p_node->token_->token_string_;
	ScriptValue *result = global_symbols_->GetValueForMember(identifier_string);
	
	if (!result)
		SLIM_TERMINATION << "ERROR (Evaluate_Identifier): undefined identifier " << identifier_string << "." << endl << slim_terminate();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Identifier() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_If(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_If() entered\n";
	
	if ((p_node->children_.size() != 2) && (p_node->children_.size() != 3))
		SLIM_TERMINATION << "ERROR (Evaluate_If): internal error (expected 2 or 3 children)." << endl << slim_terminate();
	
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
		if (!condition_result->InSymbolTable()) delete condition_result;
		
		SLIM_TERMINATION << "ERROR (Evaluate_If): condition has size() != 1." << endl << slim_terminate();
	}
	
	// free our operands
	if (!condition_result->InSymbolTable()) delete condition_result;
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_If() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_Do(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Do() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_Do): internal error (expected 2 children)." << endl << slim_terminate();
	
	do
	{
		// execute the do...while loop's statement by evaluating its node; evaluation values get thrown away
		ScriptValue *statement_value = EvaluateNode(p_node->children_[0]);
		
		if (!statement_value->InSymbolTable()) delete statement_value;
		
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
			
			if (!condition_result->InSymbolTable()) delete condition_result;
			
			if (!condition_bool)
				break;
		}
		else
		{
			if (!condition_result->InSymbolTable()) delete condition_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_Do): condition has size() != 1." << endl << slim_terminate();
		}
	}
	while (true);
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Do() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_While(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_While() entered\n";
	
	if (p_node->children_.size() != 2)
		SLIM_TERMINATION << "ERROR (Evaluate_While): internal error (expected 2 children)." << endl << slim_terminate();
	
	while (true)
	{
		// test the loop condition
		ScriptASTNode *condition_node = p_node->children_[0];
		ScriptValue *condition_result = EvaluateNode(condition_node);
		
		if (condition_result->Count() == 1)
		{
			bool condition_bool = condition_result->LogicalAtIndex(0);
			
			if (!condition_result->InSymbolTable()) delete condition_result;
			
			if (!condition_bool)
				break;
		}
		else
		{
			if (!condition_result->InSymbolTable()) delete condition_result;
			
			SLIM_TERMINATION << "ERROR (Evaluate_While): condition has size() != 1." << endl << slim_terminate();
		}
		
		// execute the while loop's statement by evaluating its node; evaluation values get thrown away
		ScriptValue *statement_value = EvaluateNode(p_node->children_[1]);
		
		if (!statement_value->InSymbolTable()) delete statement_value;
		
		// handle next and break statements
		if (next_statement_hit_)
			next_statement_hit_ = false;		// this is all we need to do; the rest of the function of "next" was done by Evaluate_CompoundStatement()
		
		if (break_statement_hit_)
		{
			break_statement_hit_ = false;
			break;							// break statements, on the other hand, get handled additionally by a break from our loop here
		}
	}
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_While() : return == " << *result << "\n";
	
	return result;
}

ScriptValue *ScriptInterpreter::Evaluate_For(const ScriptASTNode *p_node)
{
	if (logging_execution_)
		execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_For() entered\n";
	
	if (p_node->children_.size() != 3)
		SLIM_TERMINATION << "ERROR (Evaluate_For): internal error (expected 3 children)." << endl << slim_terminate();
	
	ScriptASTNode *identifier_child = p_node->children_[0];
	
	// an lvalue is needed to assign into; for right now, we require an identifier, although that isn't quite right
	// since we should also be able to assign into a subscript, a member of a class, etc.; we need a concept of lvalue references
	if (identifier_child->token_->token_type_ != TokenType::kTokenIdentifier)
		SLIM_TERMINATION << "ERROR (Evaluate_For): the 'for' keyword requires an identifier for its left operand." << endl << slim_terminate();
	
	string identifier_name = identifier_child->token_->token_string_;
	ScriptValue *range_value = EvaluateNode(p_node->children_[1]);
	int range_count = range_value->Count();
	
	for (int range_index = 0; range_index < range_count; ++range_index)
	{
		// set the index variable to the range value and then throw the range value away
		ScriptValue *range_value_at_index = range_value->GetValueAtIndex(range_index);
		
		global_symbols_->SetValueForMember(identifier_name, range_value_at_index);
		
		if (!range_value_at_index->InSymbolTable()) delete range_value_at_index;
		
		// execute the for loop's statement by evaluating its node; evaluation values get thrown away
		ScriptValue *statement_value = EvaluateNode(p_node->children_[2]);
		
		if (!statement_value->InSymbolTable()) delete statement_value;
		
		// handle next and break statements
		if (next_statement_hit_)
			next_statement_hit_ = false;		// this is all we need to do; the rest of the function of "next" was done by Evaluate_CompoundStatement()
		
		if (break_statement_hit_)
		{
			break_statement_hit_ = false;
			break;							// break statements, on the other hand, get handled additionally by a break from our loop here
		}
	}
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	// free our range operand
	if (!range_value->InSymbolTable()) delete range_value;
	
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
		SLIM_TERMINATION << "ERROR (Evaluate_Next): internal error (expected 0 children)." << endl << slim_terminate();
	
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
		SLIM_TERMINATION << "ERROR (Evaluate_Break): internal error (expected 0 children)." << endl << slim_terminate();
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	break_statement_hit_ = true;
	
	ScriptValue *result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
	
	if (logging_execution_)
		execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Break() : return == " << *result << "\n";
	
	return result;
}





























































