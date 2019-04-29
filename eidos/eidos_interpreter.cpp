//
//  eidos_interpreter.cpp
//  Eidos
//
//  Created by Ben Haller on 4/4/15.
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


#include "eidos_interpreter.h"
#include "eidos_functions.h"
#include "eidos_ast_node.h"
#include "eidos_rng.h"
#include "eidos_call_signature.h"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <cmath>
#include <algorithm>


// We have a bunch of behaviors that we want to do only when compiled DEBUG or EIDOS_GUI; #if tests everywhere are very ugly, so we make
// some #defines here that help structure this.

#if defined(DEBUG) || defined(EIDOS_GUI)

#define EIDOS_ENTRY_EXECUTION_LOG(method_name)						if (logging_execution_) *execution_log_ << IndentString(execution_log_indent_++) << method_name << " entered\n";
#define EIDOS_EXIT_EXECUTION_LOG(method_name)						if (logging_execution_) *execution_log_ << IndentString(--execution_log_indent_) << method_name << " : return == " << *result_SP << "\n";
#define EIDOS_BEGIN_EXECUTION_LOG()									if (logging_execution_) execution_log_indent_ = 0;
#define EIDOS_END_EXECUTION_LOG()									if (gEidosLogEvaluation) std::cout << ExecutionLog();
#define EIDOS_ASSERT_CHILD_COUNT(method_name, count)				if (p_node->children_.size() != count) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << count << " child(ren)." << EidosTerminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ(method_name, min_count)		if (p_node->children_.size() < min_count) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << min_count << "+ child(ren)." << EidosTerminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_RANGE(method_name, lower, upper)			if ((p_node->children_.size() < lower) || (p_node->children_.size() > upper)) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << lower << " to " << upper << " children." << EidosTerminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_COUNT_X(node, node_type, method_name, count, blame_token)		if (node->children_.size() != count) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << count << " child(ren) for " << node_type << " node." << EidosTerminate(blame_token);
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ_X(node, node_type, method_name, min_count, blame_token)		if (node->children_.size() < min_count) EIDOS_TERMINATION << "ERROR (" << method_name << "): (internal error) expected " << min_count << "+ child(ren) for " << node_type << " node." << EidosTerminate(blame_token);

#else

#define EIDOS_ENTRY_EXECUTION_LOG(method_name)
#define EIDOS_EXIT_EXECUTION_LOG(method_name)
#define EIDOS_BEGIN_EXECUTION_LOG()
#define EIDOS_END_EXECUTION_LOG()
#define EIDOS_ASSERT_CHILD_COUNT(method_name, count)
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ(method_name, min_count)
#define EIDOS_ASSERT_CHILD_RANGE(method_name, lower, upper)
#define EIDOS_ASSERT_CHILD_COUNT_X(node, node_type, method_name, count, blame_token)
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ_X(node, node_type, method_name, min_count, blame_token)

#endif


bool TypeCheckAssignmentOfEidosValueIntoEidosValue(const EidosValue &p_base_value, const EidosValue &p_dest_value)
{
	EidosValueType base_type = p_base_value.Type();
	EidosValueType dest_type = p_dest_value.Type();
	bool base_is_object = (base_type == EidosValueType::kValueObject);
	bool dest_is_object = (dest_type == EidosValueType::kValueObject);
	
	if (base_is_object && dest_is_object)
	{
		// objects must match in their element type, or one or both must have no defined element type (due to being empty)
		const EidosObjectClass *base_element_class = ((const EidosValue_Object &)p_base_value).Class();
		const EidosObjectClass *dest_element_class = ((const EidosValue_Object &)p_dest_value).Class();
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
#pragma mark -
#pragma mark EidosInterpreter
#pragma mark -

EidosInterpreter::EidosInterpreter(const EidosScript &p_script, EidosSymbolTable &p_symbols, EidosFunctionMap &p_functions, EidosContext *p_eidos_context)
	: root_node_(p_script.AST()), global_symbols_(&p_symbols), function_map_(p_functions), eidos_context_(p_eidos_context)
{
	// Initialize the random number generator if and only if it has not already been initialized.  In some cases the Context will want to
	// initialize the RNG itself, with its own seed; we don't want to override that.
	if (!EIDOS_GSL_RNG)
	{
		Eidos_InitializeRNG();
		Eidos_SetRNGSeed(Eidos_GenerateSeedFromPIDAndTime());
	}
}

EidosInterpreter::EidosInterpreter(const EidosASTNode *p_root_node_, EidosSymbolTable &p_symbols, EidosFunctionMap &p_functions, EidosContext *p_eidos_context)
	: root_node_(p_root_node_), global_symbols_(&p_symbols), function_map_(p_functions), eidos_context_(p_eidos_context)
{
	// Initialize the random number generator if and only if it has not already been initialized.  In some cases the Context will want to
	// initialize the RNG itself, with its own seed; we don't want to override that.
	if (!EIDOS_GSL_RNG)
	{
		Eidos_InitializeRNG();
		Eidos_SetRNGSeed(Eidos_GenerateSeedFromPIDAndTime());
	}
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::SetShouldLogExecution): execution logging is disabled in this build configuration of Eidos." << EidosTerminate(nullptr);
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

std::ostream &EidosInterpreter::ExecutionOutputStream(void)
{
	// lazy allocation; all use of execution_output_ should get it through this accessor
	// also, when running at the command line we send output directly to std::cout to avoid buffering issues on termination
	if (!gEidosTerminateThrows)
		return std::cout;
	
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
	int error_start_save_UTF16;
	int error_end_save_UTF16;
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
		error_start_save_UTF16 = gEidosCharacterStartOfErrorUTF16;
		error_end_save_UTF16 = gEidosCharacterEndOfErrorUTF16;
		current_script_save = gEidosCurrentScript;
		executing_runtime_script_save = gEidosExecutingRuntimeScript;
		
		gEidosCharacterStartOfError = -1;
		gEidosCharacterEndOfError = -1;
		gEidosCharacterStartOfErrorUTF16 = -1;
		gEidosCharacterEndOfErrorUTF16 = -1;
		gEidosCurrentScript = p_script_for_block;
		gEidosExecutingRuntimeScript = true;
		
		saved_error_tracking = true;
	}
	
	EidosValue_SP result_SP = FastEvaluateNode(root_node_);
	
	// if a next or break statement was hit and was not handled by a loop, throw an error
	if (next_statement_hit_ || break_statement_hit_)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInternalBlock): statement \"" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "\" encountered with no enclosing loop." << EidosTerminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
	
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
		#if (GNU_C >= 5)
		#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
		#endif
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wuninitialized"
		#pragma clang diagnostic ignored "-Wconditional-uninitialized"
		gEidosCharacterStartOfError = error_start_save;
		gEidosCharacterEndOfError = error_end_save;
		gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
		gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
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
EidosValue_SP EidosInterpreter::EvaluateInterpreterBlock(bool p_print_output, bool p_return_last_value)
{
	EIDOS_BEGIN_EXECUTION_LOG();
	EIDOS_ENTRY_EXECUTION_LOG("EvaluateInterpreterBlock()");
	
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	for (EidosASTNode *child_node : root_node_->children_)
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		EidosValue_SP statement_result_SP = FastEvaluateNode(child_node);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(child_node->profile_total_);
#endif
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInterpreterBlock): statement \"" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "\" encountered with no enclosing loop." << EidosTerminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
		
		// send the result of each statement to our output stream; result==nullptr indicates invisible NULL, so we don't print
		EidosValue *statement_result = statement_result_SP.get();
		
		if (p_print_output && statement_result && !statement_result->Invisible())
		{
			std::ostream &execution_output = ExecutionOutputStream();
			
			auto position = execution_output.tellp();
			execution_output << *statement_result;
			
			// EidosValue does not put an endl on the stream, so if it emitted any output, add an endl
			if (position != execution_output.tellp())
				execution_output << std::endl;
		}
		
		// handle a return statement; we're at the top level, so there's not much to do except stop execution
		if (return_statement_hit_)
		{
			return_statement_hit_ = false;
			result_SP = std::move(statement_result_SP);
			break;
		}
		
		// If we're returning the last value seen, keep track of it.  The policy now (starting in SLiM 3) is that lambdas – blocks
		// of code without braces, such as supplied to apply(), sapply(), executeLambda(), as command-line defines in SLiM, etc. –
		// implicitly evaluate to the value of the last statement they execute, and a return is not needed.  Functions – blocks
		// of code with braces, such as user-defined functions, events and callbacks in SLiM, etc. – do not make any implicit return,
		// and evaluate to VOID unless a return statement is explicitly executed.  Functions that are required to return a value
		// must therefore do so explicitly with a return; and functions that are declared as returning void must never return a value.
		if (p_return_last_value)
			result_SP = std::move(statement_result_SP);
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
void EidosInterpreter::_ProcessSubsetAssignment(EidosValue_SP *p_base_value_ptr, EidosGlobalStringID *p_property_string_id_ptr, std::vector<int> *p_indices_ptr, const EidosASTNode *p_parent_node)
{
	// The left operand is the thing we're subscripting.  If it is an identifier or a dot operator, then we are the deepest (i.e. first)
	// subscript operation, and we can resolve the symbol host, set up a vector of indices, and return.  If it is a subscript, we recurse.
	EidosToken *parent_token = p_parent_node->token_;
	EidosTokenType token_type = parent_token->token_type_;
	
	switch (token_type)
	{
		case EidosTokenType::kTokenLBracket:
		{
			// Note that the logic here is very parallel to that of EidosInterpreter::Evaluate_Subset()
			EIDOS_ASSERT_CHILD_COUNT_GTEQ_X(p_parent_node, "'['", "EidosInterpreter::_ProcessSubsetAssignment", 2, parent_token);
			
			EidosASTNode *left_operand = p_parent_node->children_[0];
			
			std::vector<int> base_indices;
			
			// Recurse to find the symbol host and property name that we are ultimately subscripting off of
			_ProcessSubsetAssignment(p_base_value_ptr, p_property_string_id_ptr, &base_indices, left_operand);
			
			int base_indices_count = (int)base_indices.size();
			EidosValue_SP first_child_value = *p_base_value_ptr;
			int first_child_dim_count = first_child_value->DimensionCount();
			
			// organize our subset arguments
			int child_count = (int)p_parent_node->children_.size();
			std::vector <EidosValue_SP> subset_indices;
			
			for (int child_index = 1; child_index < child_count; ++child_index)
			{
				const EidosASTNode *subset_index_node = p_parent_node->children_[child_index];
				EidosTokenType subset_index_token_type = subset_index_node->token_->token_type_;
				
				if ((subset_index_token_type == EidosTokenType::kTokenComma) || (subset_index_token_type == EidosTokenType::kTokenRBracket))
				{
					// We have a placeholder node indicating a skipped expression, so we save NULL as the value
					subset_indices.push_back(gStaticEidosValueNULL);
				}
				else
				{
					// We have an expression node, so we evaluate it, check the value, and save it
					EidosValue_SP child_value = FastEvaluateNode(subset_index_node);
					EidosValueType child_type = child_value->Type();
					
					if ((child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueFloat) && (child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueNULL))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): index operand type " << child_type << " is not supported by the '[]' operator." << EidosTerminate(parent_token);
					if (child_value->DimensionCount() != 1)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): a matrix or array index operand is not supported by the '[]' operator." << EidosTerminate(parent_token);
					
					subset_indices.emplace_back(child_value);
				}
			}
			
			int subset_index_count = (int)subset_indices.size();
			
			if ((subset_index_count != first_child_dim_count) && (subset_index_count != 1))
			{
				// We have the wrong number of subset arguments for our dimensionality, so this is an error
				if (subset_index_count > first_child_dim_count)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): too many subset arguments for the indexed operand's dimensionality." << EidosTerminate(parent_token);
				else // (subset_index_count < first_child_dim_count)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): too few subset arguments for the indexed operand's dimensionality." << EidosTerminate(parent_token);
			}
			// 	else if (first_child_type == EidosValueType::kValueNULL)	: Evaluate_Subset() returns NULL for subsets of NULL, we let them fall through; assigning into an empty subset of NULL is legal
			else if ((subset_index_count == 1) && (subset_indices[0] == gStaticEidosValueNULL))
			{
				// We have a single subset argument of NULL, so we have x[] or x[NULL]; just return all legal indices
				for (int value_idx = 0; value_idx < base_indices_count; value_idx++)
					p_indices_ptr->emplace_back(base_indices[value_idx]);
			}
			else if (subset_index_count == 1)
			{
				// OK, we have a simple vector-style subset that is not NULL; handle it as we did in Eidos 1.5 and earlier
				EidosValue_SP second_child_value = subset_indices[0];
				EidosValueType second_child_type = second_child_value->Type();
				
				if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat) && (second_child_type != EidosValueType::kValueLogical))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): index operand type " << second_child_type << " is not supported by the '[]' operator." << EidosTerminate(parent_token);
				if (second_child_value->DimensionCount() != 1)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): a matrix or array index operand is not supported by the '[]' operator." << EidosTerminate(parent_token);
				
				int second_child_count = second_child_value->Count();
				
				if (second_child_type == EidosValueType::kValueLogical)
				{
					// A logical vector must exactly match in length; if it does, it selects corresponding indices from base_indices
					if (second_child_count != base_indices_count)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << EidosTerminate(parent_token);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
					{
						eidos_logical_t logical_value = second_child_value->LogicalAtIndex(value_idx, parent_token);
						
						if (logical_value)
							p_indices_ptr->emplace_back(base_indices[value_idx]);
					}
				}
				else
				{
					// A numeric vector can be of any length; each number selects the index at that index in base_indices
					if (second_child_type == EidosValueType::kValueInt)
					{
						if (second_child_count == 1)
						{
							int64_t index_value = second_child_value->IntAtIndex(0, parent_token);
							
							if ((index_value < 0) || (index_value >= base_indices_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(parent_token);
							else
								p_indices_ptr->emplace_back(base_indices[index_value]);
						}
						else if (second_child_count)
						{
							// fast vector access for the non-singleton case
							const int64_t *second_child_data = second_child_value->IntVector()->data();
							
							for (int value_idx = 0; value_idx < second_child_count; value_idx++)
							{
								int64_t index_value = second_child_data[value_idx];
								
								if ((index_value < 0) || (index_value >= base_indices_count))
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(parent_token);
								else
									p_indices_ptr->emplace_back(base_indices[index_value]);
							}
						}
					}
					else // (second_child_type == EidosValueType::kValueFloat))
					{
						// We do not optimize the float case with direct vector access, because IntAtIndex() has complex behavior
						// for EidosValue_Float that we want to get here; subsetting with float vectors is slow, don't do it.
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, parent_token);
							
							if ((index_value < 0) || (index_value >= base_indices_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(parent_token);
							else
								p_indices_ptr->emplace_back(base_indices[index_value]);
						}
					}
				}
			}
			else
			{
				// We have a new matrix/array-style subset, like x[1,], x[,1], x[,], etc.; this is Eidos 1.6 and later
				// Handle each subset value in turn, determining which subset of each dimension will be carried forward
				// We tabulate the status of each dimension, and then put it all together at the end as a result value
				
				// First, however, we have to check that the user is not attempting something like x[1,][,3] = rvalue.
				// That is not allowed at present, because the [1,] changes the dimensions of the value, and we have
				// no obvious way of tracking that.  It would be nice if it worked, since it makes sense in principle,
				// but it isn't worth building a whole complex code architecture around.  So here we check the left-hand
				// side that provided us with the host that we're subsetting, and if it is itself a subset operation
				// then it's an error.  A similar problem exists for property access, like x.foo[1,] = rvalue, because
				// the property access erases the dimensions of x, and so a matrix/array-style subscript makes no sense;
				// that might get caught somewhere else, but to make sure we're not heading into a situation that will
				// cause us to produce incorrect output, we check for it here as well.
				{
					EidosToken *left_token = left_operand->token_;
					EidosTokenType left_token_type = left_token->token_type_;
					
					if (left_token_type == EidosTokenType::kTokenLBracket)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): chaining of matrix/array-style subsets in assignments is not currently supported (although it is permitted by the Eidos language definition)." << EidosTerminate(parent_token);
					if (left_token_type == EidosTokenType::kTokenDot)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): cannot assign into a subset of a property; not an lvalue." << EidosTerminate(parent_token);
				}
				
				// OK, we are subsetting directly off of an identifier, given the check above.  This means we can ignore
				// base_indices and p_property_string_id_ptr, and just work directly with first_child_value; it is what
				// we are subsetting off of.
				
				const int64_t *first_child_dim = first_child_value->Dimensions();
				std::vector<std::vector<int64_t>> inclusion_indices;	// the chosen indices for each dimension
				std::vector<int> inclusion_counts;						// the number of chosen indices for each dimension
				bool empty_dimension = false;
				
				for (int subset_index = 0; subset_index < subset_index_count; ++subset_index)
				{
					EidosValue_SP subset_value = subset_indices[subset_index];
					EidosValueType subset_type = subset_value->Type();
					int subset_count = subset_value->Count();
					int dim_size = (int)first_child_dim[subset_index];
					std::vector<int64_t> indices;
					
					if (subset_type == EidosValueType::kValueNULL)
					{
						// We skipped over this dimension or had NULL, so every valid index in the dimension is included
						for (int dim_index = 0; dim_index < dim_size; ++dim_index)
							indices.push_back(dim_index);
					}
					else if (subset_type == EidosValueType::kValueLogical)
					{
						// We have a logical subset, which must equal the dimension size and gets translated directly into inclusion_indices
						if (subset_count != dim_size)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): the '[]' operator requires that the size() of a logical index operand must match the corresponding dimension of the indexed operand." << EidosTerminate(parent_token);
						
						const eidos_logical_t *logical_index_data = subset_value->LogicalVector()->data();
						
						for (int dim_index = 0; dim_index < dim_size; dim_index++)
							if (logical_index_data[dim_index])
								indices.push_back(dim_index);
					}
					else
					{
						// We have a float or integer subset, which selects indices within inclusion_indices
						for (int index_index = 0; index_index < subset_count; index_index++)
						{
							int64_t index_value = subset_value->IntAtIndex(index_index, parent_token);
							
							if ((index_value < 0) || (index_value >= dim_size))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(parent_token);
							else
								indices.push_back(index_value);
						}
					}
					
					if (indices.size() == 0)
					{
						empty_dimension = true;
						break;
					}
					
					inclusion_counts.push_back((int)indices.size());
					inclusion_indices.emplace_back(indices);
				}
				
				if (empty_dimension)
				{
					// If there was a dimension where no index was included, no indices are included, so we do nothing
				}
				else
				{
					// We have tabulated the subsets; now tabulate the included values into p_indices_ptr.  To do this, we count up
					// in the base system established by inclusion_counts, and add the referenced index at each step along the way.
					std::vector<int> generating_counter(subset_index_count, 0);
					
					do
					{
						// add the value referenced by generating_counter in inclusion_indices
						int64_t referenced_index = 0;
						int64_t dim_skip = 1;
						
						for (int counter_index = 0; counter_index < subset_index_count; ++counter_index)
						{
							int counter_value = generating_counter[counter_index];
							int64_t inclusion_index_value = inclusion_indices[counter_index][counter_value];
							
							referenced_index += inclusion_index_value * dim_skip;
							
							dim_skip *= first_child_dim[counter_index];
						}
						
						p_indices_ptr->emplace_back(referenced_index);
						
						// increment generating_counter in the base system of inclusion_counts
						int generating_counter_index = 0;
						
						do
						{
							if (++generating_counter[generating_counter_index] == inclusion_counts[generating_counter_index])
							{
								generating_counter[generating_counter_index] = 0;
								generating_counter_index++;		// carry
							}
							else
								break;
						}
						while (generating_counter_index < subset_index_count);
						
						// if we carried out off the top, we are done adding included values
						if (generating_counter_index == subset_index_count)
							break;
					}
					while (true);
					
					// At this point, Evaluate_Subset() sets the dimensionality of the result.  We are just returning a set of indices,
					// so we don't do that.  If we ever wanted to actually get into allowing x[1,][,3] = rvalue; and such things, at
					// this point we would compute the dimensionality of the result and pass it upward to the caller through another
					// modifiable parameter, I suppose, so that the caller would know what sort of thing they were chaining off of.
				}
			}
			
			break;
		}
		case EidosTokenType::kTokenDot:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_parent_node, "'.'", "EidosInterpreter::_ProcessSubsetAssignment", 2, parent_token);
			
			EidosASTNode *left_operand = p_parent_node->children_[0];
			EidosASTNode *right_operand = p_parent_node->children_[1];
			
			EidosValue_SP first_child_value = FastEvaluateNode(left_operand);
			EidosValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != EidosValueType::kValueObject)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): operand type " << first_child_type << " is not supported by the '.' operator (not an object)." << EidosTerminate(parent_token);
			
			if (right_operand->token_->token_type_ != EidosTokenType::kTokenIdentifier)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): the '.' operator for x.y requires operand y to be an identifier." << EidosTerminate(parent_token);
			
			*p_base_value_ptr = first_child_value;
			*p_property_string_id_ptr = Eidos_GlobalStringIDForString(right_operand->token_->token_string_);
			
			int number_of_elements = first_child_value->Count();	// property operations are guaranteed to produce one value per element
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->emplace_back(element_idx);
			
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_parent_node, "identifier", "EidosInterpreter::_ProcessSubsetAssignment", 0, parent_token);
			
			EidosValue_SP identifier_value_SP = global_symbols_->GetValueOrRaiseForASTNode(p_parent_node);
			EidosValue *identifier_value = identifier_value_SP.get();
			
			// OK, a little bit of trickiness here.  We've got the base value from the symbol table.  The problem is that it
			// could be one of our singleton subclasses, for speed.  We almost never change EidosValue instances once
			// they are constructed, which is why we can use singleton subclasses so pervasively.  But this is one place –
			// the only place, in fact, I think – where that can bite us, because we do in fact need to modify the original
			// EidosValue.  The fix is to detect that we have a singleton value, and actually replace it in the symbol table
			// with a vector-based copy that we can manipulate.  A little gross, but this is the price we pay for speed...
			if (identifier_value->IsSingleton())
			{
				identifier_value_SP = identifier_value->VectorBasedCopy();
				identifier_value = identifier_value_SP.get();
				
				global_symbols_->SetValueForSymbolNoCopy(p_parent_node->cached_stringID_, identifier_value_SP);
			}
			
			*p_base_value_ptr = std::move(identifier_value_SP);
			
			int number_of_elements = identifier_value->Count();	// this value is already defined, so this is fast
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->emplace_back(element_idx);
			
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): (internal error) unexpected node token type " << token_type << "; lvalue required." << EidosTerminate(parent_token);
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
	
	// any assignment involving a void rvalue is illegal; check for that up front
	if (p_rvalue->Type() == EidosValueType::kValueVOID)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): void may never be assigned." << EidosTerminate(nullptr);
	
	switch (token_type)
	{
		case EidosTokenType::kTokenLBracket:
		{
			EIDOS_ASSERT_CHILD_COUNT_GTEQ_X(p_lvalue_node, "'['", "EidosInterpreter::_AssignRValueToLValue", 2, nullptr);
			
			EidosValue_SP base_value;
			EidosGlobalStringID property_string_id = gEidosID_none;
			std::vector<int> indices;
			
			_ProcessSubsetAssignment(&base_value, &property_string_id, &indices, p_lvalue_node);
			
			int index_count = (int)indices.size();
			int rvalue_count = p_rvalue->Count();
			
			if (rvalue_count == 1)
			{
				if (property_string_id == gEidosID_none)
				{
					if (!TypeCheckAssignmentOfEidosValueIntoEidosValue(*p_rvalue, *base_value))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): type mismatch in assignment (" << p_rvalue->Type() << " versus " << base_value->Type() << ")." << EidosTerminate(nullptr);
					
					// we have a multiplex assignment of one value to (maybe) more than one index in a symbol host: x[5:10] = 10
					for (int value_idx = 0; value_idx < index_count; value_idx++)
						base_value->SetValueAtIndex(indices[value_idx], *p_rvalue, nullptr);
				}
				else
				{
					// we have a multiplex assignment of one value to (maybe) more than one index in a property of a symbol host: x.foo[5:10] = 10
					// here we use the guarantee that the member operator returns one result per element, and that elements following sharing semantics,
					// to rearrange this assignment from host.property[indices] = rvalue to host[indices].property = rvalue; this must be equivalent!
					
					// BCH 12/8/2017: We used to allow assignments of the form host.property[indices] = rvalue.  I have decided to change Eidos policy
					// to disallow that form of assignment.  Conceptually, it sort of doesn't make sense, because it implies fetching the property
					// values and assigning into a subset of those fetched values; but the fetched values are not an lvalue at that point.  We did it
					// anyway through a semantic rearrangement, but I now think that was a bad idea.  The conceptual problem became more stark with the
					// addition of matrices and arrays; if host is a matrix/array, host[i,j,...] is too, and so assigning into a host with that form of
					// subset makes conceptual sense, and host[i,j,...].property = rvalue makes sense as well – fetch the indexed values and assign
					// into their property.  But host.property[i,j,...] = rvalue does not make sense, because host.property is always a vector, and
					// cannot be subset in that way!  So the underlying contradiction in the old policy is exposed.  Time for it to change.
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): cannot assign into a subset of a property; not an lvalue." << EidosTerminate(nullptr);
					
					/*
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue_SP temp_lvalue = base_value->GetValueAtIndex(indices[value_idx], nullptr);
						
						if (temp_lvalue->Type() != EidosValueType::kValueObject)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) dot operator used with non-object value." << EidosTerminate(nullptr);
						
						static_cast<EidosValue_Object *>(temp_lvalue.get())->SetPropertyOfElements(property_string_id, *p_rvalue);
					}*/
				}
			}
			else if (index_count == rvalue_count)
			{
				if (property_string_id == gEidosID_none)
				{
					if (!TypeCheckAssignmentOfEidosValueIntoEidosValue(*p_rvalue, *base_value))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): type mismatch in assignment (" << p_rvalue->Type() << " versus " << base_value->Type() << ")." << EidosTerminate(nullptr);
					
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
					
					// BCH 12/8/2017: As above, this form of assignment is no longer legal in Eidos.  See the longer comment above.
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): cannot assign into a subset of a property; not an lvalue." << EidosTerminate(nullptr);
					
					/*
					for (int value_idx = 0; value_idx < index_count; value_idx++)
					{
						EidosValue_SP temp_lvalue = base_value->GetValueAtIndex(indices[value_idx], nullptr);
						EidosValue_SP temp_rvalue = p_rvalue->GetValueAtIndex(value_idx, nullptr);
						
						if (temp_lvalue->Type() != EidosValueType::kValueObject)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) dot operator used with non-object value." << EidosTerminate(nullptr);
						
						static_cast<EidosValue_Object *>(temp_lvalue.get())->SetPropertyOfElements(property_string_id, *temp_rvalue);
					}*/
				}
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): assignment to a subscript requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << EidosTerminate(nullptr);
			}
			
			break;
		}
		case EidosTokenType::kTokenDot:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_lvalue_node, "'.'", "EidosInterpreter::_AssignRValueToLValue", 2, nullptr);
			
			EidosValue_SP first_child_value = FastEvaluateNode(p_lvalue_node->children_[0]);
			EidosValueType first_child_type = first_child_value->Type();
			
			if (first_child_type != EidosValueType::kValueObject)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): operand type " << first_child_type << " is not supported by the '.' operator." << EidosTerminate(nullptr);
			
			EidosASTNode *second_child_node = p_lvalue_node->children_[1];
			
			if (second_child_node->token_->token_type_ != EidosTokenType::kTokenIdentifier)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): (internal error) the '.' operator for x.y requires operand y to be an identifier." << EidosTerminate(nullptr);
			
			// OK, we have <object type>.<identifier>; we can work with that
			static_cast<EidosValue_Object *>(first_child_value.get())->SetPropertyOfElements(second_child_node->cached_stringID_, *p_rvalue);
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_lvalue_node, "identifier", "EidosInterpreter::_AssignRValueToLValue", 0, nullptr);
			
			// Simple identifier; the symbol host is the global symbol table, at least for now
			global_symbols_->SetValueForSymbol(p_lvalue_node->cached_stringID_, std::move(p_rvalue));
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): unexpected node token type " << token_type << "; lvalue required." << EidosTerminate(nullptr);
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
		case EidosTokenType::kTokenLParen:		return Evaluate_Call(p_node);
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
		case EidosTokenType::kTokenConditional:	return Evaluate_Conditional(p_node);
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
		case EidosTokenType::kTokenFunction:	return Evaluate_FunctionDecl(p_node);
		default:
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateNode): unexpected node token type " << p_node->token_->token_type_ << "." << EidosTerminate(p_node->token_);
	}
}

