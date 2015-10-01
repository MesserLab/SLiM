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
#include "eidos_call_signature.h"

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


// We have a bunch of behaviors that we want to do only when compiled DEBUG or EIDOS_GUI; #if tests everywhere are very ugly, so we make
// some #defines here that help structure this.

#if defined(DEBUG) || defined(EIDOS_GUI)

#define EIDOS_ENTRY_EXECUTION_LOG(method_name)						if (logging_execution_) *execution_log_ << IndentString(execution_log_indent_++) << method_name << " entered\n";
#define EIDOS_EXIT_EXECUTION_LOG(method_name)						if (logging_execution_) *execution_log_ << IndentString(--execution_log_indent_) << method_name << " : return == " << *result_SP << "\n";
#define EIDOS_BEGIN_EXECUTION_LOG()									if (logging_execution_) execution_log_indent_ = 0;
#define EIDOS_END_EXECUTION_LOG()									if (gEidosLogEvaluation) std::cout << ExecutionLog();
#define EIDOS_ASSERT_CHILD_COUNT(method_name, count)				if (p_node->children_.size() != count) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << count << " child(ren)." << eidos_terminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ(method_name, min_count)		if (p_node->children_.size() < min_count) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << min_count << "+ child(ren)." << eidos_terminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_RANGE(method_name, lower, upper)			if ((p_node->children_.size() < lower) || (p_node->children_.size() > upper)) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << lower << " to " << upper << " children." << eidos_terminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_COUNT_X(node, node_type, method_name, count, blame_token)		if (node->children_.size() != count) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << count << " child(ren) for " << node_type << " node." << eidos_terminate(blame_token);

#else

#define EIDOS_ENTRY_EXECUTION_LOG(method_name)
#define EIDOS_EXIT_EXECUTION_LOG(method_name)
#define EIDOS_BEGIN_EXECUTION_LOG()
#define EIDOS_END_EXECUTION_LOG()
#define EIDOS_ASSERT_CHILD_COUNT(method_name, count)
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ(method_name, min_count)
#define EIDOS_ASSERT_CHILD_RANGE(method_name, lower, upper)
#define EIDOS_ASSERT_CHILD_COUNT_X(node, node_type, method_name, count, blame_token)

#endif


bool TypeCheckAssignmentOfEidosValueIntoEidosValue(const EidosValue &base_value, const EidosValue &dest_value)
{
	EidosValueType base_type = base_value.Type();
	EidosValueType dest_type = dest_value.Type();
	bool base_is_object = (base_type == EidosValueType::kValueObject);
	bool dest_is_object = (dest_type == EidosValueType::kValueObject);
	
	if (base_is_object && dest_is_object)
	{
		// objects must match in their element type, or one or both must have no defined element type (due to being empty)
		const EidosObjectClass *base_element_class = ((const EidosValue_Object &)base_value).Class();
		const EidosObjectClass *dest_element_class = ((const EidosValue_Object &)dest_value).Class();
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

// the starting point for internally executed blocks, which require braces and suppress output
EidosValue_SP EidosInterpreter::EvaluateInternalBlock(EidosScript *p_script_for_block)
{
	// EvaluateInternalBlock() does not log execution, since it is not user-initiated
	
	bool saved_error_tracking = false;
	int error_start_save;
	int error_end_save;
	EidosScript *current_script_save;
	bool executing_runtime_script_save;
	
	// Internal blocks may be associated with their own script object; if so, the error tracking code needs to track that
	// Note that if there is not a separate script block, then we do NOT set up an error context here; we assume that
	// that has been done externally, just like EvaluateInterpreterBlock().
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
	
	EidosValue_SP result_SP = FastEvaluateNode(root_node_);
	
	// if a next or break statement was hit and was not handled by a loop, throw an error
	if (next_statement_hit_ || break_statement_hit_)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInternalBlock): statement \"" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "\" encountered with no enclosing loop." << eidos_terminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
	
	// handle a return statement; we're at the top level, so there's not much to do
	if (return_statement_hit_)
		return_statement_hit_ = false;
	
	// EvaluateInternalBlock() does not send the result of execution to the output stream; EvaluateInterpreterBlock() does,
	// because it is for interactive use, but EvaluateInternalBlock() is for internal use, and so interactive output
	// is undesirable.  Eidos code that wants to generate output can always use print(), cat(), etc.
	
	// Restore the normal error context; note that a raise blows through this, of course, since we want the raise-catch
	// machinery to report the error using the error information set up by the raise.
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
	
	return result_SP;
}

// the starting point for script blocks in Eidos, which do not require braces; this is not really a "block" but a series of
// independent statements grouped only by virtue of having been executed together as a unit in the interpreter
EidosValue_SP EidosInterpreter::EvaluateInterpreterBlock(bool p_print_output)
{
	EIDOS_BEGIN_EXECUTION_LOG();
	EIDOS_ENTRY_EXECUTION_LOG("EvaluateInterpreterBlock()");
	
	EidosValue_SP result_SP = gStaticEidosValueNULLInvisible;
	
	for (EidosASTNode *child_node : root_node_->children_)
	{
		result_SP = FastEvaluateNode(child_node);
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInterpreterBlock): statement \"" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "\" encountered with no enclosing loop." << eidos_terminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
		
		// send the result of each statement to our output stream; result==nullptr indicates invisible NULL, so we don't print
		EidosValue *result = result_SP.get();
		
		if (p_print_output && result && !result->Invisible())
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
	
	EIDOS_EXIT_EXECUTION_LOG("EvaluateInterpreterBlock()");
	EIDOS_END_EXECUTION_LOG();
	return result_SP;
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
void EidosInterpreter::_ProcessSubscriptAssignment(EidosValue_SP *p_base_value_ptr, EidosGlobalStringID *p_property_string_id_ptr, vector<int> *p_indices_ptr, const EidosASTNode *p_parent_node)
{
	// The left operand is the thing we're subscripting.  If it is an identifier or a dot operator, then we are the deepest (i.e. first)
	// subscript operation, and we can resolve the symbol host, set up a vector of indices, and return.  If it is a subscript, we recurse.
	EidosToken *parent_token = p_parent_node->token_;
	EidosTokenType token_type = parent_token->token_type_;
	
	switch (token_type)
	{
		case EidosTokenType::kTokenLBracket:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_parent_node, "'['", "EidosInterpreter::_ProcessSubscriptAssignment", 2, parent_token);
			
			EidosASTNode *left_operand = p_parent_node->children_[0];
			EidosASTNode *right_operand = p_parent_node->children_[1];
			
			vector<int> base_indices;
			
			// Recurse to find the symbol host and property name that we are ultimately subscripting off of
			_ProcessSubscriptAssignment(p_base_value_ptr, p_property_string_id_ptr, &base_indices, left_operand);
			
			// Find out which indices we're supposed to use within our base vector
			EidosValue_SP second_child_value = FastEvaluateNode(right_operand);
			EidosValueType second_child_type = second_child_value->Type();
			
			if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat) && (second_child_type != EidosValueType::kValueLogical))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): index operand type " << second_child_type << " is not supported by the '[]' operator." << eidos_terminate(parent_token);
			
			int second_child_count = second_child_value->Count();
			
			if (second_child_type == EidosValueType::kValueLogical)
			{
				// A logical vector must exactly match in length; if it does, it selects corresponding indices from base_indices
				if (second_child_count != (int)base_indices.size())
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << eidos_terminate(parent_token);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(parent_token);
					else
						p_indices_ptr->push_back(base_indices[index_value]);
				}
			}
			
			break;
		}
		case EidosTokenType::kTokenDot:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_parent_node, "'.'", "EidosInterpreter::_ProcessSubscriptAssignment", 2, parent_token);
			
			EidosASTNode *left_operand = p_parent_node->children_[0];
			EidosASTNode *right_operand = p_parent_node->children_[1];
			
			EidosValue_SP first_child_value = FastEvaluateNode(left_operand);
			EidosValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != EidosValueType::kValueObject)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(parent_token);
			
			if (right_operand->token_->token_type_ != EidosTokenType::kTokenIdentifier)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubscriptAssignment): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(parent_token);
			
			*p_base_value_ptr = first_child_value;
			*p_property_string_id_ptr = EidosGlobalStringIDForString(right_operand->token_->token_string_);
			
			int number_of_elements = first_child_value->Count();	// property operations are guaranteed to produce one value per element
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->push_back(element_idx);
			
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_parent_node, "identifier", "EidosInterpreter::_ProcessSubscriptAssignment", 0, parent_token);
			
			EidosValue_SP identifier_value_SP = global_symbols_.GetValueOrRaiseForToken(p_parent_node->token_);
			EidosValue *identifier_value = identifier_value_SP.get();
			
			// OK, a little bit of trickiness here.  We've got the base value from the symbol table.  The problem is that it
			// could be one of our singleton subclasses, for speed.  We almost never change EidosValue instances once
			// they are constructed, which is why we can use singleton subclasses so pervasively.  But this is one place –
			// the only place, in fact, I think – where that can bite us, because we do in fact need to modify the original
			// EidosValue.  The fix is to detect that we have a singleton value, and actually replace it in the symbol table
			// with a vector-based copy that we can manipulate.  A little gross, but this is the price we pay for speed...
			if (identifier_value->IsSingleton())
			{
				const std::string &symbol_name = p_parent_node->token_->token_string_;
				
				identifier_value_SP = identifier_value->VectorBasedCopy();
				identifier_value = identifier_value_SP.get();
				
				global_symbols_.SetValueForSymbol(symbol_name, identifier_value_SP);
			}
			
			*p_base_value_ptr = std::move(identifier_value_SP);
			
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

