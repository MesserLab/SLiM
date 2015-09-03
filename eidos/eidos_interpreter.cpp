//
//  eidos_interpreter.cpp
//  Eidos
//
//  Created by Ben Haller on 4/4/15.
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


#include "eidos_interpreter.h"
#include "eidos_functions.h"
#include "eidos_ast_node.h"
#include "eidos_rng.h"

#include <sstream>
#include <stdexcept>
#include "math.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


// It is worth noting that the memory management in EidosInterpreter leaks like a sieve.  I think it only leaks when exceptions are raised;
// but of course exceptions are raised a lot, especially in the interactive EidosScribe environment.  The leaks are generally caused by a
// raise inside a call to EvaluateNode() that blows through the caller's context and fails to free EidosValue pointers owned by the caller.
// It would be possible to fix this, of course; but since this is primarily a problem for EidosScribe, I am not overly concerned about it
// at the moment.  It also not completely clear what the best way is to fix the leaks.  Perhaps I ought to be using std::unique_ptr instead
// of naked pointers; but that's a C++11 feature, and I'm concerned about both portability and speed.  My inclination is to hold off for
// now, let it leak, and perhaps migrate to std::unique_ptr later, rather than trying to protect every EvaluateNode() call.


bool TypeCheckAssignmentOfEidosValueIntoEidosValue(EidosValue *base_value, EidosValue *dest_value)
{
	EidosValueType base_type = base_value->Type();
	EidosValueType dest_type = dest_value->Type();
	bool base_is_object = (base_type == EidosValueType::kValueObject);
	bool dest_is_object = (dest_type == EidosValueType::kValueObject);
	
	if (base_is_object && dest_is_object)
	{
		// objects must match in their element type, or one or both must have no defined element type (due to being empty)
		const EidosObjectClass *base_element_class = ((EidosValue_Object *)base_value)->Class();
		const EidosObjectClass *dest_element_class = ((EidosValue_Object *)dest_value)->Class();
		bool base_is_typeless = (base_element_class == gEidos_UndefinedClassObject);
		bool dest_is_typeless = (dest_element_class == gEidos_UndefinedClassObject);
		
		if (base_is_typeless || dest_is_typeless)
			return true;
		
		return (base_element_class == dest_element_class);
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
	if (base_type == EidosValueType::kValueNULL)
		return false;
	
	// otherwise, we follow the promotion order defined in EidosValueType
	return (dest_type > base_type);
}


//
//	EidosInterpreter
//
#pragma mark EidosInterpreter

EidosInterpreter::EidosInterpreter(const EidosScript &p_script, EidosSymbolTable &p_symbols, EidosContext *p_eidos_context)
	: root_node_(p_script.AST()), global_symbols_(p_symbols), eidos_context_(p_eidos_context)
{
	RegisterFunctionMap(EidosInterpreter::BuiltInFunctionMap());
	
	// Initialize the random number generator if and only if it has not already been initialized.  In some cases the Context will want to
	// initialize the RNG itself, with its own seed; we don't want to override that.
	if (!gEidos_rng)
		EidosInitializeRNGFromSeed(EidosGenerateSeedFromPIDAndTime());
}

EidosInterpreter::EidosInterpreter(const EidosASTNode *p_root_node_, EidosSymbolTable &p_symbols, EidosContext *p_eidos_context)
	: root_node_(p_root_node_), global_symbols_(p_symbols), eidos_context_(p_eidos_context)
{
	RegisterFunctionMap(EidosInterpreter::BuiltInFunctionMap());
	
	// Initialize the random number generator if and only if it has not already been initialized.  In some cases the Context will want to
	// initialize the RNG itself, with its own seed; we don't want to override that.
	if (!gEidos_rng)
		EidosInitializeRNGFromSeed(EidosGenerateSeedFromPIDAndTime());
}

EidosInterpreter::~EidosInterpreter(void)
{
	if (function_map_ != EidosInterpreter::BuiltInFunctionMap())
	{
		delete function_map_;
		function_map_ = nullptr;
	}
	
	if (execution_log_)
		delete execution_log_;
	
	if (execution_output_)
		delete execution_output_;
}

void EidosInterpreter::SetShouldLogExecution(bool p_log)
{
	logging_execution_ = p_log;
	
	if (logging_execution_)
	{
#if defined(DEBUG) || defined(EIDOS_GUI)
		// execution_log_ is allocated when logging execution is turned on; all use of execution_log_
		// should be inside "if (logging_execution_)", so this should suffice.
		if (!execution_log_)
			execution_log_ = new std::ostringstream();
#else
		// Logging execution is disabled until we are either in a DEBUG build or a GUI (EIDOS_GUI)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::SetShouldLogExecution): execution logging is disabled in this build configuration of Eidos." << eidos_terminate(nullptr);
#endif
	}
}

bool EidosInterpreter::ShouldLogExecution(void)
{
	return logging_execution_;
}

std::string EidosInterpreter::ExecutionLog(void)
{
	return (execution_log_ ? execution_log_->str() : gEidosStr_empty_string);
}

std::string EidosInterpreter::ExecutionOutput(void)
{
	return (execution_output_ ? execution_output_->str() : gEidosStr_empty_string);
}

std::ostringstream &EidosInterpreter::ExecutionOutputStream(void)
{
	// lazy allocation; all use of execution_output_ should get it through this accessor
	if (!execution_output_)
		execution_output_ = new std::ostringstream();
	
	return *execution_output_;
}

EidosSymbolTable &EidosInterpreter::GetSymbolTable(void)
{
	return global_symbols_;
}

// the starting point for internally executed blocks, which require braces and suppress output
EidosValue *EidosInterpreter::EvaluateInternalBlock(EidosScript *p_script_for_block)
{
	// EvaluateInternalBlock() does not log execution, since it is not user-initiated
	
	bool saved_error_tracking = false;
	int error_start_save;
	int error_end_save;
	EidosScript *current_script_save;
	bool executing_runtime_script_save;
	
	// internal blocks may be associated with their own script object; if so, the error tracking code needs to track that
	if ((p_script_for_block != nullptr) && (p_script_for_block != gEidosCurrentScript))
	{
		// This script block is constructed at runtime and has its own script, so we need to redirect error tracking
		error_start_save = gEidosCharacterStartOfError;
		error_end_save = gEidosCharacterEndOfError;
		current_script_save = gEidosCurrentScript;
		executing_runtime_script_save = gEidosExecutingRuntimeScript;
		
		gEidosCharacterStartOfError = -1;
		gEidosCharacterEndOfError = -1;
		gEidosCurrentScript = p_script_for_block;
		gEidosExecutingRuntimeScript = true;
		
		saved_error_tracking = true;
	}
	
	EidosValue *result = FastEvaluateNode(root_node_);
	
	// if a next or break statement was hit and was not handled by a loop, throw an error
	if (next_statement_hit_ || break_statement_hit_)
	{
		if (result->IsTemporary()) delete result;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInternalBlock): statement \"" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "\" encountered with no enclosing loop." << eidos_terminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
	}
	
	// handle a return statement; we're at the top level, so there's not much to do
	if (return_statement_hit_)
		return_statement_hit_ = false;
	
	// EvaluateInternalBlock() does not send the result of execution to the output stream; EvaluateInterpreterBlock() does,
	// because it is for interactive use, but EvaluateInternalBlock() is for internal use, and so interactive output
	// is undesirable.  Eidos code that wants to generate output can always use print(), cat(), etc.
	
	// Restore the normal error context
	if (saved_error_tracking)
	{
		// The saved_error_tracking flag guarantees that these statements do not use unused variables.  Unfortunately,
		// the compiler is not smart enough to know that, so I have to disable warnings across these lines.
		// I could fix the warning by initializing the variables above, but this method is a performance bottleneck for
		// callbacks and such, so I don't want to slow it down with unnecessary assignments.
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wuninitialized"
		#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wuninitialized"
		#pragma clang diagnostic ignored "-Wconditional-uninitialized"
		gEidosCharacterStartOfError = error_start_save;
		gEidosCharacterEndOfError = error_end_save;
		gEidosCurrentScript = current_script_save;
		gEidosExecutingRuntimeScript = executing_runtime_script_save;
		#pragma clang diagnostic pop
		#pragma GCC diagnostic pop
	}
	
	// EvaluateInternalBlock() does not log execution, since it is not user-initiated
	
	return result;
}

// the starting point for script blocks in Eidos, which does not require braces
EidosValue *EidosInterpreter::EvaluateInterpreterBlock(bool p_print_output)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
	{
		execution_log_indent_ = 0;
		*execution_log_ << IndentString(execution_log_indent_++) << "EvaluateInterpreterBlock() entered\n";
	}
#endif
	
	EidosValue *result = gStaticEidosValueNULLInvisible;
	
	for (EidosASTNode *child_node : root_node_->children_)
	{
		if (result->IsTemporary()) delete result;
		
		result = FastEvaluateNode(child_node);
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
		{
			if (result->IsTemporary()) delete result;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInterpreterBlock): statement \"" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "\" encountered with no enclosing loop." << eidos_terminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
		}
		
		// send the result of the block to our output stream
		if (p_print_output && !result->Invisible())
		{
			std::ostringstream &execution_output = ExecutionOutputStream();
			
			auto position = execution_output.tellp();
			execution_output << *result;
			
			// EidosValue does not put an endl on the stream, so if it emitted any output, add an endl
			if (position != execution_output.tellp())
				execution_output << endl;
		}
		
		// handle a return statement; we're at the top level, so there's not much to do except stop execution
		if (return_statement_hit_)
		{
			return_statement_hit_ = false;
			break;
		}
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "EvaluateInterpreterBlock() : return == " << *result << "\n";
#endif
	
	// if requested, send the full trace to std:cout
	if (gEidosLogEvaluation)
		std::cout << ExecutionLog();
	
	return result;
}