EidosValue_SP EidosInterpreter::Evaluate_NullStatement(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_NullStatement()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_NullStatement", 0);
	
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_NullStatement()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_CompoundStatement(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_CompoundStatement()");
	
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		EidosValue_SP statement_result_SP = FastEvaluateNode(child_node);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(child_node->profile_total_);
#endif
		
		// a next, break, or return makes us exit immediately, out to the (presumably enclosing) loop evaluator
		if (next_statement_hit_ || break_statement_hit_)
			break;
		if (return_statement_hit_)
		{
			result_SP = std::move(statement_result_SP);
			break;
		}
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operand type " << first_child_type << " is not supported by the ':' operator." << EidosTerminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operand type " << second_child_type << " is not supported by the ':' operator." << EidosTerminate(operator_token);
	
	int first_child_count = p_first_child_value.Count();
	int second_child_count = p_second_child_value.Count();
	
	if ((first_child_count != 1) || (second_child_count != 1))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operands of the ':' operator must have size() == 1." << EidosTerminate(operator_token);
	
	if ((p_first_child_value.DimensionCount() != 1) || (p_second_child_value.DimensionCount() != 1))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operands of the ':' operator must not be matrices or arrays." << EidosTerminate(operator_token);
	
	// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
	bool underflow = false;
	
	if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
	{
		int64_t first_int = p_first_child_value.IntAtIndex(0, operator_token);
		int64_t second_int = p_second_child_value.IntAtIndex(0, operator_token);
		
		if (first_int <= second_int)
		{
			if (second_int - first_int + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
			EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(second_int - first_int + 1);
			
			for (int64_t range_index = 0; range_index <= second_int - first_int; ++range_index)
				int_result->set_int_no_check(range_index + first_int, range_index);
			
			result_SP = std::move(int_result_SP);
		}
		else
		{
			if (first_int - second_int + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
			EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(first_int - second_int + 1);
			
			for (int64_t range_index = 0; range_index <= first_int - second_int; ++range_index)
				int_result->set_int_no_check(first_int - range_index, range_index);
			
			result_SP = std::move(int_result_SP);
		}
	}
	else
	{
		double first_float = p_first_child_value.FloatAtIndex(0, operator_token);
		double second_float = p_second_child_value.FloatAtIndex(0, operator_token);
		
		if (std::isnan(first_float) || std::isnan(second_float))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operands of the ':' operator must not be NAN." << EidosTerminate(operator_token);
		
		if (first_float <= second_float)
		{
			if (second_float - first_float + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->reserve((int)(second_float - first_float + 1));
			
			for (double range_index = first_float; range_index <= second_float; )
			{
				float_result->push_float(range_index);
				
				// be careful not to hang due to underflow
				double next_index = range_index + 1.0;
				
				if (next_index == range_index)
				{
					// CODE COVERAGE: This is dead code
					underflow = true;
					break;
				}
				
				range_index = next_index;
			}
			
			result_SP = std::move(float_result_SP);
		}
		else
		{
			if (first_float - second_float + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->reserve((int)(first_float - second_float + 1));
			
			for (double range_index = first_float; range_index >= second_float; )
			{
				float_result->push_float(range_index);
				
				// be careful not to hang due to underflow
				double next_index = range_index - 1.0;
				
				if (next_index == range_index)
				{
					// CODE COVERAGE: This is dead code
					underflow = true;
					break;
				}
				
				range_index = next_index;
			}
			
			result_SP = std::move(float_result_SP);
		}
	}
	
	if (underflow)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): the floating-point range could not be constructed due to underflow." << EidosTerminate(operator_token);	// CODE COVERAGE: This is dead code
	
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_RangeExpr(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_RangeExpr()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_RangeExpr", 2);

	// Constant expressions involving the range operator are particularly common, so we cache them for reuse
	// here, as an optimization.  If we have a cached value, we can simply return it.
	EidosValue_SP result_SP = p_node->cached_range_value_;
	
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
			p_node->cached_range_value_ = result_SP;
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_RangeExpr()");
	return result_SP;
}

// This processes the arguments for a call node (either a function call or a method call), ignoring the first child
// node since that is the call name node, not an argument node.  Named arguments and default arguments are handled
// here, and a final argument list is placed into the buffer passed in by the caller (guaranteed to be large enough).
// The final number of arguments for the call is returned.
int EidosInterpreter::_ProcessArgumentList(const EidosASTNode *p_node, const EidosCallSignature *p_call_signature, EidosValue_SP *p_arg_buffer)
{
	const std::vector<EidosASTNode *> &node_children = p_node->children_;
	
	// Run through the argument nodes, evaluate them, and put the resulting pointers into the arguments buffer,
	// interleaving default arguments and handling named arguments as we go.
	int processed_arg_count = 0;
	auto node_children_end = node_children.end();
	int sig_arg_index = 0;
	int sig_arg_count = (int)p_call_signature->arg_name_IDs_.size();
	bool had_named_argument = false;
	
	for (auto child_iter = node_children.begin() + 1; child_iter != node_children_end; ++child_iter)
	{
		EidosASTNode *child = *child_iter;
		bool is_named_argument = (child->token_->token_type_ == EidosTokenType::kTokenAssign);
		
#if DEBUG
		if (is_named_argument && (child->children_.size() != 2))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): (internal error) named argument node child count != 2." << EidosTerminate(nullptr);
#endif
		
		if (sig_arg_index < sig_arg_count)
		{
			if (!is_named_argument)
			{
				// We have a non-named argument; it will go into the next argument slot from the signature
				if (had_named_argument)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): unnamed argument may not follow after named arguments; once named arguments begin, all arguments must be named arguments (or ellipsis arguments)." << EidosTerminate(nullptr);
				
				sig_arg_index++;
			}
			else
			{
				// We have a named argument; get information on it from its children
				const std::vector<EidosASTNode *> &child_children = child->children_;
				
				EidosASTNode *named_arg_name_node = child_children[0];
				EidosASTNode *named_arg_value_node = child_children[1];
				
				// Get the identifier for the argument name
				EidosGlobalStringID named_arg_nameID = named_arg_name_node->cached_stringID_;
				
				// Now re-point child at the value node
				child = named_arg_value_node;
				
				// While this argument's name doesn't match the expected argument, insert default values for optional arguments
				do 
				{
					EidosGlobalStringID arg_name_ID = p_call_signature->arg_name_IDs_[sig_arg_index];
					
					if (named_arg_nameID == arg_name_ID)
					{
						sig_arg_index++;
						break;
					}
					
					EidosValueMask arg_mask = p_call_signature->arg_masks_[sig_arg_index];
					
					if (!(arg_mask & kEidosValueMaskOptional))
					{
						const std::string &named_arg = named_arg_name_node->token_->token_string_;
						
						// We have special error-handling for apply() because sapply() used to be named apply() and we want to steer users to the new call
						if ((p_call_signature->call_name_ == "apply") && ((named_arg == "lambdaSource") || (named_arg == "simplify")))
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument " << named_arg << " skipped over required argument " << p_call_signature->arg_names_[sig_arg_index] << "." << std::endl << "NOTE: The apply() function was renamed sapply() in Eidos 1.6, and a new function named apply() has been added; you may need to change this call to be a call to sapply() instead." << EidosTerminate(nullptr);
						
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument " << named_arg << " skipped over required argument " << p_call_signature->arg_names_[sig_arg_index] << "." << EidosTerminate(nullptr);
					}
					
					EidosValue_SP default_value = p_call_signature->arg_defaults_[sig_arg_index];
					
#if DEBUG
					if (!default_value)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): (internal error) missing default value for optional argument." << EidosTerminate(nullptr);
#endif
					
					p_arg_buffer[processed_arg_count] = std::move(default_value);
					processed_arg_count++;
					
					// Move to the next signature argument; if we have run out of them, then treat this argument is illegal
					sig_arg_index++;
					if (sig_arg_index == sig_arg_count)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): ran out of optional arguments while searching for named argument " << named_arg_name_node->token_->token_string_ << "." << EidosTerminate(nullptr);
				}
				while (true);
				
				had_named_argument = true;
			}
		}
		else
		{
			// We're beyond the end of the signature's arguments; check whether this argument can qualify as an ellipsis argument
			if (!p_call_signature->has_ellipsis_)
			{
				// OK, this argument is definitely illegal; check whether it is a named argument that is out of order, or just an excess argument
				if (is_named_argument)
				{
					// We have a named argument; get information on it from its children
					EidosASTNode *named_arg_name_node = child->children_[0];
					EidosGlobalStringID named_arg_nameID = named_arg_name_node->cached_stringID_;
					const std::string &named_arg = named_arg_name_node->token_->token_string_;
					
					// Look for a named parameter in the call signature that matches
					for (int sig_check_index = 0; sig_check_index < sig_arg_count; ++sig_check_index)
					{
						EidosGlobalStringID arg_name_ID = p_call_signature->arg_name_IDs_[sig_check_index];
						
						if (named_arg_nameID == arg_name_ID)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): argument " << named_arg << " to function " << p_call_signature->call_name_ << " could not be matched; probably supplied out of order or supplied more than once." << EidosTerminate(nullptr);
					}
					
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): unrecognized named argument " << named_arg << " to function " << p_call_signature->call_name_ << "." << EidosTerminate(nullptr);
				}
				else
				{
					if (had_named_argument)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): too many arguments supplied to function " << p_call_signature->call_name_ << " (after handling named arguments, which might have filled in default values for previous arguments)." << EidosTerminate(nullptr);
					else
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): too many arguments supplied to function " << p_call_signature->call_name_ << "." << EidosTerminate(nullptr);
				}
			}
			
			if (child->token_->token_type_ == EidosTokenType::kTokenAssign)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument " << child->children_[0]->token_->token_string_ << " in ellipsis argument section (after the last explicit argument)." << EidosTerminate(nullptr);
		}
		
		// Evaluate the child and emplace it in the arguments buffer
		p_arg_buffer[processed_arg_count] = FastEvaluateNode(child);
		processed_arg_count++;
	}
	
	// Handle any remaining arguments in the signature
	while (sig_arg_index < sig_arg_count)
	{
		EidosValueMask arg_mask = p_call_signature->arg_masks_[sig_arg_index];
		
		if (!(arg_mask & kEidosValueMaskOptional))
		{
			// We have special error-handling for apply() because sapply() used to be named apply() and we want to steer users to the new call
			if ((p_call_signature->call_name_ == "apply") && (p_call_signature->arg_names_[sig_arg_index] == "lambdaSource"))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): missing required argument " << p_call_signature->arg_names_[sig_arg_index] << "." << std::endl << "NOTE: The apply() function was renamed sapply() in Eidos 1.6, and a new function named apply() has been added; you may need to change this call to be a call to sapply() instead." << EidosTerminate(nullptr);
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): missing required argument " << p_call_signature->arg_names_[sig_arg_index] << "." << EidosTerminate(nullptr);
		}
		
		EidosValue_SP default_value = p_call_signature->arg_defaults_[sig_arg_index];
		