void EidosInterpreter::_AssignRValueToLValue(EidosValue_SP p_rvalue, const EidosASTNode *p_lvalue_node)
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
			EIDOS_ASSERT_CHILD_COUNT_X(p_lvalue_node, "'['", "EidosInterpreter::_AssignRValueToLValue", 2, nullptr);
			
			EidosValue_SP base_value;
			EidosGlobalStringID property_string_id = gEidosID_none;
			vector<int> indices;
			
			_ProcessSubscriptAssignment(&base_value, &property_string_id, &indices, p_lvalue_node);
			
			int index_count = (int)indices.size();
			int rvalue_count = p_rvalue->Count();
			
			if (rvalue_count == 1)
			{
				if (property_string_id == gEidosID_none)
				{
					if (!TypeCheckAssignmentOfEidosValueIntoEidosValue(*p_rvalue, *base_value))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): type mismatch in assignment (" << p_rvalue->Type() << " versus " << base_value->Type() << ")." << eidos_terminate(nullptr);
					
					// we have a multiplex assignment of one value to (maybe) more than one index in a symbol host: x[5:10] = 10
					for (int value_idx = 0; value_idx < index_count; value_idx++)
						base_value->SetValueAtIndex(indices[value_idx], *p_rvalue, nullptr);
				}
				else
				{
					// we have a multiplex assignment of one value to (maybe) more than one index in a property of a symbol host: x.foo[5:10] = 10
					// here we use the guarantee that the member operator returns one result per element, and that elements following sharing semantics,
					// to rearrange this assignment from host.property[indices] = rvalue to host[indices].property = rvalue; this must be equivalent!
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue_SP temp_lvalue = base_value->GetValueAtIndex(indices[value_idx], nullptr);
						
						if (temp_lvalue->Type() != EidosValueType::kValueObject)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) dot operator used with non-object value." << eidos_terminate(nullptr);
						
						static_cast<EidosValue_Object *>(temp_lvalue.get())->SetPropertyOfElements(property_string_id, *p_rvalue);
					}
				}
			}
			else if (index_count == rvalue_count)
			{
				if (property_string_id == gEidosID_none)
				{
					if (!TypeCheckAssignmentOfEidosValueIntoEidosValue(*p_rvalue, *base_value))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): type mismatch in assignment (" << p_rvalue->Type() << " versus " << base_value->Type() << ")." << eidos_terminate(nullptr);
					
					// we have a one-to-one assignment of values to indices in a symbol host: x[5:10] = 5:10
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue_SP temp_rvalue = p_rvalue->GetValueAtIndex(value_idx, nullptr);
						
						base_value->SetValueAtIndex(indices[value_idx], *temp_rvalue, nullptr);
					}
				}
				else
				{
					// we have a one-to-one assignment of values to indices in a property of a symbol host: x.foo[5:10] = 5:10
					// as above, we rearrange this assignment from host.property[indices1] = rvalue[indices2] to host[indices1].property = rvalue[indices2]
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue_SP temp_lvalue = base_value->GetValueAtIndex(indices[value_idx], nullptr);
						EidosValue_SP temp_rvalue = p_rvalue->GetValueAtIndex(value_idx, nullptr);
						
						if (temp_lvalue->Type() != EidosValueType::kValueObject)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) dot operator used with non-object value." << eidos_terminate(nullptr);
						
						static_cast<EidosValue_Object *>(temp_lvalue.get())->SetPropertyOfElements(property_string_id, *temp_rvalue);
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
			EIDOS_ASSERT_CHILD_COUNT_X(p_lvalue_node, "'.'", "EidosInterpreter::_AssignRValueToLValue", 2, nullptr);
			
			EidosValue_SP first_child_value = FastEvaluateNode(p_lvalue_node->children_[0]);
			EidosValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != EidosValueType::kValueObject)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(nullptr);
			
			EidosASTNode *second_child_node = p_lvalue_node->children_[1];
			
			if (second_child_node->token_->token_type_ != EidosTokenType::kTokenIdentifier)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(nullptr);
			
			// OK, we have <object type>.<identifier>; we can work with that
			static_cast<EidosValue_Object *>(first_child_value.get())->SetPropertyOfElements(second_child_node->cached_stringID_, *p_rvalue);
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_lvalue_node, "identifier", "EidosInterpreter::_AssignRValueToLValue", 0, nullptr);
			
			// Simple identifier; the symbol host is the global symbol table, at least for now
			global_symbols_.SetValueForSymbol(p_lvalue_node->token_->token_string_, std::move(p_rvalue));
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): unexpected node token type " << token_type << "; lvalue required." << eidos_terminate(nullptr);
			break;
	}
}

EidosValue_SP EidosInterpreter::EvaluateNode(const EidosASTNode *p_node)
{
	// The structure here avoids doing a break in the non-error case; a bit faster.
	switch (p_node->token_->token_type_)
	{
		case EidosTokenType::kTokenSemicolon:	return Evaluate_NullStatement(p_node);
		case EidosTokenType::kTokenColon:		return Evaluate_RangeExpr(p_node);
		case EidosTokenType::kTokenLBrace:		return Evaluate_CompoundStatement(p_node);
		case EidosTokenType::kTokenLParen:		return Evaluate_FunctionCall(p_node);
		case EidosTokenType::kTokenLBracket:	return Evaluate_Subset(p_node);
		case EidosTokenType::kTokenDot:			return Evaluate_MemberRef(p_node);
		case EidosTokenType::kTokenPlus:		return Evaluate_Plus(p_node);
		case EidosTokenType::kTokenMinus:		return Evaluate_Minus(p_node);
		case EidosTokenType::kTokenMod:			return Evaluate_Mod(p_node);
		case EidosTokenType::kTokenMult:		return Evaluate_Mult(p_node);
		case EidosTokenType::kTokenExp:			return Evaluate_Exp(p_node);
		case EidosTokenType::kTokenAnd:			return Evaluate_And(p_node);
		case EidosTokenType::kTokenOr:			return Evaluate_Or(p_node);
		case EidosTokenType::kTokenDiv:			return Evaluate_Div(p_node);
		case EidosTokenType::kTokenAssign:		return Evaluate_Assign(p_node);
		case EidosTokenType::kTokenEq:			return Evaluate_Eq(p_node);
		case EidosTokenType::kTokenLt:			return Evaluate_Lt(p_node);
		case EidosTokenType::kTokenLtEq:		return Evaluate_LtEq(p_node);
		case EidosTokenType::kTokenGt:			return Evaluate_Gt(p_node);
		case EidosTokenType::kTokenGtEq:		return Evaluate_GtEq(p_node);
		case EidosTokenType::kTokenNot:			return Evaluate_Not(p_node);
		case EidosTokenType::kTokenNotEq:		return Evaluate_NotEq(p_node);
		case EidosTokenType::kTokenNumber:		return Evaluate_Number(p_node);
		case EidosTokenType::kTokenString:		return Evaluate_String(p_node);
		case EidosTokenType::kTokenIdentifier:	return Evaluate_Identifier(p_node);
		case EidosTokenType::kTokenIf:			return Evaluate_If(p_node);
		case EidosTokenType::kTokenDo:			return Evaluate_Do(p_node);
		case EidosTokenType::kTokenWhile:		return Evaluate_While(p_node);
		case EidosTokenType::kTokenFor:			return Evaluate_For(p_node);
		case EidosTokenType::kTokenNext:		return Evaluate_Next(p_node);
		case EidosTokenType::kTokenBreak:		return Evaluate_Break(p_node);
		case EidosTokenType::kTokenReturn:		return Evaluate_Return(p_node);
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateNode): unexpected node token type " << p_node->token_->token_type_ << "." << eidos_terminate(p_node->token_);
	}
}