// A subscript has been encountered as the top-level operation on the left-hand side of an assignment – x[5] = y, x.foo[5] = y, or more
// complex cases like x[3:10].foo[2:5][1:2] = y.  The job of this function is to determine the identity of the symbol host (x, x, and
// x[3:10], respectively), the name of the property within the symbol host (none, foo, and foo, respectively), and the indices of the final
// subscript operation (5, 5, and {3,4}, respectively), and return them to the caller, who will assign into those subscripts.  Note that
// complex cases work only because of several other aspects of Eidos.  Notably, subscripting of an object creates a new object, but
// the new object refers to the same elements as the parent object, by pointer; this means that x[5].foo = y works, because x[5] refers to
// the same element, by pointer, as x does.  If Eidos did not have these shared-value semantics, assignment would be much trickier,
// since Eidos cannot use a symbol table to store values, in general (since many values accessible through script are stored in private
// representations kept by external classes in the Context).  In other words, assignment relies upon the fact that a temporary object
// constructed by Evaluate_Node() refers to the same underlying element objects as the original source of the elements does, and thus
// assigning into the temporary also assigns into the original.
void EidosInterpreter::_ProcessSubscriptAssignment(EidosValue **p_base_value_ptr, EidosGlobalStringID *p_property_string_id_ptr, vector<int> *p_indices_ptr, const EidosASTNode *p_parent_node)
{
	// The left operand is the thing we're subscripting.  If it is an identifier or a dot operator, then we are the deepest (i.e. first)
	// subscript operation, and we can resolve the symbol host, set up a vector of indices, and return.  If it is a subscript, we recurse.
	EidosToken *parent_token = p_parent_node->token_;
	EidosTokenType token_type = parent_token->token_type_;
	
	switch (token_type)
	{
		case EidosTokenType::kTokenLBracket:
		{
#ifdef DEBUG
			if (p_parent_node->children_.size() != 2)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): (internal error) expected 2 children for '[' node." << eidos_terminate(parent_token);
#endif
			
			EidosASTNode *left_operand = p_parent_node->children_[0];
			EidosASTNode *right_operand = p_parent_node->children_[1];
			
			vector<int> base_indices;
			
			// Recurse to find the symbol host and property name that we are ultimately subscripting off of
			_ProcessSubscriptAssignment(p_base_value_ptr, p_property_string_id_ptr, &base_indices, left_operand);
			
			// Find out which indices we're supposed to use within our base vector
			EidosValue *second_child_value = FastEvaluateNode(right_operand);
			EidosValueType second_child_type = second_child_value->Type();
			
			if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat) && (second_child_type != EidosValueType::kValueLogical))
			{
				if (second_child_value->IsTemporary()) delete second_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): index operand type " << second_child_type << " is not supported by the '[]' operator." << eidos_terminate(parent_token);
			}
			
			int second_child_count = second_child_value->Count();
			
			if (second_child_type == EidosValueType::kValueLogical)
			{
				// A logical vector must exactly match in length; if it does, it selects corresponding indices from base_indices
				if (second_child_count != base_indices.size())
				{
					if (second_child_value->IsTemporary()) delete second_child_value;
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << eidos_terminate(parent_token);
				}
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					bool bool_value = second_child_value->LogicalAtIndex(value_idx, parent_token);
					
					if (bool_value)
						p_indices_ptr->push_back(base_indices[value_idx]);
				}
			}
			else if ((second_child_type == EidosValueType::kValueInt) || (second_child_type == EidosValueType::kValueFloat))
			{
				// A numeric vector can be of any length; each number selects the index at that index in base_indices
				int base_indices_count =  (int)base_indices.size();
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					int64_t index_value = second_child_value->IntAtIndex(value_idx, parent_token);
					
					if ((index_value < 0) || (index_value >= base_indices_count))
					{
						if (second_child_value->IsTemporary()) delete second_child_value;
						
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(parent_token);
					}
					else
						p_indices_ptr->push_back(base_indices[index_value]);
				}
			}
			
			break;
		}
		case EidosTokenType::kTokenDot:
		{
#ifdef DEBUG
			if (p_parent_node->children_.size() != 2)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): (internal error) expected 2 children for '.' node." << eidos_terminate(parent_token);
#endif
			
			EidosASTNode *left_operand = p_parent_node->children_[0];
			EidosASTNode *right_operand = p_parent_node->children_[1];
			
			EidosValue *first_child_value = FastEvaluateNode(left_operand);
			EidosValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != EidosValueType::kValueObject)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(parent_token);
			}
			
			if (right_operand->token_->token_type_ != EidosTokenType::kTokenIdentifier)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(parent_token);
			}
			
			*p_base_value_ptr = first_child_value;
			*p_property_string_id_ptr = EidosGlobalStringIDForString(right_operand->token_->token_string_);
			
			int number_of_elements = first_child_value->Count();	// property operations are guaranteed to produce one value per element
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->push_back(element_idx);
			
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
#ifdef DEBUG
			if (p_parent_node->children_.size() != 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): (internal error) expected 0 children for identifier node." << eidos_terminate(parent_token);
#endif
			
			EidosValue *identifier_value = global_symbols_.GetValueOrRaiseForToken(p_parent_node->token_);
			
			// OK, a little bit of trickiness here.  We've got the base value from the symbol table.  The problem is that it
			// could be one of our const singleton subclasses, for speed.  We almost never change EidosValue instances once
			// they are constructed, which is why we can use immutable subclasses so pervasively.  But this is one place –
			// the only place, in fact, I think – where that can bite us, because we do in fact need to modify the original
			// EidosValue.  The fix is to detect that we have an immutable value, and actually replace it in the symbol table
			// with a mutable copy that we can manipulate.  A little gross, but this is the price we pay for speed...
			if (!identifier_value->IsMutable())
			{
				const std::string &symbol_name = p_parent_node->token_->token_string_;
				
				identifier_value = identifier_value->MutableCopy();
				global_symbols_.SetValueForSymbol(symbol_name, identifier_value);	// takes ownership from us
			}
			
			*p_base_value_ptr = identifier_value;
			
			int number_of_elements = identifier_value->Count();	// this value is already defined, so this is fast
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->push_back(element_idx);
			
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): unexpected node token type " << token_type << "; lvalue required." << eidos_terminate(parent_token);
			break;
	}
}

void EidosInterpreter::_AssignRValueToLValue(EidosValue *p_rvalue, const EidosASTNode *p_lvalue_node)
{
	// This function expects an error range to be set bracketing it externally,
	// so no blame token is needed here.
	
	EidosTokenType token_type = p_lvalue_node->token_->token_type_;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
	{
		*execution_log_ << IndentString(execution_log_indent_) << "_AssignRValueToLValue() : lvalue token ";
		p_lvalue_node->PrintToken(*execution_log_);
		*execution_log_ << "\n";
	}
#endif
	
	switch (token_type)
	{
		case EidosTokenType::kTokenLBracket:
		{
#ifdef DEBUG
			if (p_lvalue_node->children_.size() != 2)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) expected 2 children for '[' node." << eidos_terminate(nullptr);
#endif
			
			EidosValue *base_value;
			EidosGlobalStringID property_string_id = gEidosID_none;
			vector<int> indices;
			
			_ProcessSubscriptAssignment(&base_value, &property_string_id, &indices, p_lvalue_node);
			
			int index_count = (int)indices.size();
			int rvalue_count = p_rvalue->Count();
			
			if (rvalue_count == 1)
			{
				if (property_string_id == gEidosID_none)
				{
					if (!TypeCheckAssignmentOfEidosValueIntoEidosValue(p_rvalue, base_value))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): type mismatch in assignment (" << p_rvalue->Type() << " versus " << base_value->Type() << ")." << eidos_terminate(nullptr);
					
					// we have a multiplex assignment of one value to (maybe) more than one index in a symbol host: x[5:10] = 10
					for (int value_idx = 0; value_idx < index_count; value_idx++)
						base_value->SetValueAtIndex(indices[value_idx], p_rvalue, nullptr);
				}
				else
				{
					// we have a multiplex assignment of one value to (maybe) more than one index in a property of a symbol host: x.foo[5:10] = 10
					// here we use the guarantee that the member operator returns one result per element, and that elements following sharing semantics,
					// to rearrange this assignment from host.property[indices] = rvalue to host[indices].property = rvalue; this must be equivalent!
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue *temp_lvalue = base_value->GetValueAtIndex(indices[value_idx], nullptr);
						
						if (temp_lvalue->Type() != EidosValueType::kValueObject)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) dot operator used with non-object value." << eidos_terminate(nullptr);
						
						static_cast<EidosValue_Object *>(temp_lvalue)->SetPropertyOfElements(property_string_id, p_rvalue);
						
						if (temp_lvalue->IsTemporary()) delete temp_lvalue;
					}
				}
			}
			else if (index_count == rvalue_count)
			{
				if (property_string_id == gEidosID_none)
				{
					if (!TypeCheckAssignmentOfEidosValueIntoEidosValue(p_rvalue, base_value))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): type mismatch in assignment (" << p_rvalue->Type() << " versus " << base_value->Type() << ")." << eidos_terminate(nullptr);
					
					// we have a one-to-one assignment of values to indices in a symbol host: x[5:10] = 5:10
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue *temp_rvalue = p_rvalue->GetValueAtIndex(value_idx, nullptr);
						
						base_value->SetValueAtIndex(indices[value_idx], temp_rvalue, nullptr);
						
						if (temp_rvalue->IsTemporary()) delete temp_rvalue;
					}
				}
				else
				{
					// we have a one-to-one assignment of values to indices in a property of a symbol host: x.foo[5:10] = 5:10
					// as above, we rearrange this assignment from host.property[indices1] = rvalue[indices2] to host[indices1].property = rvalue[indices2]
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue *temp_lvalue = base_value->GetValueAtIndex(indices[value_idx], nullptr);
						EidosValue *temp_rvalue = p_rvalue->GetValueAtIndex(value_idx, nullptr);
						
						if (temp_lvalue->Type() != EidosValueType::kValueObject)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) dot operator used with non-object value." << eidos_terminate(nullptr);
						
						static_cast<EidosValue_Object *>(temp_lvalue)->SetPropertyOfElements(property_string_id, temp_rvalue);
						
						if (temp_lvalue->IsTemporary()) delete temp_lvalue;
						if (temp_rvalue->IsTemporary()) delete temp_rvalue;
					}
				}
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): assignment to a subscript requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << eidos_terminate(nullptr);
			}
			
			break;
		}
		case EidosTokenType::kTokenDot:
		{
#ifdef DEBUG
			if (p_lvalue_node->children_.size() != 2)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) expected 2 children for '.' node." << eidos_terminate(nullptr);