#if DEBUG
		if (!default_value)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): (internal error) missing default value for optional argument." << EidosTerminate(nullptr);
#endif
		
		p_arg_buffer[processed_arg_count] = std::move(default_value);
		processed_arg_count++;
		
		sig_arg_index++;
	}
	
	// Now that we have a fully assembled argument list, check it
	p_call_signature->CheckArguments(p_arg_buffer, processed_arg_count);
	
	return processed_arg_count;
}

EidosValue_SP EidosInterpreter::DispatchUserDefinedFunction(const EidosFunctionSignature &p_function_signature, const EidosValue_SP *const p_arguments, int p_argument_count)
{
	EidosValue_SP result_SP(nullptr);
	
	// We need to add a new variables symbol table on to the top of the symbol table stack, for parameters and local variables.
	EidosSymbolTable new_symbols(EidosSymbolTableType::kVariablesTable, global_symbols_);
	
	// Set up variables for the function's parameters; they have already been type-checked and had default
	// values substituted and so forth, by the Eidos function call dispatch code.
	if ((int)p_function_signature.arg_name_IDs_.size() != p_argument_count)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::DispatchUserDefinedFunction): (internal error) parameter count does not match argument count." << EidosTerminate(nullptr);
	
	for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		new_symbols.SetValueForSymbol(p_function_signature.arg_name_IDs_[arg_index], std::move(p_arguments[arg_index]));
	
	// Errors in functions should be reported for the function's script, not for the calling script,
	// if possible.  In the GUI this does not work well, however; there, errors should be
	// reported as occurring in the call to the function.  Here we save off the current
	// error context and set up the error context for reporting errors inside the function,
	// in case that is possible; see how exceptions are handled below.
	int error_start_save = gEidosCharacterStartOfError;
	int error_end_save = gEidosCharacterEndOfError;
	int error_start_save_UTF16 = gEidosCharacterStartOfErrorUTF16;
	int error_end_save_UTF16 = gEidosCharacterEndOfErrorUTF16;
	EidosScript *current_script_save = gEidosCurrentScript;
	bool executing_runtime_script_save = gEidosExecutingRuntimeScript;
	
	// Execute inside try/catch so we can handle errors well
	gEidosCharacterStartOfError = -1;
	gEidosCharacterEndOfError = -1;
	gEidosCharacterStartOfErrorUTF16 = -1;
	gEidosCharacterEndOfErrorUTF16 = -1;
	gEidosCurrentScript = p_function_signature.body_script_;
	gEidosExecutingRuntimeScript = true;
	
	try
	{
		EidosInterpreter interpreter(*p_function_signature.body_script_, new_symbols, function_map_, Context());
		
		// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
		// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
		// bugs that would otherwise not manifest.
		result_SP = interpreter.EvaluateInterpreterBlock(false, false);	// don't print output, don't return last statement value
		
		// Assimilate output
		interpreter.FlushExecutionOutputToStream(ExecutionOutputStream());
	}
	catch (...)
	{
		// If exceptions throw, then we want to set up the error information to highlight the
		// function call that failed, since we can't highlight the actual error.  (If exceptions
		// don't throw, this catch block will never be hit; exit() will already have been called
		// and the error will have been reported from the context of the function's script.)
		if (gEidosTerminateThrows)
		{
			gEidosCharacterStartOfError = error_start_save;
			gEidosCharacterEndOfError = error_end_save;
			gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
			gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
			gEidosCurrentScript = current_script_save;
			gEidosExecutingRuntimeScript = executing_runtime_script_save;
		}
		
		throw;
	}
	
	// Restore the normal error context in the event that no exception occurring within the function
	gEidosCharacterStartOfError = error_start_save;
	gEidosCharacterEndOfError = error_end_save;
	gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
	gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
	gEidosCurrentScript = current_script_save;
	gEidosExecutingRuntimeScript = executing_runtime_script_save;
	
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Call(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Call()");
	
	EidosValue_SP result_SP;
	
	// We do not evaluate the call name node (our first child) to get a function/method object; there is no such type
	// in Eidos for now.  Instead, we extract the identifier name directly from the node and work with it.  If the
	// node is an identifier, it is a function call; if it is a dot operator, it is a method call; other constructs
	// are illegal since expressions cannot evaluate to function/method objects.
	const std::vector<EidosASTNode *> &node_children = p_node->children_;
	EidosASTNode *call_name_node = node_children[0];
	EidosTokenType call_name_token_type = call_name_node->token_->token_type_;
	EidosToken *call_identifier_token = nullptr;
	
	if (call_name_token_type == EidosTokenType::kTokenIdentifier)
	{
		//
		//	FUNCTION CALL DISPATCH
		//
		call_identifier_token = call_name_node->token_;
		
		// OK, we have <identifier>(...); that's a well-formed function call
		const std::string *function_name = &(call_identifier_token->token_string_);
		const EidosFunctionSignature *function_signature = call_name_node->cached_signature_.get();
		
		// If the function call is a built-in Eidos function, we might already have a pointer to its signature cached; if not, we'll have to look it up
		if (!function_signature)
		{
			// Get the function signature and check our arguments against it
			auto signature_iter = function_map_.find(*function_name);
			
			if (signature_iter == function_map_.end())
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): unrecognized function name " << *function_name << "." << EidosTerminate(call_identifier_token);
			
			function_signature = signature_iter->second.get();
		}
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(call_identifier_token);
		
		// Argument processing
		int max_arg_count = (function_signature->has_ellipsis_ ? ((int)(node_children.size() - 1)) + 1 : (int)function_signature->arg_name_IDs_.size() + 1);
		
		if (max_arg_count <= 5)
		{
			EidosValue_SP (arguments_array[5]);
			int processed_arg_count = _ProcessArgumentList(p_node, function_signature, arguments_array);
			
			if (function_signature->internal_function_)
			{
				result_SP = function_signature->internal_function_(arguments_array, processed_arg_count, *this);
			}
			else if (!function_signature->delegate_name_.empty())
			{
				if (eidos_context_)
					result_SP = eidos_context_->ContextDefinedFunctionDispatch(*function_name, arguments_array, processed_arg_count, *this);
				else
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): function " << function_name << " is defined by the Context, but the Context is not defined." << EidosTerminate(nullptr);
			}
			else if (function_signature->body_script_)
			{
				result_SP = DispatchUserDefinedFunction(*function_signature, arguments_array, processed_arg_count);
			}
			else
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): unbound function " << *function_name << "." << EidosTerminate(call_identifier_token);
		}
		else
		{
			std::vector<EidosValue_SP> arguments;
			
			arguments.resize(max_arg_count);
			
			EidosValue_SP *arguments_ptr = arguments.data();
			int processed_arg_count = _ProcessArgumentList(p_node, function_signature, arguments_ptr);
			
			if (function_signature->internal_function_)
			{
				result_SP = function_signature->internal_function_(arguments_ptr, processed_arg_count, *this);
			}
			else if (!function_signature->delegate_name_.empty())
			{
				if (eidos_context_)
					result_SP = eidos_context_->ContextDefinedFunctionDispatch(*function_name, arguments_ptr, processed_arg_count, *this);
				else
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): function " << function_name << " is defined by the Context, but the Context is not defined." << EidosTerminate(nullptr);
			}
			else if (function_signature->body_script_)
			{
				result_SP = DispatchUserDefinedFunction(*function_signature, arguments_ptr, processed_arg_count);
			}
			else
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): unbound function " << *function_name << "." << EidosTerminate(call_identifier_token);
		}
		
		// If the code above supplied no return value, raise when in debug.  Not in debug, we crash.
#ifdef DEBUG
		if (!result_SP)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): (internal error) function " << *function_name << " returned nullptr." << EidosTerminate(call_identifier_token);