EidosValue_SP EidosInterpreter::Evaluate_NullStatement(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_NullStatement()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_NullStatement", 0);
	
	EidosValue_SP result_SP = gStaticEidosValueNULLInvisible;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_NullStatement()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_CompoundStatement(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_CompoundStatement()");
	
	EidosValue_SP result_SP = gStaticEidosValueNULLInvisible;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		result_SP = FastEvaluateNode(child_node);
		
		// a next, break, or return makes us exit immediately, out to the (presumably enclosing) loop evaluator
		if (next_statement_hit_ || break_statement_hit_ || return_statement_hit_)
			break;
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_CompoundStatement()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::_Evaluate_RangeExpr_Internal(const EidosASTNode *p_node, const EidosValue &p_first_child_value, const EidosValue &p_second_child_value)
{
	EidosToken *operator_token = p_node->token_;
	EidosValueType first_child_type = p_first_child_value.Type();
	EidosValueType second_child_type = p_second_child_value.Type();
	EidosValue_SP result_SP;
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operand type " << first_child_type << " is not supported by the ':' operator." << eidos_terminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operand type " << second_child_type << " is not supported by the ':' operator." << eidos_terminate(operator_token);
	
	int first_child_count = p_first_child_value.Count();
	int second_child_count = p_second_child_value.Count();
	
	if ((first_child_count != 1) || (second_child_count != 1))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operands of the ':' operator must have size() == 1." << eidos_terminate(operator_token);
	
	// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
	bool underflow = false;
	
	if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
	{
		int64_t first_int = p_first_child_value.IntAtIndex(0, operator_token);
		int64_t second_int = p_second_child_value.IntAtIndex(0, operator_token);
		
		if (first_int <= second_int)
		{
			if (second_int - first_int + 1 >= 1000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 1000000 entries cannot be constructed." << eidos_terminate(operator_token);
			
			EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
			EidosValue_Int_vector *int_result = int_result_SP->Reserve((int)(second_int - first_int + 1));
			
			for (int64_t range_index = first_int; range_index <= second_int; ++range_index)
				int_result->PushInt(range_index);
			
			result_SP = std::move(int_result_SP);
		}
		else
		{
			if (first_int - second_int + 1 >= 1000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 1000000 entries cannot be constructed." << eidos_terminate(operator_token);
			
			EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
			EidosValue_Int_vector *int_result = int_result_SP->Reserve((int)(first_int - second_int + 1));
			
			for (int64_t range_index = first_int; range_index >= second_int; --range_index)
				int_result->PushInt(range_index);
			
			result_SP = std::move(int_result_SP);
		}
	}
	else
	{
		double first_float = p_first_child_value.FloatAtIndex(0, operator_token);
		double second_float = p_second_child_value.FloatAtIndex(0, operator_token);
		
		if (isnan(first_float) || isnan(second_float))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operands of the ':' operator must not be NAN." << eidos_terminate(operator_token);
		
		if (first_float <= second_float)
		{
			if (second_float - first_float + 1 >= 1000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 1000000 entries cannot be constructed." << eidos_terminate(operator_token);
			
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->Reserve((int)(second_float - first_float + 1));
			
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
			
			result_SP = std::move(float_result_SP);
		}
		else
		{
			if (first_float - second_float + 1 >= 1000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 1000000 entries cannot be constructed." << eidos_terminate(operator_token);
			
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->Reserve((int)(first_float - second_float + 1));
			
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
			
			result_SP = std::move(float_result_SP);
		}
	}
	
	if (underflow)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): the floating-point range could not be constructed due to underflow." << eidos_terminate(operator_token);
	
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_RangeExpr(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_RangeExpr()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_RangeExpr", 2);

	// Constant expressions involving the range operator are particularly common, so we cache them for reuse
	// here, as an optimization.  If we have a cached value, we can simply return it.
	EidosValue_SP result_SP = p_node->cached_value_;
	
	if (!result_SP)
	{
		const EidosASTNode *child0 = p_node->children_[0];
		const EidosASTNode *child1 = p_node->children_[1];
		bool cacheable = ((child0->token_->token_type_ == EidosTokenType::kTokenNumber) && (child1->token_->token_type_ == EidosTokenType::kTokenNumber));
		
		EidosValue_SP first_child_value = FastEvaluateNode(child0);
		EidosValue_SP second_child_value = FastEvaluateNode(child1);
		
		result_SP = _Evaluate_RangeExpr_Internal(p_node, *first_child_value, *second_child_value);		// gives ownership of the child values
		
		// cache our range as a constant in the tree if we can
		if (cacheable)
			p_node->cached_value_ = result_SP;
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_RangeExpr()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_FunctionCall(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_FunctionCall()");
	
	EidosValue_SP result_SP;
	
	// We do not evaluate the function name node (our first child) to get a function object; there is no such type in
	// Eidos for now.  Instead, we extract the identifier name directly from the node and work with it.  If the
	// node is an identifier, it is a function call; if it is a dot operator, it is a method call; other constructs
	// are illegal since expressions cannot evaluate to function objects, there being no function objects in Eidos.
	EidosASTNode *function_name_node = p_node->children_[0];
	EidosTokenType function_name_token_type = function_name_node->token_->token_type_;
	
	const string *function_name = nullptr;
	const EidosFunctionSignature *function_signature = nullptr;
	EidosGlobalStringID method_id = gEidosID_none;
	EidosValue_Object_SP method_object;
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
		EIDOS_ASSERT_CHILD_COUNT_X(function_name_node, "'.'", "EidosInterpreter::Evaluate_FunctionCall", 2, function_name_node->token_);
		
		EidosValue_SP first_child_value = FastEvaluateNode(function_name_node->children_[0]);
		EidosValueType first_child_type = first_child_value->Type();
		
		if (first_child_type != EidosValueType::kValueObject)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionCall): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(function_name_node->token_);
		
		EidosASTNode *second_child_node = function_name_node->children_[1];
		
		if (second_child_node->token_->token_type_ != EidosTokenType::kTokenIdentifier)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionCall): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(function_name_node->token_);
		
		// OK, we have <object type>.<identifier>(...); that's a well-formed method call
		call_identifier_token = second_child_node->token_;
		method_id = second_child_node->cached_stringID_;
		method_object = static_pointer_cast<EidosValue_Object>(std::move(first_child_value));	// guaranteed by the Type() call above
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionCall): type " << function_name_token_type << " is not supported by the '()' operator (illegal operand for a function call operation)." << eidos_terminate(function_name_node->token_);
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
		EidosValue_SP (arguments_array[5]);
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
			result_SP = ExecuteMethodCall(*method_object, method_id, arguments_array, arguments_count);
		else
			result_SP = ExecuteFunctionCall(*function_name, function_signature, arguments_array, arguments_count);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
	}
	else
	{
		// We have a lot of arguments, so we need to use a vector
		vector<EidosValue_SP> arguments;
		
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
		EidosValue_SP *arguments_ptr = arguments.data();
		
		if (method_object)
			result_SP = ExecuteMethodCall(*method_object, method_id, arguments_ptr, arguments_count);
		else
			result_SP = ExecuteFunctionCall(*function_name, function_signature, arguments_ptr, arguments_count);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_FunctionCall()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Subset(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Subset()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Subset", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	
	if (first_child_type == EidosValueType::kValueNULL)
	{
		// Any subscript of NULL returns NULL
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
		EidosValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat) && (second_child_type != EidosValueType::kValueLogical))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): index operand type " << second_child_type << " is not supported by the '[]' operator." << eidos_terminate(operator_token);
		
		// OK, we can definitely do this subset
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (second_child_type == EidosValueType::kValueLogical)
		{
			// Subsetting with a logical vector means the vectors must match in length; indices with a T value will be taken
			if (first_child_count != second_child_count)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << eidos_terminate(operator_token);
			
			// Subsetting with a logical vector does not attempt to allocate singleton values, for now; seems unlikely to be a frequently hit case
			const std::vector<bool> &logical_index_vec = ((EidosValue_Logical *)second_child_value.get())->LogicalVector();
			
			if (first_child_count == 1)
			{
				// This is the simple logic using NewMatchingType() / PushValueFromIndexOfEidosValue()
				result_SP = first_child_value->NewMatchingType();
				
				EidosValue *result = result_SP.get();
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
					if (logical_index_vec[value_idx])
						result->PushValueFromIndexOfEidosValue(value_idx, *first_child_value, operator_token);
			}
			else
			{
				// Here we can special-case each type for speed since we know we're not dealing with singletons
				if (first_child_type == EidosValueType::kValueLogical)
				{
					const std::vector<bool> &first_child_vec = ((EidosValue_Logical *)first_child_value.get())->LogicalVector();
					EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
					EidosValue_Logical *logical_result = logical_result_SP->Reserve(second_child_count);
					std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_vec[value_idx])
							logical_result_vec.push_back(first_child_vec[value_idx]);
					
					result_SP = std::move(logical_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueInt)
				{
					const std::vector<int64_t> &first_child_vec = ((EidosValue_Int_vector *)first_child_value.get())->IntVector();
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->Reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_vec[value_idx])
							int_result->PushInt(first_child_vec[value_idx]);
					
					result_SP = std::move(int_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueFloat)
				{
					const std::vector<double> &first_child_vec = ((EidosValue_Float_vector *)first_child_value.get())->FloatVector();
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->Reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_vec[value_idx])
							float_result->PushFloat(first_child_vec[value_idx]);
					
					result_SP = std::move(float_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueString)
				{
					const std::vector<std::string> &first_child_vec = ((EidosValue_String_vector *)first_child_value.get())->StringVector();
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_vec[value_idx])
							string_result->PushString(first_child_vec[value_idx]);
					
					result_SP = std::move(string_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueObject)
				{
					const std::vector<EidosObjectElement *> &first_child_vec = ((EidosValue_Object_vector *)first_child_value.get())->ObjectElementVector();
					EidosValue_Object_vector_SP obj_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector());
					EidosValue_Object_vector *obj_result = obj_result_SP->Reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_vec[value_idx])
							obj_result->PushObjectElement(first_child_vec[value_idx]);
					
					result_SP = std::move(obj_result_SP);
				}
			}
		}
		else
		{
			if (second_child_count == 1)
			{
				// Subsetting with a singleton int/float vector is common and should return a singleton value for speed
				// This is guaranteed to return a singleton value (when available), and bounds-checks for us
				result_SP = first_child_value->GetValueAtIndex((int)second_child_value->IntAtIndex(0, operator_token), operator_token);
			}
			else
			{
				// Subsetting with a int/float vector can use a vector of any length; the specific indices referenced will be taken
				if (first_child_type == EidosValueType::kValueFloat)
				{
					// result type is float; optimize for that
					const std::vector<double> &first_child_vec = ((EidosValue_Float_vector *)first_child_value.get())->FloatVector();
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->Reserve(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const std::vector<int64_t> &int_index_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_vec[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								float_result->PushFloat(first_child_vec[index_value]);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								float_result->PushFloat(first_child_vec[index_value]);
						}
					}
					
					result_SP = std::move(float_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueInt)
				{
					// result type is integer; optimize for that
					const std::vector<int64_t> &first_child_vec = ((EidosValue_Int_vector *)first_child_value.get())->IntVector();
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->Reserve(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const std::vector<int64_t> &int_index_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_vec[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								int_result->PushInt(first_child_vec[index_value]);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								int_result->PushInt(first_child_vec[index_value]);
						}
					}
					
					result_SP = std::move(int_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueObject)
				{
					// result type is object; optimize for that
					const std::vector<EidosObjectElement *> &first_child_vec = ((EidosValue_Object_vector *)first_child_value.get())->ObjectElementVector();
					EidosValue_Object_vector_SP obj_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector());
					EidosValue_Object_vector *obj_result = obj_result_SP->Reserve(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const std::vector<int64_t> &int_index_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_vec[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								obj_result->PushObjectElement(first_child_vec[index_value]);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								obj_result->PushObjectElement(first_child_vec[index_value]);
						}
					}
					
					result_SP = std::move(obj_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueLogical)
				{
					// result type is logical; optimize for that
					const std::vector<bool> &first_child_vec = ((EidosValue_Logical *)first_child_value.get())->LogicalVector();
					EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
					EidosValue_Logical *logical_result = logical_result_SP->Reserve(second_child_count);
					std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const std::vector<int64_t> &int_index_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_vec[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								logical_result_vec.push_back(first_child_vec[index_value]);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								logical_result_vec.push_back(first_child_vec[index_value]);
						}
					}
					
					result_SP = std::move(logical_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueString)
				{
					// result type is string; optimize for that
					const std::vector<std::string> &first_child_vec = ((EidosValue_String_vector *)first_child_value.get())->StringVector();
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const std::vector<int64_t> &int_index_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_vec[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								string_result->PushString(first_child_vec[index_value]);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
							else
								string_result->PushString(first_child_vec[index_value]);
						}
					}
					
					result_SP = std::move(string_result_SP);
				}
				else
				{
					// This is the general case; it should never be hit
					result_SP = first_child_value->NewMatchingType();
					
					EidosValue *result = result_SP.get();
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
					{
						int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
						
						if ((index_value < 0) || (index_value >= first_child_count))
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << eidos_terminate(operator_token);
						else
							result->PushValueFromIndexOfEidosValue((int)index_value, *first_child_value, operator_token);
					}
				}
			}
		}
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Subset()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_MemberRef(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_MemberRef()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_MemberRef", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	
	if (first_child_type != EidosValueType::kValueObject)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): operand type " << first_child_type << " is not supported by the '.' operator." << eidos_terminate(operator_token);
	
	EidosASTNode *second_child_node = p_node->children_[1];
	EidosToken *second_child_token = second_child_node->token_;
	
	if (second_child_token->token_type_ != EidosTokenType::kTokenIdentifier)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): the '.' operator for x.y requires operand y to be an identifier." << eidos_terminate(operator_token);
	
	// If an error occurs inside a function or method call, we want to highlight the call
	EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(second_child_token);
	
	// We offload the actual work to GetPropertyOfElements() to keep things simple here
	EidosGlobalStringID property_string_ID = second_child_node->cached_stringID_;
	result_SP = static_cast<EidosValue_Object *>(first_child_value.get())->GetPropertyOfElements(property_string_ID);
	
	// Forget the function token, since it is not responsible for any future errors
	EidosScript::RestoreErrorPosition(error_pos_save);
	
	// check result; this should never happen, since GetProperty should check
	if (!result_SP)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): undefined property " << StringForEidosGlobalStringID(property_string_ID) << "." << eidos_terminate(operator_token);
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_MemberRef()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Plus(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Plus()");
	EIDOS_ASSERT_CHILD_RANGE("EidosInterpreter::Evaluate_Plus", 1, 2);
	
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	if (p_node->children_.size() == 1)
	{
		// unary plus is a no-op, but legal only for numeric types
		EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
		EidosValueType first_child_type = first_child_value->Type();
		
		if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): operand type " << first_child_type << " is not supported by the unary '+' operator." << eidos_terminate(operator_token);
		
		result_SP = std::move(first_child_value);
	}
	else
	{
		// binary plus is legal either between two numeric types, or between a string and any other non-NULL operand
		EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
		EidosValueType first_child_type = first_child_value->Type();
		
		EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
		EidosValueType second_child_type = second_child_value->Type();
		
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if ((first_child_type == EidosValueType::kValueString) || (second_child_type == EidosValueType::kValueString))
		{
			// If either operand is a string, then we are doing string concatenation, with promotion to strings if needed
			if (((first_child_type == EidosValueType::kValueNULL) || (second_child_type == EidosValueType::kValueNULL)))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the binary '+' operator does not support operands of type NULL." << eidos_terminate(operator_token);
			
			if ((first_child_count == 1) && (second_child_count == 1))
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(first_child_value->StringAtIndex(0, operator_token) + second_child_value->StringAtIndex(0, operator_token)));
			}
			else
			{
				if (first_child_count == second_child_count)
				{
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						string_result->PushString(first_child_value->StringAtIndex(value_index, operator_token) + second_child_value->StringAtIndex(value_index, operator_token));
					
					result_SP = std::move(string_result_SP);
				}
				else if (first_child_count == 1)
				{
					string singleton_int = first_child_value->StringAtIndex(0, operator_token);
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(second_child_count);
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						string_result->PushString(singleton_int + second_child_value->StringAtIndex(value_index, operator_token));
					
					result_SP = std::move(string_result_SP);
				}
				else if (second_child_count == 1)
				{
					string singleton_int = second_child_value->StringAtIndex(0, operator_token);
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						string_result->PushString(first_child_value->StringAtIndex(value_index, operator_token) + singleton_int);
					
					result_SP = std::move(string_result_SP);
				}
				else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
				{
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
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
					//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_child_value->IntAtIndex(0, operator_token) + second_child_value->IntAtIndex(0, operator_token));
					
					int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
					int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
					int64_t add_result;
					bool overflow = __builtin_saddll_overflow(first_operand, second_operand, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << eidos_terminate(operator_token);
					
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(add_result));
				}
				else
				{
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->Reserve(first_child_count);
					
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
					
					result_SP = std::move(int_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->Reserve(second_child_count);
				
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
				
				result_SP = std::move(int_result_SP);
			}
			else if (second_child_count == 1)
			{
				int64_t singleton_int = second_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->Reserve(first_child_count);
				
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
				
				result_SP = std::move(int_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
			}
		}
		else
		{
			if (((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat)) || ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat)))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the combination of operand types " << first_child_type << " and " << second_child_type << " is not supported by the binary '+' operator." << eidos_terminate(operator_token);
			
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(first_child_value->FloatAtIndex(0, operator_token) + second_child_value->FloatAtIndex(0, operator_token)));
				}
				else
				{
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) + second_child_value->FloatAtIndex(value_index, operator_token));
					
					result_SP = std::move(float_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->Reserve(second_child_count);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					float_result->PushFloat(singleton_float + second_child_value->FloatAtIndex(value_index, operator_token));
				
				result_SP = std::move(float_result_SP);
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) + singleton_float);
				
				result_SP = std::move(float_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
			}
		}
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Plus()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Minus(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Minus()");
	EIDOS_ASSERT_CHILD_RANGE("EidosInterpreter::Evaluate_Minus", 1, 2);
	
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): operand type " << first_child_type << " is not supported by the '-' operator." << eidos_terminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	
	if (p_node->children_.size() == 1)
	{
		// unary minus
		if (first_child_type == EidosValueType::kValueInt)
		{
			if (first_child_count == 1)
			{
				// This is an overflow-safe version of:
				//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-first_child_value->IntAtIndex(0, operator_token));
				
				int64_t operand = first_child_value->IntAtIndex(0, operator_token);
				int64_t subtract_result;
				bool overflow = __builtin_ssubll_overflow(0, operand, &subtract_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer negation overflow with the unary '-' operator." << eidos_terminate(operator_token);
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subtract_result));
			}
			else
			{
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->Reserve(first_child_count);
				
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
				
				result_SP = std::move(int_result_SP);
			}
		}
		else
		{
			if (first_child_count == 1)
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-first_child_value->FloatAtIndex(0, operator_token)));
			}
			else
			{
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(-first_child_value->FloatAtIndex(value_index, operator_token));
				
				result_SP = std::move(float_result_SP);
			}
		}
	}
	else
	{
		// binary minus
		EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
		EidosValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): operand type " << second_child_type << " is not supported by the '-' operator." << eidos_terminate(operator_token);
		
		int second_child_count = second_child_value->Count();
		
		if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					// This is an overflow-safe version of:
					//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_child_value->IntAtIndex(0, operator_token) - second_child_value->IntAtIndex(0, operator_token));
					
					int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
					int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
					int64_t subtract_result;
					bool overflow = __builtin_ssubll_overflow(first_operand, second_operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << eidos_terminate(operator_token);
					
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subtract_result));
				}
				else
				{
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->Reserve(first_child_count);
					
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
					
					result_SP = std::move(int_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->Reserve(second_child_count);
				
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
				
				result_SP = std::move(int_result_SP);
			}
			else if (second_child_count == 1)
			{
				int64_t singleton_int = second_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->Reserve(first_child_count);
				
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
				
				result_SP = std::move(int_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): the '-' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
			}
		}
		else
		{
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(first_child_value->FloatAtIndex(0, operator_token) - second_child_value->FloatAtIndex(0, operator_token)));
				}
				else
				{
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) - second_child_value->FloatAtIndex(value_index, operator_token));
					
					result_SP = std::move(float_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->Reserve(second_child_count);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					float_result->PushFloat(singleton_float - second_child_value->FloatAtIndex(value_index, operator_token));
				
				result_SP = std::move(float_result_SP);
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) - singleton_float);
				
				result_SP = std::move(float_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): the '-' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
			}
		}
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Minus()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Mod(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Mod()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Mod", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): operand type " << first_child_type << " is not supported by the '%' operator." << eidos_terminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): operand type " << second_child_type << " is not supported by the '%' operator." << eidos_terminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, they will need to do float division and then use floor() and asInteger().  Alternatively, we could provide a separate
	// operator for integer division, as R does.  However, the definition of integer division is tricky and contested; best to let people do their own.
	// This decision applies also to modulo, for consistency.
	
	// floating-point modulo by zero is safe; it will produce an NaN, following IEEE as implemented by C++
	if (first_child_count == second_child_count)
	{
		if (first_child_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fmod(first_child_value->FloatAtIndex(0, operator_token), second_child_value->FloatAtIndex(0, operator_token))));
		}
		else
		{
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(fmod(first_child_value->FloatAtIndex(value_index, operator_token), second_child_value->FloatAtIndex(value_index, operator_token)));
			
			result_SP = std::move(float_result_SP);
		}
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->Reserve(second_child_count);
		
		for (int value_index = 0; value_index < second_child_count; ++value_index)
			float_result->PushFloat(fmod(singleton_float, second_child_value->FloatAtIndex(value_index, operator_token)));
		
		result_SP = std::move(float_result_SP);
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
		
		for (int value_index = 0; value_index < first_child_count; ++value_index)
			float_result->PushFloat(fmod(first_child_value->FloatAtIndex(value_index, operator_token), singleton_float));
		
		result_SP = std::move(float_result_SP);
	}
	else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): the '%' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Mod()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Mult(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Mult()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Mult", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): operand type " << first_child_type << " is not supported by the '*' operator." << eidos_terminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): operand type " << second_child_type << " is not supported by the '*' operator." << eidos_terminate(operator_token);
	
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
				//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_child_value->IntAtIndex(0, operator_token) * second_child_value->IntAtIndex(0, operator_token));
				
				int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
				int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
				int64_t multiply_result;
				bool overflow = __builtin_smulll_overflow(first_operand, second_operand, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << eidos_terminate(operator_token);
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(multiply_result));
			}
			else
			{
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->Reserve(first_child_count);
				
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
				
				result_SP = std::move(int_result_SP);
			}
		}
		else
		{
			if (first_child_count == 1)
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(first_child_value->FloatAtIndex(0, operator_token) * second_child_value->FloatAtIndex(0, operator_token)));
			}
			else
			{
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) * second_child_value->FloatAtIndex(value_index, operator_token));
				
				result_SP = std::move(float_result_SP);
			}
		}
	}
	else if ((first_child_count == 1) || (second_child_count == 1))
	{
		EidosValue_SP one_count_child;
		EidosValue_SP any_count_child;
		int any_count;
		
		if (first_child_count == 1)
		{
			one_count_child = std::move(first_child_value);
			any_count_child = std::move(second_child_value);
			any_count = second_child_count;
		}
		else
		{
			one_count_child = std::move(second_child_value);
			any_count_child = std::move(first_child_value);
			any_count = first_child_count;
		}
		
		// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
		if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			int64_t singleton_int = one_count_child->IntAtIndex(0, operator_token);
			EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
			EidosValue_Int_vector *int_result = int_result_SP->Reserve(any_count);
			
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
			
			result_SP = std::move(int_result_SP);
		}
		else
		{
			double singleton_float = one_count_child->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->Reserve(any_count);
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				float_result->PushFloat(any_count_child->FloatAtIndex(value_index, operator_token) * singleton_float);
			
			result_SP = std::move(float_result_SP);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): the '*' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Mult()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Div(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Div()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Div", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): operand type " << first_child_type << " is not supported by the '/' operator." << eidos_terminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): operand type " << second_child_type << " is not supported by the '/' operator." << eidos_terminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, they will need to do float division and then use floor() and asInteger().  Alternatively, we could provide a separate
	// operator for integer division, as R does.  However, the definition of integer division is tricky and contested; best to let people do their own.

	// floating-point division by zero is safe; it will produce an infinity, following IEEE as implemented by C++
	if (first_child_count == second_child_count)
	{
		if (first_child_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(first_child_value->FloatAtIndex(0, operator_token) / second_child_value->FloatAtIndex(0, operator_token)));
		}
		else
		{
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) / second_child_value->FloatAtIndex(value_index, operator_token));
			
			result_SP = std::move(float_result_SP);
		}
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->Reserve(second_child_count);
		
		for (int value_index = 0; value_index < second_child_count; ++value_index)
			float_result->PushFloat(singleton_float / second_child_value->FloatAtIndex(value_index, operator_token));
		
		result_SP = std::move(float_result_SP);
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
		
		for (int value_index = 0; value_index < first_child_count; ++value_index)
			float_result->PushFloat(first_child_value->FloatAtIndex(value_index, operator_token) / singleton_float);
		
		result_SP = std::move(float_result_SP);
	}
	else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): the '/' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Div()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Exp(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Exp()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Exp", 2);
	
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): operand type " << first_child_type << " is not supported by the '^' operator." << eidos_terminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): operand type " << second_child_type << " is not supported by the '^' operator." << eidos_terminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	// Exponentiation always produces a float result; the user can cast back to integer if they really want
	EidosValue_SP result_SP;
	
	if (first_child_count == second_child_count)
	{
		if (first_child_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(pow(first_child_value->FloatAtIndex(0, operator_token), second_child_value->FloatAtIndex(0, operator_token))));
		}
		else
		{
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->PushFloat(pow(first_child_value->FloatAtIndex(value_index, operator_token), second_child_value->FloatAtIndex(value_index, operator_token)));
			
			result_SP = std::move(float_result_SP);
		}
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->Reserve(second_child_count);
		
		for (int value_index = 0; value_index < second_child_count; ++value_index)
			float_result->PushFloat(pow(singleton_float, second_child_value->FloatAtIndex(value_index, operator_token)));
		
		result_SP = std::move(float_result_SP);
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->Reserve(first_child_count);
		
		for (int value_index = 0; value_index < first_child_count; ++value_index)
			float_result->PushFloat(pow(first_child_value->FloatAtIndex(value_index, operator_token), singleton_float));
		
		result_SP = std::move(float_result_SP);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): the '^' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
	}
	
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Exp()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_And(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_And()");
	EIDOS_ASSERT_CHILD_COUNT_GTEQ("EidosInterpreter::Evaluate_And", 2);
	
	EidosToken *operator_token = p_node->token_;
	
	// We try to avoid allocating a result object if we can.  If result==nullptr but result_count==1, bool_result contains the result so far.
	EidosValue_Logical_SP result_SP;
	bool bool_result = false;
	int result_count = 0;
	bool first_child = true;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		EidosValue_SP child_result = FastEvaluateNode(child_node);
		
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
				if (result_SP)
				{
					// we have a result allocated, so alter the values in it
					EidosValue_Logical *result = result_SP.get();
					
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
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): operand type " << child_type << " is not supported by the '&' operator." << eidos_terminate(operator_token);
			
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
				else if ((child_type == EidosValueType::kValueLogical) && child_result->unique())
				{
					// child_result is a logical EidosValue owned only by us, so we can just take it over as our initial result
					result_SP = static_pointer_cast<EidosValue_Logical>(std::move(child_result));
					result_count = child_count;
				}
				else
				{
					// for other cases, we just clone child_result – but note that it may not be of type logical
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(child_count));
					result_count = child_count;
					
					std::vector<bool> &logical_result_vec = result_SP->LogicalVector_Mutable();
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						logical_result_vec.push_back(child_result->LogicalAtIndex(value_index, operator_token));
				}
			}
			else
			{
				// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
				if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): operands to the '&' operator are not compatible in size()." << eidos_terminate(operator_token);
				
				if (child_count == 1)
				{
					// if child_bool is T, it has no effect on result; if it is F, it turns result to all F
					bool child_bool = child_result->LogicalAtIndex(0, operator_token);
					
					if (!child_bool)
					{
						if (result_SP)
						{
							// we have a result allocated, so alter the values in it
							EidosValue_Logical *result = result_SP.get();
							
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
					
					if (result_SP)
					{
						// we have a result allocated; work with that
						result_bool = result_SP->LogicalAtIndex(0, operator_token);
					}
					else
					{
						// no result allocated, so we now need to upgrade to an allocated result
						result_bool = bool_result;
					}
					
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(child_count));
					result_count = child_count;
					
					std::vector<bool> &logical_result_vec = result_SP->LogicalVector_Mutable();
					
					if (result_bool)
						for (int value_index = 0; value_index < child_count; ++value_index)
							logical_result_vec.push_back(child_result->LogicalAtIndex(value_index, operator_token));
					else
						for (int value_index = 0; value_index < child_count; ++value_index)
							logical_result_vec.push_back(false);
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is F we turn result to F
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (!child_result->LogicalAtIndex(value_index, operator_token))
							result->SetLogicalAtIndex(value_index, false, operator_token);
				}
			}
		}
	}
	
	// if we avoided allocating a result, use a static logical value
	if (!result_SP)
		result_SP = (bool_result ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_And()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Or(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Or()");
	EIDOS_ASSERT_CHILD_COUNT_GTEQ("EidosInterpreter::Evaluate_Or", 2);
	
	EidosToken *operator_token = p_node->token_;
	
	// We try to avoid allocating a result object if we can.  If result==nullptr but result_count==1, bool_result contains the result so far.
	EidosValue_Logical_SP result_SP;
	bool bool_result = false;
	int result_count = 0;
	bool first_child = true;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		EidosValue_SP child_result = FastEvaluateNode(child_node);
		
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
				if (result_SP)
				{
					// we have a result allocated, so alter the values in it
					EidosValue_Logical *result = result_SP.get();
					
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
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): operand type " << child_type << " is not supported by the '|' operator." << eidos_terminate(operator_token);
			
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
				else if ((child_type == EidosValueType::kValueLogical) && child_result->unique())
				{
					// child_result is a logical EidosValue owned only by us, so we can just take it over as our initial result
					result_SP = static_pointer_cast<EidosValue_Logical>(std::move(child_result));
					result_count = child_count;
					
					continue;	// do not free child_result
				}
				else
				{
					// for other cases, we just clone child_result – but note that it may not be of type logical
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(child_count));
					result_count = child_count;
					
					std::vector<bool> &logical_result_vec = result_SP->LogicalVector_Mutable();
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						logical_result_vec.push_back(child_result->LogicalAtIndex(value_index, operator_token));
				}
			}
			else
			{
				// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
				if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): operands to the '|' operator are not compatible in size()." << eidos_terminate(operator_token);
				
				if (child_count == 1)
				{
					// if child_bool is F, it has no effect on result; if it is T, it turns result to all T
					bool child_bool = child_result->LogicalAtIndex(0, operator_token);
					
					if (child_bool)
					{
						if (result_SP)
						{
							// we have a result allocated, so alter the values in it
							EidosValue_Logical *result = result_SP.get();
							
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
					
					if (result_SP)
					{
						// we have a result allocated; work with that
						result_bool = result_SP->LogicalAtIndex(0, operator_token);
					}
					else
					{
						// no result allocated, so we now need to upgrade to an allocated result
						result_bool = bool_result;
					}
					
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(child_count));
					result_count = child_count;
					
					std::vector<bool> &logical_result_vec = result_SP->LogicalVector_Mutable();
					
					if (result_bool)
						for (int value_index = 0; value_index < child_count; ++value_index)
							logical_result_vec.push_back(true);
					else
						for (int value_index = 0; value_index < child_count; ++value_index)
							logical_result_vec.push_back(child_result->LogicalAtIndex(value_index, operator_token));
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is T we turn result to T
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (child_result->LogicalAtIndex(value_index, operator_token))
							result->SetLogicalAtIndex(value_index, true, operator_token);
				}
			}
		}
	}
	
	// if we avoided allocating a result, use a static logical value
	if (!result_SP)
		result_SP = (bool_result ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Or()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Not(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Not()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Not", 1);
	
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_Logical_SP result_SP;
	
	if (first_child_value == gStaticEidosValue_LogicalT)
	{
		// Handle a static singleton logical true super fast; no need for type check, count, etc
		result_SP = gStaticEidosValue_LogicalF;
	}
	else if (first_child_value == gStaticEidosValue_LogicalF)
	{
		// Handle a static singleton logical false super fast; no need for type check, count, etc
		result_SP = gStaticEidosValue_LogicalT;
	}
	else
	{
		EidosValueType first_child_type = first_child_value->Type();
		
		if ((first_child_type != EidosValueType::kValueLogical) && (first_child_type != EidosValueType::kValueString) && (first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Not): operand type " << first_child_type << " is not supported by the '!' operator." << eidos_terminate(operator_token);
		
		int first_child_count = first_child_value->Count();
		
		if (first_child_count == 1)
		{
			// If we're generating a singleton result, use cached static logical values
			result_SP = (first_child_value->LogicalAtIndex(0, operator_token) ? gStaticEidosValue_LogicalF : gStaticEidosValue_LogicalT);
		}
		else
		{
			// for other cases, we just clone the negation of child_result – but note that it may not be of type logical
			result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(first_child_count));
			
			std::vector<bool> &logical_result_vec = result_SP->LogicalVector_Mutable();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				logical_result_vec.push_back(!first_child_value->LogicalAtIndex(value_index, operator_token));
		}
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Not()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Assign(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Assign()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Assign", 2);
	
	if (p_node->cached_compound_assignment_)
	{
		// if _OptimizeAssignments() set this flag, this assignment is of the form "x = x <operator> <number>",
		// where x is a simple identifier and the operator is one of +-/%*^; we try to optimize that case
		EidosASTNode *lvalue_node = p_node->children_[0];
		EidosValue_SP lvalue_SP = global_symbols_.GetNonConstantValueOrRaiseForToken(lvalue_node->token_);	// raises if undefined or const
		EidosValue *lvalue = lvalue_SP.get();
		int lvalue_count = lvalue->Count();
		
		// somewhat unusually, we will now modify the lvalue in place, for speed; this is legal since we just got
		// it from the symbol table (we want to modify it in the symbol table, and others should not have a
		// reference to the object that they expect to be constant), but doing it right requires care given
		// different value subclasses, singletons, etc.
		if (lvalue_count > 0)
		{
			EidosValueType lvalue_type = lvalue->Type();
			
			if (lvalue_type == EidosValueType::kValueInt)
			{
				EidosASTNode *rvalue_node = p_node->children_[1];								// the operator node
				EidosValue *cached_operand2 = rvalue_node->children_[1]->cached_value_.get();	// the numeric constant
				
				// if the lvalue is an integer, we require the rvalue to be an integer also; we don't handle mixed types here
				if (cached_operand2->Type() == EidosValueType::kValueInt)
				{
					EidosTokenType compound_operator = rvalue_node->token_->token_type_;
					int64_t operand2_value = cached_operand2->IntAtIndex(0, nullptr);
					
					if ((lvalue_count == 1) && lvalue->IsSingleton())
					{
						EidosValue_Int_singleton *int_singleton = static_cast<EidosValue_Int_singleton *>(lvalue);
						
						switch (compound_operator)
						{
							case EidosTokenType::kTokenPlus:
							{
								int64_t &operand1_value = int_singleton->IntValue_Mutable();
								bool overflow = __builtin_saddll_overflow(operand1_value, operand2_value, &operand1_value);
								
								if (overflow)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer addition overflow with the binary '+' operator." << eidos_terminate(rvalue_node->token_);
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMinus:
							{
								int64_t &operand1_value = int_singleton->IntValue_Mutable();
								bool overflow = __builtin_ssubll_overflow(operand1_value, operand2_value, &operand1_value);
								
								if (overflow)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer subtraction overflow with the binary '-' operator." << eidos_terminate(rvalue_node->token_);
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMult:
							{
								int64_t &operand1_value = int_singleton->IntValue_Mutable();
								bool overflow = __builtin_smulll_overflow(operand1_value, operand2_value, &operand1_value);
								
								if (overflow)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer multiplication overflow with the '*' operator." << eidos_terminate(rvalue_node->token_);
								goto compoundAssignmentSuccess;
							}
							default:	// div, mod, and exp always produce float, so we don't handle them for int; we can't change the type of x here
								break;
						}
					}
					else
					{
						std::vector<int64_t> &int_vec = ((EidosValue_Int_vector *)lvalue)->IntVector_Mutable();
						
						switch (compound_operator)
						{
							case EidosTokenType::kTokenPlus:
							{
								for (int value_index = 0; value_index < lvalue_count; ++value_index)
								{
									int64_t &int_vec_value = int_vec[value_index];
									bool overflow = __builtin_saddll_overflow(int_vec_value, operand2_value, &int_vec_value);
									
									if (overflow)
										EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer addition overflow with the binary '+' operator." << eidos_terminate(rvalue_node->token_);
								}
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMinus:
							{
								for (int value_index = 0; value_index < lvalue_count; ++value_index)
								{
									int64_t &int_vec_value = int_vec[value_index];
									bool overflow = __builtin_ssubll_overflow(int_vec_value, operand2_value, &int_vec_value);
									
									if (overflow)
										EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer subtraction overflow with the binary '-' operator." << eidos_terminate(rvalue_node->token_);
								}
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMult:
							{
								for (int value_index = 0; value_index < lvalue_count; ++value_index)
								{
									int64_t &int_vec_value = int_vec[value_index];
									bool overflow = __builtin_smulll_overflow(int_vec_value, operand2_value, &int_vec_value);
									
									if (overflow)
										EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer multiplication overflow with the '*' operator." << eidos_terminate(rvalue_node->token_);
								}
								goto compoundAssignmentSuccess;
							}
							default:	// div, mod, and exp always produce float, so we don't handle them for int; we can't change the type of x here
								break;
						}
					}
				}
			}
			else if (lvalue_type == EidosValueType::kValueFloat)
			{
				// if the lvalue is a float, we do not require the rvalue to be a float also; the integer will promote to float seamlessly
				EidosASTNode *rvalue_node = p_node->children_[1];								// the operator node
				EidosValue *cached_operand2 = rvalue_node->children_[1]->cached_value_.get();	// the numeric constant
				EidosTokenType compound_operator = rvalue_node->token_->token_type_;
				double operand2_value = cached_operand2->FloatAtIndex(0, nullptr);				// might be an int64_t and get converted
				
				if ((lvalue_count == 1) && lvalue->IsSingleton())
				{
					EidosValue_Float_singleton *float_singleton = static_cast<EidosValue_Float_singleton *>(lvalue);
					
					switch (compound_operator)
					{
						case EidosTokenType::kTokenPlus:
							float_singleton->FloatValue_Mutable() += operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMinus:
							float_singleton->FloatValue_Mutable() -= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMult:
							float_singleton->FloatValue_Mutable() *= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenDiv:
							float_singleton->FloatValue_Mutable() /= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMod:
						{
							double &operand1_value = float_singleton->FloatValue_Mutable();
							
							operand1_value = fmod(operand1_value, operand2_value);
							goto compoundAssignmentSuccess;
						}
							
						case EidosTokenType::kTokenExp:
						{
							double &operand1_value = float_singleton->FloatValue_Mutable();
							
							operand1_value = pow(operand1_value, operand2_value);
							goto compoundAssignmentSuccess;
						}
							
						default:
							break;
					}
					
				}
				else
				{
					std::vector<double> &float_vec = ((EidosValue_Float_vector *)lvalue)->FloatVector_Mutable();
					
					switch (compound_operator)
					{
						case EidosTokenType::kTokenPlus:
							for (int value_index = 0; value_index < lvalue_count; ++value_index)
								float_vec[value_index] += operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMinus:
							for (int value_index = 0; value_index < lvalue_count; --value_index)
								float_vec[value_index] -= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMult:
							for (int value_index = 0; value_index < lvalue_count; --value_index)
								float_vec[value_index] *= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenDiv:
							for (int value_index = 0; value_index < lvalue_count; --value_index)
								float_vec[value_index] /= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMod:
							for (int value_index = 0; value_index < lvalue_count; --value_index)
							{
								double &float_vec_value = float_vec[value_index];
								
								float_vec_value = fmod(float_vec_value, operand2_value);
							}
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenExp:
							for (int value_index = 0; value_index < lvalue_count; --value_index)
							{
								double &float_vec_value = float_vec[value_index];
								
								float_vec_value = pow(float_vec_value, operand2_value);
							}
							goto compoundAssignmentSuccess;
							
						default:
							break;
					}
				}
				
				goto compoundAssignmentSuccess;
			}
		}
		
		// maybe we should flip our flag so we don't waste time trying this again for this node
		p_node->cached_compound_assignment_ = false;
		
		// and then we drop through to be handled normally by the standard assign operator code
	}
	
	// we can drop through to here even if cached_compound_assignment_ is set, if the code above bailed for some reason
	{
		EidosToken *operator_token = p_node->token_;
		EidosASTNode *lvalue_node = p_node->children_[0];
		EidosValue_SP rvalue = FastEvaluateNode(p_node->children_[1]);
		
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(operator_token);
		
		_AssignRValueToLValue(std::move(rvalue), lvalue_node);
		
		EidosScript::RestoreErrorPosition(error_pos_save);
	}
	
compoundAssignmentSuccess:
	
	// by design, assignment does not yield a usable value; instead it produces NULL – this prevents the error "if (x = 3) ..."
	// since the condition is NULL and will raise; the loss of legitimate uses of "if (x = 3)" seems a small price to pay
	EidosValue_SP result_SP = gStaticEidosValueNULLInvisible;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Assign()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Eq(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Eq()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Eq", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_Logical_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = EidosGetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				result_SP = (compare_result == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
				std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					// Direct float-to-float compare can be optimized through vector access
					const std::vector<double> &float1_vec = ((EidosValue_Float_vector *)first_child_value.get())->FloatVector();
					const std::vector<double> &float2_vec = ((EidosValue_Float_vector *)second_child_value.get())->FloatVector();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(float1_vec[value_index] == float2_vec[value_index]);
				}
				else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
				{
					// Direct int-to-int compare can be optimized through vector access
					const std::vector<int64_t> &int1_vec = ((EidosValue_Int_vector *)first_child_value.get())->IntVector();
					const std::vector<int64_t> &int2_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(int1_vec[value_index] == int2_vec[value_index]);
				}
				else if ((first_child_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
				{
					// Direct object-to-object compare can be optimized through vector access
					const std::vector<EidosObjectElement *> &obj1_vec = ((EidosValue_Object_vector *)first_child_value.get())->ObjectElementVector();
					const std::vector<EidosObjectElement *> &obj2_vec = ((EidosValue_Object_vector *)second_child_value.get())->ObjectElementVector();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(obj1_vec[value_index] == obj2_vec[value_index]);
				}
				else
				{
					// General case
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token) == 0);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(second_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			if ((compareFunc == CompareEidosValues_Float) && (second_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float1 = first_child_value->FloatAtIndex(0, operator_token);
				const std::vector<double> &float_vec = ((EidosValue_Float_vector *)second_child_value.get())->FloatVector();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(float1 == float_vec[value_index]);
			}
			else if ((compareFunc == CompareEidosValues_Int) && (second_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int1 = first_child_value->IntAtIndex(0, operator_token);
				const std::vector<int64_t> &int_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(int1 == int_vec[value_index]);
			}
			else if ((compareFunc == CompareEidosValues_Object) && (second_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj1 = first_child_value->ObjectElementAtIndex(0, operator_token);
				const std::vector<EidosObjectElement *> &obj_vec = ((EidosValue_Object_vector *)second_child_value.get())->ObjectElementVector();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(obj1 == obj_vec[value_index]);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token) == 0);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			if ((compareFunc == CompareEidosValues_Float) && (first_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float2 = second_child_value->FloatAtIndex(0, operator_token);
				const std::vector<double> &float_vec = ((EidosValue_Float_vector *)first_child_value.get())->FloatVector();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(float_vec[value_index] == float2);
			}
			else if ((compareFunc == CompareEidosValues_Int) && (first_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int2 = second_child_value->IntAtIndex(0, operator_token);
				const std::vector<int64_t> &int_vec = ((EidosValue_Int_vector *)first_child_value.get())->IntVector();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(int_vec[value_index] == int2);
			}
			else if ((compareFunc == CompareEidosValues_Object) && (first_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj2 = second_child_value->ObjectElementAtIndex(0, operator_token);
				const std::vector<EidosObjectElement *> &obj_vec = ((EidosValue_Object_vector *)first_child_value.get())->ObjectElementVector();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(obj_vec[value_index] == obj2);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token) == 0);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): the '==' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): testing NULL with the '==' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Eq()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Lt(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Lt()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Lt", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_Logical_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): the '<' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = EidosGetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				result_SP = (compare_result == -1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
				std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result_vec.push_back(compare_result == -1);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result_vec.push_back(compare_result == -1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result_vec.push_back(compare_result == -1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): the '<' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): testing NULL with the '<' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Lt()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_LtEq(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_LtEq()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_LtEq", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_Logical_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): the '<=' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = EidosGetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				result_SP = (compare_result != 1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
				std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result_vec.push_back(compare_result != 1);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result_vec.push_back(compare_result != 1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result_vec.push_back(compare_result != 1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): the '<=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): testing NULL with the '<=' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_LtEq()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Gt(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Gt()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Gt", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_Logical_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): the '>' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = EidosGetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				result_SP = (compare_result == 1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
				std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result_vec.push_back(compare_result == 1);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result_vec.push_back(compare_result == 1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result_vec.push_back(compare_result == 1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): the '>' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): testing NULL with the '>' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Gt()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_GtEq(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_GtEq()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_GtEq", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_Logical_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): the '>=' operator cannot be used with type object." << eidos_terminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = EidosGetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				result_SP = (compare_result != -1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
				std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result_vec.push_back(compare_result != -1);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result_vec.push_back(compare_result != -1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result_vec.push_back(compare_result != -1);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): the '>=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): testing NULL with the '>=' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_GtEq()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_NotEq(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_NotEq()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_NotEq", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_Logical_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = EidosGetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				result_SP = (compare_result != 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
				std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					// Direct float-to-float compare can be optimized through vector access
					const std::vector<double> &float1_vec = ((EidosValue_Float_vector *)first_child_value.get())->FloatVector();
					const std::vector<double> &float2_vec = ((EidosValue_Float_vector *)second_child_value.get())->FloatVector();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(float1_vec[value_index] != float2_vec[value_index]);
				}
				else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
				{
					// Direct int-to-int compare can be optimized through vector access
					const std::vector<int64_t> &int1_vec = ((EidosValue_Int_vector *)first_child_value.get())->IntVector();
					const std::vector<int64_t> &int2_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(int1_vec[value_index] != int2_vec[value_index]);
				}
				else if ((first_child_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
				{
					// Direct object-to-object compare can be optimized through vector access
					const std::vector<EidosObjectElement *> &obj1_vec = ((EidosValue_Object_vector *)first_child_value.get())->ObjectElementVector();
					const std::vector<EidosObjectElement *> &obj2_vec = ((EidosValue_Object_vector *)second_child_value.get())->ObjectElementVector();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(obj1_vec[value_index] != obj2_vec[value_index]);
				}
				else
				{
					// General case
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result_vec.push_back(compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token) != 0);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(second_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			if ((compareFunc == CompareEidosValues_Float) && (second_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float1 = first_child_value->FloatAtIndex(0, operator_token);
				const std::vector<double> &float_vec = ((EidosValue_Float_vector *)second_child_value.get())->FloatVector();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(float1 != float_vec[value_index]);
			}
			else if ((compareFunc == CompareEidosValues_Int) && (second_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int1 = first_child_value->IntAtIndex(0, operator_token);
				const std::vector<int64_t> &int_vec = ((EidosValue_Int_vector *)second_child_value.get())->IntVector();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(int1 != int_vec[value_index]);
			}
			else if ((compareFunc == CompareEidosValues_Object) && (second_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj1 = first_child_value->ObjectElementAtIndex(0, operator_token);
				const std::vector<EidosObjectElement *> &obj_vec = ((EidosValue_Object_vector *)second_child_value.get())->ObjectElementVector();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(obj1 != obj_vec[value_index]);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result_vec.push_back(compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token) != 0);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->Reserve(first_child_count);
			std::vector<bool> &logical_result_vec = logical_result->LogicalVector_Mutable();	// direct push_back() for speed
			
			if ((compareFunc == CompareEidosValues_Float) && (first_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float2 = second_child_value->FloatAtIndex(0, operator_token);
				const std::vector<double> &float_vec = ((EidosValue_Float_vector *)first_child_value.get())->FloatVector();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(float_vec[value_index] != float2);
			}
			else if ((compareFunc == CompareEidosValues_Int) && (first_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int2 = second_child_value->IntAtIndex(0, operator_token);
				const std::vector<int64_t> &int_vec = ((EidosValue_Int_vector *)first_child_value.get())->IntVector();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(int_vec[value_index] != int2);
			}
			else if ((compareFunc == CompareEidosValues_Object) && (first_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj2 = second_child_value->ObjectElementAtIndex(0, operator_token);
				const std::vector<EidosObjectElement *> &obj_vec = ((EidosValue_Object_vector *)first_child_value.get())->ObjectElementVector();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(obj_vec[value_index] != obj2);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result_vec.push_back(compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token) != 0);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): the '!=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(operator_token);
		}
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): testing NULL with the '!=' operator is an error; use isNULL()." << eidos_terminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_NotEq()");
	return result_SP;
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
EidosValue_SP EidosInterpreter::NumericValueForString(const std::string &p_number_string, const EidosToken *p_blame_token)
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
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(converted_value));
	}
	else if ((p_number_string.find('e') != string::npos) || (p_number_string.find('E') != string::npos))		// has an exponent
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (strtod conversion error)." << eidos_terminate(p_blame_token);
		
		// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
		if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (out of range)." << eidos_terminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(static_cast<int64_t>(converted_value)));
	}
	else																										// plain integer
	{
		int64_t converted_value = strtoq(c_str, &last_used_char, 10);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (strtoq conversion error)." << eidos_terminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(converted_value));
	}
}

EidosValue_SP EidosInterpreter::Evaluate_Number(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Number()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Number", 0);
	
	// use a cached value from EidosASTNode::_OptimizeConstants() if present; this should always be hit now!
	EidosValue_SP result_SP = p_node->cached_value_;
	
	if (!result_SP)
	{
		EidosToken *string_token = p_node->token_;
		
		result_SP = EidosInterpreter::NumericValueForString(string_token->token_string_, string_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Number()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_String(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_String()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_String", 0);
	
	// use a cached value from EidosASTNode::_OptimizeConstants() if present; this should always be hit now!
	EidosValue_SP result_SP = p_node->cached_value_;
	
	if (!result_SP)
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(p_node->token_->token_string_));
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_String()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Identifier(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Identifier()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Identifier", 0);
	
	EidosValue_SP result_SP = EidosValue_SP(global_symbols_.GetValueOrRaiseForToken(p_node->token_));	// raises if undefined
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Identifier()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_If(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_If()");
	EIDOS_ASSERT_CHILD_RANGE("EidosInterpreter::Evaluate_If", 2, 3);

	EidosToken *operator_token = p_node->token_;
	auto children_size = p_node->children_.size();
	
	EidosValue_SP result_SP;
	
	EidosASTNode *condition_node = p_node->children_[0];
	EidosValue_SP condition_result = FastEvaluateNode(condition_node);
	
	if (condition_result == gStaticEidosValue_LogicalT)
	{
		// Handle a static singleton logical true super fast; no need for type check, count, etc
		EidosASTNode *true_node = p_node->children_[1];
		result_SP = FastEvaluateNode(true_node);
	}
	else if (condition_result == gStaticEidosValue_LogicalF)
	{
		// Handle a static singleton logical false super fast; no need for type check, count, etc
		if (children_size == 3)		// has an 'else' node
		{
			EidosASTNode *false_node = p_node->children_[2];
			result_SP = FastEvaluateNode(false_node);
		}
		else										// no 'else' node, so the result is NULL
		{
			result_SP = gStaticEidosValueNULLInvisible;
		}
	}
	else if (condition_result->Count() == 1)
	{
		bool condition_bool = condition_result->LogicalAtIndex(0, operator_token);
		
		if (condition_bool)
		{
			EidosASTNode *true_node = p_node->children_[1];
			result_SP = FastEvaluateNode(true_node);
		}
		else if (children_size == 3)		// has an 'else' node
		{
			EidosASTNode *false_node = p_node->children_[2];
			result_SP = FastEvaluateNode(false_node);
		}
		else										// no 'else' node, so the result is NULL
		{
			result_SP = gStaticEidosValueNULLInvisible;
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_If): condition for if statement has size() != 1." << eidos_terminate(p_node->token_);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_If()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Do(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Do()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Do", 2);
	
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	do
	{
		// execute the do...while loop's statement by evaluating its node; evaluation values get thrown away
		EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[0]);
		
		// if a return statement has occurred, we pass the return value outward
		if (return_statement_hit_)
		{
			result_SP = std::move(statement_value);
			break;
		}
		
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
		EidosValue_SP condition_result = FastEvaluateNode(condition_node);
		
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
			
			if (!condition_bool)
				break;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Do): condition for do-while loop has size() != 1." << eidos_terminate(p_node->token_);
		}
	}
	while (true);
	
	if (!result_SP)
		result_SP = gStaticEidosValueNULLInvisible;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Do()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_While(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_While()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_While", 2);
	
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	while (true)
	{
		// test the loop condition
		EidosASTNode *condition_node = p_node->children_[0];
		EidosValue_SP condition_result = FastEvaluateNode(condition_node);
		
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
			
			if (!condition_bool)
				break;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_While): condition for while loop has size() != 1." << eidos_terminate(p_node->token_);
		}
		
		// execute the while loop's statement by evaluating its node; evaluation values get thrown away
		EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[1]);
		
		// if a return statement has occurred, we pass the return value outward
		if (return_statement_hit_)
		{
			result_SP = std::move(statement_value);
			break;
		}
		
		// handle next and break statements
		if (next_statement_hit_)
			next_statement_hit_ = false;		// this is all we need to do; the rest of the function of "next" was done by Evaluate_CompoundStatement()
		
		if (break_statement_hit_)
		{
			break_statement_hit_ = false;
			break;							// break statements, on the other hand, get handled additionally by a break from our loop here
		}
	}
	
	if (!result_SP)
		result_SP = gStaticEidosValueNULLInvisible;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_While()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_For(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_For()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_For", 3);
	
	EidosToken *operator_token = p_node->token_;
	EidosASTNode *identifier_child = p_node->children_[0];
	
	// we require an identifier to assign into; I toyed with allowing any lvalue, but that is kind of weird / complicated...
	if (identifier_child->token_->token_type_ != EidosTokenType::kTokenIdentifier)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword requires an identifier for its left operand." << eidos_terminate(p_node->token_);
	
	const string &identifier_name = identifier_child->token_->token_string_;
	const EidosASTNode *range_node = p_node->children_[1];
	EidosValue_SP result_SP;
	
	// true if the for-loop statement references the index variable; if so, it needs to be set up
	// each iteration, but that can be done very cheaply by replacing its internal value
	uint8_t references_index = p_node->cached_for_references_index_;
	
	// true if the for-loop statement assigns to the index variable; if so, it needs to be set up
	// each iteration, and that has to be done without any assumptions regarding its current value
	uint8_t assigns_index = p_node->cached_for_assigns_index_;
	
	// In some cases we do not need to actually construct the range that we are going to iterate over; we check for that case here
	// and handle it immediately, otherwise we drop through to the (!simpleIntegerRange) case below
	bool simpleIntegerRange = false;
	int64_t start_int = 0, end_int = 0;
	EidosValue_SP range_value(nullptr);
	
	if (!assigns_index)
	{
		if ((range_node->token_->token_type_ == EidosTokenType::kTokenColon) && (range_node->children_.size() == 2))
		{
			// Maybe we can streamline a colon-operator range expression; let's check
			if (!range_node->cached_value_)
			{
				EidosValue_SP range_start_value_SP = FastEvaluateNode(range_node->children_[0]);
				EidosValue *range_start_value = range_start_value_SP.get();
				EidosValue_SP range_end_value_SP = FastEvaluateNode(range_node->children_[1]);
				EidosValue *range_end_value = range_end_value_SP.get();
				
				if ((range_start_value->Type() == EidosValueType::kValueInt) && (range_start_value->Count() == 1) && (range_end_value->Type() == EidosValueType::kValueInt) && (range_end_value->Count() == 1))
				{
					// OK, we have a simple integer:integer range, so this should be very straightforward
					simpleIntegerRange = true;
					
					start_int = range_start_value->IntAtIndex(0, nullptr);
					end_int = range_end_value->IntAtIndex(0, nullptr);
				}
				else
				{
					// If we are not using the general case, we have a bit of a problem now, because we have evaluated the child nodes
					// of the range expression.  Because that might have side effects, we can't let the code below do it again.
					// We therefore have to construct the range here that will be used below.  No good deed goes unpunished.
					
					// Note that this call to Evaluate_RangeExpr_Internal() gives ownership of the child values; it deletes them for us
					range_value = _Evaluate_RangeExpr_Internal(range_node, *range_start_value, *range_end_value);
				}
			}
		}
		else if ((range_node->token_->token_type_ == EidosTokenType::kTokenLParen) && (range_node->children_.size() == 2))
		{
			// Maybe we can streamline a seqAlong() call; let's check
			const EidosASTNode *call_name_node = range_node->children_[0];
			
			if (call_name_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
			{
				const EidosFunctionSignature *signature = call_name_node->cached_signature_;
				
				if (signature && (signature->function_id_ == EidosFunctionIdentifier::seqAlongFunction))
				{
					const EidosASTNode *argument_node = range_node->children_[1];
					
					if (argument_node->token_->token_type_ != EidosTokenType::kTokenComma)
					{
						// We have a qualifying seqAlong() call, so evaluate its argument and set up our simple integer sequence
						simpleIntegerRange = true;
						
						EidosValue_SP argument_value = FastEvaluateNode(argument_node);
						
						start_int = 0;
						end_int = argument_value->Count() - 1;
					}
				}
			}
		}
	}
	
	if (simpleIntegerRange)
	{
		// OK, we have a simple integer:integer range, so this should be very straightforward
		bool counting_up = (start_int < end_int);
		int64_t range_count = (counting_up ? (end_int - start_int + 1) : (start_int - end_int + 1));
		
		if (!assigns_index && !references_index)
		{
			// the loop index variable is not actually used at all; we are just being asked to do a set number of iterations
			// we do need to set up the index variable on exit, though, since code below us might use the final value
			int range_index;
			
			for (range_index = 0; range_index < range_count; ++range_index)
			{
				EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
				
				if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
				if (next_statement_hit_)				next_statement_hit_ = false;
				if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
			}
			
			// set up the final value on exit; if we completed the loop, we need to go back one step
			if (range_index == range_count)
				range_index--;
			
			global_symbols_.SetValueForSymbol(identifier_name, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(counting_up ? start_int + range_index : start_int - range_index)));
		}
		else	// !assigns_index, guaranteed above
		{
			// the loop index variable is referenced in the loop body but is not assigned to, so we can use a single
			// EidosValue that we stick new values into – much, much faster.
			EidosValue_Int_singleton_SP index_value_SP = EidosValue_Int_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
			EidosValue_Int_singleton *index_value = index_value_SP.get();
			
			global_symbols_.SetValueForSymbol(identifier_name, std::move(index_value_SP));
			
			for (int range_index = 0; range_index < range_count; ++range_index)
			{
				index_value->SetValue(counting_up ? start_int + range_index : start_int - range_index);
				
				EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
				
				if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
				if (next_statement_hit_)				next_statement_hit_ = false;
				if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
			}
		}
	}
	else	// (!simpleIntegerRange)
	{
		// We have something other than a simple integer range, so we have to do more work to figure out what type of
		// range we are iterating over; we have optimizations for several cases if assigns_index is false
		if (!range_value)
			range_value = FastEvaluateNode(range_node);
		
		int range_count = range_value->Count();
		EidosValueType range_type = range_value->Type();
		
		if (range_count > 0)
		{
			// try to handle the loop with fast special-case code; if !loop_handled, we will drop into the general case
			// at the bottom, which is more readable and commented
			bool loop_handled = false;
			
			if (!assigns_index && !references_index)
			{
				// the loop index variable is not actually used at all; we are just being asked to do a set number of iterations
				// we do need to set up the index variable on exit, though, since code below us might use the final value
				int range_index;
				
				for (range_index = 0; range_index < range_count; ++range_index)
				{
					EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
					
					if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
					if (next_statement_hit_)				next_statement_hit_ = false;
					if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
				}
				
				// set up the final value on exit; if we completed the loop, we need to go back one step
				if (range_index == range_count)
					range_index--;
				
				global_symbols_.SetValueForSymbol(identifier_name, range_value->GetValueAtIndex(range_index, operator_token));
				
				loop_handled = true;
			}
			else if (!assigns_index && (range_count > 1))
			{
				// the loop index variable is referenced in the loop body but is not assigned to, so we can use a single
				// EidosValue that we stick new values into – much, much faster.
				if (range_type == EidosValueType::kValueInt)
				{
					const std::vector<int64_t> &range_vec = ((EidosValue_Int_vector *)range_value.get())->IntVector();
					EidosValue_Int_singleton_SP index_value_SP = EidosValue_Int_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
					EidosValue_Int_singleton *index_value = index_value_SP.get();
					
					global_symbols_.SetValueForSymbol(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_vec[range_index]);
						
						EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueFloat)
				{
					const std::vector<double> &range_vec = ((EidosValue_Float_vector *)range_value.get())->FloatVector();
					EidosValue_Float_singleton_SP index_value_SP = EidosValue_Float_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0));
					EidosValue_Float_singleton *index_value = index_value_SP.get();
					
					global_symbols_.SetValueForSymbol(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_vec[range_index]);
						
						EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueString)
				{
					const std::vector<std::string> &range_vec = ((EidosValue_String_vector *)range_value.get())->StringVector();
					EidosValue_String_singleton_SP index_value_SP = EidosValue_String_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_empty_string));
					EidosValue_String_singleton *index_value = index_value_SP.get();
					
					global_symbols_.SetValueForSymbol(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_vec[range_index]);
						
						EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueObject)
				{
					const std::vector<EidosObjectElement *> &range_vec = ((EidosValue_Object_vector *)range_value.get())->ObjectElementVector();
					EidosValue_Object_singleton_SP index_value_SP = EidosValue_Object_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(nullptr));
					EidosValue_Object_singleton *index_value = index_value_SP.get();
					
					global_symbols_.SetValueForSymbol(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_vec[range_index]);
						
						EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueLogical)
				{
					const std::vector<bool> &range_vec = ((EidosValue_Logical *)range_value.get())->LogicalVector();
					EidosValue_Logical_SP index_value_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
					EidosValue_Logical *index_value = index_value_SP.get();
					std::vector<bool> &index_vec = index_value->LogicalVector_Mutable();
					
					index_value->PushLogical(false);	// initial placeholder
					
					global_symbols_.SetValueForSymbol(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_vec[0] = range_vec[range_index];
						
						EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
			}
			
			if (!loop_handled)
			{
				// general case
				for (int range_index = 0; range_index < range_count; ++range_index)
				{
					// set the index variable to the range value and then throw the range value away
					EidosValue_SP range_value_at_index = range_value->GetValueAtIndex(range_index, operator_token);
					
					global_symbols_.SetValueForSymbol(identifier_name, range_value_at_index);
					
					// execute the for loop's statement by evaluating its node; evaluation values get thrown away
					EidosValue_SP statement_value = FastEvaluateNode(p_node->children_[2]);
					
					// if a return statement has occurred, we pass the return value outward
					if (return_statement_hit_)
					{
						result_SP = std::move(statement_value);
						break;
					}
					
					// handle next and break statements
					if (next_statement_hit_)
						next_statement_hit_ = false;		// this is all we need to do; the rest of the function of "next" was done by Evaluate_CompoundStatement()
					
					if (break_statement_hit_)
					{
						break_statement_hit_ = false;
						break;							// break statements, on the other hand, get handled additionally by a break from our loop here
					}
				}
			}
		}
		else
		{
			if (range_type == EidosValueType::kValueNULL)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword does not allow NULL for its right operand (the range to be iterated over)." << eidos_terminate(p_node->token_);
		}
	}
	
	if (!result_SP)
		result_SP = gStaticEidosValueNULLInvisible;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_For()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Next(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Next()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Next", 0);
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	next_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	EidosScript::PushErrorPositionFromToken(p_node->token_);
	
	EidosValue_SP result_SP = gStaticEidosValueNULLInvisible;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Next()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Break(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Break()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Break", 0);

	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	break_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	EidosScript::PushErrorPositionFromToken(p_node->token_);
	
	EidosValue_SP result_SP = gStaticEidosValueNULLInvisible;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Break()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Return(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Return()");
	EIDOS_ASSERT_CHILD_RANGE("EidosInterpreter::Evaluate_Return", 0, 1);
	
	// set a flag in the interpreter, which will be seen by the eval methods and will cause them to return up to the top-level block immediately
	return_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	EidosScript::PushErrorPositionFromToken(p_node->token_);
	
	EidosValue_SP result_SP;
	
	if (p_node->children_.size() == 0)
		result_SP = gStaticEidosValueNULLInvisible;	// default return value
	else
		result_SP = FastEvaluateNode(p_node->children_[0]);
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Return()");
	return result_SP;
}





























