#endif
			
			EidosValue *first_child_value = FastEvaluateNode(p_lvalue_node->children_[0]);
			EidosValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != EidosValueType::kValueObject)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(nullptr);
			}
			
			EidosASTNode *second_child_node = p_lvalue_node->children_[1];
			
			if (second_child_node->token_->token_type_ != EidosTokenType::kTokenIdentifier)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(nullptr);
			}
			
			// OK, we have <object type>.<identifier>; we can work with that
			static_cast<EidosValue_Object *>(first_child_value)->SetPropertyOfElements(second_child_node->cached_stringID_, p_rvalue);
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
#ifdef DEBUG
			if (p_lvalue_node->children_.size() != 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) expected 0 children for identifier node." << eidos_terminate(nullptr);
#endif
			
			// Simple identifier; the symbol host is the global symbol table, at least for now
			global_symbols_.SetValueForSymbol(p_lvalue_node->token_->token_string_, p_rvalue);
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): unexpected node token type " << token_type << "; lvalue required." << eidos_terminate(nullptr);
			break;
	}
}

void EidosInterpreter::NullReturnRaiseForNode(const EidosASTNode *p_node)
{
	EIDOS_TERMINATION << "ERROR (EidosInterpreter::NullReturnRaiseForNode): nullptr returned from evaluation of token type " << p_node->token_->token_type_ << "." << eidos_terminate(p_node->token_);
}