#endif
		
		// Check the return value against the signature
		function_signature->CheckReturn(*result_SP);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
	}
	else if (call_name_token_type == EidosTokenType::kTokenDot)
	{
		//
		//	METHOD CALL DISPATCH
		//
		EIDOS_ASSERT_CHILD_COUNT_X(call_name_node, "'.'", "EidosInterpreter::Evaluate_Call", 2, call_name_node->token_);
		
		EidosValue_SP first_child_value = FastEvaluateNode(call_name_node->children_[0]);
		EidosValueType first_child_type = first_child_value->Type();
		
		if (first_child_type != EidosValueType::kValueObject)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): operand type " << first_child_type << " is not supported by the '.' operator." << EidosTerminate(call_name_node->token_);
		
		EidosASTNode *second_child_node = call_name_node->children_[1];
		
		if (second_child_node->token_->token_type_ != EidosTokenType::kTokenIdentifier)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): (internal error) the '.' operator for x.y requires operand y to be an identifier." << EidosTerminate(call_name_node->token_);
		
		// OK, we have <object type>.<identifier>(...); that's a well-formed method call
		call_identifier_token = second_child_node->token_;
		
		EidosGlobalStringID method_id = second_child_node->cached_stringID_;
		EidosValue_Object_SP method_object = static_pointer_cast<EidosValue_Object>(std::move(first_child_value));	// guaranteed by the Type() call above
		
		// Look up the method signature; this could be cached in the tree, probably, since we guarantee that method signatures are unique by name
		const EidosMethodSignature *method_signature = method_object->Class()->SignatureForMethod(method_id);
		
		if (!method_signature)
		{
			// Give more helpful error messages for deprecated methods
			if (call_identifier_token->token_string_ == "method")
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): method " << Eidos_StringForGlobalStringID(method_id) << "() is not defined on object element type " << method_object->ElementType() << ".  Note that method() has been renamed to methodSignature()." << EidosTerminate(call_identifier_token);
			else if (call_identifier_token->token_string_ == "property")
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): method " << Eidos_StringForGlobalStringID(method_id) << "() is not defined on object element type " << method_object->ElementType() << ".  Note that property() has been renamed to propertySignature()." << EidosTerminate(call_identifier_token);
			else
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): method " << Eidos_StringForGlobalStringID(method_id) << "() is not defined on object element type " << method_object->ElementType() << "." << EidosTerminate(call_identifier_token);
		}
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(call_identifier_token);
		
		// Argument processing
		int max_arg_count = (method_signature->has_ellipsis_ ? ((int)(node_children.size() - 1)) + 1 : (int)method_signature->arg_name_IDs_.size() + 1);
		
		if (max_arg_count <= 10)
		{
			// This flag is used to prevent re-entrancy below.  Note that re-entrancy applies to this whole section of code,
			// including both argument processing and call execution.  If the user writes Eidos code like f(g()), that will
			// re-enter here, because g() will be called as a side effect of argument processing while managing the f() call;
			// the call to g() is finished before the call to f() begins, but Evaluate_Call() is re-entered nevertheless.
			// It would be nice to be able to use a static argument buffer in the re-entrant case too, but I guess that would
			// require a stack of buffers, which seems excessively complex.  Maybe this is already excessively complex, but
			// the argument buffer construction/destruction did show up significantly in profiling.  BCH 1/18/2018
			static bool reentrancy_flag = false;
			
			if (!reentrancy_flag)
			{
				// We are not re-entrant, so we can use a statically allocated buffer to hold our arguments.  This is faster
				// because the buffer doesn't need to be constructed and destructed; we just reset used indices below.
				static EidosValue_SP (arguments_array[10]);
				Eidos_simple_lock reentrancy_lock(reentrancy_flag);	// lock with RAII to prevent re-entrancy here
				
				int processed_arg_count = _ProcessArgumentList(p_node, method_signature, arguments_array);
				
				// If the method is a class method, dispatch it to the class object
				if (method_signature->is_class_method)
				{
					// Note that starting in Eidos 1.1 we pass method_object to ExecuteClassMethod(), to allow class methods to
					// act as non-multicasting methods that operate on the whole target vector.  BCH 11 June 2016
					result_SP = method_object->Class()->ExecuteClassMethod(method_id, method_object.get(), arguments_array, processed_arg_count, *this);
					
					method_signature->CheckReturn(*result_SP);
				}
				else
				{
					result_SP = method_object->ExecuteMethodCall(method_id, (EidosInstanceMethodSignature *)method_signature, arguments_array, processed_arg_count, *this);
				}
				
				// Clean up the entries of arguments_array that were used.  Using a static buffer this way means that we avoid
				// constructing/destructing the buffer every time; we just reset the values we use back to nullptr each time.
				// A throw will blow past this, but it doesn't really matter; that just means a few EidosValues will get held
				// longer than they otherwise would be, but they'll get throw out eventually and it shouldn't matter.  They
				// might have invalid pointers in them by then, if they're of type object, but nobody will use them.
				for (int arg_index = 0; arg_index < processed_arg_count; ++arg_index)
					arguments_array[arg_index].reset();
			}
			else
			{
				// We are re-entrant, so we can't use the static buffer above; we have to allocate and deallocate a local buffer.
				// This is identical to the dispatch code above, except for the use of the local stack-allocated argument buffer.
				EidosValue_SP (arguments_array[10]);
				
				int processed_arg_count = _ProcessArgumentList(p_node, method_signature, arguments_array);
				
				// If the method is a class method, dispatch it to the class object
				if (method_signature->is_class_method)
				{
					// Note that starting in Eidos 1.1 we pass method_object to ExecuteClassMethod(), to allow class methods to
					// act as non-multicasting methods that operate on the whole target vector.  BCH 11 June 2016
					result_SP = method_object->Class()->ExecuteClassMethod(method_id, method_object.get(), arguments_array, processed_arg_count, *this);
					
					method_signature->CheckReturn(*result_SP);
				}
				else
				{
					result_SP = method_object->ExecuteMethodCall(method_id, (EidosInstanceMethodSignature *)method_signature, arguments_array, processed_arg_count, *this);
				}
			}
		}
		else
		{
			std::vector<EidosValue_SP> arguments;
			
			arguments.resize(max_arg_count);
			
			EidosValue_SP *arguments_ptr = arguments.data();
			int processed_arg_count = _ProcessArgumentList(p_node, method_signature, arguments_ptr);
			
			// If the method is a class method, dispatch it to the class object
			if (method_signature->is_class_method)
			{
				// Note that starting in Eidos 1.1 we pass method_object to ExecuteClassMethod(), to allow class methods to
				// act as non-multicasting methods that operate on the whole target vector.  BCH 11 June 2016
				result_SP = method_object->Class()->ExecuteClassMethod(method_id, method_object.get(), arguments_ptr, processed_arg_count, *this);
				
				method_signature->CheckReturn(*result_SP);
			}
			else
			{
				result_SP = method_object->ExecuteMethodCall(method_id, (EidosInstanceMethodSignature *)method_signature, arguments_ptr, processed_arg_count, *this);
			}
		}
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): the '()' operator may only be used with a function name or method name (illegal operand for a function call operation)." << EidosTerminate(call_name_node->token_);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Call()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Subset(const EidosASTNode *p_node)
{
	// Note that the logic here is very parallel to that of EidosInterpreter::_ProcessSubsetAssignment()
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Subset()");
	EIDOS_ASSERT_CHILD_COUNT_GTEQ("EidosInterpreter::Evaluate_Subset", 2);
	
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValueType first_child_type = first_child_value->Type();
	int first_child_dim_count = first_child_value->DimensionCount();
	
	if (first_child_type == EidosValueType::kValueVOID)
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): subsetting of a value of type void is not supported by the '[]' operator." << EidosTerminate(operator_token);
	
	// organize our subset arguments
	int child_count = (int)p_node->children_.size();
	std::vector <EidosValue_SP> subset_indices;
	
	for (int child_index = 1; child_index < child_count; ++child_index)
	{
		const EidosASTNode *subset_index_node = p_node->children_[child_index];
		EidosTokenType subset_index_token_type = subset_index_node->token_->token_type_;
		
		if ((subset_index_token_type == EidosTokenType::kTokenComma) || (subset_index_token_type == EidosTokenType::kTokenRBracket))
		{
			// We have a placeholder node indicating a skipped expression, so we save NULL as the value
			subset_indices.push_back(gStaticEidosValueNULL);
		}
		else
		{
			// We have an expression node, so we evaluate it, check the value, and save it
			EidosValue_SP child_value = FastEvaluateNode(subset_index_node);
			EidosValueType child_type = child_value->Type();
			
			// BCH 4/29/2019: handle the simple case of a singleton integer subset as fast as we can, since this is the common case
			// This can be commented out harmlessly; this case is also handled below, just slower
			if ((child_count == 2) && (child_type == EidosValueType::kValueInt) && (child_value->Count() == 1) && (child_value->DimensionCount() == 1))
			{
				int subset_index = (int)child_value->IntAtIndex(0, operator_token);
				EIDOS_EXIT_EXECUTION_LOG("Evaluate_Subset()");
				return first_child_value->GetValueAtIndex(subset_index, operator_token);
			}
			
			if ((child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueFloat) && (child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueNULL))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): index operand type " << child_type << " is not supported by the '[]' operator." << EidosTerminate(operator_token);
			if (child_value->DimensionCount() != 1)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): a matrix or array index operand is not supported by the '[]' operator." << EidosTerminate(operator_token);
			
			subset_indices.emplace_back(child_value);
		}
	}
	
	int subset_index_count = (int)subset_indices.size();
	
	if ((subset_index_count != first_child_dim_count) && (subset_index_count != 1))
	{
		// We have the wrong number of subset arguments for our dimensionality, so this is an error
		if (subset_index_count > first_child_dim_count)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): too many subset arguments for the indexed operand's dimensionality." << EidosTerminate(operator_token);
		else // (subset_index_count < first_child_dim_count)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): too few subset arguments for the indexed operand's dimensionality." << EidosTerminate(operator_token);
	}
	else if (first_child_type == EidosValueType::kValueNULL)
	{
		// Any subscript of NULL returns NULL; however, we evaluated the expression nodes above first, for side effects and to check for errors
		result_SP = gStaticEidosValueNULL;
	}
	else if ((subset_index_count == 1) && (subset_indices[0] == gStaticEidosValueNULL))
	{
		// We have a single subset argument of null, so we have x[] or x[NULL]; just return x, but as a vector, not a matrix/array
		if (first_child_dim_count == 1)
			result_SP = first_child_value;
		else
		{
			result_SP = first_child_value->CopyValues();
			
			result_SP->SetDimensions(1, nullptr);
		}
	}
	else if (subset_index_count == 1)
	{
		// OK, we have a simple vector-style subset that is not NULL; handle it as we did in Eidos 1.5 and earlier
		EidosValue_SP second_child_value = subset_indices[0];
		EidosValueType second_child_type = second_child_value->Type();
		
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		
		if (second_child_type == EidosValueType::kValueLogical)
		{
			// Subsetting with a logical vector means the vectors must match in length; indices with a T value will be taken
			if (first_child_count != second_child_count)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << EidosTerminate(operator_token);
			
			// Subsetting with a logical vector does not attempt to allocate singleton values, for now; seems unlikely to be a frequently hit case
			const eidos_logical_t *logical_index_data = second_child_value->LogicalVector()->data();
			
			if (first_child_count == 1)
			{
				// This is the simple logic using NewMatchingType() / PushValueFromIndexOfEidosValue()
				result_SP = first_child_value->NewMatchingType();
				
				EidosValue *result = result_SP.get();
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
					if (logical_index_data[value_idx])
						result->PushValueFromIndexOfEidosValue(value_idx, *first_child_value, operator_token);
			}
			else
			{
				// Here we can special-case each type for speed since we know we're not dealing with singletons
				if (first_child_type == EidosValueType::kValueLogical)
				{
					const eidos_logical_t *first_child_data = first_child_value->LogicalVector()->data();
					EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
					EidosValue_Logical *logical_result = logical_result_SP->reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_data[value_idx])
							logical_result->push_logical_no_check(first_child_data[value_idx]);
					
					result_SP = std::move(logical_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueInt)
				{
					const int64_t *first_child_data = first_child_value->IntVector()->data();
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_data[value_idx])
							int_result->push_int_no_check(first_child_data[value_idx]);		// cannot use set_int_no_check() because of the if()
					
					result_SP = std::move(int_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueFloat)
				{
					const double *first_child_data = first_child_value->FloatVector()->data();
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_data[value_idx])
							float_result->push_float_no_check(first_child_data[value_idx]);	// cannot use set_int_no_check() because of the if()
					
					result_SP = std::move(float_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueString)
				{
					const std::vector<std::string> &first_child_vec = *first_child_value->StringVector();
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_data[value_idx])
							string_result->PushString(first_child_vec[value_idx]);
					
					result_SP = std::move(string_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueObject)
				{
					EidosObjectElement * const *first_child_vec = first_child_value->ObjectElementVector()->data();
					EidosValue_Object_vector_SP obj_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)first_child_value.get())->Class()));
					EidosValue_Object_vector *obj_result = obj_result_SP->reserve(second_child_count);
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						if (logical_index_data[value_idx])
							obj_result->push_object_element_no_check(first_child_vec[value_idx]);
					
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
			else if (first_child_count == 1)
			{
				// We can't use direct access on first_child_value if it is a singleton, so this needs to be special-cased
				// Note this is identical to the general-case code below that is never hit
				result_SP = first_child_value->NewMatchingType();
				
				EidosValue *result = result_SP.get();
				
				for (int value_idx = 0; value_idx < second_child_count; value_idx++)
				{
					int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
					
					if ((index_value < 0) || (index_value >= first_child_count))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
					else
						result->PushValueFromIndexOfEidosValue((int)index_value, *first_child_value, operator_token);
				}
			}
			else
			{
				// Subsetting with a int/float vector can use a vector of any length; the specific indices referenced will be taken
				if (first_child_type == EidosValueType::kValueFloat)
				{
					// result type is float; optimize for that
					const double *first_child_data = first_child_value->FloatVector()->data();
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const int64_t *int_index_data = second_child_value->IntVector()->data();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_data[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								float_result->set_float_no_check(first_child_data[index_value], value_idx);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								float_result->set_float_no_check(first_child_data[index_value], value_idx);
						}
					}
					
					result_SP = std::move(float_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueInt)
				{
					// result type is integer; optimize for that
					const int64_t *first_child_data = first_child_value->IntVector()->data();
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const int64_t *int_index_data = second_child_value->IntVector()->data();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_data[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								int_result->set_int_no_check(first_child_data[index_value], value_idx);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								int_result->set_int_no_check(first_child_data[index_value], value_idx);
						}
					}
					
					result_SP = std::move(int_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueObject)
				{
					// result type is object; optimize for that
					EidosObjectElement * const *first_child_vec = first_child_value->ObjectElementVector()->data();
					EidosValue_Object_vector_SP obj_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)first_child_value.get())->Class()));
					EidosValue_Object_vector *obj_result = obj_result_SP->resize_no_initialize(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const int64_t *int_index_data = second_child_value->IntVector()->data();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_data[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								obj_result->set_object_element_no_check(first_child_vec[index_value], value_idx);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								obj_result->set_object_element_no_check(first_child_vec[index_value], value_idx);
						}
					}
					
					result_SP = std::move(obj_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueLogical)
				{
					// result type is logical; optimize for that
					const eidos_logical_t *first_child_data = first_child_value->LogicalVector()->data();
					EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
					EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const int64_t *int_index_data = second_child_value->IntVector()->data();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_data[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								logical_result->set_logical_no_check(first_child_data[index_value], value_idx);
						}
					}
					else
					{
						// float indices; we use IntAtIndex() since it has complex behavior
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								logical_result->set_logical_no_check(first_child_data[index_value], value_idx);
						}
					}
					
					result_SP = std::move(logical_result_SP);
				}
				else if (first_child_type == EidosValueType::kValueString)
				{
					// result type is string; optimize for that
					const std::vector<std::string> &first_child_vec = *first_child_value->StringVector();
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(second_child_count);
					
					if (second_child_type == EidosValueType::kValueInt)
					{
						// integer indices; we can use fast access since we know second_child_count != 1
						const int64_t *int_index_data = second_child_value->IntVector()->data();
						
						for (int value_idx = 0; value_idx < second_child_count; value_idx++)
						{
							int64_t index_value = int_index_data[value_idx];
							
							if ((index_value < 0) || (index_value >= first_child_count))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
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
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
							else
								string_result->PushString(first_child_vec[index_value]);
						}
					}
					
					result_SP = std::move(string_result_SP);
				}
				else
				{
					// This is the general case; it should never be hit
					// CODE COVERAGE: This is dead code
					result_SP = first_child_value->NewMatchingType();
					
					EidosValue *result = result_SP.get();
					
					for (int value_idx = 0; value_idx < second_child_count; value_idx++)
					{
						int64_t index_value = second_child_value->IntAtIndex(value_idx, operator_token);
						
						if ((index_value < 0) || (index_value >= first_child_count))
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
						else
							result->PushValueFromIndexOfEidosValue((int)index_value, *first_child_value, operator_token);
					}
				}
			}
		}
	}
	else
	{
		// We have a new matrix/array-style subset, like x[1,], x[,1], x[,], etc.; this is Eidos 1.6 and later
		// Handle each subset value in turn, determining which subset of each dimension will be carried forward
		// We tabulate the status of each dimension, and then put it all together at the end as a result value
		const int64_t *first_child_dim = first_child_value->Dimensions();
		std::vector<std::vector<int64_t>> inclusion_indices;	// the chosen indices for each dimension
		bool empty_dimension = false;
		
		for (int subset_index = 0; subset_index < subset_index_count; ++subset_index)
		{
			EidosValue_SP subset_value = subset_indices[subset_index];
			EidosValueType subset_type = subset_value->Type();
			int subset_count = subset_value->Count();
			int dim_size = (int)first_child_dim[subset_index];
			std::vector<int64_t> indices;
			
			if (subset_type == EidosValueType::kValueNULL)
			{
				// We skipped over this dimension or had NULL, so every valid index in the dimension is included
				for (int dim_index = 0; dim_index < dim_size; ++dim_index)
					indices.push_back(dim_index);
			}
			else if (subset_type == EidosValueType::kValueLogical)
			{
				// We have a logical subset, which must equal the dimension size and gets translated directly into inclusion_indices
				if (subset_count != dim_size)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): the '[]' operator requires that the size() of a logical index operand must match the corresponding dimension of the indexed operand." << EidosTerminate(operator_token);
				
				const eidos_logical_t *logical_index_data = subset_value->LogicalVector()->data();
				
				for (int dim_index = 0; dim_index < dim_size; dim_index++)
					if (logical_index_data[dim_index])
						indices.push_back(dim_index);
			}
			else
			{
				// We have a float or integer subset, which selects indices within inclusion_indices
				for (int index_index = 0; index_index < subset_count; index_index++)
				{
					int64_t index_value = subset_value->IntAtIndex(index_index, operator_token);
					
					if ((index_value < 0) || (index_value >= dim_size))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
					else
						indices.push_back(index_value);
				}
			}
			
			if (indices.size() == 0)
			{
				empty_dimension = true;
				break;
			}
			
			inclusion_indices.emplace_back(indices);
		}
		
		if (empty_dimension)
		{
			// If there was a dimension where no index was included, the result is an empty vector of the right type
			result_SP = first_child_value->NewMatchingType();
		}
		else
		{
			result_SP = first_child_value->Subset(inclusion_indices, false, operator_token);
		}
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Subset()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_MemberRef(const EidosASTNode *p_node)
{
#if defined(DEBUG) || defined(EIDOS_GUI)
	if (logging_execution_)
	{
		// When logging execution, use the slow path so everything gets logged correctly
		EIDOS_ENTRY_EXECUTION_LOG("Evaluate_MemberRef()");
		EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_MemberRef", 2);
		
		EidosToken *operator_token = p_node->token_;
		EidosValue_SP result_SP;
		
		const EidosASTNode *first_child_node = p_node->children_[0];
		EidosValue_SP first_child_value = FastEvaluateNode(first_child_node);
		EidosValueType first_child_type = first_child_value->Type();
		
		if (first_child_type != EidosValueType::kValueObject)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): (internal error) operand type " << first_child_type << " is not supported by the '.' operator." << EidosTerminate(operator_token);
		
		EidosASTNode *second_child_node = p_node->children_[1];
		EidosToken *second_child_token = second_child_node->token_;
		
		if (second_child_token->token_type_ != EidosTokenType::kTokenIdentifier)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): (internal error) the '.' operator for x.y requires operand y to be an identifier." << EidosTerminate(operator_token);
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(second_child_token);
		
		// We offload the actual work to GetPropertyOfElements() to keep things simple here
		result_SP = static_cast<EidosValue_Object *>(first_child_value.get())->GetPropertyOfElements(second_child_node->cached_stringID_);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
		
		EIDOS_EXIT_EXECUTION_LOG("Evaluate_MemberRef()");
		return result_SP;
	}
#endif
	
	// When not logging execution, we can use a fast code path that assumes no logging
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	const EidosASTNode *first_child_node = p_node->children_[0];
	
	if (first_child_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
	{
		// <identifier>.<identifier> is an extremely common pattern so we optimize for it here
		// with Evaluate_Identifier_RAW(), which avoids EidosValue_SP, call logging, and other overhead
		EidosValue *first_child_value = Evaluate_Identifier_RAW(first_child_node);
		EidosValueType first_child_type = first_child_value->Type();
		
		if (first_child_type != EidosValueType::kValueObject)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): (internal error) operand type " << first_child_type << " is not supported by the '.' operator." << EidosTerminate(operator_token);
		
		EidosASTNode *second_child_node = p_node->children_[1];
		EidosToken *second_child_token = second_child_node->token_;
		
		if (second_child_token->token_type_ != EidosTokenType::kTokenIdentifier)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): (internal error) the '.' operator for x.y requires operand y to be an identifier." << EidosTerminate(operator_token);
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(second_child_token);
		
		// We offload the actual work to GetPropertyOfElements() to keep things simple here
		result_SP = static_cast<EidosValue_Object *>(first_child_value)->GetPropertyOfElements(second_child_node->cached_stringID_);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
	}
	else
	{
		// the general <expression>.<identifier> case has to use EidosValue_SP
		EidosValue_SP first_child_value = FastEvaluateNode(first_child_node);
		EidosValueType first_child_type = first_child_value->Type();
		
		if (first_child_type != EidosValueType::kValueObject)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): (internal error) operand type " << first_child_type << " is not supported by the '.' operator." << EidosTerminate(operator_token);
		
		EidosASTNode *second_child_node = p_node->children_[1];
		EidosToken *second_child_token = second_child_node->token_;
		
		if (second_child_token->token_type_ != EidosTokenType::kTokenIdentifier)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_MemberRef): (internal error) the '.' operator for x.y requires operand y to be an identifier." << EidosTerminate(operator_token);
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = EidosScript::PushErrorPositionFromToken(second_child_token);
		
		// We offload the actual work to GetPropertyOfElements() to keep things simple here
		result_SP = static_cast<EidosValue_Object *>(first_child_value.get())->GetPropertyOfElements(second_child_node->cached_stringID_);
		
		// Forget the function token, since it is not responsible for any future errors
		EidosScript::RestoreErrorPosition(error_pos_save);
	}
	
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
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): operand type " << first_child_type << " is not supported by the unary '+' operator." << EidosTerminate(operator_token);
		
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
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): non-conformable array operands to binary '+' operator." << EidosTerminate(operator_token);
		
		if ((first_child_type == EidosValueType::kValueVOID) || (second_child_type == EidosValueType::kValueVOID))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): operand type void is not supported by the '+' operator." << EidosTerminate(operator_token);
		
		if ((first_child_type == EidosValueType::kValueString) || (second_child_type == EidosValueType::kValueString))
		{
			// If either operand is a string, then we are doing string concatenation, with promotion to strings if needed
			// BCH 10/12/2018: Starting in Eidos 2.2, we allow string concatenation of NULL, which acts just as if the NULL were
			// a singleton string vector containing "NULL".  It is handled by pretending that NULL is length 1 and special-casing.
			if (first_child_type == EidosValueType::kValueNULL)
			{
				first_child_count = 1;
				result_dim_source = second_child_value;
			}
			if (second_child_type == EidosValueType::kValueNULL)
			{
				second_child_count = 1;
				result_dim_source = first_child_value;
			}
			
			if ((first_child_count == 1) && (second_child_count == 1))
			{
				const std::string &&first_string = (first_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : first_child_value->StringAtIndex(0, operator_token);
				const std::string &&second_string = (second_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : second_child_value->StringAtIndex(0, operator_token);
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(first_string + second_string));
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
					std::string singleton_string = (first_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : first_child_value->StringAtIndex(0, operator_token);
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(second_child_count);
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						string_result->PushString(singleton_string + second_child_value->StringAtIndex(value_index, operator_token));
					
					result_SP = std::move(string_result_SP);
				}
				else if (second_child_count == 1)
				{
					std::string singleton_string = (second_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : second_child_value->StringAtIndex(0, operator_token);
					EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
					EidosValue_String_vector *string_result = string_result_SP->Reserve(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						string_result->PushString(first_child_value->StringAtIndex(value_index, operator_token) + singleton_string);
					
					result_SP = std::move(string_result_SP);
				}
				else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
				{
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the string concatenation '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1, or (3) one operand is NULL." << EidosTerminate(operator_token);
				}
			}
		}
		else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			// both operands are integer, so we are computing an integer result, which entails overflow testing
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					// This is an overflow-safe version of:
					//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_child_value->IntAtIndex(0, operator_token) + second_child_value->IntAtIndex(0, operator_token));
					
					int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
					int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
					int64_t add_result;
					bool overflow = Eidos_add_overflow(first_operand, second_operand, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << EidosTerminate(operator_token);
					
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(add_result));
				}
				else
				{
					const int64_t *first_child_data = first_child_value->IntVector()->data();
					const int64_t *second_child_data = second_child_value->IntVector()->data();
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
					{
						// This is an overflow-safe version of:
						//int_result->set_int_no_check(first_child_value->IntAtIndex(value_index, operator_token) + second_child_value->IntAtIndex(value_index, operator_token));
						
						int64_t first_operand = first_child_data[value_index];
						int64_t second_operand = second_child_data[value_index];
						int64_t add_result;
						bool overflow = Eidos_add_overflow(first_operand, second_operand, &add_result);
						
						if (overflow)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << EidosTerminate(operator_token);
						
						int_result->set_int_no_check(add_result, value_index);
					}
					
					result_SP = std::move(int_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0, operator_token);
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(second_child_count);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(singleton_int + second_child_value->IntAtIndex(value_index, operator_token));
					
					int64_t second_operand = second_child_data[value_index];
					int64_t add_result;
					bool overflow = Eidos_add_overflow(singleton_int, second_operand, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(add_result, value_index);
				}
				
				result_SP = std::move(int_result_SP);
			}
			else if (second_child_count == 1)
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				int64_t singleton_int = second_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(first_child_value->IntAtIndex(value_index, operator_token) + singleton_int);
					
					int64_t first_operand = first_child_data[value_index];
					int64_t add_result;
					bool overflow = Eidos_add_overflow(first_operand, singleton_int, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(add_result, value_index);
				}
				
				result_SP = std::move(int_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
			}
		}
		else
		{
			if (((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat)) || ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat)))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the combination of operand types " << first_child_type << " and " << second_child_type << " is not supported by the binary '+' operator." << EidosTerminate(operator_token);
			
			// We have at least one float operand, so we are computing a float result
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(first_child_value->FloatAtIndex(0, operator_token) + second_child_value->FloatAtIndex(0, operator_token)));
				}
				else
				{
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
					
					if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
					{
						const double *first_child_data = first_child_value->FloatVector()->data();
						const double *second_child_data = second_child_value->FloatVector()->data();
						
						for (int value_index = 0; value_index < first_child_count; ++value_index)
							float_result->set_float_no_check(first_child_data[value_index] + second_child_data[value_index], value_index);
					}
					else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
					{
						const double *first_child_data = first_child_value->FloatVector()->data();
						const int64_t *second_child_data = second_child_value->IntVector()->data();
						
						for (int value_index = 0; value_index < first_child_count; ++value_index)
							float_result->set_float_no_check(first_child_data[value_index] + second_child_data[value_index], value_index);
					}
					else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
					{
						const int64_t *first_child_data = first_child_value->IntVector()->data();
						const double *second_child_data = second_child_value->FloatVector()->data();
						
						for (int value_index = 0; value_index < first_child_count; ++value_index)
							float_result->set_float_no_check(first_child_data[value_index] + second_child_data[value_index], value_index);
					}
					
					result_SP = std::move(float_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(second_child_count);
				
				if (second_child_type == EidosValueType::kValueInt)
				{
					const int64_t *second_child_data = second_child_value->IntVector()->data();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float + second_child_data[value_index], value_index);
				}
				else	// (second_child_type == EidosValueType::kValueFloat)
				{
					const double *second_child_data = second_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float + second_child_data[value_index], value_index);
				}
				
				result_SP = std::move(float_result_SP);
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				if (first_child_type == EidosValueType::kValueInt)
				{
					const int64_t *first_child_data = first_child_value->IntVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] + singleton_float, value_index);
				}
				else	// (first_child_type == EidosValueType::kValueFloat)
				{
					const double *first_child_data = first_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] + singleton_float, value_index);
				}
				
				result_SP = std::move(float_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): the '+' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
			}
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): operand type " << first_child_type << " is not supported by the '-' operator." << EidosTerminate(operator_token);
	
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
				bool overflow = Eidos_sub_overflow((int64_t)0, operand, &subtract_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer negation overflow with the unary '-' operator." << EidosTerminate(operator_token);
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subtract_result));
			}
			else
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(-first_child_value->IntAtIndex(value_index, operator_token), value_index);
					
					int64_t operand = first_child_data[value_index];
					int64_t subtract_result;
					bool overflow = Eidos_sub_overflow((int64_t)0, operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer negation overflow with the unary '-' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(subtract_result, value_index);
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
				const double *first_child_data = first_child_value->FloatVector()->data();
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(-first_child_data[value_index], value_index);
				
				result_SP = std::move(float_result_SP);
			}
		}
		
		result_SP->CopyDimensionsFromValue(first_child_value.get());
	}
	else
	{
		// binary minus
		EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
		EidosValueType second_child_type = second_child_value->Type();
		
		if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): operand type " << second_child_type << " is not supported by the '-' operator." << EidosTerminate(operator_token);
		
		int second_child_count = second_child_value->Count();
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): non-conformable array operands to binary '-' operator." << EidosTerminate(operator_token);
		
		if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			// both operands are integer, so we are computing an integer result, which entails overflow testing
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					// This is an overflow-safe version of:
					//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_child_value->IntAtIndex(0, operator_token) - second_child_value->IntAtIndex(0, operator_token));
					
					int64_t first_operand = first_child_value->IntAtIndex(0, operator_token);
					int64_t second_operand = second_child_value->IntAtIndex(0, operator_token);
					int64_t subtract_result;
					bool overflow = Eidos_sub_overflow(first_operand, second_operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << EidosTerminate(operator_token);
					
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subtract_result));
				}
				else
				{
					const int64_t *first_child_data = first_child_value->IntVector()->data();
					const int64_t *second_child_data = second_child_value->IntVector()->data();
					EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
					EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
					{
						// This is an overflow-safe version of:
						//int_result->set_int_no_check(first_child_value->IntAtIndex(value_index, operator_token) - second_child_value->IntAtIndex(value_index, operator_token));
						
						int64_t first_operand = first_child_data[value_index];
						int64_t second_operand = second_child_data[value_index];
						int64_t subtract_result;
						bool overflow = Eidos_sub_overflow(first_operand, second_operand, &subtract_result);
						
						if (overflow)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << EidosTerminate(operator_token);
						
						int_result->set_int_no_check(subtract_result, value_index);
					}
					
					result_SP = std::move(int_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex(0, operator_token);
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(second_child_count);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(singleton_int - second_child_value->IntAtIndex(value_index, operator_token));
					
					int64_t second_operand = second_child_data[value_index];
					int64_t subtract_result;
					bool overflow = Eidos_sub_overflow(singleton_int, second_operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(subtract_result, value_index);
				}
				
				result_SP = std::move(int_result_SP);
			}
			else if (second_child_count == 1)
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				int64_t singleton_int = second_child_value->IntAtIndex(0, operator_token);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(first_child_value->IntAtIndex(value_index, operator_token) - singleton_int);
					
					int64_t first_operand = first_child_data[value_index];
					int64_t subtract_result;
					bool overflow = Eidos_sub_overflow(first_operand, singleton_int, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(subtract_result, value_index);
				}
				
				result_SP = std::move(int_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): the '-' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
			}
		}
		else
		{
			// We have at least one float operand, so we are computing a float result
			if (first_child_count == second_child_count)
			{
				if (first_child_count == 1)
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(first_child_value->FloatAtIndex(0, operator_token) - second_child_value->FloatAtIndex(0, operator_token)));
				}
				else
				{
					EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
					EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
					
					if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
					{
						const double *first_child_data = first_child_value->FloatVector()->data();
						const double *second_child_data = second_child_value->FloatVector()->data();
						
						for (int value_index = 0; value_index < first_child_count; ++value_index)
							float_result->set_float_no_check(first_child_data[value_index] - second_child_data[value_index], value_index);
					}
					else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
					{
						const double *first_child_data = first_child_value->FloatVector()->data();
						const int64_t *second_child_data = second_child_value->IntVector()->data();
						
						for (int value_index = 0; value_index < first_child_count; ++value_index)
							float_result->set_float_no_check(first_child_data[value_index] - second_child_data[value_index], value_index);
					}
					else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
					{
						const int64_t *first_child_data = first_child_value->IntVector()->data();
						const double *second_child_data = second_child_value->FloatVector()->data();
						
						for (int value_index = 0; value_index < first_child_count; ++value_index)
							float_result->set_float_no_check(first_child_data[value_index] - second_child_data[value_index], value_index);
					}
					
					result_SP = std::move(float_result_SP);
				}
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(second_child_count);
				
				if (second_child_type == EidosValueType::kValueInt)
				{
					const int64_t *second_child_data = second_child_value->IntVector()->data();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float - second_child_data[value_index], value_index);
				}
				else	// (second_child_type == EidosValueType::kValueFloat)
				{
					const double *second_child_data = second_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float - second_child_data[value_index], value_index);
				}
				
				result_SP = std::move(float_result_SP);
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				if (first_child_type == EidosValueType::kValueInt)
				{
					const int64_t *first_child_data = first_child_value->IntVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] - singleton_float, value_index);
				}
				else	// (first_child_type == EidosValueType::kValueFloat)
				{
					const double *first_child_data = first_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] - singleton_float, value_index);
				}
				
				result_SP = std::move(float_result_SP);
			}
			else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): the '-' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
			}
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Minus()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Mod(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Mod()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Mod", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): operand type " << first_child_type << " is not supported by the '%' operator." << EidosTerminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): operand type " << second_child_type << " is not supported by the '%' operator." << EidosTerminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int first_child_dimcount = first_child_value->DimensionCount();
	int second_child_dimcount = second_child_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
	
	if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): non-conformable array operands to the '%' operator." << EidosTerminate(operator_token);
	
	EidosValue_SP result_SP;
	
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, a function has been provided, integerDiv(). This decision applies also to modulo, with function integerMod().
	
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
			EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
			
			if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
			{
				const double *first_child_data = first_child_value->FloatVector()->data();
				const double *second_child_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
			{
				const double *first_child_data = first_child_value->FloatVector()->data();
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				const double *second_child_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			
			result_SP = std::move(float_result_SP);
		}
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(second_child_count);
		
		if (second_child_type == EidosValueType::kValueInt)
		{
			const int64_t *second_child_data = second_child_value->IntVector()->data();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(fmod(singleton_float, second_child_data[value_index]), value_index);
		}
		else	// (second_child_type == EidosValueType::kValueFloat)
		{
			const double *second_child_data = second_child_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(fmod(singleton_float, second_child_data[value_index]), value_index);
		}
		
		result_SP = std::move(float_result_SP);
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if (first_child_type == EidosValueType::kValueInt)
		{
			const int64_t *first_child_data = first_child_value->IntVector()->data();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], singleton_float), value_index);
		}
		else	// (first_child_type == EidosValueType::kValueFloat)
		{
			const double *first_child_data = first_child_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], singleton_float), value_index);
		}
		
		result_SP = std::move(float_result_SP);
	}
	else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mod): the '%' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Mod()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Mult(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Mult()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Mult", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): operand type " << first_child_type << " is not supported by the '*' operator." << EidosTerminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): operand type " << second_child_type << " is not supported by the '*' operator." << EidosTerminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int first_child_dimcount = first_child_value->DimensionCount();
	int second_child_dimcount = second_child_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
	
	if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): non-conformable array operands to the '*' operator." << EidosTerminate(operator_token);
	
	EidosValue_SP result_SP;
	
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
				bool overflow = Eidos_mul_overflow(first_operand, second_operand, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << EidosTerminate(operator_token);
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(multiply_result));
			}
			else
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(first_child_value->IntAtIndex(value_index, operator_token) * second_child_value->IntAtIndex(value_index, operator_token));
					
					int64_t first_operand = first_child_data[value_index];
					int64_t second_operand = second_child_data[value_index];
					int64_t multiply_result;
					bool overflow = Eidos_mul_overflow(first_operand, second_operand, &multiply_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(multiply_result, value_index);
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
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					const double *first_child_data = first_child_value->FloatVector()->data();
					const double *second_child_data = second_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] * second_child_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
				{
					const double *first_child_data = first_child_value->FloatVector()->data();
					const int64_t *second_child_data = second_child_value->IntVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] * second_child_data[value_index], value_index);
				}
				else	// ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
				{
					const int64_t *first_child_data = first_child_value->IntVector()->data();
					const double *second_child_data = second_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] * second_child_data[value_index], value_index);
				}
				
				result_SP = std::move(float_result_SP);
			}
		}
	}
	else if ((first_child_count == 1) || (second_child_count == 1))
	{
		EidosValue_SP one_count_child;
		EidosValue_SP any_count_child;
		int any_count;
		EidosValueType any_type;
		
		if (first_child_count == 1)
		{
			one_count_child = std::move(first_child_value);
			any_count_child = std::move(second_child_value);
			any_count = second_child_count;
			any_type = second_child_type;
		}
		else
		{
			one_count_child = std::move(second_child_value);
			any_count_child = std::move(first_child_value);
			any_count = first_child_count;
			any_type = first_child_type;
		}
		
		// OK, we've got good operands; calculate the result.  If both operands are int, the result is int, otherwise float.
		if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			const int64_t *any_count_data = any_count_child->IntVector()->data();
			int64_t singleton_int = one_count_child->IntAtIndex(0, operator_token);
			EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
			EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(any_count);
			
			for (int value_index = 0; value_index < any_count; ++value_index)
			{
				// This is an overflow-safe version of:
				//int_result->set_int_no_check(any_count_child->IntAtIndex(value_index, operator_token) * singleton_int);
				
				int64_t first_operand = any_count_data[value_index];
				int64_t multiply_result;
				bool overflow = Eidos_mul_overflow(first_operand, singleton_int, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << EidosTerminate(operator_token);
				
				int_result->set_int_no_check(multiply_result, value_index);
			}
			
			result_SP = std::move(int_result_SP);
		}
		else if (any_type == EidosValueType::kValueInt)
		{
			const int64_t *any_count_data = any_count_child->IntVector()->data();
			double singleton_float = one_count_child->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(any_count);
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				float_result->set_float_no_check(any_count_data[value_index] * singleton_float, value_index);
			
			result_SP = std::move(float_result_SP);
		}
		else	// (any_type == EidosValueType::kValueFloat)
		{
			const double *any_count_data = any_count_child->FloatVector()->data();
			double singleton_float = one_count_child->FloatAtIndex(0, operator_token);
			EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
			EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(any_count);
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				float_result->set_float_no_check(any_count_data[value_index] * singleton_float, value_index);
			
			result_SP = std::move(float_result_SP);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): the '*' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Mult()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Div(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Div()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Div", 2);

	EidosToken *operator_token = p_node->token_;
	EidosValue_SP first_child_value = FastEvaluateNode(p_node->children_[0]);
	EidosValue_SP second_child_value = FastEvaluateNode(p_node->children_[1]);
	
	EidosValueType first_child_type = first_child_value->Type();
	EidosValueType second_child_type = second_child_value->Type();
	
	if ((first_child_type != EidosValueType::kValueInt) && (first_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): operand type " << first_child_type << " is not supported by the '/' operator." << EidosTerminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): operand type " << second_child_type << " is not supported by the '/' operator." << EidosTerminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int first_child_dimcount = first_child_value->DimensionCount();
	int second_child_dimcount = second_child_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
	
	if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): non-conformable array operands to the '/' operator." << EidosTerminate(operator_token);
	
	EidosValue_SP result_SP;
	
	// I've decided to make division perform float division always; wanting integer division is rare, and providing it as the default is error-prone.  If
	// people want integer division, a function has been provided, integerDiv(). This decision applies also to modulo, with function integerMod().

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
			EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
			
			if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
			{
				const double *first_child_data = first_child_value->FloatVector()->data();
				const double *second_child_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(first_child_data[value_index] / second_child_data[value_index], value_index);
			}
			else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
			{
				const double *first_child_data = first_child_value->FloatVector()->data();
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(first_child_data[value_index] / second_child_data[value_index], value_index);
			}
			else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				const double *second_child_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(first_child_data[value_index] / second_child_data[value_index], value_index);
			}
			else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(first_child_data[value_index] / (double)second_child_data[value_index], value_index);
			}
			
			result_SP = std::move(float_result_SP);
		}
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(second_child_count);
		
		if (second_child_type == EidosValueType::kValueInt)
		{
			const int64_t *second_child_data = second_child_value->IntVector()->data();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(singleton_float / second_child_data[value_index], value_index);
		}
		else	// (second_child_type == EidosValueType::kValueFloat)
		{
			const double *second_child_data = second_child_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(singleton_float / second_child_data[value_index], value_index);
		}
		
		result_SP = std::move(float_result_SP);
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if (first_child_type == EidosValueType::kValueInt)
		{
			const int64_t *first_child_data = first_child_value->IntVector()->data();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / singleton_float, value_index);
		}
		else	// (first_child_type == EidosValueType::kValueFloat)
		{
			const double *first_child_data = first_child_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / singleton_float, value_index);
		}
		
		result_SP = std::move(float_result_SP);
	}
	else	// if ((first_child_count != second_child_count) && (first_child_count != 1) && (second_child_count != 1))
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Div): the '/' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Div()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Conditional(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Conditional()");
	EIDOS_ASSERT_CHILD_RANGE("EidosInterpreter::Evaluate_Conditional", 3, 3);
	
	EidosToken *operator_token = p_node->token_;
	
	EidosValue_SP result_SP;
	
	EidosASTNode *condition_node = p_node->children_[0];
	EidosValue_SP condition_result = FastEvaluateNode(condition_node);
	
	if (condition_result == gStaticEidosValue_LogicalT)
	{
		// Handle a static singleton logical true super fast; no need for type check, count, etc
		result_SP = FastEvaluateNode(p_node->children_[1]);
	}
	else if (condition_result == gStaticEidosValue_LogicalF)
	{
		// Handle a static singleton logical false super fast; no need for type check, count, etc
		result_SP = FastEvaluateNode(p_node->children_[2]);
	}
	else if (condition_result->Count() == 1)
	{
		eidos_logical_t condition_logical = condition_result->LogicalAtIndex(0, operator_token);
		
		if (condition_logical)
			result_SP = FastEvaluateNode(p_node->children_[1]);
		else
			result_SP = FastEvaluateNode(p_node->children_[2]);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Conditional): condition for ternary conditional has size() != 1." << EidosTerminate(p_node->token_);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Conditional()");
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): operand type " << first_child_type << " is not supported by the '^' operator." << EidosTerminate(operator_token);
	
	if ((second_child_type != EidosValueType::kValueInt) && (second_child_type != EidosValueType::kValueFloat))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): operand type " << second_child_type << " is not supported by the '^' operator." << EidosTerminate(operator_token);
	
	int first_child_count = first_child_value->Count();
	int second_child_count = second_child_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int first_child_dimcount = first_child_value->DimensionCount();
	int second_child_dimcount = second_child_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
	
	if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): non-conformable array operands to the '^' operator." << EidosTerminate(operator_token);
	
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
			EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
			
			if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
			{
				const double *first_child_data = first_child_value->FloatVector()->data();
				const double *second_child_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
			{
				const double *first_child_data = first_child_value->FloatVector()->data();
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				const double *second_child_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
			{
				const int64_t *first_child_data = first_child_value->IntVector()->data();
				const int64_t *second_child_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
			}
			
			result_SP = std::move(float_result_SP);
		}
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(second_child_count);
		
		if (second_child_type == EidosValueType::kValueInt)
		{
			const int64_t *second_child_data = second_child_value->IntVector()->data();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(pow(singleton_float, second_child_data[value_index]), value_index);
		}
		else	// (second_child_type == EidosValueType::kValueFloat)
		{
			const double *second_child_data = second_child_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(pow(singleton_float, second_child_data[value_index]), value_index);
		}
		
		result_SP = std::move(float_result_SP);
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->FloatAtIndex(0, operator_token);
		EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if (first_child_type == EidosValueType::kValueInt)
		{
			const int64_t *first_child_data = first_child_value->IntVector()->data();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], singleton_float), value_index);
		}
		else	// (first_child_type == EidosValueType::kValueFloat)
		{
			const double *first_child_data = first_child_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], singleton_float), value_index);
		}
		
		result_SP = std::move(float_result_SP);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Exp): the '^' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Exp()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_And(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_And()");
	EIDOS_ASSERT_CHILD_COUNT_GTEQ("EidosInterpreter::Evaluate_And", 2);
	
	EidosToken *operator_token = p_node->token_;
	
	// We try to avoid allocating a result object if we can.  If result==nullptr but result_count==1, logical_result contains the result so far.
	EidosValue_Logical_SP result_SP;
	eidos_logical_t logical_result = false;
	int result_count = 0;
	bool first_child = true;
	
	// Dimensionality with & and | is different from the binary operators, since & and | can have any number of children.  Basically we want
	// that (1) a simple T or F can always be included, but never determines the dimensionality of the result, (2) if more than one matrix or
	// array is included, all such must be conformable, even singletons, (3) singleton matrices/arrays may be mixed with non-singleton vectors,
	// in which case the result is a non-singleton vector, *not* a matrix/array, and (4) in all other cases dimensionality is determined by any
	// matrix/array operand.  These rules are consistent with the binary operator rules, just extended to an arbitrary number of arguments.
	// To implement these rules, we keep track of two new pieces of state.  One, result_dim_source, keeps track of the value that will be used
	// as the source of the result's dimensionality; it is set to the first matrix/array argument seen, but if a non-singleton vector is seen
	// and result_dim_source is set to a singleton matrix/array, it gets upgraded to the non-singleton vector since that has priority.  Two,
	// first_array_operand forever keeps the first matrix/array operand seen, to check conformability against any later matrix/array operands.
	// Between these two mechanisms, I believe the intended rules are fully implemented.
	EidosValue_SP result_dim_source;
	EidosValue_SP first_array_operand;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		EidosValue_SP child_result = FastEvaluateNode(child_node);
		
		if (child_result == gStaticEidosValue_LogicalT)
		{
			// Handle a static singleton logical true super fast; no need for type check, count, etc
			if (first_child)
			{
				first_child = false;
				logical_result = true;
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
				logical_result = false;
				result_count = 1;
			}
			else
			{
				if (result_SP)
				{
					// we have a result allocated, so alter the values in it
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						result->set_logical_no_check(false, value_index);
				}
				else
				{
					// we have no result allocated, so we must be using logical_result
					logical_result = false;
				}
			}
		}
		else
		{
			// Cases that the code above can't handle drop through to the more general case here
			EidosValueType child_type = child_result->Type();
			
			if ((child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueString) && (child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueFloat))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): operand type " << child_type << " is not supported by the '&' operator." << EidosTerminate(operator_token);
			
			int child_count = child_result->Count();
			int child_dimcount = child_result->DimensionCount();
			
			// handle dimensionality issues first; see the comment at top regarding the policies we enforce here
			if (child_dimcount > 1)
			{
				// we are an array operand; if we're the first, track that, otherwise check conformability against the first
				if (!first_array_operand)
					first_array_operand = child_result;
				else if (!EidosValue::MatchingDimensions(first_array_operand.get(), child_result.get()))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): non-conformable array operands to the '&' operator." << EidosTerminate(operator_token);
				
				// if there is no dimensionality source yet, we qualify; otherwise, if we are a non-singleton array, and the current
				// dimensionality source is a (non-singleton) vector, we represent an upgrade; if we're a singleton, though, we don't
				if (!result_dim_source || ((child_count != 1) && (result_dim_source->DimensionCount() == 1)))
					result_dim_source = child_result;
			}
			else if (child_count != 1)
			{
				// we are a non-singleton vector operand; no updating of first_array_operand and no conformability check,
				// but we could be the dimensionality source if there is none, or if it is currently a singleton array
				if (!result_dim_source || (result_dim_source->Count() == 1))
					result_dim_source = child_result;
			}
			
			if (first_child)
			{
				// if this is our first operand, we need to set up an initial result value from it
				first_child = false;
				
				if (child_count == 1)
				{
					// if we have a singleton, avoid allocating a result yet, by using logical_result instead
					logical_result = child_result->LogicalAtIndex(0, operator_token);
					result_count = 1;
				}
				else if ((child_type == EidosValueType::kValueLogical) && (child_result->UseCount() == 1))
				{
					// child_result is a logical EidosValue owned only by us, so we can just take it over as our initial result
					result_SP = static_pointer_cast<EidosValue_Logical>(std::move(child_result));
					result_count = child_count;
				}
				else
				{
					// for other cases, we just clone child_result – but note that it may not be of type logical
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(child_count));
					result_count = child_count;
					
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->set_logical_no_check(child_result->LogicalAtIndex(value_index, operator_token), value_index);
				}
			}
			else
			{
				// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
				if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_And): operands to the '&' operator are not compatible in size()." << EidosTerminate(operator_token);
				
				if (child_count == 1)
				{
					// if child_logical is T, it has no effect on result; if it is F, it turns result to all F
					eidos_logical_t child_logical = child_result->LogicalAtIndex(0, operator_token);
					
					if (!child_logical)
					{
						if (result_SP)
						{
							// we have a result allocated, so alter the values in it
							EidosValue_Logical *result = result_SP.get();
							
							for (int value_index = 0; value_index < result_count; ++value_index)
								result->set_logical_no_check(false, value_index);
						}
						else
						{
							// we have no result allocated, so we must be using logical_result
							logical_result = false;
						}
					}
				}
				else if (result_count == 1)
				{
					// we had a one-length result vector, but now we need to upscale it to match child_result
					eidos_logical_t result_logical;
					
					if (result_SP)
					{
						// we have a result allocated; work with that
						result_logical = result_SP->LogicalAtIndex(0, operator_token);
					}
					else
					{
						// no result allocated, so we now need to upgrade to an allocated result
						result_logical = logical_result;
					}
					
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(child_count));
					result_count = child_count;
					
					EidosValue_Logical *result = result_SP.get();
					
					if (result_logical)
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->set_logical_no_check(child_result->LogicalAtIndex(value_index, operator_token), value_index);
					else
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->set_logical_no_check(false, value_index);
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is F we turn result to F
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (!child_result->LogicalAtIndex(value_index, operator_token))
							result->set_logical_no_check(false, value_index);
				}
			}
		}
	}
	
	if (result_dim_source)
	{
		// if we avoided allocating a result, make a singleton now, because we need to apply dimensionality to it; this
		// occurs when matrix/array singletons were mixed into the operands and a non-singleton vector was not present
		if (!result_SP)
		{
			result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(1));
			result_SP->set_logical_no_check(logical_result, 0);
		}
		
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		// if we avoided allocating a result, use a static logical value
		if (!result_SP)
			result_SP = (logical_result ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_And()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Or(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Or()");
	EIDOS_ASSERT_CHILD_COUNT_GTEQ("EidosInterpreter::Evaluate_Or", 2);
	
	EidosToken *operator_token = p_node->token_;
	
	// We try to avoid allocating a result object if we can.  If result==nullptr but result_count==1, logical_result contains the result so far.
	EidosValue_Logical_SP result_SP;
	eidos_logical_t logical_result = false;
	int result_count = 0;
	bool first_child = true;
	
	// Dimensionality with & and | is different from the binary operators, since & and | can have any number of children.  Basically we want
	// that (1) a simple T or F can always be included, but never determines the dimensionality of the result, (2) if more than one matrix or
	// array is included, all such must be conformable, even singletons, (3) singleton matrices/arrays may be mixed with non-singleton vectors,
	// in which case the result is a non-singleton vector, *not* a matrix/array, and (4) in all other cases dimensionality is determined by any
	// matrix/array operand.  These rules are consistent with the binary operator rules, just extended to an arbitrary number of arguments.
	// To implement these rules, we keep track of two new pieces of state.  One, result_dim_source, keeps track of the value that will be used
	// as the source of the result's dimensionality; it is set to the first matrix/array argument seen, but if a non-singleton vector is seen
	// and result_dim_source is set to a singleton matrix/array, it gets upgraded to the non-singleton vector since that has priority.  Two,
	// first_array_operand forever keeps the first matrix/array operand seen, to check conformability against any later matrix/array operands.
	// Between these two mechanisms, I believe the intended rules are fully implemented.
	EidosValue_SP result_dim_source;
	EidosValue_SP first_array_operand;
	
	for (EidosASTNode *child_node : p_node->children_)
	{
		EidosValue_SP child_result = FastEvaluateNode(child_node);
		
		if (child_result == gStaticEidosValue_LogicalT)
		{
			// Handle a static singleton logical true super fast; no need for type check, count, etc
			if (first_child)
			{
				first_child = false;
				logical_result = true;
				result_count = 1;
			}
			else
			{
				if (result_SP)
				{
					// we have a result allocated, so alter the values in it
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						result->set_logical_no_check(true, value_index);
				}
				else
				{
					// we have no result allocated, so we must be using logical_result
					logical_result = true;
				}
			}
		}
		else if (child_result == gStaticEidosValue_LogicalF)
		{
			// Handle a static singleton logical false super fast; no need for type check, count, etc
			if (first_child)
			{
				first_child = false;
				logical_result = false;
				result_count = 1;
			}
			// if we're not on the first child, doing an OR with F is a no-op; it does not even change the size of the result vector
		}
		else
		{
			// Cases that the code above can't handle drop through to the more general case here
			EidosValueType child_type = child_result->Type();
			
			if ((child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueString) && (child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueFloat))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): operand type " << child_type << " is not supported by the '|' operator." << EidosTerminate(operator_token);
			
			int child_count = child_result->Count();
			int child_dimcount = child_result->DimensionCount();
			
			// handle dimensionality issues first; see the comment at top regarding the policies we enforce here
			if (child_dimcount > 1)
			{
				// we are an array operand; if we're the first, track that, otherwise check conformability against the first
				if (!first_array_operand)
					first_array_operand = child_result;
				else if (!EidosValue::MatchingDimensions(first_array_operand.get(), child_result.get()))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): non-conformable array operands to the '|' operator." << EidosTerminate(operator_token);
				
				// if there is no dimensionality source yet, we qualify; otherwise, if we are a non-singleton array, and the current
				// dimensionality source is a (non-singleton) vector, we represent an upgrade; if we're a singleton, though, we don't
				if (!result_dim_source || ((child_count != 1) && (result_dim_source->DimensionCount() == 1)))
					result_dim_source = child_result;
			}
			else if (child_count != 1)
			{
				// we are a non-singleton vector operand; no updating of first_array_operand and no conformability check,
				// but we could be the dimensionality source if there is none, or if it is currently a singleton array
				if (!result_dim_source || (result_dim_source->Count() == 1))
					result_dim_source = child_result;
			}
			
			if (first_child)
			{
				// if this is our first operand, we need to set up an initial result value from it
				first_child = false;
				
				if (child_count == 1)
				{
					// if we have a singleton, avoid allocating a result yet, by using logical_result instead
					logical_result = child_result->LogicalAtIndex(0, operator_token);
					result_count = 1;
				}
				else if ((child_type == EidosValueType::kValueLogical) && (child_result->UseCount() == 1))
				{
					// child_result is a logical EidosValue owned only by us, so we can just take it over as our initial result
					result_SP = static_pointer_cast<EidosValue_Logical>(std::move(child_result));
					result_count = child_count;
				}
				else
				{
					// for other cases, we just clone child_result – but note that it may not be of type logical
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(child_count));
					result_count = child_count;
					
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->set_logical_no_check(child_result->LogicalAtIndex(value_index, operator_token), value_index);
				}
			}
			else
			{
				// otherwise, we treat our current result as the left operand, and perform our operation with the right operand
				if ((result_count != child_count) && (result_count != 1) && (child_count != 1))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Or): operands to the '|' operator are not compatible in size()." << EidosTerminate(operator_token);
				
				if (child_count == 1)
				{
					// if child_logical is F, it has no effect on result; if it is T, it turns result to all T
					eidos_logical_t child_logical = child_result->LogicalAtIndex(0, operator_token);
					
					if (child_logical)
					{
						if (result_SP)
						{
							// we have a result allocated, so alter the values in it
							EidosValue_Logical *result = result_SP.get();
							
							for (int value_index = 0; value_index < result_count; ++value_index)
								result->set_logical_no_check(true, value_index);
						}
						else
						{
							// we have no result allocated, so we must be using logical_result
							logical_result = true;
						}
					}
				}
				else if (result_count == 1)
				{
					// we had a one-length result vector, but now we need to upscale it to match child_result
					eidos_logical_t result_logical;
					
					if (result_SP)
					{
						// we have a result allocated; work with that
						result_logical = result_SP->LogicalAtIndex(0, operator_token);
					}
					else
					{
						// no result allocated, so we now need to upgrade to an allocated result
						result_logical = logical_result;
					}
					
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(child_count));
					result_count = child_count;
					
					EidosValue_Logical *result = result_SP.get();
					
					if (result_logical)
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->set_logical_no_check(true, value_index);
					else
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->set_logical_no_check(child_result->LogicalAtIndex(value_index, operator_token), value_index);
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is T we turn result to T
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (child_result->LogicalAtIndex(value_index, operator_token))
							result->set_logical_no_check(true, value_index);
				}
			}
		}
	}
	
	if (result_dim_source)
	{
		// if we avoided allocating a result, make a singleton now, because we need to apply dimensionality to it; this
		// occurs when matrix/array singletons were mixed into the operands and a non-singleton vector was not present
		if (!result_SP)
		{
			result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(1));
			result_SP->set_logical_no_check(logical_result, 0);
		}
		
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		// if we avoided allocating a result, use a static logical value
		if (!result_SP)
			result_SP = (logical_result ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	
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
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Not): operand type " << first_child_type << " is not supported by the '!' operator." << EidosTerminate(operator_token);
		
		int first_child_count = first_child_value->Count();
		int first_child_dimcount = first_child_value->DimensionCount();
		
		if ((first_child_count == 1) && (first_child_dimcount == 1))
		{
			// If we're generating a singleton non-array result, use cached static logical values
			result_SP = (first_child_value->LogicalAtIndex(0, operator_token) ? gStaticEidosValue_LogicalF : gStaticEidosValue_LogicalT);
		}
		else
		{
			// for other cases, we just create the negation of first_child_value – but note that it may not be of type logical
			result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(first_child_count));
			
			EidosValue_Logical *result = result_SP.get();
			
			if (first_child_type == EidosValueType::kValueLogical)
			{
				const eidos_logical_t *child_data = first_child_value->LogicalVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(!child_data[value_index], value_index);
			}
			else if (first_child_type == EidosValueType::kValueInt)
			{
				const int64_t *child_data = first_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(child_data[value_index] == 0, value_index);
			}
			else if (first_child_type == EidosValueType::kValueString)
			{
				const std::vector<std::string> &child_vec = (*first_child_value->StringVector());
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(child_vec[value_index].length() == 0, value_index);
			}
			else
			{
				// General case; hit by type float, since we don't want to duplicate the NAN check here
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(!first_child_value->LogicalAtIndex(value_index, operator_token), value_index);
			}
			
			result_SP->CopyDimensionsFromValue(first_child_value.get());
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
		bool is_const;
		EidosValue_SP lvalue_SP = global_symbols_->GetValueOrRaiseForASTNode_IsConst(lvalue_node, &is_const);
		
		if (is_const)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): identifier '" << lvalue_node->token_->token_string_ << "' cannot be redefined because it is a constant." << EidosTerminate(p_node->token_);
		
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
				EidosASTNode *rvalue_node = p_node->children_[1];										// the operator node
				EidosValue *cached_operand2 = rvalue_node->children_[1]->cached_literal_value_.get();	// the numeric constant
				
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
								bool overflow = Eidos_add_overflow(operand1_value, operand2_value, &operand1_value);
								
								if (overflow)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer addition overflow with the binary '+' operator." << EidosTerminate(rvalue_node->token_);
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMinus:
							{
								int64_t &operand1_value = int_singleton->IntValue_Mutable();
								bool overflow = Eidos_sub_overflow(operand1_value, operand2_value, &operand1_value);
								
								if (overflow)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer subtraction overflow with the binary '-' operator." << EidosTerminate(rvalue_node->token_);
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMult:
							{
								int64_t &operand1_value = int_singleton->IntValue_Mutable();
								bool overflow = Eidos_mul_overflow(operand1_value, operand2_value, &operand1_value);
								
								if (overflow)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer multiplication overflow with the '*' operator." << EidosTerminate(rvalue_node->token_);
								goto compoundAssignmentSuccess;
							}
							default:	// div, mod, and exp always produce float, so we don't handle them for int; we can't change the type of x here
								break;
						}
					}
					else
					{
						int64_t *int_data = lvalue->IntVector_Mutable()->data();
						
						switch (compound_operator)
						{
							case EidosTokenType::kTokenPlus:
							{
								for (int value_index = 0; value_index < lvalue_count; ++value_index)
								{
									int64_t &int_vec_value = int_data[value_index];
									bool overflow = Eidos_add_overflow(int_vec_value, operand2_value, &int_vec_value);
									
									if (overflow)
										EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer addition overflow with the binary '+' operator." << EidosTerminate(rvalue_node->token_);
								}
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMinus:
							{
								for (int value_index = 0; value_index < lvalue_count; ++value_index)
								{
									int64_t &int_vec_value = int_data[value_index];
									bool overflow = Eidos_sub_overflow(int_vec_value, operand2_value, &int_vec_value);
									
									if (overflow)
										EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer subtraction overflow with the binary '-' operator." << EidosTerminate(rvalue_node->token_);
								}
								goto compoundAssignmentSuccess;
							}
							case EidosTokenType::kTokenMult:
							{
								for (int value_index = 0; value_index < lvalue_count; ++value_index)
								{
									int64_t &int_vec_value = int_data[value_index];
									bool overflow = Eidos_mul_overflow(int_vec_value, operand2_value, &int_vec_value);
									
									if (overflow)
										EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): integer multiplication overflow with the '*' operator." << EidosTerminate(rvalue_node->token_);
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
				EidosASTNode *rvalue_node = p_node->children_[1];										// the operator node
				EidosValue *cached_operand2 = rvalue_node->children_[1]->cached_literal_value_.get();	// the numeric constant
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
							// CODE COVERAGE: This is dead code
							break;
					}
					
				}
				else
				{
					double *float_data = lvalue->FloatVector_Mutable()->data();
					
					switch (compound_operator)
					{
						case EidosTokenType::kTokenPlus:
							for (int value_index = 0; value_index < lvalue_count; ++value_index)
								float_data[value_index] += operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMinus:
							for (int value_index = 0; value_index < lvalue_count; ++value_index)
								float_data[value_index] -= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMult:
							for (int value_index = 0; value_index < lvalue_count; ++value_index)
								float_data[value_index] *= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenDiv:
							for (int value_index = 0; value_index < lvalue_count; ++value_index)
								float_data[value_index] /= operand2_value;
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenMod:
							for (int value_index = 0; value_index < lvalue_count; ++value_index)
							{
								double &float_vec_value = float_data[value_index];
								
								float_vec_value = fmod(float_vec_value, operand2_value);
							}
							goto compoundAssignmentSuccess;
							
						case EidosTokenType::kTokenExp:
							for (int value_index = 0; value_index < lvalue_count; ++value_index)
							{
								double &float_vec_value = float_data[value_index];
								
								float_vec_value = pow(float_vec_value, operand2_value);
							}
							goto compoundAssignmentSuccess;
							
						default:
							// CODE COVERAGE: This is dead code
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
	
	// by design, assignment does not yield a usable value; instead it produces void – this prevents the error "if (x = 3) ..."
	// since the condition is void and will raise; the loss of legitimate uses of "if (x = 3)" seems a small price to pay
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
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
	
	if ((first_child_type == EidosValueType::kValueVOID) || (second_child_type == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): operand type void is not supported by the '==' operator." << EidosTerminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = Eidos_GetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): non-conformable array operands to the '==' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				if (!result_dim_source)
					result_SP = (compare_result == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				else
					result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{compare_result == 0});	// so we can modify it
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					// Direct float-to-float compare can be optimized through vector access
					const double *float1_data = first_child_value->FloatVector()->data();
					const double *float2_data = second_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(float1_data[value_index] == float2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
				{
					// Direct int-to-int compare can be optimized through vector access
					const int64_t *int1_data = first_child_value->IntVector()->data();
					const int64_t *int2_data = second_child_value->IntVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(int1_data[value_index] == int2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
				{
					// Direct object-to-object compare can be optimized through vector access
					EidosObjectElement * const *obj1_vec = first_child_value->ObjectElementVector()->data();
					EidosObjectElement * const *obj2_vec = second_child_value->ObjectElementVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(obj1_vec[value_index] == obj2_vec[value_index], value_index);
				}
				else
				{
					// General case
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token) == 0, value_index);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			if ((compareFunc == &CompareEidosValues_Float) && (second_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float1 = first_child_value->FloatAtIndex(0, operator_token);
				const double *float_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(float1 == float_data[value_index], value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Int) && (second_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int1 = first_child_value->IntAtIndex(0, operator_token);
				const int64_t *int_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(int1 == int_data[value_index], value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Object) && (second_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj1 = first_child_value->ObjectElementAtIndex(0, operator_token);
				EidosObjectElement * const *obj_vec = second_child_value->ObjectElementVector()->data();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(obj1 == obj_vec[value_index], value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token) == 0, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			if ((compareFunc == &CompareEidosValues_Float) && (first_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float2 = second_child_value->FloatAtIndex(0, operator_token);
				const double *float_data = first_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(float_data[value_index] == float2, value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Int) && (first_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int2 = second_child_value->IntAtIndex(0, operator_token);
				const int64_t *int_data = first_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(int_data[value_index] == int2, value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Object) && (first_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj2 = second_child_value->ObjectElementAtIndex(0, operator_token);
				EidosObjectElement * const *obj_vec = first_child_value->ObjectElementVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(obj_vec[value_index] == obj2, value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token) == 0, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): the '==' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): testing NULL with the '==' operator is an error; use isNULL()." << EidosTerminate(operator_token);
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
	
	if ((first_child_type == EidosValueType::kValueVOID) || (second_child_type == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): operand type void is not supported by the '<' operator." << EidosTerminate(operator_token);
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): the '<' operator cannot be used with type object." << EidosTerminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = Eidos_GetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): non-conformable array operands to the '<' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				if (!result_dim_source)
					result_SP = (compare_result == -1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				else
					result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{compare_result == -1});	// so we can modify it
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result->set_logical_no_check(compare_result == -1, value_index);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result->set_logical_no_check(compare_result == -1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result->set_logical_no_check(compare_result == -1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): the '<' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): testing NULL with the '<' operator is an error; use isNULL()." << EidosTerminate(operator_token);
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
	
	if ((first_child_type == EidosValueType::kValueVOID) || (second_child_type == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): operand type void is not supported by the '<=' operator." << EidosTerminate(operator_token);
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): the '<=' operator cannot be used with type object." << EidosTerminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = Eidos_GetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): non-conformable array operands to the '<=' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				if (!result_dim_source)
					result_SP = (compare_result != 1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				else
					result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{compare_result != 1});	// so we can modify it
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result->set_logical_no_check(compare_result != 1, value_index);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result->set_logical_no_check(compare_result != 1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result->set_logical_no_check(compare_result != 1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): the '<=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): testing NULL with the '<=' operator is an error; use isNULL()." << EidosTerminate(operator_token);
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
	
	if ((first_child_type == EidosValueType::kValueVOID) || (second_child_type == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): operand type void is not supported by the '>' operator." << EidosTerminate(operator_token);
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): the '>' operator cannot be used with type object." << EidosTerminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = Eidos_GetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): non-conformable array operands to the '>' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				if (!result_dim_source)
					result_SP = (compare_result == 1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				else
					result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{compare_result == 1});	// so we can modify it
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result->set_logical_no_check(compare_result == 1, value_index);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result->set_logical_no_check(compare_result == 1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result->set_logical_no_check(compare_result == 1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): the '>' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): testing NULL with the '>' operator is an error; use isNULL()." << EidosTerminate(operator_token);
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
	
	if ((first_child_type == EidosValueType::kValueVOID) || (second_child_type == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): operand type void is not supported by the '>=' operator." << EidosTerminate(operator_token);
	if ((first_child_type == EidosValueType::kValueObject) || (second_child_type == EidosValueType::kValueObject))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): the '>=' operator cannot be used with type object." << EidosTerminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = Eidos_GetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): non-conformable array operands to the '>=' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				if (!result_dim_source)
					result_SP = (compare_result != -1) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				else
					result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{compare_result != -1});	// so we can modify it
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token);
					
					logical_result->set_logical_no_check(compare_result != -1, value_index);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token);
				
				logical_result->set_logical_no_check(compare_result != -1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				int compare_result = compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token);
				
				logical_result->set_logical_no_check(compare_result != -1, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): the '>=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): testing NULL with the '>=' operator is an error; use isNULL()." << EidosTerminate(operator_token);
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
	
	if ((first_child_type == EidosValueType::kValueVOID) || (second_child_type == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): operand type void is not supported by the '!=' operator." << EidosTerminate(operator_token);
	
	if ((first_child_type != EidosValueType::kValueNULL) && (second_child_type != EidosValueType::kValueNULL))
	{
		// both operands are non-NULL, so we're doing a real comparison
		int first_child_count = first_child_value->Count();
		int second_child_count = second_child_value->Count();
		EidosCompareFunctionPtr compareFunc = Eidos_GetCompareFunctionForTypes(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): non-conformable array operands to the '!=' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if (first_child_count == 1)
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				int compare_result = compareFunc(*first_child_value, 0, *second_child_value, 0, operator_token);
				
				if (!result_dim_source)
					result_SP = (compare_result != 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				else
					result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{compare_result != 0});	// so we can modify it
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					// Direct float-to-float compare can be optimized through vector access
					const double *float1_data = first_child_value->FloatVector()->data();
					const double *float2_data = second_child_value->FloatVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(float1_data[value_index] != float2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
				{
					// Direct int-to-int compare can be optimized through vector access
					const int64_t *int1_data = first_child_value->IntVector()->data();
					const int64_t *int2_data = second_child_value->IntVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(int1_data[value_index] != int2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
				{
					// Direct object-to-object compare can be optimized through vector access
					EidosObjectElement * const *obj1_vec = first_child_value->ObjectElementVector()->data();
					EidosObjectElement * const *obj2_vec = second_child_value->ObjectElementVector()->data();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(obj1_vec[value_index] != obj2_vec[value_index], value_index);
				}
				else
				{
					// General case
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(compareFunc(*first_child_value, value_index, *second_child_value, value_index, operator_token) != 0, value_index);
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			if ((compareFunc == &CompareEidosValues_Float) && (second_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float1 = first_child_value->FloatAtIndex(0, operator_token);
				const double *float_data = second_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(float1 != float_data[value_index], value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Int) && (second_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int1 = first_child_value->IntAtIndex(0, operator_token);
				const int64_t *int_data = second_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(int1 != int_data[value_index], value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Object) && (second_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj1 = first_child_value->ObjectElementAtIndex(0, operator_token);
				EidosObjectElement * const *obj_vec = second_child_value->ObjectElementVector()->data();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(obj1 != obj_vec[value_index], value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(compareFunc(*first_child_value, 0, *second_child_value, value_index, operator_token) != 0, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			if ((compareFunc == &CompareEidosValues_Float) && (first_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float2 = second_child_value->FloatAtIndex(0, operator_token);
				const double *float_data = first_child_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(float_data[value_index] != float2, value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Int) && (first_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int2 = second_child_value->IntAtIndex(0, operator_token);
				const int64_t *int_data = first_child_value->IntVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(int_data[value_index] != int2, value_index);
			}
			else if ((compareFunc == &CompareEidosValues_Object) && (first_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObjectElement *obj2 = second_child_value->ObjectElementAtIndex(0, operator_token);
				EidosObjectElement * const *obj_vec = first_child_value->ObjectElementVector()->data();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(obj_vec[value_index] != obj2, value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(compareFunc(*first_child_value, value_index, *second_child_value, 0, operator_token) != 0, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): the '!=' operator requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(operator_token);
		}
		
		// Copy dimensions from whichever operand we chose at the beginning
		result_SP->CopyDimensionsFromValue(result_dim_source.get());
	}
	else
	{
		// if either operand is NULL (including if both are), it is an error
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): testing NULL with the '!=' operator is an error; use isNULL()." << EidosTerminate(operator_token);
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_NotEq()");
	return result_SP;
}

// A utility static method for getting an int for a string outside of a EidosInterpreter session
int64_t EidosInterpreter::NonnegativeIntegerForString(const std::string &p_number_string, const EidosToken *p_blame_token)
{
	// This needs to use the same criteria as NumericValueForString() below; it raises if the number is a float.
	// BCH 23 Jan. 2017: actually, this is *slightly* different from NumericValueForString(), because this
	// function will only parse/return non-negative ints; it does not allow a leading minus sign.  I'm changing
	// the name of this function to reflect this fact, since it is the expected/desired behavior for this function.
	const char *c_str = p_number_string.c_str();
	char *last_used_char = nullptr;
	
	errno = 0;
	
	if ((p_number_string.find('.') != std::string::npos) || (p_number_string.find('-') != std::string::npos))
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): \"" << p_number_string << "\" could not be represented as an integer (decimal or negative exponent)." << EidosTerminate(p_blame_token);
		return 0;
	}
	else if ((p_number_string.find('e') != std::string::npos) || (p_number_string.find('E') != std::string::npos))	// has an exponent
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): \"" << p_number_string << "\" could not be represented as an integer (strtod conversion error)." << EidosTerminate(p_blame_token);
		
		// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
		if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): \"" << p_number_string << "\" could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
		
		return static_cast<int64_t>(converted_value);
	}
	else																								// plain integer
	{
		int64_t converted_value = strtoq(c_str, &last_used_char, 10);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): \"" << p_number_string << "\" could not be represented as an integer (strtoq conversion error)." << EidosTerminate(p_blame_token);
		
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::FloatForString): \"" << p_number_string << "\" could not be represented as a float (strtod conversion error)." << EidosTerminate(p_blame_token);
	
	return converted_value;
}

// A utility static method for getting a numeric EidosValue* for a string outside of a EidosInterpreter session
EidosValue_SP EidosInterpreter::NumericValueForString(const std::string &p_number_string, const EidosToken *p_blame_token)
{
	const char *c_str = p_number_string.c_str();
	char *last_used_char = nullptr;
	
	errno = 0;
	
	// At this point, we have to decide whether to instantiate an int or a float.  If it has a decimal point or
	// a non-leading minus sign (which would be in the exponent), we'll make a float.  Otherwise, we'll make an int.
	// This might need revision in future; 1.2e3 could be an int, for example.  However, it is an ambiguity in
	// the syntax that will never be terribly comfortable; it's the price we pay for wanting ints to be
	// expressable using scientific notation.
	if ((p_number_string.find('.') != std::string::npos) || (p_number_string.find('-', 1) != std::string::npos))			// requires a float
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as a float (strtod conversion error)." << EidosTerminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(converted_value));
	}
	else if ((p_number_string.find('e') != std::string::npos) || (p_number_string.find('E') != std::string::npos))		// has an exponent
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (strtod conversion error)." << EidosTerminate(p_blame_token);
		
		// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
		if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(static_cast<int64_t>(converted_value)));
	}
	else																										// plain integer
	{
		int64_t converted_value = strtoq(c_str, &last_used_char, 10);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): \"" << p_number_string << "\" could not be represented as an integer (strtoq conversion error)." << EidosTerminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(converted_value));
	}
}

EidosValue_SP EidosInterpreter::Evaluate_Number(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Number()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Number", 0);
	
	// use a cached value from EidosASTNode::_OptimizeConstants() if present; this should always be hit now!
	EidosValue_SP result_SP = p_node->cached_literal_value_;
	
	if (!result_SP)
	{
		// CODE COVERAGE: This is dead code
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
	EidosValue_SP result_SP = p_node->cached_literal_value_;
	
	if (!result_SP)
	{
		// CODE COVERAGE: This is dead code
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(p_node->token_->token_string_));
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_String()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Identifier(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Identifier()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Identifier", 0);
	
	// use a cached value from EidosASTNode::_OptimizeConstants() if present
	EidosValue_SP result_SP = p_node->cached_literal_value_;
	
	if (!result_SP)
		result_SP = global_symbols_->GetValueOrRaiseForASTNode(p_node);	// raises if undefined
	
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
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
		SLIM_PROFILE_BLOCK_START_CONDITION(true_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
		
		result_SP = FastEvaluateNode(true_node);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END_CONDITION(true_node->profile_total_);
#endif
	}
	else if (condition_result == gStaticEidosValue_LogicalF)
	{
		// Handle a static singleton logical false super fast; no need for type check, count, etc
		if (children_size == 3)		// has an 'else' node
		{
			EidosASTNode *false_node = p_node->children_[2];
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
			SLIM_PROFILE_BLOCK_START_CONDITION(false_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
			
			result_SP = FastEvaluateNode(false_node);
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END_CONDITION(false_node->profile_total_);
#endif
		}
		else								// no 'else' node, so the result is void
		{
			result_SP = gStaticEidosValueVOID;
		}
	}
	else if (condition_result->Count() == 1)
	{
		eidos_logical_t condition_logical = condition_result->LogicalAtIndex(0, operator_token);
		
		if (condition_logical)
		{
			EidosASTNode *true_node = p_node->children_[1];
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
			SLIM_PROFILE_BLOCK_START_CONDITION(true_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
			
			result_SP = FastEvaluateNode(true_node);
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END_CONDITION(true_node->profile_total_);
#endif
		}
		else if (children_size == 3)		// has an 'else' node
		{
			EidosASTNode *false_node = p_node->children_[2];
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
			SLIM_PROFILE_BLOCK_START_CONDITION(false_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
			
			result_SP = FastEvaluateNode(false_node);
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END_CONDITION(false_node->profile_total_);
#endif
		}
		else								// no 'else' node, so the result is void
		{
			result_SP = gStaticEidosValueVOID;
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_If): condition for if statement has size() != 1." << EidosTerminate(p_node->token_);
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
		EidosASTNode *statement_node = p_node->children_[0];
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
		SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
		
		EidosValue_SP statement_value = FastEvaluateNode(statement_node);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
		
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
			eidos_logical_t condition_logical = condition_result->LogicalAtIndex(0, operator_token);
			
			if (!condition_logical)
				break;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Do): condition for do-while loop has size() != 1." << EidosTerminate(p_node->token_);
		}
	}
	while (true);
	
	if (!result_SP)
		result_SP = gStaticEidosValueVOID;
	
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
			eidos_logical_t condition_logical = condition_result->LogicalAtIndex(0, operator_token);
			
			if (!condition_logical)
				break;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_While): condition for while loop has size() != 1." << EidosTerminate(p_node->token_);
		}
		
		// execute the while loop's statement by evaluating its node; evaluation values get thrown away
		EidosASTNode *statement_node = p_node->children_[1];
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
		SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
		
		EidosValue_SP statement_value = FastEvaluateNode(statement_node);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
		
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
		result_SP = gStaticEidosValueVOID;
	
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword requires an identifier for its left operand." << EidosTerminate(p_node->token_);
	
	EidosGlobalStringID identifier_name = identifier_child->cached_stringID_;
	
	// check for a constant up front, to give a better error message with the token highlighted
	bool is_const = false;
	
	if (global_symbols_->ContainsSymbol_IsConstant(identifier_name, &is_const))
		if (is_const)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): identifier '" << identifier_child->token_->token_string_ << "' cannot be redefined because it is a constant." << EidosTerminate(identifier_child->token_);
	
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
			if (!range_node->cached_range_value_)
			{
				EidosValue_SP range_start_value_SP = FastEvaluateNode(range_node->children_[0]);
				EidosValue *range_start_value = range_start_value_SP.get();
				EidosValue_SP range_end_value_SP = FastEvaluateNode(range_node->children_[1]);
				EidosValue *range_end_value = range_end_value_SP.get();
				
				if ((range_start_value->Type() == EidosValueType::kValueInt) && (range_start_value->Count() == 1) && (range_start_value->DimensionCount() == 1) &&
					(range_end_value->Type() == EidosValueType::kValueInt) && (range_end_value->Count() == 1) && (range_end_value->DimensionCount() == 1))
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
			// Maybe we can streamline a seqAlong() or seqLen() call; let's check
			const EidosASTNode *call_name_node = range_node->children_[0];
			
			if (call_name_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
			{
				const EidosFunctionSignature *signature = call_name_node->cached_signature_.get();
				
				if (signature)
				{
					if (signature->internal_function_ == &Eidos_ExecuteFunction_seqAlong)
					{
						if (range_node->children_.size() == 2)
						{
							// We have a qualifying seqAlong() call, so evaluate its argument and set up our simple integer sequence
							const EidosASTNode *argument_node = range_node->children_[1];
							
							simpleIntegerRange = true;
							
							EidosValue_SP argument_value = FastEvaluateNode(argument_node);
							
							start_int = 0;
							end_int = argument_value->Count() - 1;
							
							// A seqAlong() on a zero-length operand would give us a loop from 0 to -1; short-circuit that
							if (end_int == -1)
								goto for_exit;
						}
					}
					else if (signature->internal_function_ == &Eidos_ExecuteFunction_seqLen)
					{
						if (range_node->children_.size() == 2)
						{
							// We have a qualifying seqLen() call, so evaluate its argument and set up our simple integer sequence
							const EidosASTNode *argument_node = range_node->children_[1];
							
							simpleIntegerRange = true;
							
							EidosValue_SP argument_value = FastEvaluateNode(argument_node);
							EidosValueType arg_type = argument_value->Type();
							
							// check the argument; could call CheckArguments() except that it would use the wrong error position
							if (arg_type != EidosValueType::kValueInt)
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): argument 1 (length) cannot be type " << arg_type << " for function seqLen()." << EidosTerminate(call_name_node->token_);
							if (argument_value->Count() != 1)
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): argument 1 (length) must be a singleton (size() == 1) for function seqLen(), but size() == " << argument_value->Count() << "." << EidosTerminate(call_name_node->token_);
							
							int64_t length = argument_value->IntAtIndex(0, call_name_node->token_);
							
							if (length < 0)
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): function seqLen() requires length to be greater than or equal to 0 (" << length << " supplied)." << EidosTerminate(call_name_node->token_);
							
							start_int = 0;
							end_int = length - 1;
							
							// A seqLen() with a length of 0 would give us a loop from 0 to -1; short-circuit that
							if (end_int == -1)
								goto for_exit;
						}
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
		
		// An empty simple integer range can be skipped altogether, without modifying the index variable
		if (range_count <= 0)
			goto for_exit;
		
		if (!assigns_index && !references_index)
		{
			// the loop index variable is not actually used at all; we are just being asked to do a set number of iterations
			// we do need to set up the index variable on exit, though, since code below us might use the final value
			int range_index;
			
			for (range_index = 0; range_index < range_count; ++range_index)
			{
				EidosASTNode *statement_node = p_node->children_[2];
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
				SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
				
				EidosValue_SP statement_value = FastEvaluateNode(statement_node);
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
				
				if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
				if (next_statement_hit_)				next_statement_hit_ = false;
				if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
			}
			
			// set up the final value on exit; if we completed the loop, we need to go back one step
			if (range_index == range_count)
				range_index--;
			
			global_symbols_->SetValueForSymbolNoCopy(identifier_name, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(counting_up ? start_int + range_index : start_int - range_index)));
		}
		else	// !assigns_index, guaranteed above
		{
			// the loop index variable is referenced in the loop body but is not assigned to, so we can use a single
			// EidosValue that we stick new values into – much, much faster.
			EidosValue_Int_singleton_SP index_value_SP = EidosValue_Int_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
			EidosValue_Int_singleton *index_value = index_value_SP.get();
			
			global_symbols_->SetValueForSymbolNoCopy(identifier_name, std::move(index_value_SP));
			
			for (int range_index = 0; range_index < range_count; ++range_index)
			{
				index_value->SetValue(counting_up ? start_int + range_index : start_int - range_index);
				
				EidosASTNode *statement_node = p_node->children_[2];
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
				SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
				
				EidosValue_SP statement_value = FastEvaluateNode(statement_node);
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
				
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
					EidosASTNode *statement_node = p_node->children_[2];
					
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
					// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
					SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
					
					EidosValue_SP statement_value = FastEvaluateNode(statement_node);
					
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
					// PROFILING
					SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
					
					if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
					if (next_statement_hit_)				next_statement_hit_ = false;
					if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
				}
				
				// set up the final value on exit; if we completed the loop, we need to go back one step
				if (range_index == range_count)
					range_index--;
				
				global_symbols_->SetValueForSymbolNoCopy(identifier_name, range_value->GetValueAtIndex(range_index, operator_token));
				
				loop_handled = true;
			}
			else if (!assigns_index && (range_count > 1))
			{
				// the loop index variable is referenced in the loop body but is not assigned to, so we can use a single
				// EidosValue that we stick new values into – much, much faster.
				if (range_type == EidosValueType::kValueInt)
				{
					const int64_t *range_data = range_value->IntVector()->data();
					EidosValue_Int_singleton_SP index_value_SP = EidosValue_Int_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
					EidosValue_Int_singleton *index_value = index_value_SP.get();
					
					global_symbols_->SetValueForSymbolNoCopy(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_data[range_index]);
						
						EidosASTNode *statement_node = p_node->children_[2];
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
						SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
						
						EidosValue_SP statement_value = FastEvaluateNode(statement_node);
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING
						SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueFloat)
				{
					const double *range_data = range_value->FloatVector()->data();
					EidosValue_Float_singleton_SP index_value_SP = EidosValue_Float_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0));
					EidosValue_Float_singleton *index_value = index_value_SP.get();
					
					global_symbols_->SetValueForSymbolNoCopy(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_data[range_index]);
						
						EidosASTNode *statement_node = p_node->children_[2];
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
						SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
						
						EidosValue_SP statement_value = FastEvaluateNode(statement_node);
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING
						SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueString)
				{
					const std::vector<std::string> &range_vec = *range_value->StringVector();
					EidosValue_String_singleton_SP index_value_SP = EidosValue_String_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_empty_string));
					EidosValue_String_singleton *index_value = index_value_SP.get();
					
					global_symbols_->SetValueForSymbolNoCopy(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_vec[range_index]);
						
						EidosASTNode *statement_node = p_node->children_[2];
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
						SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
						
						EidosValue_SP statement_value = FastEvaluateNode(statement_node);
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING
						SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueObject)
				{
					EidosObjectElement * const *range_vec = range_value->ObjectElementVector()->data();
					EidosValue_Object_singleton_SP index_value_SP = EidosValue_Object_singleton_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(nullptr, ((EidosValue_Object *)range_value.get())->Class()));
					EidosValue_Object_singleton *index_value = index_value_SP.get();
					
					global_symbols_->SetValueForSymbolNoCopy(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->SetValue(range_vec[range_index]);
						
						EidosASTNode *statement_node = p_node->children_[2];
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
						SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
						
						EidosValue_SP statement_value = FastEvaluateNode(statement_node);
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING
						SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
						
						if (return_statement_hit_)				{ result_SP = std::move(statement_value); break; }
						if (next_statement_hit_)				next_statement_hit_ = false;
						if (break_statement_hit_)				{ break_statement_hit_ = false; break; }
					}
					
					loop_handled = true;
				}
				else if (range_type == EidosValueType::kValueLogical)
				{
					const eidos_logical_t *range_data = range_value->LogicalVector()->data();
					EidosValue_Logical_SP index_value_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
					EidosValue_Logical *index_value = index_value_SP->resize_no_initialize(1);
					
					index_value->set_logical_no_check(false, 0);	// initial placeholder
					
					global_symbols_->SetValueForSymbolNoCopy(identifier_name, std::move(index_value_SP));
					
					for (int range_index = 0; range_index < range_count; ++range_index)
					{
						index_value->set_logical_no_check(range_data[range_index], 0);
						
						EidosASTNode *statement_node = p_node->children_[2];
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
						SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
						
						EidosValue_SP statement_value = FastEvaluateNode(statement_node);
						
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
						// PROFILING
						SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
						
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
					global_symbols_->SetValueForSymbolNoCopy(identifier_name, range_value->GetValueAtIndex(range_index, operator_token));
					
					// execute the for loop's statement by evaluating its node; evaluation values get thrown away
					EidosASTNode *statement_node = p_node->children_[2];
					
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
					// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
					SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
					
					EidosValue_SP statement_value = FastEvaluateNode(statement_node);
					
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
					// PROFILING
					SLIM_PROFILE_BLOCK_END_CONDITION(statement_node->profile_total_);
#endif
					
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
			if (range_type == EidosValueType::kValueVOID)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword does not allow void for its right operand (the range to be iterated over)." << EidosTerminate(p_node->token_);
			if (range_type == EidosValueType::kValueNULL)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword does not allow NULL for its right operand (the range to be iterated over)." << EidosTerminate(p_node->token_);
		}
	}
	
for_exit:
	
	if (!result_SP)
		result_SP = gStaticEidosValueVOID;
	
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
	
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
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
	
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
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
		result_SP = gStaticEidosValueVOID;	// no return value; note that "return;" is semantically different from "return NULL;" because it returns void
	else
		result_SP = FastEvaluateNode(p_node->children_[0]);
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Return()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_FunctionDecl(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_FunctionDecl()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_FunctionDecl", 4);
	
	const EidosASTNode *return_type_node = p_node->children_[0];
	const EidosASTNode *function_name_node = p_node->children_[1];
	const EidosASTNode *param_list_node = p_node->children_[2];
	const EidosASTNode *body_node = p_node->children_[3];
	
	// Build a signature object for this function
	EidosTypeSpecifier &return_type = return_type_node->typespec_;
	const std::string &function_name = function_name_node->token_->token_string_;
	EidosFunctionSignature *sig;
	
	if (return_type.object_class == nullptr)
		sig = (EidosFunctionSignature *)(new EidosFunctionSignature(function_name,
																	nullptr,
																	return_type.type_mask));
	else
		sig = (EidosFunctionSignature *)(new EidosFunctionSignature(function_name,
																	nullptr,
																	return_type.type_mask,
																	return_type.object_class));
	
	const std::vector<EidosASTNode *> &param_nodes = param_list_node->children_;
	std::vector<std::string> used_param_names;
	
	for (EidosASTNode *param_node : param_nodes)
	{
		const std::vector<EidosASTNode *> &param_children = param_node->children_;
		int param_children_count = (int)param_children.size();
		
		if ((param_children_count == 2) || (param_children_count == 3))
		{
			EidosTypeSpecifier &param_type = param_children[0]->typespec_;
			const std::string &param_name = param_children[1]->token_->token_string_;
			
			// Check param_name; it needs to not be used by another parameter
			if (std::find(used_param_names.begin(), used_param_names.end(), param_name) != used_param_names.end())
			{
				delete sig;
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionDecl): invalid name for parameter '" << param_name << "'; this name was already used for a previous parameter in this declaration." << EidosTerminate(p_node->token_);
			}
			
			if (param_children_count == 2)
			{
				// param_node has 2 children (type, identifier)
				sig->AddArg(param_type.type_mask, param_name, param_type.object_class);
			}
			else if (param_children_count == 3)
			{
				// param_node has 3 children (type, identifier, default); we need to figure out the default value,
				// which is either a constant of some sort, or is an identifier representing a built-in Eidos constant
				EidosASTNode *default_node = param_children[2];
				EidosValue_SP default_value;
				
				if (default_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
				{
					// We just hard-code the names of the built-in Eidos constants here, for now
					const std::string &default_string = default_node->token_->token_string_;
					
					if (std::find(gEidosConstantNames.begin(), gEidosConstantNames.end(), default_string) != gEidosConstantNames.end())
						default_value = FastEvaluateNode(default_node);
					else
					{
						delete sig;
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionDecl): invalid default value for parameter '" << param_name << "'; a default value must be a numeric constant, a string constant, or a built-in Eidos constant (T, F, NULL, PI, E, INF, or NAN)." << EidosTerminate(p_node->token_);
					}
				}
				else
				{
					default_value = FastEvaluateNode(default_node);
				}
				
				sig->AddArgWithDefault(param_type.type_mask, param_name, param_type.object_class, std::move(default_value));
			}
			
			used_param_names.push_back(param_name);
		}
	}
	
	// Add the function body to the function signature.  We can't just use body_node, because we don't know what its
	// lifetime will be; we need our own private copy.  The best thing, really, is to make our own EidosScript object
	// with the appropriate substring, and re-tokenize and re-parse.  This is somewhat wasteful, but it's one-time
	// overhead so it shouldn't matter.  It will smooth out all of the related issues with error reporting, etc.,
	// since we won't be dependent upon somebody else's script object.  Errors in tokenization/parsing should never
	// occur, since the code for the function body already passed through that process once.
	EidosScript *script = new EidosScript(body_node->token_->token_string_);
	
	script->Tokenize();
	script->ParseInterpreterBlockToAST(false);
	
	sig->body_script_ = script;
	sig->user_defined_ = true;
	
	//std::cout << *sig << std::endl;
	
	// Check that a built-in function is not already defined with this name; no replacing the built-ins.
	auto signature_iter = function_map_.find(function_name);
	
	if (signature_iter != function_map_.end())
	{
		const EidosFunctionSignature *prior_sig = signature_iter->second.get();
		
		if (prior_sig->internal_function_ || !prior_sig->delegate_name_.empty() || !prior_sig->user_defined_)
		{
			delete sig;
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionDecl): cannot replace built-in function " << function_name << "()." << EidosTerminate(p_node->token_);
		}
	}
	
	// Add the user-defined function to our function map (possibly replacing a previous version)
	auto found_iter = function_map_.find(sig->call_name_);
	
	if (found_iter != function_map_.end())
		function_map_.erase(found_iter);
	
	function_map_.insert(EidosFunctionMapPair(sig->call_name_, EidosFunctionSignature_SP(sig)));
	
	// Always return void
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_FunctionDecl()");
	
	return result_SP;
}



























