EidosValue *EidosInterpreter::EvaluateNode(const EidosASTNode *p_node)
{
	EidosValue *result;
	
	// The structure here avoids doing a break in the non-error case; a bit faster.
	switch (p_node->token_->token_type_)
	{
		case EidosTokenType::kTokenSemicolon:	result = Evaluate_NullStatement(p_node);		if (result) return result;	break;
		case EidosTokenType::kTokenColon:		result = Evaluate_RangeExpr(p_node);			if (result) return result;	break;
		case EidosTokenType::kTokenLBrace:		result = Evaluate_CompoundStatement(p_node);	if (result) return result;	break;
		case EidosTokenType::kTokenLParen:		result = Evaluate_FunctionCall(p_node);			if (result) return result;	break;
		case EidosTokenType::kTokenLBracket:	result = Evaluate_Subset(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenDot:			result = Evaluate_MemberRef(p_node);			if (result) return result;	break;
		case EidosTokenType::kTokenPlus:		result = Evaluate_Plus(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenMinus:		result = Evaluate_Minus(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenMod:			result = Evaluate_Mod(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenMult:		result = Evaluate_Mult(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenExp:			result = Evaluate_Exp(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenAnd:			result = Evaluate_And(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenOr:			result = Evaluate_Or(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenDiv:			result = Evaluate_Div(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenAssign:		result = Evaluate_Assign(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenEq:			result = Evaluate_Eq(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenLt:			result = Evaluate_Lt(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenLtEq:		result = Evaluate_LtEq(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenGt:			result = Evaluate_Gt(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenGtEq:		result = Evaluate_GtEq(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenNot:			result = Evaluate_Not(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenNotEq:		result = Evaluate_NotEq(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenNumber:		result = Evaluate_Number(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenString:		result = Evaluate_String(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenIdentifier:	result = Evaluate_Identifier(p_node);			if (result) return result;	break;
		case EidosTokenType::kTokenIf:			result = Evaluate_If(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenDo:			result = Evaluate_Do(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenWhile:		result = Evaluate_While(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenFor:			result = Evaluate_For(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenNext:		result = Evaluate_Next(p_node);					if (result) return result;	break;
		case EidosTokenType::kTokenBreak:		result = Evaluate_Break(p_node);				if (result) return result;	break;
		case EidosTokenType::kTokenReturn:		result = Evaluate_Return(p_node);				if (result) return result;	break;
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateNode): unexpected node token type " << p_node->token_->token_type_ << "." << eidos_terminate(p_node->token_);
			result = nullptr;
			break;
	}
	
	NullReturnRaiseForNode(p_node);
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_NullStatement(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_NullStatement() entered\n";
#endif
	
#ifdef DEBUG
	if (p_node->children_.size() != 0)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NullStatement): (internal error) expected 0 children." << eidos_terminate(p_node->token_);
#endif
	
	EidosValue *result = gStaticEidosValueNULLInvisible;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_NullStatement() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_CompoundStatement(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_CompoundStatement() entered\n";
#endif
	
	EidosValue *result = gStaticEidosValueNULLInvisible;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		if (result->IsTemporary()) delete result;
		
		result = FastEvaluateNode(child_node);
		
		// a next, break, or return makes us exit immediately, out to the (presumably enclosing) loop evaluator
		if (next_statement_hit_ || break_statement_hit_ || return_statement_hit_)
			break;
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_CompoundStatement() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_RangeExpr(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_RangeExpr() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_RangeExpr): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	bool too_wide = false;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_RangeExpr): operand type " << first_child_type << " is not supported by the ':' operator." << eidos_terminate(operator_token);
	}
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_RangeExpr): operand type " << second_child_type << " is not supported by the ':' operator." << eidos_terminate(operator_token);
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != 1) || (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_RangeExpr): operands of the ':' operator must have size() == 1." << eidos_terminate(operator_token);
	}
	
	// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
	bool underflow = false;
	
	if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
	{
		int64_t first_int = first_child_value->IntAtIndex(0, operator_token);
		int64_t second_int = second_child_value->IntAtIndex(0, operator_token);
		
		EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
		
		if (first_int <= second_int)
		{
			if (second_int - first_int >= 1000000)
				too_wide = true;
			else
				for (int64_t range_index = first_int; range_index <= second_int; ++range_index)
					int_result->PushInt(range_index);
		}
		else
		{
			if (first_int - second_int >= 1000000)
				too_wide = true;
			else
				for (int64_t range_index = first_int; range_index >= second_int; --range_index)
					int_result->PushInt(range_index);
		}
		
		result = int_result;
	}
	else
	{
		double first_float = first_child_value->FloatAtIndex(0, operator_token);
		double second_float = second_child_value->FloatAtIndex(0, operator_token);
		
		if (isnan(first_float) || isnan(second_float))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_RangeExpr): operands of the ':' operator must not be NAN." << eidos_terminate(operator_token);
		}
		
		EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
		
		if (first_float <= second_float)
		{
			if (second_float - first_float >= 1000000)
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
			if (first_float - second_float >= 1000000)
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
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_RangeExpr): the floating-point range could not be constructed due to underflow." << eidos_terminate(operator_token);
	}
	if (too_wide)
	{
		if (result->IsTemporary()) delete result;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_RangeExpr): a range with more than 1000000 entries cannot be constructed." << eidos_terminate(operator_token);
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_RangeExpr() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_FunctionCall(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_FunctionCall() entered\n";
#endif
	
	EidosValue *result = nullptr;
	
	// We do not evaluate the function name node (our first child) to get a function object; there is no such type in
	// Eidos for now.  Instead, we extract the identifier name directly from the node and work with it.  If the
	// node is an identifier, it is a function call; if it is a dot operator, it is a method call; other constructs
	// are illegal since expressions cannot evaluate to function objects, there being no function objects in Eidos.
	EidosASTNode *function_name_node = p_node->children_[0];
	EidosTokenType function_name_token_type = function_name_node->token_->token_type_;
	
	const string *function_name = nullptr;
	const EidosFunctionSignature *function_signature = nullptr;
	EidosGlobalStringID method_id = gEidosID_none;
	EidosValue_Object *method_object = nullptr;
	EidosToken *call_identifier_token = nullptr;
	
	if (function_name_token_type == EidosTokenType::kTokenIdentifier)
	{
		// OK, we have <identifier>(...); that's a well-formed function call
		call_identifier_token = function_name_node->token_;
		function_name = &(call_identifier_token->token_string_);
		function_signature = function_name_node->cached_signature_;
	}
	else if (function_name_token_type == EidosTokenType::kTokenDot)
	{
#ifdef DEBUG
		if (function_name_node->children_.size() != 2)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionCall): (internal error) expected 2 children for '.' node." << eidos_terminate(function_name_node->token_);
#endif
		
		EidosValue *first_child_value = FastEvaluateNode(function_name_node->children_[0]);
		EidosValueType first_child_type = first_child_value->Type();
		
		if (first_child_type != EidosValueType::kValueObject)
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionCall): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(function_name_node->token_);
		}
		
		EidosASTNode *second_child_node = function_name_node->children_[1];
		
		if (second_child_node->token_->token_type_ != EidosTokenType::kTokenIdentifier)
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionCall): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(function_name_node->token_);
		}
		
		// OK, we have <object type>.<identifier>(...); that's a well-formed method call
		call_identifier_token = second_child_node->token_;
		method_id = second_child_node->cached_stringID_;
		method_object = static_cast<EidosValue_Object *>(first_child_value);	// guaranteed by the Type() call above
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionCall): type " << function_name_token_type << " is not supported by the '()' operator (illegal operand for a function call operation)." << eidos_terminate(function_name_node->token_);
		function_name = nullptr;	// to get rid of a warning
	}
	
	// Evaluate all arguments; note this occurs before the function call itself is evaluated at all
	const std::vector<EidosASTNode *> &node_children = p_node->children_;
	auto node_children_end = node_children.end();
	int arguments_count = 0;
	
	for (auto child_iter = node_children.begin() + 1; child_iter != node_children_end; ++child_iter)
	{
		EidosASTNode *child = *child_iter;
		
		if (child->token_->token_type_ == EidosTokenType::kTokenComma)
			arguments_count += child->children_.size();
		else
			arguments_count++;
	}
	
	if (arguments_count <= 5)
	{
		// We have 5 or fewer arguments, so use a stack-local array of arguments to avoid the malloc/free
		EidosValue *(arguments_array[5]);
		int argument_index = 0;
		
		for (auto child_iter = node_children.begin() + 1; child_iter != node_children_end; ++child_iter)
		{
			EidosASTNode *child = *child_iter;
			
			if (child->token_->token_type_ == EidosTokenType::kTokenComma)
			{
				// a child with token type kTokenComma is an argument list node; we need to take its children and evaluate them
				std::vector<EidosASTNode *> &child_children = child->children_;
				auto end_iterator = child_children.end();
				
				for (auto arg_list_iter = child_children.begin(); arg_list_iter != end_iterator; ++arg_list_iter)
					arguments_array[argument_index++] = FastEvaluateNode(*arg_list_iter);
			}
			else
			{
				// all other children get evaluated, and the results added to the arguments vector
				arguments_array[argument_index++] = FastEvaluateNode(child);
			}
		}
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(call_identifier_token);
		
		// We offload the actual work to ExecuteMethodCall() / ExecuteFunctionCall() to keep things simple here
		if (method_object)
			result = ExecuteMethodCall(method_object, method_id, arguments_array, arguments_count);
		else
			result = ExecuteFunctionCall(*function_name, function_signature, arguments_array, arguments_count);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
		
		// And now we can free the arguments
		for (argument_index = 0; argument_index < arguments_count; ++argument_index)
		{
			EidosValue *arg = arguments_array[argument_index];
			
			if (arg->IsTemporary()) delete arg;
		}
	}
	else
	{
		// We have a lot of arguments, so we need to use a vector
		vector<EidosValue*> arguments;
		
		for (auto child_iter = node_children.begin() + 1; child_iter != node_children_end; ++child_iter)
		{
			EidosASTNode *child = *child_iter;
			
			if (child->token_->token_type_ == EidosTokenType::kTokenComma)
			{
				// a child with token type kTokenComma is an argument list node; we need to take its children and evaluate them
				std::vector<EidosASTNode *> &child_children = child->children_;
				auto end_iterator = child_children.end();
				
				for (auto arg_list_iter = child_children.begin(); arg_list_iter != end_iterator; ++arg_list_iter)
					arguments.push_back(FastEvaluateNode(*arg_list_iter));
			}
			else
			{
				// all other children get evaluated, and the results added to the arguments vector
				arguments.push_back(FastEvaluateNode(child));
			}
		}
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(call_identifier_token);
		
		// We offload the actual work to ExecuteMethodCall() / ExecuteFunctionCall() to keep things simple here
		EidosValue **arguments_ptr = arguments.data();
		
		if (method_object)
			result = ExecuteMethodCall(method_object, method_id, arguments_ptr, arguments_count);
		else
			result = ExecuteFunctionCall(*function_name, function_signature, arguments_ptr, arguments_count);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
		
		// And now we can free the arguments
		for (auto arg_iter = arguments.begin(); arg_iter != arguments.end(); ++arg_iter)
		{
			EidosValue *arg = *arg_iter;
			
			if (arg->IsTemporary()) delete arg;
		}
	}
	
	// And if it was a method call, we can free the method object now, too
	if (method_object && method_object->IsTemporary()) delete method_object;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_FunctionCall() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Subset(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Subset() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	
	if (first_child_type == EidosValueType::kValueNULL)
	{
		// Any subscript of NULL returns NULL
		result = gStaticEidosValueNULL;
		
		if (first_child_value->IsTemporary()) delete first_child_value;
	}
	else
	{
		EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
		EidosValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat) && (second_child_type != EidosValueType::kValueLogical))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): index operand type " << second_child_type << " is not supported by the '[]' operator." << eidos_terminate(operator_token);
		}
		
		// OK, we can definitely do this subset
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (second_child_type == EidosValueType::kValueLogical)
		{
			// Subsetting with a logical vector means the vectors must match in length; indices with a T value will be taken
			if (first_child_count != second_child_count)
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				if (second_child_value->IsTemporary()) delete second_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << eidos_terminate(operator_token);
			}
			
			// Subsetting with a logical vector does not attempt to allocate singleton values, for now; seems unlikely to be a frequently hit case
			result = first_child_value->NewMatchingType();
			
			for (int value_idx = 0; value_idx < second_child_count; value_idx++)
			{
				bool bool_value = second_child_value->LogicalAtIndex(value_idx, operator_token);
				
				if (bool_value)
					result->PushValueFromIndexOfEidosValue(value_idx, first_child_value, operator_token);
			}
		}
		else
		{
			if (second_child_count == 1)
			{
				// Subsetting with a singleton int/float vector is common and should return a singleton value for speed
				// This is guaranteed to return a singleton value (when available), and bounds-checks for us
				result = first_child_value->GetValueAtIndex((int)second_child_value->IntAtIndex(0, operator_token), operator_token);
			}
			else
			{
				// Subsetting with a int/float vector can use a vector of any length; the specific indices referenced will be taken
				result = first_child_value->NewMatchingType();
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
					
					if ((index_value < 0) || (index_value >= first_child_count))
					{
						if (first_child_value->IsTemporary()) delete first_child_value;
						if (second_child_value->IsTemporary()) delete second_child_value;
						if (result->IsTemporary()) delete result;
						
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
					}
					else
						result->PushValueFromIndexOfEidosValue((int)index_value, first_child_value, operator_token);
				}
			}
		}
		
		// Free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Subset() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_MemberRef(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_MemberRef() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	
	if (first_child_type != EidosValueType::kValueObject)
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(operator_token);
	}
	
	EidosASTNode *second_child_node = p_node->children_[1];
	EidosToken *second_child_token = second_child_node->token_;
	
	if (second_child_token->token_type_ != EidosTokenType::kTokenIdentifier)
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(operator_token);
	}
	
	// If an error occurs inside a function or method call, we want to highlight the call
	EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(second_child_token);
	
	// We offload the actual work to ExecuteMethodCall() / ExecuteFunctionCall() to keep things simple here
	EidosGlobalStringID property_string_ID = second_child_node->cached_stringID_;
	result = static_cast<EidosValue_Object *>(first_child_value)->GetPropertyOfElements(property_string_ID);
	
	// Forget the function token, since it is not responsible for any future errors
	EidosScript::RestoreErrorPosition(error_pos_save);
	
	// free our operand
	if (first_child_value->IsTemporary()) delete first_child_value;
	
	// check result; this should never happen, since GetProperty should check
	if (!result)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): undefined property " << StringForEidosGlobalStringID(property_string_ID) << "." << eidos_terminate(operator_token);
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_MemberRef() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Plus(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Plus() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if ((p_node->children_.size() != 1) && (p_node->children_.size() != 2))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): (internal error) expected 1 or 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	
	if (p_node->children_.size() == 1)
	{
		// unary plus is a no-op, but legal only for numeric types
		if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): operand type " << first_child_type << " is not supported by the unary '+' operator." << eidos_terminate(operator_token);
		}
		
		result = first_child_value;
		
		// don't free our operand, because result points to it
	}
	else
	{
		// binary plus is legal either between two numeric types, or between a string and any other non-NULL operand
		EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
		EidosValueType second_child_type = second_child_value->Type();
		
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
		
		if ((first_child_type == EidosValueType::kValueString) || (second_child_type == EidosValueType::kValueString))
		{
			// If either operand is a string, then we are doing string concatenation, with promotion to strings if needed
			if (((first_child_type == EidosValueType::kValueNULL) || (second_child_type == EidosValueType::kValueNULL)))
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				if (second_child_value->IsTemporary()) delete second_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the binary '+' operator does not support operands of type NULL." << eidos_terminate(operator_token);
			}
			
			if ((first_child_count == 1) && (second_child_count == 1))
			{
				result = new EidosValue_String_singleton_const(first_child_value->StringAtIndex(0, operator_token) + second_child_value->StringAtIndex(0, operator_token));
			}
			else
			{
				EidosValue_String_vector *string_result = new EidosValue_String_vector();
				result = string_result;
				
				if (first_child_count == second_child_count)
				{
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						string_result->PushString(first_child_value->StringAtIndex(value_index, operator_token) + second_child_value->StringAtIndex(value_index, operator_token));
				}
				else if (first_child_count == 1)
				{
					string singleton_int = first_child_value->StringAtIndex(0, operator_token);
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						string_result->PushString(singleton_int + second_child_value->StringAtIndex(value_index, operator_token));
				}
				else if (second_child_count == 1)
				{
					string singleton_int = second_child_value->StringAtIndex(0, operator_token);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						string_result->PushString(first_child_value->StringAtIndex(value_index, operator_token) + singleton_int);
				}
			}
		}
		else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					// This is an overflow-safe version of:
					//result = new EidosValue_Int_singleton_const(first_child_value->IntAtIndex(0, operator_token) + second_child_value->IntAtIndex(0, operator_token));
					
					int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
					int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
					int64_t add_result;
					bool overflow = __builtin_saddll_overflow(first_operand, second_operand, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << eidos_terminate(operator_token);
					
					result = new EidosValue_Int_singleton_const(add_result);
				}
				else
				{
					EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
					result = int_result;
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
					{
						// This is an overflow-safe version of:
						//int_result->PushInt(first_child_value->IntAtIndex(value_index, operator_token) + second_child_value->IntAtIndex(value_index, operator_token));
						
						int64_t first_operand = first_child_value->IntAtIndex(value_index, operator_token);
						int64_t second_operand = second_child_value->IntAtIndex(value_index, operator_token);
						int64_t add_result;
						bool overflow = __builtin_saddll_overflow(first_operand, second_operand, &add_result);
						
						if (overflow)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << eidos_terminate(operator_token);
						
						int_result->PushInt(add_result);
					}
				}
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(singleton_int + second_child_value->IntAtIndex(value_index, operator_token));
					
					int64_t second_operand = second_child_value->IntAtIndex(value_index, operator_token);
					int64_t add_result;
					bool overflow = __builtin_saddll_overflow(singleton_int, second_operand, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << eidos_terminate(operator_token);
					
					int_result->PushInt(add_result);
				}
			}
			else if (second_child_count == 1)
			{
				int64_t singleton_int = second_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(first_child_value->IntAtIndex(value_index, operator_token) + singleton_int);
					
					int64_t first_operand = first_child_value->IntAtIndex(value_index, operator_token);
					int64_t add_result;
					bool overflow = __builtin_saddll_overflow(first_operand, singleton_int, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << eidos_terminate(operator_token);
					
					int_result->PushInt(add_result);
				}
			}
		}
		else
		{
			if (((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat)) || ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat)))
			{
				if (first_child_value->IsTemporary()) delete first_child_value;
				if (second_child_value->IsTemporary()) delete second_child_value;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the combination of operand types " << first_child_type << " and " << second_child_type << " is not supported by the binary '+' operator." << eidos_terminate(operator_token);
			}
			
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					result = new EidosValue_Float_singleton_const(first_child_value->FloatAtIndex(0, operator_token) + second_child_value->FloatAtIndex(0, operator_token));
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) + second_child_value->FloatAtIndex(value_index, operator_token));
				}
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					float_result->PushFloat(singleton_float + second_child_value->FloatAtIndex(value_index, operator_token));
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) + singleton_float);
			}
		}
		
		// free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Plus() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Minus(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Minus() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if ((p_node->children_.size() != 1) && (p_node->children_.size() != 2))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): (internal error) expected 1 or 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): operand type " << first_child_type << " is not supported by the '-' operator." << eidos_terminate(operator_token);
	}
	
	int first_child_count = first_child_value->Count();
	
	if (p_node->children_.size() == 1)
	{
		// unary minus
		if (first_child_type == EidosValueType::kValueInt)
		{
			if (first_child_count == 1)
			{
				// This is an overflow-safe version of:
				//result = new EidosValue_Int_singleton_const(-first_child_value->IntAtIndex(0, operator_token));
				
				int64_t operand = first_child_value->IntAtIndex(0, operator_token);
				int64_t subtract_result;
				bool overflow = __builtin_ssubll_overflow(0, operand, &subtract_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer negation overflow with the unary '-' operator." << eidos_terminate(operator_token);
				
				result = new EidosValue_Int_singleton_const(subtract_result);
			}
			else
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(-first_child_value->IntAtIndex(value_index, operator_token));
					
					int64_t operand = first_child_value->IntAtIndex(value_index, operator_token);
					int64_t subtract_result;
					bool overflow = __builtin_ssubll_overflow(0, operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer negation overflow with the unary '-' operator." << eidos_terminate(operator_token);
					
					int_result->PushInt(subtract_result);
				}
			}
		}
		else
		{
			if (first_child_count == 1)
			{
				result = new EidosValue_Float_singleton_const(-first_child_value->FloatAtIndex(0, operator_token));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(-first_child_value->FloatAtIndex(value_index, operator_token));
			}
		}
		
		// free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
	}
	else
	{
		// binary minus
		EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
		EidosValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): operand type " << second_child_type << " is not supported by the '-' operator." << eidos_terminate(operator_token);
		}
		
		int second_child_count = second_child_value->Count();
		
		if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): the '-' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
		
		if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					// This is an overflow-safe version of:
					//result = new EidosValue_Int_singleton_const(first_child_value->IntAtIndex(0, operator_token) - second_child_value->IntAtIndex(0, operator_token));
					
					int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
					int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
					int64_t subtract_result;
					bool overflow = __builtin_ssubll_overflow(first_operand, second_operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << eidos_terminate(operator_token);
					
					result = new EidosValue_Int_singleton_const(subtract_result);
				}
				else
				{
					EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
					result = int_result;
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
					{
						// This is an overflow-safe version of:
						//int_result->PushInt(first_child_value->IntAtIndex(value_index, operator_token) - second_child_value->IntAtIndex(value_index, operator_token));
						
						int64_t first_operand = first_child_value->IntAtIndex(value_index, operator_token);
						int64_t second_operand = second_child_value->IntAtIndex(value_index, operator_token);
						int64_t subtract_result;
						bool overflow = __builtin_ssubll_overflow(first_operand, second_operand, &subtract_result);
						
						if (overflow)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << eidos_terminate(operator_token);
						
						int_result->PushInt(subtract_result);
					}
				}
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(singleton_int - second_child_value->IntAtIndex(value_index, operator_token));
					
					int64_t second_operand = second_child_value->IntAtIndex(value_index, operator_token);
					int64_t subtract_result;
					bool overflow = __builtin_ssubll_overflow(singleton_int, second_operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << eidos_terminate(operator_token);
					
					int_result->PushInt(subtract_result);
				}
			}
			else if (second_child_count == 1)
			{
				int64_t singleton_int = second_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(first_child_value->IntAtIndex(value_index, operator_token) - singleton_int);
					
					int64_t first_operand = first_child_value->IntAtIndex(value_index, operator_token);
					int64_t subtract_result;
					bool overflow = __builtin_ssubll_overflow(first_operand, singleton_int, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << eidos_terminate(operator_token);
					
					int_result->PushInt(subtract_result);
				}
			}
		}
		else
		{
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					result = new EidosValue_Float_singleton_const(first_child_value->FloatAtIndex(0, operator_token) - second_child_value->FloatAtIndex(0, operator_token));
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) - second_child_value->FloatAtIndex(value_index, operator_token));
				}
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					float_result->PushFloat(singleton_float - second_child_value->FloatAtIndex(value_index, operator_token));
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) - singleton_float);
			}
		}
		
		// free our operands
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Minus() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Mod(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Mod() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): operand type " << first_child_type << " is not supported by the '%' operator." << eidos_terminate(operator_token);
	}
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): operand type " << second_child_type << " is not supported by the '%' operator." << eidos_terminate(operator_token);
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): the '%' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	/*
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, they will need to do float division and then use floor() and asInteger().  Alternatively, we could provide a separate
	// operator for integer division, as R does.  However, the definition of integer division is tricky and contested; best to let people do their own.
	// This decision applies also to modulo, for consistency.
	
	if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
	{
		// integer modulo has to check for division by zero and throw, otherwise we crash
		EidosValue_Int *int_result = new EidosValue_Int();
		
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
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): integer modulo by zero." << eidos_terminate(operator_token);
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
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): integer modulo by zero." << eidos_terminate(operator_token);
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
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): integer modulo by zero." << eidos_terminate(operator_token);
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
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				result = new EidosValue_Float_singleton_const(fmod(first_child_value->FloatAtIndex(0, operator_token), second_child_value->FloatAtIndex(0, operator_token)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(fmod(first_child_value->FloatAtIndex(value_index, operator_token), second_child_value->FloatAtIndex(value_index, operator_token)));
			}
		}
		else if (first_child_count == 1)
		{
			double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			result = float_result;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->PushFloat(fmod(singleton_float, second_child_value->FloatAtIndex(value_index, operator_token)));
		}
		else if (second_child_count == 1)
		{
			double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			result = float_result;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(fmod(first_child_value->FloatAtIndex(value_index, operator_token), singleton_float));
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Mod() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Mult(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Mult() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): operand type " << first_child_type << " is not supported by the '*' operator." << eidos_terminate(operator_token);
	}
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): operand type " << second_child_type << " is not supported by the '*' operator." << eidos_terminate(operator_token);
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if (first_child_count == second_child_count)
	{
		// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
		if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			if (first_child_count == 1)
			{
				// This is an overflow-safe version of:
				//result = new EidosValue_Int_singleton_const(first_child_value->IntAtIndex(0, operator_token) * second_child_value->IntAtIndex(0, operator_token));
				
				int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
				int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
				int64_t multiply_result;
				bool overflow = __builtin_smulll_overflow(first_operand, second_operand, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << eidos_terminate(operator_token);
				
				result = new EidosValue_Int_singleton_const(multiply_result);
			}
			else
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(first_child_value->IntAtIndex(value_index, operator_token) * second_child_value->IntAtIndex(value_index, operator_token));
					
					int64_t first_operand = first_child_value->IntAtIndex(value_index, operator_token);
					int64_t second_operand = second_child_value->IntAtIndex(value_index, operator_token);
					int64_t multiply_result;
					bool overflow = __builtin_smulll_overflow(first_operand, second_operand, &multiply_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << eidos_terminate(operator_token);
					
					int_result->PushInt(multiply_result);
				}
			}
		}
		else
		{
			if (first_child_count == 1)
			{
				result = new EidosValue_Float_singleton_const(first_child_value->FloatAtIndex(0, operator_token) * second_child_value->FloatAtIndex(0, operator_token));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) * second_child_value->FloatAtIndex(value_index, operator_token));
			}
		}
	}
	else if ((first_child_count == 1) || (second_child_count == 1))
	{
		EidosValue *one_count_child = ((first_child_count == 1) ? first_child_value : second_child_value);
		EidosValue *any_count_child = ((first_child_count == 1) ? second_child_value : first_child_value);
		int any_count = ((first_child_count == 1) ? second_child_count : first_child_count);
		
		// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
		if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			int64_t singleton_int = one_count_child->IntAtIndex(0, operator_token);
			EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
			result = int_result;
			
			for (int value_index = 0; value_index < any_count; ++value_index)
			{
				// This is an overflow-safe version of:
				//int_result->PushInt(any_count_child->IntAtIndex(value_index, operator_token) * singleton_int);
				
				int64_t first_operand = any_count_child->IntAtIndex(value_index, operator_token);
				int64_t multiply_result;
				bool overflow = __builtin_smulll_overflow(first_operand, singleton_int, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << eidos_terminate(operator_token);
				
				int_result->PushInt(multiply_result);
			}
		}
		else
		{
			double singleton_float = one_count_child->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			result = float_result;
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				float_result->PushFloat(any_count_child->FloatAtIndex(value_index, operator_token) * singleton_float);
		}
	}
	else
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): the '*' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Mult() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Div(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Div() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): operand type " << first_child_type << " is not supported by the '/' operator." << eidos_terminate(operator_token);
	}
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): operand type " << second_child_type << " is not supported by the '/' operator." << eidos_terminate(operator_token);
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): the '/' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	/*
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, they will need to do float division and then use floor() and asInteger().  Alternatively, we could provide a separate
	// operator for integer division, as R does.  However, the definition of integer division is tricky and contested; best to let people do their own.
	 
	if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
	{
		// integer division has to check for division by zero and throw, otherwise we crash
		EidosValue_Int *int_result = new EidosValue_Int();
		
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
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): integer divide by zero." << eidos_terminate(operator_token);
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
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): integer divide by zero." << eidos_terminate(operator_token);
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
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): integer divide by zero." << eidos_terminate(operator_token);
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
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				result = new EidosValue_Float_singleton_const(first_child_value->FloatAtIndex(0, operator_token) / second_child_value->FloatAtIndex(0, operator_token));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) / second_child_value->FloatAtIndex(value_index, operator_token));
			}
		}
		else if (first_child_count == 1)
		{
			double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			result = float_result;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->PushFloat(singleton_float / second_child_value->FloatAtIndex(value_index, operator_token));
		}
		else if (second_child_count == 1)
		{
			double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			result = float_result;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) / singleton_float);
		}
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Div() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Exp(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Exp() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): operand type " << first_child_type << " is not supported by the '^' operator." << eidos_terminate(operator_token);
	}
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): operand type " << second_child_type << " is not supported by the '^' operator." << eidos_terminate(operator_token);
	}
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): the '^' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	// Exponentiation always produces a float result; the user can cast back to integer if they really want
	EidosValue *result;
	
	if (first_child_count == second_child_count)
	{
		if (first_child_count == 1)
		{
			result = new EidosValue_Float_singleton_const(pow(first_child_value->FloatAtIndex(0, operator_token), second_child_value->FloatAtIndex(0, operator_token)));
		}
		else
		{
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			result = float_result;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(pow(first_child_value->FloatAtIndex(value_index, operator_token), second_child_value->FloatAtIndex(value_index, operator_token)));
		}
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
		result = float_result;
		
		for (int value_index = 0; value_index < second_child_count; ++value_index)
			float_result->PushFloat(pow(singleton_float, second_child_value->FloatAtIndex(value_index, operator_token)));
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
		result = float_result;
		
		for (int value_index = 0; value_index < first_child_count; ++value_index)
			float_result->PushFloat(pow(first_child_value->FloatAtIndex(value_index, operator_token), singleton_float));
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Exp() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_And(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_And() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() < 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): (internal error) expected 2+ children." << eidos_terminate(operator_token);
#endif
	
	// We try to avoid allocating a result object if we can.  If result==nullptr but result_count==1, bool_result contains the result so far.
	EidosValue_Logical *result = nullptr;
	bool bool_result = false;
	int result_count = 0;
	bool first_child = true;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		EidosValue *child_result = FastEvaluateNode(child_node);
		
		if (child_result == gStaticEidosValue_LogicalT)
		{
			// Handle a static singleton logical true super fast; no need for type check, count, etc
			if (first_child)
			{
				first_child = false;
				bool_result = true;
				result_count = 1;
			}
			// if we're not on the first child, doing an AND with T is a no-op; it does not even change the size of the result vector
		}
		else if (child_result == gStaticEidosValue_LogicalF)
		{
			// Handle a static singleton logical false super fast; no need for type check, count, etc
			if (first_child)
			{
				first_child = false;
				bool_result = false;
				result_count = 1;
			}
			else
			{
				if (result)
				{
					// we have a result allocated, so alter the values in it
					for (int value_index = 0; value_index < result_count; ++value_index)
						result->SetLogicalAtIndex(value_index, false, operator_token);
				}
				else
				{
					// we have no result allocated, so we must be using bool_result
					bool_result = false;
				}
			}
		}
		else
		{
			// Cases that the code above can't handle drop through to the more general case here
			EidosValueType child_type = child_result->Type();
			
			if ((child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueString) && (child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueFloat))
			{
				if (child_result->IsTemporary()) delete child_result;
				if (result && result->IsTemporary()) delete result;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): operand type " << child_type << " is not supported by the '&' operator." << eidos_terminate(operator_token);
			}
			
			int child_count = child_result->Count();
			
			if (first_child)
			{
				// if this is our first operand, we need to set up an initial result value from it
				first_child = false;
				
				if (child_count == 1)
				{
					// if we have a singleton, avoid allocating a result yet, by using bool_result instead
					bool_result = child_result->LogicalAtIndex(0, operator_token);
					result_count = 1;
				}
				else if ((child_type == EidosValueType::kValueLogical) && child_result->IsTemporary())
				{
					// child_result is a temporary logical EidosValue, so we can just take it over as our initial result
					result = (EidosValue_Logical *)child_result;
					result_count = child_count;
					
					continue;	// do not free child_result
				}
				else
				{
					// for other cases, we just clone child_result
					result = new EidosValue_Logical();
					result_count = child_count;
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->PushLogical(child_result->LogicalAtIndex(value_index, operator_token));
				}
			}
			else
			{
				// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
				if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
				{
					if (child_result->IsTemporary()) delete child_result;
					if (result && result->IsTemporary()) delete result;
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): operands to the '&' operator are not compatible in size()." << eidos_terminate(operator_token);
				}
				
				if (child_count == 1)
				{
					// if child_bool is T, it has no effect on result; if it is F, it turns result to all F
					bool child_bool = child_result->LogicalAtIndex(0, operator_token);
					
					if (!child_bool)
					{
						if (result)
						{
							// we have a result allocated, so alter the values in it
							for (int value_index = 0; value_index < result_count; ++value_index)
								result->SetLogicalAtIndex(value_index, false, operator_token);
						}
						else
						{
							// we have no result allocated, so we must be using bool_result
							bool_result = false;
						}
					}
				}
				else if (result_count == 1)
				{
					// we had a one-length result vector, but now we need to upscale it to match child_result
					bool result_bool;
					
					if (result)
					{
						// we have a result allocated; work with that
						result_bool = result->LogicalAtIndex(0, operator_token);
						
						if (result->IsTemporary()) delete result;
					}
					else
					{
						// no result allocated, so we now need to upgrade to an allocated result
						result_bool = bool_result;
					}
					
					result = new EidosValue_Logical();
					result_count = child_count;
					
					if (result_bool)
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->PushLogical(child_result->LogicalAtIndex(value_index, operator_token));
					else
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->PushLogical(false);
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is F we turn result to F
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (!child_result->LogicalAtIndex(value_index, operator_token))
							result->SetLogicalAtIndex(value_index, false, operator_token);
				}
			}
			
			// free our operand
			if (child_result->IsTemporary()) delete child_result;
		}
	}
	
	// if we avoided allocating a result, use a static logical value
	if (!result)
		result = (bool_result ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_And() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Or(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Or() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() < 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): (internal error) expected 2+ children." << eidos_terminate(operator_token);
#endif
	
	// We try to avoid allocating a result object if we can.  If result==nullptr but result_count==1, bool_result contains the result so far.
	EidosValue_Logical *result = nullptr;
	bool bool_result = false;
	int result_count = 0;
	bool first_child = true;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		EidosValue *child_result = FastEvaluateNode(child_node);
		
		if (child_result == gStaticEidosValue_LogicalT)
		{
			// Handle a static singleton logical true super fast; no need for type check, count, etc
			if (first_child)
			{
				first_child = false;
				bool_result = true;
				result_count = 1;
			}
			else
			{
				if (result)
				{
					// we have a result allocated, so alter the values in it
					for (int value_index = 0; value_index < result_count; ++value_index)
						result->SetLogicalAtIndex(value_index, true, operator_token);
				}
				else
				{
					// we have no result allocated, so we must be using bool_result
					bool_result = true;
				}
			}
		}
		else if (child_result == gStaticEidosValue_LogicalF)
		{
			// Handle a static singleton logical false super fast; no need for type check, count, etc
			if (first_child)
			{
				first_child = false;
				bool_result = false;
				result_count = 1;
			}
			// if we're not on the first child, doing an OR with F is a no-op; it does not even change the size of the result vector
		}
		else
		{
			// Cases that the code above can't handle drop through to the more general case here
			EidosValueType child_type = child_result->Type();
			
			if ((child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueString) && (child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueFloat))
			{
				if (child_result->IsTemporary()) delete child_result;
				if (result && result->IsTemporary()) delete result;
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): operand type " << child_type << " is not supported by the '|' operator." << eidos_terminate(operator_token);
			}
			
			int child_count = child_result->Count();
			
			if (first_child)
			{
				// if this is our first operand, we need to set up an initial result value from it
				first_child = false;
				
				if (child_count == 1)
				{
					// if we have a singleton, avoid allocating a result yet, by using bool_result instead
					bool_result = child_result->LogicalAtIndex(0, operator_token);
					result_count = 1;
				}
				else if ((child_type == EidosValueType::kValueLogical) && child_result->IsTemporary())
				{
					// child_result is a temporary logical EidosValue, so we can just take it over as our initial result
					result = (EidosValue_Logical *)child_result;
					result_count = child_count;
					
					continue;	// do not free child_result
				}
				else
				{
					// for other cases, we just clone child_result
					result = new EidosValue_Logical();
					result_count = child_count;
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->PushLogical(child_result->LogicalAtIndex(value_index, operator_token));
				}
			}
			else
			{
				// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
				if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
				{
					if (child_result->IsTemporary()) delete child_result;
					if (result && result->IsTemporary()) delete result;
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): operands to the '|' operator are not compatible in size()." << eidos_terminate(operator_token);
				}
				
				if (child_count == 1)
				{
					// if child_bool is F, it has no effect on result; if it is T, it turns result to all T
					bool child_bool = child_result->LogicalAtIndex(0, operator_token);
					
					if (child_bool)
					{
						if (result)
						{
							// we have a result allocated, so alter the values in it
							for (int value_index = 0; value_index < result_count; ++value_index)
								result->SetLogicalAtIndex(value_index, true, operator_token);
						}
						else
						{
							// we have no result allocated, so we must be using bool_result
							bool_result = true;
						}
					}
				}
				else if (result_count == 1)
				{
					// we had a one-length result vector, but now we need to upscale it to match child_result
					bool result_bool;
					
					if (result)
					{
						// we have a result allocated; work with that
						result_bool = result->LogicalAtIndex(0, operator_token);
						
						if (result->IsTemporary()) delete result;
					}
					else
					{
						// no result allocated, so we now need to upgrade to an allocated result
						result_bool = bool_result;
					}
					
					result = new EidosValue_Logical();
					result_count = child_count;
					
					if (result_bool)
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->PushLogical(true);
					else
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->PushLogical(child_result->LogicalAtIndex(value_index, operator_token));
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is T we turn result to T
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (child_result->LogicalAtIndex(value_index, operator_token))
							result->SetLogicalAtIndex(value_index, true, operator_token);
				}
			}
			
			// free our operand
			if (child_result->IsTemporary()) delete child_result;
		}
	}
	
	// if we avoided allocating a result, use a static logical value
	if (!result)
		result = (bool_result ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Or() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Not(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Not() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 1)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Not): (internal error) expected 1 child." << eidos_terminate(operator_token);
#endif
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_Logical *result;
	
	if (first_child_value == gStaticEidosValue_LogicalT)
	{
		// Handle a static singleton logical true super fast; no need for type check, count, etc
		result = gStaticEidosValue_LogicalF;
	}
	else if (first_child_value == gStaticEidosValue_LogicalF)
	{
		// Handle a static singleton logical false super fast; no need for type check, count, etc
		result = gStaticEidosValue_LogicalT;
	}
	else
	{
		EidosValueType first_child_type = first_child_value->Type();
		
		if ((first_child_type != EidosValueType::kValueLogical) && (first_child_type != EidosValueType::kValueString) && (first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Not): operand type " << first_child_type << " is not supported by the '!' operator." << eidos_terminate(operator_token);
		}
		
		int first_child_count = first_child_value->Count();
		
		if (first_child_count == 1)
		{
			// If we're generating a singleton result, use cached static logical values
			result = (first_child_value->LogicalAtIndex(0, operator_token) ? gStaticEidosValue_LogicalF : gStaticEidosValue_LogicalT);
		}
		else
		{
			result = new EidosValue_Logical();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				result->PushLogical(!first_child_value->LogicalAtIndex(value_index, operator_token));
		}
		
		// free our operand
		if (first_child_value->IsTemporary()) delete first_child_value;
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Not() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Assign(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Assign() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosASTNode *lvalue_node = p_node->children_[0];
	EidosValue *rvalue = FastEvaluateNode(p_node->children_[1]);
	
	EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(operator_token);
	
	_AssignRValueToLValue(rvalue, lvalue_node);		// disposes of rvalue somehow
	
	EidosScript::RestoreErrorPosition(error_pos_save);
	
	// by design, assignment does not yield a usable value; instead it produces NULL – this prevents the error "if (x = 3) ..."
	// since the condition is NULL and will raise; the loss of legitimate uses of "if (x = 3)" seems a small price to pay
	EidosValue *result = gStaticEidosValueNULLInvisible;
	
	// free our operand
	if (rvalue->IsTemporary()) delete rvalue;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Assign() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Eq(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Eq() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue_Logical *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, 0, operator_token);
				
				result = (compare_result == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				result = new EidosValue_Logical;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, value_index, operator_token);
					
					result->PushLogical(compare_result == 0);
				}
			}
		}
		else if (first_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, value_index, operator_token);
				
				result->PushLogical(compare_result == 0);
			}
		}
		else if (second_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, 0, operator_token);
				
				result->PushLogical(compare_result == 0);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): the '==' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): testing NULL with the '==' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Eq() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Lt(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Lt() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue_Logical *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): the '<' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, 0, operator_token);
				
				result = (compare_result == -1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				result = new EidosValue_Logical;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, value_index, operator_token);
					
					result->PushLogical(compare_result == -1);
				}
			}
		}
		else if (first_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, value_index, operator_token);
				
				result->PushLogical(compare_result == -1);
			}
		}
		else if (second_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, 0, operator_token);
				
				result->PushLogical(compare_result == -1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): the '<' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): testing NULL with the '<' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Lt() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_LtEq(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_LtEq() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue_Logical *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): the '<=' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, 0, operator_token);
				
				result = (compare_result != 1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				result = new EidosValue_Logical;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, value_index, operator_token);
					
					result->PushLogical(compare_result != 1);
				}
			}
		}
		else if (first_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, value_index, operator_token);
				
				result->PushLogical(compare_result != 1);
			}
		}
		else if (second_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, 0, operator_token);
				
				result->PushLogical(compare_result != 1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): the '<=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): testing NULL with the '<=' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_LtEq() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Gt(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Gt() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue_Logical *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): the '>' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, 0, operator_token);
				
				result = (compare_result == 1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				result = new EidosValue_Logical;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, value_index, operator_token);
					
					result->PushLogical(compare_result == 1);
				}
			}
		}
		else if (first_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, value_index, operator_token);
				
				result->PushLogical(compare_result == 1);
			}
		}
		else if (second_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, 0, operator_token);
				
				result->PushLogical(compare_result == 1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): the '>' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): testing NULL with the '>' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Gt() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_GtEq(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_GtEq() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue_Logical *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): the '>=' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, 0, operator_token);
				
				result = (compare_result != -1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				result = new EidosValue_Logical;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, value_index, operator_token);
					
					result->PushLogical(compare_result != -1);
				}
			}
		}
		else if (first_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, value_index, operator_token);
				
				result->PushLogical(compare_result != -1);
			}
		}
		else if (second_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, 0, operator_token);
				
				result->PushLogical(compare_result != -1);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): the '>=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): testing NULL with the '>=' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_GtEq() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_NotEq(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_NotEq() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): (internal error) expected 2 children." << eidos_terminate(operator_token);
#endif
	
	EidosValue_Logical *result = nullptr;
	
	EidosValue *first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue *second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, 0, operator_token);
				
				result = (compare_result != 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				result = new EidosValue_Logical;
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, value_index, operator_token);
					
					result->PushLogical(compare_result != 0);
				}
			}
		}
		else if (first_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, 0, second_child_value, value_index, operator_token);
				
				result->PushLogical(compare_result != 0);
			}
		}
		else if (second_child_count == 1)
		{
			result = new EidosValue_Logical;
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = CompareEidosValues(first_child_value, value_index, second_child_value, 0, operator_token);
				
				result->PushLogical(compare_result != 0);
			}
		}
		else
		{
			if (first_child_value->IsTemporary()) delete first_child_value;
			if (second_child_value->IsTemporary()) delete second_child_value;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): the '!=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		if (first_child_value->IsTemporary()) delete first_child_value;
		if (second_child_value->IsTemporary()) delete second_child_value;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): testing NULL with the '!=' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	// free our operands
	if (first_child_value->IsTemporary()) delete first_child_value;
	if (second_child_value->IsTemporary()) delete second_child_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_NotEq() : return == " << *result << "\n";
#endif
	
	return result;
}

// A utility static method for getting an int for a string outside of a EidosInterpreter session
int64_t EidosInterpreter::IntegerForString(const std::string &p_number_string, const EidosToken *p_blame_token)
{
	// This needs to use the same criteria as NumericValueForString() below; it raises if the number is a float.
	const char *c_str = p_number_string.c_str();
	char *last_used_char = nullptr;
	
	errno = 0;
	
	if ((p_number_string.find('.') != string::npos) || (p_number_string.find('-') != string::npos))
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::IntegerForString): \"" << p_number_string << "\" could not be represented as an integer (decimal or negative exponent)." << eidos_terminate(p_blame_token);
		return 0;
	}
	else if ((p_number_string.find('e') != string::npos) || (p_number_string.find('E') != string::npos))	// has an exponent
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::IntegerForString): \"" << p_number_string << "\" could not be represented as an integer (strtod conversion error)." << eidos_terminate(p_blame_token);
		
		// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
		if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::IntegerForString): \"" << p_number_string << "\" could not be represented as an integer (out of range)." << eidos_terminate(p_blame_token);
		
		return static_cast<int64_t>(converted_value);
	}
	else																								// plain integer
	{
		int64_t converted_value = strtoq(c_str, &last_used_char, 10);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::IntegerForString): \"" << p_number_string << "\" could not be represented as an integer (strtoq conversion error)." << eidos_terminate(p_blame_token);
		
		return converted_value;
	}
}

// A utility static method for getting a double for a string outside of a EidosInterpreter session
double EidosInterpreter::FloatForString(const std::string &p_number_string, const EidosToken *p_blame_token)
{
	// This needs to use the same criteria as NumericValueForString() below; it raises if the number is a float.
	const char *c_str = p_number_string.c_str();
	char *last_used_char = nullptr;
	
	errno = 0;
	
	double converted_value = strtod(c_str, &last_used_char);
	
	if (errno || (last_used_char == c_str))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::FloatForString): \"" << p_number_string << "\" could not be represented as a float (strtod conversion error)." << eidos_terminate(p_blame_token);
	
	return converted_value;
}

// A utility static method for getting a numeric EidosValue* for a string outside of a EidosInterpreter session
EidosValue *EidosInterpreter::NumericValueForString(const std::string &p_number_string, const EidosToken *p_blame_token)
{
	const char *c_str = p_number_string.c_str();
	char *last_used_char = nullptr;
	
	errno = 0;
	
	// At this point, we have to decide whether to instantiate an int or a float.  If it has a decimal point or
	// a minus sign in it (which would be in the exponent), we'll make a float.  Otherwise, we'll make an int.
	// This might need revision in future; 1.2e3 could be an int, for example.  However, it is an ambiguity in
	// the syntax that will never be terribly comfortable; it's the price we pay for wanting ints to be
	// expressable using scientific notation.
	if ((p_number_string.find('.') != string::npos) || (p_number_string.find('-') != string::npos))				// requires a float
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as a float (strtod conversion error)." << eidos_terminate(p_blame_token);
		
		return new EidosValue_Float_singleton_const(converted_value);
	}
	else if ((p_number_string.find('e') != string::npos) || (p_number_string.find('E') != string::npos))		// has an exponent
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (strtod conversion error)." << eidos_terminate(p_blame_token);
		
		// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
		if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (out of range)." << eidos_terminate(p_blame_token);
		
		return new EidosValue_Int_singleton_const(static_cast<int64_t>(converted_value));
	}
	else																										// plain integer
	{
		int64_t converted_value = strtoq(c_str, &last_used_char, 10);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (strtoq conversion error)." << eidos_terminate(p_blame_token);
		
		return new EidosValue_Int_singleton_const(converted_value);
	}
}

EidosValue *EidosInterpreter::Evaluate_Number(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Number() entered\n";
#endif
	
#ifdef DEBUG
	if (p_node->children_.size() > 0)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Number): (internal error) expected 0 children." << eidos_terminate(p_node->token_);
#endif
	
	// use a cached value from EidosASTNode::_OptimizeConstants() if present; this should always be hit now!
	EidosValue *result = p_node->cached_value_;
	
	if (!result)
	{
		EidosToken *string_token = p_node->token_;
		
		result = EidosInterpreter::NumericValueForString(string_token->token_string_, string_token);
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Number() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_String(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_String() entered\n";
#endif
	
#ifdef DEBUG
	if (p_node->children_.size() > 0)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_String): (internal error) expected 0 children." << eidos_terminate(p_node->token_);
#endif
	
	// use a cached value from EidosASTNode::_OptimizeConstants() if present; this should always be hit now!
	EidosValue *result = p_node->cached_value_;
	
	if (!result)
		result = new EidosValue_String_singleton_const(p_node->token_->token_string_);
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_String() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Identifier(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Identifier() entered\n";
#endif
	
#ifdef DEBUG
	if (p_node->children_.size() > 0)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Identifier): (internal error) expected 0 children." << eidos_terminate(p_node->token_);
#endif
	
	EidosValue *result = global_symbols_.GetValueOrRaiseForToken(p_node->token_);	// raises if undefined
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Identifier() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_If(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_If() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	auto children_size = p_node->children_.size();
	
#ifdef DEBUG
	if ((children_size != 2) && (children_size != 3))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_If): (internal error) expected 2 or 3 children." << eidos_terminate(p_node->token_);
#endif
	
	EidosValue *result = nullptr;
	
	EidosASTNode *condition_node = p_node->children_[0];
	EidosValue *condition_result = FastEvaluateNode(condition_node);
	
	if (condition_result == gStaticEidosValue_LogicalT)
	{
		// Handle a static singleton logical true super fast; no need for type check, count, etc
		EidosASTNode *true_node = p_node->children_[1];
		result = FastEvaluateNode(true_node);
	}
	else if (condition_result == gStaticEidosValue_LogicalF)
	{
		// Handle a static singleton logical false super fast; no need for type check, count, etc
		if (children_size == 3)		// has an 'else' node
		{
			EidosASTNode *false_node = p_node->children_[2];
			result = FastEvaluateNode(false_node);
		}
		else										// no 'else' node, so the result is NULL
		{
			result = gStaticEidosValueNULLInvisible;
		}
	}
	else if (condition_result->Count() == 1)
	{
		bool condition_bool = condition_result->LogicalAtIndex(0, operator_token);
		
		if (condition_bool)
		{
			EidosASTNode *true_node = p_node->children_[1];
			result = FastEvaluateNode(true_node);
		}
		else if (children_size == 3)		// has an 'else' node
		{
			EidosASTNode *false_node = p_node->children_[2];
			result = FastEvaluateNode(false_node);
		}
		else										// no 'else' node, so the result is NULL
		{
			result = gStaticEidosValueNULLInvisible;
		}
		
		// free our operands
		if (condition_result->IsTemporary()) delete condition_result;
	}
	else
	{
		if (condition_result->IsTemporary()) delete condition_result;
		
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_If): condition for if statement has size() != 1." << eidos_terminate(p_node->token_);
	}
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_If() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Do(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Do() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Do): (internal error) expected 2 children." << eidos_terminate(p_node->token_);
#endif
	
	EidosValue *result = nullptr;
	
	do
	{
		// execute the do...while loop's statement by evaluating its node; evaluation values get thrown away
		EidosValue *statement_value = FastEvaluateNode(p_node->children_[0]);
		
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
		EidosASTNode *condition_node = p_node->children_[1];
		EidosValue *condition_result = FastEvaluateNode(condition_node);
		
		if (condition_result == gStaticEidosValue_LogicalT)
		{
			// Handle a static singleton logical true super fast; no need for type check, count, etc
		}
		else if (condition_result == gStaticEidosValue_LogicalF)
		{
			// Handle a static singleton logical false super fast; no need for type check, count, etc
			break;
		}
		else if (condition_result->Count() == 1)
		{
			bool condition_bool = condition_result->LogicalAtIndex(0, operator_token);
			
			if (condition_result->IsTemporary()) delete condition_result;
			
			if (!condition_bool)
				break;
		}
		else
		{
			if (condition_result->IsTemporary()) delete condition_result;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Do): condition for do-while loop has size() != 1." << eidos_terminate(p_node->token_);
		}
	}
	while (true);
	
	if (!result)
		result = gStaticEidosValueNULLInvisible;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Do() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_While(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_While() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 2)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_While): (internal error) expected 2 children." << eidos_terminate(p_node->token_);
#endif
	
	EidosValue *result = nullptr;
	
	while (true)
	{
		// test the loop condition
		EidosASTNode *condition_node = p_node->children_[0];
		EidosValue *condition_result = FastEvaluateNode(condition_node);
		
		if (condition_result == gStaticEidosValue_LogicalT)
		{
			// Handle a static singleton logical true super fast; no need for type check, count, etc
		}
		else if (condition_result == gStaticEidosValue_LogicalF)
		{
			// Handle a static singleton logical false super fast; no need for type check, count, etc
			break;
		}
		else if (condition_result->Count() == 1)
		{
			bool condition_bool = condition_result->LogicalAtIndex(0, operator_token);
			
			if (condition_result->IsTemporary()) delete condition_result;
			
			if (!condition_bool)
				break;
		}
		else
		{
			if (condition_result->IsTemporary()) delete condition_result;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_While): condition for while loop has size() != 1." << eidos_terminate(p_node->token_);
		}
		
		// execute the while loop's statement by evaluating its node; evaluation values get thrown away
		EidosValue *statement_value = FastEvaluateNode(p_node->children_[1]);
		
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
		result = gStaticEidosValueNULLInvisible;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_While() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_For(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_For() entered\n";
#endif
	
	EidosToken *operator_token = p_node->token_;
	
#ifdef DEBUG
	if (p_node->children_.size() != 3)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): (internal error) expected 3 children." << eidos_terminate(p_node->token_);
#endif
	
	EidosASTNode *identifier_child = p_node->children_[0];
	
	// we require an identifier to assign into; I toyed with allowing any lvalue, but that is kind of weird / complicated...
	if (identifier_child->token_->token_type_ != EidosTokenType::kTokenIdentifier)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword requires an identifier for its left operand." << eidos_terminate(p_node->token_);
	
	const string &identifier_name = identifier_child->token_->token_string_;
	EidosValue *range_value = FastEvaluateNode(p_node->children_[1]);
	int range_count = range_value->Count();
	EidosValue *result = nullptr;
	
	if ((range_count == 0) && (range_value->Type() == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword does not allow NULL for its right operand (the range to be iterated over)." << eidos_terminate(p_node->token_);
	
	for (int range_index = 0; range_index < range_count; ++range_index)
	{
		// set the index variable to the range value and then throw the range value away
		EidosValue *range_value_at_index = range_value->GetValueAtIndex(range_index, operator_token);
		
		global_symbols_.SetValueForSymbol(identifier_name, range_value_at_index);
		
		if (range_value_at_index->IsTemporary()) delete range_value_at_index;
		
		// execute the for loop's statement by evaluating its node; evaluation values get thrown away
		EidosValue *statement_value = FastEvaluateNode(p_node->children_[2]);
		
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
		result = gStaticEidosValueNULLInvisible;
	
	// free our range operand
	if (range_value->IsTemporary()) delete range_value;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_For() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Next(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Next() entered\n";
#endif
	
#ifdef DEBUG
	if (p_node->children_.size() != 0)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Next): (internal error) expected 0 children." << eidos_terminate(p_node->token_);
#endif
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	next_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	EidosScript::PushErrorPositionFromToken(p_node->token_);
	
	EidosValue *result = gStaticEidosValueNULLInvisible;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Next() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Break(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Break() entered\n";
#endif
	
#ifdef DEBUG
	if (p_node->children_.size() != 0)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Break): (internal error) expected 0 children." << eidos_terminate(p_node->token_);
#endif
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	break_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	EidosScript::PushErrorPositionFromToken(p_node->token_);
	
	EidosValue *result = gStaticEidosValueNULLInvisible;
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Break() : return == " << *result << "\n";
#endif
	
	return result;
}

EidosValue *EidosInterpreter::Evaluate_Return(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(execution_log_indent_++) << "Evaluate_Return() entered\n";
#endif
	
#ifdef DEBUG
	if (p_node->children_.size() > 1)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Return): (internal error) expected 0 or 1 children." << eidos_terminate(p_node->token_);
#endif
	
	// set a flag in the interpreter, which will be seen by the eval methods and will cause them to return up to the top-level block immediately
	return_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	EidosScript::PushErrorPositionFromToken(p_node->token_);
	
	EidosValue *result = nullptr;
	
	if (p_node->children_.size() == 0)
		result = gStaticEidosValueNULLInvisible;	// default return value
	else
		result = FastEvaluateNode(p_node->children_[0]);
	
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
		*execution_log_ << IndentString(--execution_log_indent_) << "Evaluate_Return() : return == " << *result << "\n";
#endif
	
	return result;
}





























































