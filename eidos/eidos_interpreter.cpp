//
//  eidos_interpreter.cpp
//  Eidos
//
//  Created by Ben Haller on 4/4/15.
//  Copyright (c) 2015-2025 Benjamin C. Haller.  All rights reserved.
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
#include <limits.h>


// We have a bunch of behaviors that we want to do only when compiled DEBUG or EIDOS_GUI; #if tests everywhere are very ugly, so we make
// some #defines here that help structure this.

#if DEBUG || defined(EIDOS_GUI)

#define EIDOS_ENTRY_EXECUTION_LOG(method_name)						if (logging_execution_) *execution_log_ << IndentString(execution_log_indent_++) << (method_name) << " entered\n";
#define EIDOS_EXIT_EXECUTION_LOG(method_name)						if (logging_execution_) *execution_log_ << IndentString(--execution_log_indent_) << (method_name) << " : return == " << *result_SP << "\n";
#define EIDOS_BEGIN_EXECUTION_LOG()									if (logging_execution_) execution_log_indent_ = 0;
#define EIDOS_END_EXECUTION_LOG()									if (gEidosLogEvaluation) std::cout << ExecutionLog();
#define EIDOS_ASSERT_CHILD_COUNT(method_name, count)				if (p_node->children_.size() != (count)) EIDOS_TERMINATION << "ERROR (" << (method_name) << "): (internal error) expected " << (count) << " child(ren)." << EidosTerminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ(method_name, min_count)		if (p_node->children_.size() < (min_count)) EIDOS_TERMINATION << "ERROR (" << (method_name) << "): (internal error) expected " << (min_count) << "+ child(ren)." << EidosTerminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_RANGE(method_name, lower, upper)			if ((p_node->children_.size() < (lower)) || (p_node->children_.size() > (upper))) EIDOS_TERMINATION << "ERROR (" << (method_name) << "): (internal error) expected " << (lower) << " to " << (upper) << " children." << EidosTerminate(p_node->token_);
#define EIDOS_ASSERT_CHILD_COUNT_X(node, node_type, method_name, count, blame_token)		if ((node)->children_.size() != (count)) EIDOS_TERMINATION << "ERROR (" << (method_name) << "): (internal error) expected " << (count) << " child(ren) for " << (node_type) << " node." << EidosTerminate(blame_token);
#define EIDOS_ASSERT_CHILD_COUNT_GTEQ_X(node, node_type, method_name, min_count, blame_token)		if ((node)->children_.size() < (min_count)) EIDOS_TERMINATION << "ERROR (" << (method_name) << "): (internal error) expected " << (min_count) << "+ child(ren) for " << (node_type) << " node." << EidosTerminate(blame_token);

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
		const EidosClass *base_element_class = ((const EidosValue_Object &)p_base_value).Class();
		const EidosClass *dest_element_class = ((const EidosValue_Object &)p_dest_value).Class();
		bool base_is_typeless = (base_element_class == gEidosObject_Class);
		bool dest_is_typeless = (dest_element_class == gEidosObject_Class);
		
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

EidosInterpreter::EidosInterpreter(const EidosScript &p_script, EidosSymbolTable &p_symbols, EidosFunctionMap &p_functions, EidosContext *p_eidos_context, std::ostream &p_outstream, std::ostream &p_errstream)
	: eidos_context_(p_eidos_context), root_node_(p_script.AST()), global_symbols_(&p_symbols), function_map_(p_functions), execution_output_(p_outstream), error_output_(p_errstream)
{
#ifdef SLIMGUI
	// Take a pointer to the context's debugging points; we do not copy, so the context can update the debug points underneath us
	if (eidos_context_)
        debug_points_ = eidos_context_->DebugPoints();
#endif
}

EidosInterpreter::EidosInterpreter(const EidosASTNode *p_root_node_, EidosSymbolTable &p_symbols, EidosFunctionMap &p_functions, EidosContext *p_eidos_context, std::ostream &p_outstream, std::ostream &p_errstream)
	: eidos_context_(p_eidos_context), root_node_(p_root_node_), global_symbols_(&p_symbols), function_map_(p_functions), execution_output_(p_outstream), error_output_(p_errstream)
{
#ifdef SLIMGUI
	// Take a pointer to the context's debugging points; we do not copy, so the context can update the debug points underneath us
	debug_points_ = eidos_context_->DebugPoints();
#endif
}

void EidosInterpreter::SetShouldLogExecution(bool p_log)
{
	logging_execution_ = p_log;
	
	if (logging_execution_)
	{
#if DEBUG || defined(EIDOS_GUI)
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

// the starting point for internally executed blocks, which require braces and suppress output
EidosValue_SP EidosInterpreter::EvaluateInternalBlock(EidosScript *p_script_for_block)
{
	// EvaluateInternalBlock() does not log execution, since it is not user-initiated
	
	// Internal blocks may be associated with their own script object; if so, the error tracking code needs to track that
	// Note that if there is not a separate script block, then we do NOT set up an error context here; we assume that
	// that has been done externally, just like EvaluateInterpreterBlock().
	EidosValue_SP result_SP;
	
	if ((p_script_for_block != nullptr) && (p_script_for_block != gEidosErrorContext.currentScript))
	{
		// This script block is constructed at runtime and has its own script, so we need to redirect error tracking
		EidosErrorContext error_context_save = gEidosErrorContext;
		gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, p_script_for_block};
		
		// Same code as below, just bracketed by the error context save/restore
		result_SP = FastEvaluateNode(root_node_);
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInternalBlock): statement '" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "' encountered with no enclosing loop." << EidosTerminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
		
		// Restore the normal error context; note that a raise blows through this, of course, since we want the raise-catch
		// machinery to report the error using the error information set up by the raise.
		gEidosErrorContext = error_context_save;
	}
	else
	{
		result_SP = FastEvaluateNode(root_node_);
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInternalBlock): statement '" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "' encountered with no enclosing loop." << EidosTerminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
	}
	
	// handle a return statement; we're at the top level, so there's not much to do
	if (return_statement_hit_)
		return_statement_hit_ = false;
	
	// EvaluateInternalBlock() does not send the result of execution to the output stream; EvaluateInterpreterBlock() does,
	// because it is for interactive use, but EvaluateInternalBlock() is for internal use, and so interactive output
	// is undesirable.  Eidos code that wants to generate output can always use print(), cat(), etc.
	
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
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		EidosValue_SP statement_result_SP = FastEvaluateNode(child_node);
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(child_node->profile_total_);
#endif
		
		// if a next or break statement was hit and was not handled by a loop, throw an error
		if (next_statement_hit_ || break_statement_hit_)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::EvaluateInterpreterBlock): statement '" << (next_statement_hit_ ? gEidosStr_next : gEidosStr_break) << "' encountered with no enclosing loop." << EidosTerminate(nullptr);		// nullptr used to allow the token set by the next/break to be used
		
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
// subscript operation (5, 5, and {3,4}, respectively), and return them to the caller, who will assign into those subscripts.
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
					subset_indices.emplace_back(gStaticEidosValueNULL);
				}
				else
				{
					// We have an expression node, so we evaluate it, check the value, and save it
					EidosValue_SP child_value = FastEvaluateNode(subset_index_node);
					EidosValueType child_type = child_value->Type();
					
					if (child_type == EidosValueType::kValueFloat)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): it is no longer legal to subset with float indices; use asInteger() to cast the indices to integer." << EidosTerminate(parent_token);
					if ((child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueNULL))
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
						eidos_logical_t logical_value = second_child_value->LogicalAtIndex_NOCAST(value_idx, parent_token);
						
						if (logical_value)
							p_indices_ptr->emplace_back(base_indices[value_idx]);
					}
				}
				else	// (second_child_type == EidosValueType::kValueInt)
				{
					// An integer vector can be of any length; each number selects the index at that index in base_indices
					const int64_t *second_child_data = second_child_value->IntData();
					
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): chaining of matrix/array-style subsets in assignments is not supported." << EidosTerminate(parent_token);
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
							indices.emplace_back(dim_index);
					}
					else if (subset_type == EidosValueType::kValueLogical)
					{
						// We have a logical subset, which must equal the dimension size and gets translated directly into inclusion_indices
						if (subset_count != dim_size)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): the '[]' operator requires that the size() of a logical index operand must match the corresponding dimension of the indexed operand." << EidosTerminate(parent_token);
						
						const eidos_logical_t *logical_index_data = subset_value->LogicalData();
						
						for (int dim_index = 0; dim_index < dim_size; dim_index++)
							if (logical_index_data[dim_index])
								indices.emplace_back(dim_index);
					}
					else	// (subset_type == EidosValueType::kValueInt)
					{
						// We have an integer subset, which selects indices within inclusion_indices
						for (int index_index = 0; index_index < subset_count; index_index++)
						{
							int64_t index_value = subset_value->IntAtIndex_NOCAST(index_index, parent_token);
							
							if ((index_value < 0) || (index_value >= dim_size))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(parent_token);
							else
								indices.emplace_back(index_value);
						}
					}
					
					if (indices.size() == 0)
					{
						empty_dimension = true;
						break;
					}
					
					inclusion_counts.emplace_back((int)indices.size());
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
			*p_property_string_id_ptr = EidosStringRegistry::GlobalStringIDForString(right_operand->token_->token_string_);
			
			int number_of_elements = first_child_value->Count();	// property operations are guaranteed to produce one value per element
			
			for (int element_idx = 0; element_idx < number_of_elements; element_idx++)
				p_indices_ptr->emplace_back(element_idx);
			
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_parent_node, "identifier", "EidosInterpreter::_ProcessSubsetAssignment", 0, parent_token);
			
			bool identifier_is_const = false;
			bool identifier_is_local = false;		// not used, just needed by the API
			EidosValue_SP identifier_value_SP = global_symbols_->GetValueOrRaiseForASTNode_IsConstIsLocal(p_parent_node, &identifier_is_const, &identifier_is_local);
			EidosValue *identifier_value = identifier_value_SP.get();
			
			// Check for a constant symbol table.  Note that _AssignRValueToLValue() will check for a constant EidosValue.
			// The work of checking constness is divided between these methods for historical reasons.
			if (identifier_is_const || identifier_value->IsIteratorVariable())
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessSubsetAssignment): identifier '" << EidosStringRegistry::StringForGlobalStringID(p_parent_node->cached_stringID_) << "' cannot be redefined because it is a constant." << EidosTerminate(nullptr);
			
			// BCH 12/21/2023: We used to munge singletons into vectors here, because we didn't have the tools to modify the
			// element in a singleton value.  That limitation has now been fixed, so this munging is no longer necessary.
			/*if (identifier_value->IsSingleton())
			{
				identifier_value_SP = identifier_value->VectorBasedCopy();
				identifier_value = identifier_value_SP.get();
				
				global_symbols_->SetValueForSymbolNoCopy(p_parent_node->cached_stringID_, identifier_value_SP);
			}*/
			
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
	
#if DEBUG || defined(EIDOS_GUI)
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
			
			if ((rvalue_count != 1) && (rvalue_count != index_count))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): assignment to a subscript requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << EidosTerminate(nullptr);
			
			if (property_string_id != gEidosID_none)
			{
				// This would be a multiplex assignment of one value to (maybe) more than one index in a property of a symbol host: x.foo[5:10] = 10,
				// or a one-to-one assignment of values to indices in a property of a symbol host: x.foo[5:10] = 5:10
				
				// BCH 12/8/2017: We used to allow assignments of the form host.property[indices] = rvalue.  I have decided to change Eidos policy
				// to disallow that form of assignment.  Conceptually, it sort of doesn't make sense, because it implies fetching the property
				// values and assigning into a subset of those fetched values; but the fetched values are not an lvalue at that point.  We did it
				// anyway through a semantic rearrangement, but I now think that was a bad idea.  The conceptual problem became more stark with the
				// addition of matrices and arrays; if host is a matrix/array, host[i,j,...] is too, and so assigning into a host with that form of
				// subset makes conceptual sense, and host[i,j,...].property = rvalue makes sense as well – fetch the indexed values and assign
				// into their property.  But host.property[i,j,...] = rvalue does not make sense, because host.property is always a vector, and
				// cannot be subset in that way!  So the underlying contradiction in the old policy is exposed.  Time for it to change.
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): cannot assign into a subset of a property; not an lvalue." << EidosTerminate(nullptr);
			}
			
			if (!TypeCheckAssignmentOfEidosValueIntoEidosValue(*p_rvalue, *base_value))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): type mismatch in assignment (" << p_rvalue->Type() << " versus " << base_value->Type() << ")." << EidosTerminate(nullptr);
			
			// Assignments like x[logical(0)] = y, where y is zero-length, are no-ops as long as they're not errors
			if (index_count == 0)
				return;
			
			// Check for a constant value.  Note that ProcessSubsetAssignment() already checked for a constant symbol table.
			// The work of checking constness is divided between these methods for historical reasons.
			if (base_value->IsConstant())
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): value cannot be redefined because it is a constant." << EidosTerminate(nullptr);
			
			// At this point, we have either a multiplex assignment of one value to (maybe) more than one index in a symbol host: x[5:10] = 10, when
			// (rvalue_count == 1), or a one-to-one assignment of values to indices in a symbol host: x[5:10] = 5:10, when (rvalue_count == index_count)
			// We handle them both together, as far as we can.  BCH 12/21/2023: This used to be done with SetValueAtIndex(); that method has been removed.
			switch (base_value->Type())
			{
				case EidosValueType::kValueLogical:
				{
					eidos_logical_t *base_value_data = base_value->LogicalData_Mutable();
					
					if (rvalue_count == 1)
					{
						eidos_logical_t rvalue = p_rvalue->LogicalAtIndex_CAST(0, nullptr);
						
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = rvalue;
					}
					else
					{
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = p_rvalue->LogicalAtIndex_CAST(value_idx, nullptr);
					}
					break;
				}
				case EidosValueType::kValueInt:
				{
					int64_t *base_value_data = base_value->IntData_Mutable();
					
					if (rvalue_count == 1)
					{
						int64_t rvalue = p_rvalue->IntAtIndex_CAST(0, nullptr);
						
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = rvalue;
					}
					else
					{
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = p_rvalue->IntAtIndex_CAST(value_idx, nullptr);
					}
					break;
				}
				case EidosValueType::kValueFloat:
				{
					double *base_value_data = base_value->FloatData_Mutable();
					
					if (rvalue_count == 1)
					{
						double rvalue = p_rvalue->FloatAtIndex_CAST(0, nullptr);
						
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = rvalue;
					}
					else
					{
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = p_rvalue->FloatAtIndex_CAST(value_idx, nullptr);
					}
					break;
				}
				case EidosValueType::kValueString:
				{
					std::string *base_value_data = base_value->StringData_Mutable();
					
					if (rvalue_count == 1)
					{
						std::string rvalue = p_rvalue->StringAtIndex_CAST(0, nullptr);
						
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = rvalue;
					}
					else
					{
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_value_data[indices[value_idx]] = p_rvalue->StringAtIndex_CAST(value_idx, nullptr);
					}
					break;
				}
				case EidosValueType::kValueObject:
				{
					EidosValue_Object *base_object_vector = (EidosValue_Object *)base_value.get();
					
					if (rvalue_count == 1)
					{
						EidosObject *rvalue = p_rvalue->ObjectElementAtIndex_CAST(0, nullptr);
						
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_object_vector->set_object_element_no_check_CRR(rvalue, indices[value_idx]);
					}
					else
					{
						for (int value_idx = 0; value_idx < index_count; value_idx++)
							base_object_vector->set_object_element_no_check_CRR(p_rvalue->ObjectElementAtIndex_CAST(value_idx, nullptr), indices[value_idx]);
					}
					break;
				}
				default:
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_AssignRValueToLValue): cannot do subset assignment into type " << base_value->Type() << ")." << EidosTerminate(nullptr);
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
			static_cast<EidosValue_Object *>(first_child_value.get())->SetPropertyOfElements(second_child_node->cached_stringID_, *p_rvalue, second_child_node->token_);
			
			break;
		}
		case EidosTokenType::kTokenIdentifier:
		{
			EIDOS_ASSERT_CHILD_COUNT_X(p_lvalue_node, "identifier", "EidosInterpreter::_AssignRValueToLValue", 0, nullptr);
			
			// Check for an lvalue that is a loop iterator, which cannot be changed
			if (global_symbols_->ContainsSymbol(p_lvalue_node->cached_stringID_))
			{
				EidosValue *existing_value = global_symbols_->GetValueRawOrRaiseForSymbol(p_lvalue_node->cached_stringID_);
				
				if (existing_value->IsIteratorVariable())
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): identifier '" << p_lvalue_node->token_->token_string_ << "' cannot be redefined because it is a constant." << EidosTerminate(nullptr);
			}
			
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
		case EidosTokenType::kTokenAssign_R:	return Evaluate_Assign_R(p_node);
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
	
#ifndef DEBUG_POINTS_ENABLED
#error "DEBUG_POINTS_ENABLED is not defined; include eidos_globals.h"
#endif
#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	if (debug_points_ && debug_points_->set.size() && (p_node->token_->token_line_ != -1) &&
		(debug_points_->set.find(p_node->token_->token_line_) != debug_points_->set.end()))
	{
		ErrorOutputStream() << EidosDebugPointIndent::Indent() << "#DEBUG NULL_STATEMENT (line " << (p_node->token_->token_line_ + 1) << eidos_context_->DebugPointInfo() << ")" << std::endl;
	}
#endif
	
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
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_START();
#endif
		
		EidosValue_SP statement_result_SP = FastEvaluateNode(child_node);
		
#if (SLIMPROFILING == 1)
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
		int64_t first_int = p_first_child_value.IntAtIndex_NOCAST(0, operator_token);
		int64_t second_int = p_second_child_value.IntAtIndex_NOCAST(0, operator_token);
		
		if (first_int <= second_int)
		{
			if (second_int - first_int + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
			EidosValue_Int *int_result = int_result_SP->resize_no_initialize(second_int - first_int + 1);
			
			for (int64_t range_index = 0; range_index <= second_int - first_int; ++range_index)
				int_result->set_int_no_check(range_index + first_int, range_index);
			
			result_SP = int_result_SP;
		}
		else
		{
			if (first_int - second_int + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
			EidosValue_Int *int_result = int_result_SP->resize_no_initialize(first_int - second_int + 1);
			
			for (int64_t range_index = 0; range_index <= first_int - second_int; ++range_index)
				int_result->set_int_no_check(first_int - range_index, range_index);
			
			result_SP = int_result_SP;
		}
	}
	else
	{
		double first_float = p_first_child_value.NumericAtIndex_NOCAST(0, operator_token);
		double second_float = p_second_child_value.NumericAtIndex_NOCAST(0, operator_token);
		
		if (std::isnan(first_float) || std::isnan(second_float))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): operands of the ':' operator must not be NAN." << EidosTerminate(operator_token);
		
		if (first_float <= second_float)
		{
			if (second_float - first_float + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
			EidosValue_Float *float_result = float_result_SP->reserve((int)(second_float - first_float + 1));
			
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
			
			result_SP = float_result_SP;
		}
		else
		{
			if (first_float - second_float + 1 > 100000000)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_Evaluate_RangeExpr_Internal): a range with more than 100000000 entries cannot be constructed." << EidosTerminate(operator_token);
			
			EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
			EidosValue_Float *float_result = float_result_SP->reserve((int)(first_float - second_float + 1));
			
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
			
			result_SP = float_result_SP;
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
		{
			p_node->cached_range_value_ = result_SP;
			p_node->cached_range_value_->MarkAsConstant();
		}
	}
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_RangeExpr()");
	return result_SP;
}

void EidosInterpreter::_CreateArgumentList(const EidosASTNode *p_node, const EidosCallSignature *p_call_signature)
{
	EidosASTNode_ArgumentCache *argument_cache = new EidosASTNode_ArgumentCache();
	p_node->argument_cache_ = argument_cache;
	
	std::vector<EidosValue_SP> &arg_buffer = argument_cache->argument_buffer_;
	std::vector<EidosASTNode_ArgumentFill> &fill_info = argument_cache->fill_info_;
	std::vector<uint8_t> &no_fill_index = argument_cache->no_fill_index_;
	const std::vector<EidosASTNode *> &node_children = p_node->children_;
	
	std::vector<uint8_t> filled_explicitly;		// locally, we need a vector that tells us whether an index was filed explicitly or by default
	
	// Run through the argument nodes, reserve space for them in the arguments buffer, and evaluate default/constant values once for all calls
	auto node_children_end = node_children.end();
	int sig_arg_index = 0;
	int sig_arg_count = (int)p_call_signature->arg_name_IDs_.size();
	bool had_named_argument = false;
	bool in_ellipsis = (sig_arg_count > 0) && (p_call_signature->arg_name_IDs_[0] == gEidosID_ELLIPSIS);
	
	for (auto child_iter = node_children.begin() + 1; child_iter != node_children_end; ++child_iter)
	{
		EidosASTNode *child = *child_iter;
		bool is_named_argument = (child->token_->token_type_ == EidosTokenType::kTokenAssign);
		
#if DEBUG
		if (is_named_argument && (child->children_.size() != 2))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): (internal error) named argument node child count != 2." << EidosTerminate(nullptr);
#endif
		
		if (in_ellipsis)
		{
			// While in an ellipsis, all arguments get consumed without type-checking; this ends when we hit a named argument, or run out of arguments
			if (is_named_argument)
			{
				// We have a named argument, so it doesn't go in the ellipsis section
				in_ellipsis = false;
				sig_arg_index++;
			}
			else
			{
				// Unnamed ellipsis argument; make a fill entry for it and move on
				if (child->cached_literal_value_)
				{
					// if a cached literal value is available for the node, we don't need to evaluate it at runtime, we can use the cached value forever
					p_call_signature->CheckArgument(child->cached_literal_value_.get(), sig_arg_index);
					no_fill_index.emplace_back((uint8_t)arg_buffer.size());
					arg_buffer.emplace_back(child->cached_literal_value_);
					filled_explicitly.emplace_back(true);
				}
				else
				{
					fill_info.emplace_back(child, arg_buffer.size(), sig_arg_index, false, kEidosValueMaskAny);
					arg_buffer.emplace_back(nullptr);
					filled_explicitly.emplace_back(true);
				}
				continue;
			}
		}
		
		if (sig_arg_index < sig_arg_count)
		{
			bool sig_arg_is_singleton = !!(p_call_signature->arg_masks_[sig_arg_index] & kEidosValueMaskSingleton);
			EidosValueMask sig_arg_type_mask = p_call_signature->arg_masks_[sig_arg_index];
			
			if (!is_named_argument)
			{
				// We have a non-named argument; it will go into the next argument slot from the signature
				if (had_named_argument)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): unnamed argument may not follow after named arguments; once named arguments begin, all arguments must be named arguments (or ellipsis arguments)." << EidosTerminate(nullptr);
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
						break;
					
					// An ellipsis argument may be omitted; it is optional, but gets no default value
					if (p_call_signature->arg_name_IDs_[sig_arg_index] != gEidosID_ELLIPSIS)
					{
						if (!(sig_arg_type_mask & kEidosValueMaskOptional))
						{
							const std::string &named_arg = named_arg_name_node->token_->token_string_;
							
							// We have special error-handling for apply() because sapply() used to be named apply() and we want to steer users to the new call
							if ((p_call_signature->call_name_ == "apply") && ((named_arg == "lambdaSource") || (named_arg == "simplify")))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' skipped over required argument '" << p_call_signature->arg_names_[sig_arg_index] << "'." << std::endl << "NOTE: The apply() function was renamed sapply() in Eidos 1.6, and a new function named apply() has been added; you may need to change this call to be a call to sapply() instead." << EidosTerminate(nullptr);
							
							// Special error-handling for defineSpatialMap() because its gridSize parameter was removed in SLiM 3.5
							if ((p_call_signature->call_name_ == "defineSpatialMap") && (named_arg == "gridSize"))
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' skipped over required argument '" << p_call_signature->arg_names_[sig_arg_index] << "'." << std::endl << "NOTE: The defineSpatialMap() method was changed in SLiM 3.5, breaking backward compatibility.  Please see the manual for guidance on updating your code." << EidosTerminate(nullptr);
							
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' skipped over required argument '" << p_call_signature->arg_names_[sig_arg_index] << "'; all required arguments must be supplied in order." << EidosTerminate(nullptr);
						}
						
						EidosValue_SP default_value = p_call_signature->arg_defaults_[sig_arg_index];
						
#if DEBUG
						if (!default_value)
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): (internal error) missing default value for optional argument." << EidosTerminate(nullptr);
#endif
						
						no_fill_index.emplace_back((uint8_t)arg_buffer.size());
						arg_buffer.emplace_back(default_value);
						filled_explicitly.emplace_back(false);
					}
					
					// Move to the next signature argument; if we have run out of them, then treat this argument as illegal
					sig_arg_index++;
					if (sig_arg_index == sig_arg_count)
					{
						const std::string &named_arg = named_arg_name_node->token_->token_string_;
						
						// Special error-handling for defineSpatialMap() because its gridSize parameter was removed in SLiM 3.5
						if ((p_call_signature->call_name_ == "defineSpatialMap") && ((named_arg == "values") || (named_arg == "interpolate")))
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' could not be matched." << std::endl << "NOTE: The defineSpatialMap() method was changed in SLiM 3.5, breaking backward compatibility.  Please see the manual for guidance on updating your code." << EidosTerminate(nullptr);
						
						// Special error-handling for initializeSLiMOptions() because its mutationRuns parameter changed to doMutationRunExperiments in SLiM 5
						if ((p_call_signature->call_name_ == "initializeSLiMOptions") && (named_arg == "mutationRuns"))
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' could not be matched." << std::endl << "NOTE: The mutationRuns parameter to initializeSLiMOptions() was changed in SLiM 5, breaking backward compatibility.  Please see the manual for guidance on updating your code." << EidosTerminate(nullptr);
						
						// Try to diagnose the exact nature of the error, to give a better error message
						const std::vector<EidosGlobalStringID> &arg_list = p_call_signature->arg_name_IDs_;
						auto arg_iter = std::find(arg_list.begin(), arg_list.end(), named_arg_nameID);
						
						if (arg_iter != arg_list.end())
						{
							// it is a legit parameter name; so either it was supplied twice, or out of order
							// we distinguish those cases because if it was supplied twice, it was filled explicitly;
							// if it was supplied out of order, then its value was filled non-explicitly, by a default
							size_t named_arg_index = std::distance(arg_list.begin(), arg_iter);
							bool named_arg_was_filled_explicitly = filled_explicitly[named_arg_index];
							
							if (named_arg_was_filled_explicitly)
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' was supplied twice in the argument list; each parameter may be supplied only once." << EidosTerminate(nullptr);
							else
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' was supplied out of order; another argument that comes after it in the parameter list was supplied before it.  Eidos requires that parameters be supplied in the order they are given." << EidosTerminate(nullptr);
						}
						else
						{
							// this parameter name does not exist in the call signature
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): named argument '" << named_arg << "' could not be matched, because there is no parameter with that name in the call signature." << EidosTerminate(nullptr);
						}
					}
				}
				while (true);
				
				had_named_argument = true;
			}
			
			// This argument will need to be evaluated at dispatch time; place a nullptr in the arguments buffer for now, and create a fill info entry for it
			if (child->cached_literal_value_)
			{
				// if a cached literal value is available for the node, we don't need to evaluate it at runtime, we can use the cached value forever
				p_call_signature->CheckArgument(child->cached_literal_value_.get(), sig_arg_index);
				no_fill_index.emplace_back((uint8_t)arg_buffer.size());
				arg_buffer.emplace_back(child->cached_literal_value_);
				filled_explicitly.emplace_back(true);
			}
			else
			{
				fill_info.emplace_back(child, arg_buffer.size(), sig_arg_index, sig_arg_is_singleton, sig_arg_type_mask & kEidosValueMaskFlagStrip);
				arg_buffer.emplace_back(nullptr);
				filled_explicitly.emplace_back(true);
			}
			
			// Move to the next signature argument, and check whether it is an ellipsis
			sig_arg_index++;
			if (sig_arg_index < sig_arg_count)
				in_ellipsis = (p_call_signature->arg_name_IDs_[sig_arg_index] == gEidosID_ELLIPSIS);
		}
		else
		{
			// this argument is illegal; check whether it is a named argument that is out of order, or just an excess argument
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): argument '" << named_arg << "' to " << p_call_signature->call_name_ << "() could not be matched; probably supplied more than once or supplied out of order (note that arguments must be supplied in order)." << EidosTerminate(nullptr);
				}
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): unrecognized named argument '" << named_arg << "' to " << p_call_signature->call_name_ << "()." << EidosTerminate(nullptr);
			}
			else
			{
				if (had_named_argument)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): too many arguments supplied to " << p_call_signature->call_name_ << "() (after handling named arguments, which might have filled in default values for previous arguments)." << EidosTerminate(nullptr);
				else
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): too many arguments supplied to " << p_call_signature->call_name_ << "()." << EidosTerminate(nullptr);
			}
		}
	}
	
	// Handle any remaining arguments in the signature
	while (sig_arg_index < sig_arg_count)
	{
		// An ellipsis argument may be omitted; it is optional, but gets no default value
		if (p_call_signature->arg_name_IDs_[sig_arg_index] != gEidosID_ELLIPSIS)
		{
			EidosValueMask arg_mask = p_call_signature->arg_masks_[sig_arg_index];
			
			if (!(arg_mask & kEidosValueMaskOptional))
			{
				// We have special error-handling for apply() because sapply() used to be named apply() and we want to steer users to the new call
				if ((p_call_signature->call_name_ == "apply") && (p_call_signature->arg_names_[sig_arg_index] == "lambdaSource"))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): missing required argument '" << p_call_signature->arg_names_[sig_arg_index] << "'." << std::endl << "NOTE: The apply() function was renamed sapply() in Eidos 1.6, and a new function named apply() has been added; you may need to change this call to be a call to sapply() instead." << EidosTerminate(nullptr);
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): missing required argument '" << p_call_signature->arg_names_[sig_arg_index] << "'." << EidosTerminate(nullptr);
			}
			
			EidosValue_SP default_value = p_call_signature->arg_defaults_[sig_arg_index];
			
#if DEBUG
			if (!default_value)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::_ProcessArgumentList): (internal error) missing default value for optional argument." << EidosTerminate(nullptr);
#endif
			
			no_fill_index.emplace_back((uint8_t)arg_buffer.size());
			arg_buffer.emplace_back(default_value);
			filled_explicitly.emplace_back(true);
		}
		
		sig_arg_index++;
	}
}

EidosValue_SP EidosInterpreter::DispatchUserDefinedFunction(const EidosFunctionSignature &p_function_signature, const std::vector<EidosValue_SP> &p_arguments)
{
#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	EidosDebugPointIndent indenter;
	
	if (debug_points_ && debug_points_->set.size() && (p_function_signature.user_definition_line_ != -1) &&
		(debug_points_->set.find(p_function_signature.user_definition_line_) != debug_points_->set.end()))
	{
		std::ostream &output_stream = ErrorOutputStream();
		
		output_stream << EidosDebugPointIndent::Indent() << "#DEBUG FUNCTION (line " << (p_function_signature.user_definition_line_ + 1) << eidos_context_->DebugPointInfo() << "): function " << p_function_signature.call_name_ << "() called with arguments:" << std::endl;
		
		indenter.indent(2);
		
		for (size_t arg_index = 0; arg_index < p_arguments.size(); ++arg_index)
		{
			output_stream << EidosDebugPointIndent::Indent() << p_function_signature.arg_names_[arg_index] << " == ";
			p_arguments[arg_index]->PrintStructure(output_stream, 5);
			output_stream << std::endl;
		}
		
		indenter.indent(2);
	}
#endif
	
	EidosValue_SP result_SP(nullptr);
	
	// We need to add a new variables symbol table on to the top of the symbol table stack, for parameters and local variables.
	EidosSymbolTable new_symbols(EidosSymbolTableType::kLocalVariablesTable, global_symbols_);
	
	// Set up variables for the function's parameters; they have already been type-checked and had default
	// values substituted and so forth, by the Eidos function call dispatch code.
	if (p_function_signature.arg_name_IDs_.size() != p_arguments.size())
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::DispatchUserDefinedFunction): (internal error) parameter count does not match argument count." << EidosTerminate(nullptr);
	
	for (size_t arg_index = 0; arg_index < p_arguments.size(); ++arg_index)
		new_symbols.SetValueForSymbol(p_function_signature.arg_name_IDs_[arg_index], p_arguments[arg_index]);
	
	// Save off the current error context for restoration later, if no error occurs.
	EidosErrorContext error_context_save = gEidosErrorContext;
	
	// Set up to indicate that we're now going to be executing from our own private script object.  That
	// private script object might or might not be derived from the user script, so errors that occur
	// within it might or might not be reportable as positions in the user script; see below.
	gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, p_function_signature.body_script_};
	
	// Execute inside try/catch so we can handle errors well
	try
	{
		EidosInterpreter interpreter(*p_function_signature.body_script_, new_symbols, function_map_, Context(), execution_output_, error_output_);
		
		// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
		// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
		// bugs that would otherwise not manifest.
		result_SP = interpreter.EvaluateInterpreterBlock(false, false);	// don't print output, don't return last statement value
	}
	catch (...)
	{
		// If we're executing a script that does not have a position in the user's script, then the best
		// we can do is to report the call to the function as the error position.  If the script knows
		// its position in the user's script, then we don't need to make that correction.  We only make
		// this correction for gEidosTerminateThrows==true because that is the case when errors are
		// shown in the GUI.  When at the command line, showing an error position inside an unmoored
		// script is perfectly fine, and preferable to obscuring that position.  (We could show *both*
		// pieces of information, in fact, but that is a project for another day.)
		if (gEidosTerminateThrows && (p_function_signature.body_script_->UserScriptCharOffset() == -1))
		{
			// In some cases, such as if the error occurred in a derived user-defined function, we can
			// actually get a user script error context at this point, and don't need to intervene.
			if (!gEidosErrorContext.currentScript || (gEidosErrorContext.currentScript->UserScriptUTF16Offset() == -1))
			{
				gEidosErrorContext = error_context_save;
				TranslateErrorContextToUserScript("DispatchUserDefinedFunction()");
			}
		}
		
		throw;
	}
	
	// Restore the normal error context in the event that no exception occurring within the function
	gEidosErrorContext = error_context_save;
	
	return result_SP;
}

#ifdef SLIMGUI
void EidosInterpreter::_LogCallArguments(const EidosCallSignature *call_signature, std::vector<EidosValue_SP> *argument_buffer)
{
	// called by EidosInterpreter::Evaluate_Call() to do debug point logging of call arguments
	std::ostream &output_stream = ErrorOutputStream();
	
#if 0
	// log out arguments with positional numbers
	for (size_t argument_index = 0; argument_index < argument_buffer->size(); ++argument_index)
	{
		output_stream << EidosDebugPointIndent::Indent() << "[" << argument_index << "] == ";
		(*argument_buffer)[argument_index]->PrintStructure(output_stream, 5);
		output_stream << std::endl;
	}
#else
	// log out arguments with symbols; complicated because of ellipsis arguments
	int sig_ellipsis_index = -1, ellipsis_arg_count = 0, ellipsis_start = INT_MAX, ellipsis_end = INT_MAX;
	int signature_arg_count = (int)call_signature->arg_name_IDs_.size();
	int buffer_arg_count = (int)argument_buffer->size();
	
	if (call_signature->has_ellipsis_)
	{
		for (int sig_index = 0; sig_index < signature_arg_count; sig_index++)
			if (call_signature->arg_name_IDs_[sig_index] == gEidosID_ELLIPSIS)
			{
				sig_ellipsis_index = sig_index;
				break;
			}
		
		if (sig_ellipsis_index != -1)
		{
			// found the ellipsis; figure out the details
			ellipsis_arg_count = buffer_arg_count - signature_arg_count + 1;	// + 1 because the ellipsis gets one entry in the sig
			ellipsis_start = sig_ellipsis_index;
			ellipsis_end = sig_ellipsis_index + ellipsis_arg_count - 1;
		}
	}
	
	for (int buffer_arg_index = 0; buffer_arg_index < buffer_arg_count; ++buffer_arg_index)
	{
		int signature_arg_index;
		
		if (buffer_arg_index < ellipsis_start)
			signature_arg_index = buffer_arg_index;
		else if ((buffer_arg_index >= ellipsis_start) && (buffer_arg_index <= ellipsis_end))
			signature_arg_index = sig_ellipsis_index;
		else
			signature_arg_index = buffer_arg_index - ellipsis_arg_count + 1;	// + 1 because the ellipsis gets one entry in the sig
		
		output_stream << EidosDebugPointIndent::Indent() << call_signature->arg_names_[signature_arg_index] << " == ";
		(*argument_buffer)[buffer_arg_index]->PrintStructure(output_stream, 5);
		output_stream << std::endl;
	}
#endif
}
#endif

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
			{
				// This raises a special exception if the function name is undefined.
				// This facility is used by Community::_EvaluateTickRangeNode() for tolerant evaluation.
				if (use_custom_undefined_function_raise_)
					throw SLiMUndefinedFunctionException();
				
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): unrecognized function name " << *function_name << ".";
				if (Context() == nullptr)
					EIDOS_TERMINATION << "  This may be because the current Eidos context (such as the current SLiM simulation) is invalid.";
				EIDOS_TERMINATION << EidosTerminate(call_identifier_token);
			}
			
			function_signature = signature_iter->second.get();
		}
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = PushErrorPositionFromToken(call_identifier_token);
		
		// Argument processing
		std::vector<EidosValue_SP> *argument_buffer = _ProcessArgumentList(p_node, function_signature);
		
#if DEBUG_POINTS_ENABLED
		// SLiMgui debugging point
		EidosDebugPointIndent indenter;
		
		if (debug_points_ && debug_points_->set.size() && (call_identifier_token->token_line_ != -1) &&
			(debug_points_->set.find(call_identifier_token->token_line_) != debug_points_->set.end()))
		{
			std::ostream &output_stream = ErrorOutputStream();
			
			output_stream << EidosDebugPointIndent::Indent() << "#DEBUG CALL (line " << (call_identifier_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): call to function " <<
				*function_name << "() with arguments:" << std::endl;
			indenter.indent(2);
			_LogCallArguments(function_signature, argument_buffer);
			indenter.indent(2);
		}
#endif
		
		try {
			if (function_signature->internal_function_)
			{
				result_SP = function_signature->internal_function_(*argument_buffer, *this);
			}
			else if (function_signature->body_script_)
			{
				result_SP = DispatchUserDefinedFunction(*function_signature, *argument_buffer);
			}
			else if (!function_signature->delegate_name_.empty())
			{
				if (eidos_context_)
					result_SP = eidos_context_->ContextDefinedFunctionDispatch(*function_name, *argument_buffer, *this);
				else
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): function " << function_name << " is defined by the Context, but the Context is not defined." << EidosTerminate(nullptr);
			}
			else
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): unbound function " << *function_name << "." << EidosTerminate(call_identifier_token);
		} catch (...) {
			// BCH 4/15/2025: Adding the try/catch and _DeprocessArgumentList() call here to fix a leak that
			// occurred for recursive function calls, for which argument_buffer is heap-allocated.
			_DeprocessArgumentList(p_node, argument_buffer);
			throw;
		}
		
		_DeprocessArgumentList(p_node, argument_buffer);
		
		// If the code above supplied no return value, raise when in debug.  Not in debug, we crash.
#if DEBUG
		if (!result_SP)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): (internal error) function " << *function_name << " returned nullptr." << EidosTerminate(call_identifier_token);
#endif
		
#if DEBUG_POINTS_ENABLED
		// SLiMgui debugging point
		if (debug_points_ && debug_points_->set.size() && (call_identifier_token->token_line_ != -1) &&
			(debug_points_->set.find(call_identifier_token->token_line_) != debug_points_->set.end()))
		{
			std::ostream &output_stream = ErrorOutputStream();
			
			indenter.outdent();
			output_stream << EidosDebugPointIndent::Indent() << "#DEBUG CALL (line " << (call_identifier_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): function " <<
				*function_name << "() return: ";
			if (result_SP->Count() <= 1)
			{
				result_SP->PrintStructure(output_stream, 1);
			}
			else {
				// print multiple values on a new line, with indent
				result_SP->PrintStructure(output_stream, 0);
				output_stream << std::endl;
				indenter.indent(2);
				result_SP->Print(output_stream, EidosDebugPointIndent::Indent());
				indenter.outdent(2);
			}
			output_stream << std::endl;
		}
#endif
		
		// Check the return value against the signature
		function_signature->CheckReturn(*result_SP);
		
		// Forget the function token, since it is not responsible for any future errors
		RestoreErrorPosition(error_pos_save);
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
		EidosValue_Object_SP method_object = static_pointer_cast<EidosValue_Object>(first_child_value);	// guaranteed by the Type() call above
		
		// Look up the method signature; this could be cached in the tree, probably, since we guarantee that method signatures are unique by name
		const EidosMethodSignature *method_signature = method_object->Class()->SignatureForMethod(method_id);
		
		if (!method_signature)
		{
			// Give more helpful error messages for deprecated methods
			if (call_identifier_token->token_string_ == "method")
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): method " << EidosStringRegistry::StringForGlobalStringID(method_id) << "() is not defined on object element type " << method_object->ElementType() << ".  Note that method() has been renamed to methodSignature()." << EidosTerminate(call_identifier_token);
			else if (call_identifier_token->token_string_ == "property")
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): method " << EidosStringRegistry::StringForGlobalStringID(method_id) << "() is not defined on object element type " << method_object->ElementType() << ".  Note that property() has been renamed to propertySignature()." << EidosTerminate(call_identifier_token);
			else
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Call): method " << EidosStringRegistry::StringForGlobalStringID(method_id) << "() is not defined on object element type " << method_object->ElementType() << "." << EidosTerminate(call_identifier_token);
		}
		
		// If an error occurs inside a function or method call, we want to highlight the call
		EidosErrorPosition error_pos_save = PushErrorPositionFromToken(call_identifier_token);
		
		// Argument processing
		std::vector<EidosValue_SP> *argument_buffer = _ProcessArgumentList(p_node, method_signature);
		
#if DEBUG_POINTS_ENABLED
		// SLiMgui debugging point
		EidosDebugPointIndent indenter;
		
		if (debug_points_ && debug_points_->set.size() && (call_identifier_token->token_line_ != -1) &&
			(debug_points_->set.find(call_identifier_token->token_line_) != debug_points_->set.end()))
		{
			std::ostream &output_stream = ErrorOutputStream();
			
			output_stream << EidosDebugPointIndent::Indent() << "#DEBUG CALL (line " << (call_identifier_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): call to method " <<
				EidosStringRegistry::StringForGlobalStringID(method_id) << "() with arguments:" << std::endl;
			indenter.indent(2);
			_LogCallArguments(method_signature, argument_buffer);
			indenter.indent(2);
		}
#endif
		
		// If the method is a class method, dispatch it to the class object
		if (method_signature->is_class_method)
		{
			// Note that starting in Eidos 1.1 we pass method_object to ExecuteClassMethod(), to allow class methods to
			// act as non-multicasting methods that operate on the whole target vector.  BCH 11 June 2016
			result_SP = method_object->Class()->ExecuteClassMethod(method_id, method_object.get(), *argument_buffer, *this);
			
			method_signature->CheckReturn(*result_SP);
		}
		else
		{
			result_SP = method_object->ExecuteMethodCall(method_id, (EidosInstanceMethodSignature *)method_signature, *argument_buffer, *this);
		}
		
		_DeprocessArgumentList(p_node, argument_buffer);
		
#if DEBUG_POINTS_ENABLED
		// SLiMgui debugging point
		if (debug_points_ && debug_points_->set.size() && (call_identifier_token->token_line_ != -1) &&
			(debug_points_->set.find(call_identifier_token->token_line_) != debug_points_->set.end()))
		{
			std::ostream &output_stream = ErrorOutputStream();
			
			indenter.outdent();
			output_stream << EidosDebugPointIndent::Indent() << "#DEBUG CALL (line " << (call_identifier_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): method " <<
				EidosStringRegistry::StringForGlobalStringID(method_id) << "() return: ";
			if (result_SP->Count() <= 1)
			{
				result_SP->PrintStructure(output_stream, 1);
			}
			else {
				// print multiple values on a new line, with indent
				result_SP->PrintStructure(output_stream, 0);
				output_stream << std::endl;
				indenter.indent(2);
				result_SP->Print(output_stream, EidosDebugPointIndent::Indent());
				indenter.outdent(2);
			}
			output_stream << std::endl;
		}
#endif
		
		// Forget the function token, since it is not responsible for any future errors
		RestoreErrorPosition(error_pos_save);
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
			subset_indices.emplace_back(gStaticEidosValueNULL);
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
				int subset_index = (int)child_value->IntData()[0];
				
				result_SP = first_child_value->GetValueAtIndex(subset_index, operator_token);
				
				EIDOS_EXIT_EXECUTION_LOG("Evaluate_Subset()");
				return result_SP;
			}
			
			if (child_type == EidosValueType::kValueFloat)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): it is no longer legal to subset with float indices; use asInteger() to cast the indices to integer." << EidosTerminate(operator_token);
			if ((child_type != EidosValueType::kValueInt) && (child_type != EidosValueType::kValueLogical) && (child_type != EidosValueType::kValueNULL))
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
		
		result_SP = SubsetEidosValue(first_child_value.get(), second_child_value.get(), operator_token, /* p_raise_range_errors */ true);
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
					indices.emplace_back(dim_index);
			}
			else if (subset_type == EidosValueType::kValueLogical)
			{
				// We have a logical subset, which must equal the dimension size and gets translated directly into inclusion_indices
				if (subset_count != dim_size)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): the '[]' operator requires that the size() of a logical index operand must match the corresponding dimension of the indexed operand." << EidosTerminate(operator_token);
				
				const eidos_logical_t *logical_index_data = subset_value->LogicalData();
				
				for (int dim_index = 0; dim_index < dim_size; dim_index++)
					if (logical_index_data[dim_index])
						indices.emplace_back(dim_index);
			}
			else
			{
				// We have an integer subset, which selects indices within inclusion_indices
				for (int index_index = 0; index_index < subset_count; index_index++)
				{
					int64_t index_value = subset_value->IntAtIndex_NOCAST(index_index, operator_token);
					
					if ((index_value < 0) || (index_value >= dim_size))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Subset): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(operator_token);
					else
						indices.emplace_back(index_value);
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
#if DEBUG || defined(EIDOS_GUI)
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
		EidosErrorPosition error_pos_save = PushErrorPositionFromToken(second_child_token);
		
		// We offload the actual work to GetPropertyOfElements() to keep things simple here
		result_SP = static_cast<EidosValue_Object *>(first_child_value.get())->GetPropertyOfElements(second_child_node->cached_stringID_);
		
		// Forget the function token, since it is not responsible for any future errors
		RestoreErrorPosition(error_pos_save);
		
		EIDOS_EXIT_EXECUTION_LOG("Evaluate_MemberRef()");
		return result_SP;
	}
#endif
	
	// When not logging execution, we can use a fast code path that assumes no logging
	EidosToken *operator_token = p_node->token_;
	EidosValue_SP result_SP;
	
	const EidosASTNode *first_child_node = p_node->children_[0];
	
	// use_custom_undefined_identifier_raise_ is tested here because when it is set, we want EidosInterpreter::Evaluate_Identifier() to be
	// called, to receive the special treatment of that flag that that method implements; Evaluate_Identifier_RAW() doesn't do it.
	if ((first_child_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && !use_custom_undefined_identifier_raise_)
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
		EidosErrorPosition error_pos_save = PushErrorPositionFromToken(second_child_token);
		
		// We offload the actual work to GetPropertyOfElements() to keep things simple here
		result_SP = static_cast<EidosValue_Object *>(first_child_value)->GetPropertyOfElements(second_child_node->cached_stringID_);
		
		// Forget the function token, since it is not responsible for any future errors
		RestoreErrorPosition(error_pos_save);
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
		EidosErrorPosition error_pos_save = PushErrorPositionFromToken(second_child_token);
		
		// We offload the actual work to GetPropertyOfElements() to keep things simple here
		result_SP = static_cast<EidosValue_Object *>(first_child_value.get())->GetPropertyOfElements(second_child_node->cached_stringID_);
		
		// Forget the function token, since it is not responsible for any future errors
		RestoreErrorPosition(error_pos_save);
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
				const std::string &&first_string = (first_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : first_child_value->StringAtIndex_CAST(0, operator_token);
				const std::string &&second_string = (second_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : second_child_value->StringAtIndex_CAST(0, operator_token);
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(first_string + second_string));
			}
			else
			{
				if (first_child_count == second_child_count)
				{
					EidosValue_String_SP string_result_SP = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String());
					EidosValue_String *string_result = string_result_SP->Reserve(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						string_result->PushString(first_child_value->StringAtIndex_CAST(value_index, operator_token) + second_child_value->StringAtIndex_CAST(value_index, operator_token));
					
					result_SP = string_result_SP;
				}
				else if (first_child_count == 1)
				{
					std::string singleton_string = (first_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : first_child_value->StringAtIndex_CAST(0, operator_token);
					EidosValue_String_SP string_result_SP = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String());
					EidosValue_String *string_result = string_result_SP->Reserve(second_child_count);
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						string_result->PushString(singleton_string + second_child_value->StringAtIndex_CAST(value_index, operator_token));
					
					result_SP = string_result_SP;
				}
				else if (second_child_count == 1)
				{
					std::string singleton_string = (second_child_type == EidosValueType::kValueNULL) ? gEidosStr_NULL : second_child_value->StringAtIndex_CAST(0, operator_token);
					EidosValue_String_SP string_result_SP = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String());
					EidosValue_String *string_result = string_result_SP->Reserve(first_child_count);
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						string_result->PushString(first_child_value->StringAtIndex_CAST(value_index, operator_token) + singleton_string);
					
					result_SP = string_result_SP;
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
				const int64_t *first_child_data = first_child_value->IntData();
				const int64_t *second_child_data = second_child_value->IntData();
				EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
				EidosValue_Int *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(first_child_value->IntAtIndex_NOCAST(value_index, operator_token) + second_child_value->IntAtIndex_NOCAST(value_index, operator_token));
					
					int64_t first_operand = first_child_data[value_index];
					int64_t second_operand = second_child_data[value_index];
					int64_t add_result;
					bool overflow = Eidos_add_overflow(first_operand, second_operand, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(add_result, value_index);
				}
				
				result_SP = int_result_SP;
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex_NOCAST(0, operator_token);
				const int64_t *second_child_data = second_child_value->IntData();
				EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
				EidosValue_Int *int_result = int_result_SP->resize_no_initialize(second_child_count);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(singleton_int + second_child_value->IntAtIndex_NOCAST(value_index, operator_token));
					
					int64_t second_operand = second_child_data[value_index];
					int64_t add_result;
					bool overflow = Eidos_add_overflow(singleton_int, second_operand, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(add_result, value_index);
				}
				
				result_SP = int_result_SP;
			}
			else if (second_child_count == 1)
			{
				const int64_t *first_child_data = first_child_value->IntData();
				int64_t singleton_int = second_child_value->IntAtIndex_NOCAST(0, operator_token);
				EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
				EidosValue_Int *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->PushInt(first_child_value->IntAtIndex_NOCAST(value_index, operator_token) + singleton_int);
					
					int64_t first_operand = first_child_data[value_index];
					int64_t add_result;
					bool overflow = Eidos_add_overflow(first_operand, singleton_int, &add_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Plus): integer addition overflow with the binary '+' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(add_result, value_index);
				}
				
				result_SP = int_result_SP;
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
				EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
				EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					const double *first_child_data = first_child_value->FloatData();
					const double *second_child_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] + second_child_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
				{
					const double *first_child_data = first_child_value->FloatData();
					const int64_t *second_child_data = second_child_value->IntData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] + second_child_data[value_index], value_index);
				}
				else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
				{
					const int64_t *first_child_data = first_child_value->IntData();
					const double *second_child_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] + second_child_data[value_index], value_index);
				}
				
				result_SP = float_result_SP;
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->NumericAtIndex_NOCAST(0, operator_token);
				EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
				EidosValue_Float *float_result = float_result_SP->resize_no_initialize(second_child_count);
				
				if (second_child_type == EidosValueType::kValueInt)
				{
					const int64_t *second_child_data = second_child_value->IntData();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float + second_child_data[value_index], value_index);
				}
				else	// (second_child_type == EidosValueType::kValueFloat)
				{
					const double *second_child_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float + second_child_data[value_index], value_index);
				}
				
				result_SP = float_result_SP;
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->NumericAtIndex_NOCAST(0, operator_token);
				EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
				EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				if (first_child_type == EidosValueType::kValueInt)
				{
					const int64_t *first_child_data = first_child_value->IntData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] + singleton_float, value_index);
				}
				else	// (first_child_type == EidosValueType::kValueFloat)
				{
					const double *first_child_data = first_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] + singleton_float, value_index);
				}
				
				result_SP = float_result_SP;
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
			const int64_t *first_child_data = first_child_value->IntData();
			EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
			EidosValue_Int *int_result = int_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				// This is an overflow-safe version of:
				//int_result->set_int_no_check(-first_child_value->IntAtIndex_NOCAST(value_index, operator_token), value_index);
				
				int64_t operand = first_child_data[value_index];
				int64_t subtract_result;
				bool overflow = Eidos_sub_overflow((int64_t)0, operand, &subtract_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer negation overflow with the unary '-' operator." << EidosTerminate(operator_token);
				
				int_result->set_int_no_check(subtract_result, value_index);
			}
			
			result_SP = int_result_SP;
		}
		else
		{
			const double *first_child_data = first_child_value->FloatData();
			EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
			EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(-first_child_data[value_index], value_index);
			
			result_SP = float_result_SP;
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
				const int64_t *first_child_data = first_child_value->IntData();
				const int64_t *second_child_data = second_child_value->IntData();
				EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
				EidosValue_Int *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(first_child_value->IntAtIndex_NOCAST(value_index, operator_token) - second_child_value->IntAtIndex_NOCAST(value_index, operator_token));
					
					int64_t first_operand = first_child_data[value_index];
					int64_t second_operand = second_child_data[value_index];
					int64_t subtract_result;
					bool overflow = Eidos_sub_overflow(first_operand, second_operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(subtract_result, value_index);
				}
				
				result_SP = int_result_SP;
			}
			else if (first_child_count == 1)
			{
				int64_t singleton_int = first_child_value->IntAtIndex_NOCAST(0, operator_token);
				const int64_t *second_child_data = second_child_value->IntData();
				EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
				EidosValue_Int *int_result = int_result_SP->resize_no_initialize(second_child_count);
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(singleton_int - second_child_value->IntAtIndex_NOCAST(value_index, operator_token));
					
					int64_t second_operand = second_child_data[value_index];
					int64_t subtract_result;
					bool overflow = Eidos_sub_overflow(singleton_int, second_operand, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(subtract_result, value_index);
				}
				
				result_SP = int_result_SP;
			}
			else if (second_child_count == 1)
			{
				const int64_t *first_child_data = first_child_value->IntData();
				int64_t singleton_int = second_child_value->IntAtIndex_NOCAST(0, operator_token);
				EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
				EidosValue_Int *int_result = int_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					// This is an overflow-safe version of:
					//int_result->set_int_no_check(first_child_value->IntAtIndex_NOCAST(value_index, operator_token) - singleton_int);
					
					int64_t first_operand = first_child_data[value_index];
					int64_t subtract_result;
					bool overflow = Eidos_sub_overflow(first_operand, singleton_int, &subtract_result);
					
					if (overflow)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Minus): integer subtraction overflow with the binary '-' operator." << EidosTerminate(operator_token);
					
					int_result->set_int_no_check(subtract_result, value_index);
				}
				
				result_SP = int_result_SP;
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
				EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
				EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					const double *first_child_data = first_child_value->FloatData();
					const double *second_child_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] - second_child_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
				{
					const double *first_child_data = first_child_value->FloatData();
					const int64_t *second_child_data = second_child_value->IntData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] - second_child_data[value_index], value_index);
				}
				else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
				{
					const int64_t *first_child_data = first_child_value->IntData();
					const double *second_child_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] - second_child_data[value_index], value_index);
				}
				
				result_SP = float_result_SP;
			}
			else if (first_child_count == 1)
			{
				double singleton_float = first_child_value->NumericAtIndex_NOCAST(0, operator_token);
				EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
				EidosValue_Float *float_result = float_result_SP->resize_no_initialize(second_child_count);
				
				if (second_child_type == EidosValueType::kValueInt)
				{
					const int64_t *second_child_data = second_child_value->IntData();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float - second_child_data[value_index], value_index);
				}
				else	// (second_child_type == EidosValueType::kValueFloat)
				{
					const double *second_child_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < second_child_count; ++value_index)
						float_result->set_float_no_check(singleton_float - second_child_data[value_index], value_index);
				}
				
				result_SP = float_result_SP;
			}
			else if (second_child_count == 1)
			{
				double singleton_float = second_child_value->NumericAtIndex_NOCAST(0, operator_token);
				EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
				EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
				
				if (first_child_type == EidosValueType::kValueInt)
				{
					const int64_t *first_child_data = first_child_value->IntData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] - singleton_float, value_index);
				}
				else	// (first_child_type == EidosValueType::kValueFloat)
				{
					const double *first_child_data = first_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						float_result->set_float_no_check(first_child_data[value_index] - singleton_float, value_index);
				}
				
				result_SP = float_result_SP;
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
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
		{
			const double *first_child_data = first_child_value->FloatData();
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
		{
			const double *first_child_data = first_child_value->FloatData();
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
		{
			const int64_t *first_child_data = first_child_value->IntData();
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			const int64_t *first_child_data = first_child_value->IntData();
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		
		result_SP = float_result_SP;
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->NumericAtIndex_NOCAST(0, operator_token);
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(second_child_count);
		
		if (second_child_type == EidosValueType::kValueInt)
		{
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(fmod(singleton_float, second_child_data[value_index]), value_index);
		}
		else	// (second_child_type == EidosValueType::kValueFloat)
		{
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(fmod(singleton_float, second_child_data[value_index]), value_index);
		}
		
		result_SP = float_result_SP;
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->NumericAtIndex_NOCAST(0, operator_token);
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if (first_child_type == EidosValueType::kValueInt)
		{
			const int64_t *first_child_data = first_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], singleton_float), value_index);
		}
		else	// (first_child_type == EidosValueType::kValueFloat)
		{
			const double *first_child_data = first_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(fmod(first_child_data[value_index], singleton_float), value_index);
		}
		
		result_SP = float_result_SP;
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
			const int64_t *first_child_data = first_child_value->IntData();
			const int64_t *second_child_data = second_child_value->IntData();
			EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
			EidosValue_Int *int_result = int_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				// This is an overflow-safe version of:
				//int_result->set_int_no_check(first_child_value->IntAtIndex_NOCAST(value_index, operator_token) * second_child_value->IntAtIndex_NOCAST(value_index, operator_token));
				
				int64_t first_operand = first_child_data[value_index];
				int64_t second_operand = second_child_data[value_index];
				int64_t multiply_result;
				bool overflow = Eidos_mul_overflow(first_operand, second_operand, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Mult): integer multiplication overflow with the '*' operator." << EidosTerminate(operator_token);
				
				int_result->set_int_no_check(multiply_result, value_index);
			}
			
			result_SP = int_result_SP;
		}
		else
		{
			EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
			EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
			
			if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
			{
				const double *first_child_data = first_child_value->FloatData();
				const double *second_child_data = second_child_value->FloatData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(first_child_data[value_index] * second_child_data[value_index], value_index);
			}
			else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
			{
				const double *first_child_data = first_child_value->FloatData();
				const int64_t *second_child_data = second_child_value->IntData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(first_child_data[value_index] * second_child_data[value_index], value_index);
			}
			else	// ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
			{
				const int64_t *first_child_data = first_child_value->IntData();
				const double *second_child_data = second_child_value->FloatData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					float_result->set_float_no_check(first_child_data[value_index] * second_child_data[value_index], value_index);
			}
			
			result_SP = float_result_SP;
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
			const int64_t *any_count_data = any_count_child->IntData();
			int64_t singleton_int = one_count_child->IntAtIndex_NOCAST(0, operator_token);
			EidosValue_Int_SP int_result_SP = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
			EidosValue_Int *int_result = int_result_SP->resize_no_initialize(any_count);
			
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
			
			result_SP = int_result_SP;
		}
		else if (any_type == EidosValueType::kValueInt)
		{
			const int64_t *any_count_data = any_count_child->IntData();
			double singleton_float = one_count_child->NumericAtIndex_NOCAST(0, operator_token);
			EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
			EidosValue_Float *float_result = float_result_SP->resize_no_initialize(any_count);
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				float_result->set_float_no_check(any_count_data[value_index] * singleton_float, value_index);
			
			result_SP = float_result_SP;
		}
		else	// (any_type == EidosValueType::kValueFloat)
		{
			const double *any_count_data = any_count_child->FloatData();
			double singleton_float = one_count_child->NumericAtIndex_NOCAST(0, operator_token);
			EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
			EidosValue_Float *float_result = float_result_SP->resize_no_initialize(any_count);
			
			for (int value_index = 0; value_index < any_count; ++value_index)
				float_result->set_float_no_check(any_count_data[value_index] * singleton_float, value_index);
			
			result_SP = float_result_SP;
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

#if defined(__clang__)
__attribute__((no_sanitize("float-divide-by-zero")))
__attribute__((no_sanitize("integer-divide-by-zero")))
#endif
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
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
		{
			const double *first_child_data = first_child_value->FloatData();
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / second_child_data[value_index], value_index);
		}
		else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
		{
			const double *first_child_data = first_child_value->FloatData();
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / second_child_data[value_index], value_index);
		}
		else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
		{
			const int64_t *first_child_data = first_child_value->IntData();
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / second_child_data[value_index], value_index);
		}
		else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			const int64_t *first_child_data = first_child_value->IntData();
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / (double)second_child_data[value_index], value_index);
		}
		
		result_SP = float_result_SP;
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->NumericAtIndex_NOCAST(0, operator_token);
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(second_child_count);
		
		if (second_child_type == EidosValueType::kValueInt)
		{
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(singleton_float / second_child_data[value_index], value_index);
		}
		else	// (second_child_type == EidosValueType::kValueFloat)
		{
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(singleton_float / second_child_data[value_index], value_index);
		}
		
		result_SP = float_result_SP;
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->NumericAtIndex_NOCAST(0, operator_token);
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if (first_child_type == EidosValueType::kValueInt)
		{
			const int64_t *first_child_data = first_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / singleton_float, value_index);
		}
		else	// (first_child_type == EidosValueType::kValueFloat)
		{
			const double *first_child_data = first_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(first_child_data[value_index] / singleton_float, value_index);
		}
		
		result_SP = float_result_SP;
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
		eidos_logical_t condition_logical = condition_result->LogicalAtIndex_CAST(0, operator_token);
		
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
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
		{
			const double *first_child_data = first_child_value->FloatData();
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		else if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueInt))
		{
			const double *first_child_data = first_child_value->FloatData();
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueFloat))
		{
			const int64_t *first_child_data = first_child_value->IntData();
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		else // ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
		{
			const int64_t *first_child_data = first_child_value->IntData();
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], second_child_data[value_index]), value_index);
		}
		
		result_SP = float_result_SP;
	}
	else if (first_child_count == 1)
	{
		double singleton_float = first_child_value->NumericAtIndex_NOCAST(0, operator_token);
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(second_child_count);
		
		if (second_child_type == EidosValueType::kValueInt)
		{
			const int64_t *second_child_data = second_child_value->IntData();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(pow(singleton_float, second_child_data[value_index]), value_index);
		}
		else	// (second_child_type == EidosValueType::kValueFloat)
		{
			const double *second_child_data = second_child_value->FloatData();
			
			for (int value_index = 0; value_index < second_child_count; ++value_index)
				float_result->set_float_no_check(pow(singleton_float, second_child_data[value_index]), value_index);
		}
		
		result_SP = float_result_SP;
	}
	else if (second_child_count == 1)
	{
		double singleton_float = second_child_value->NumericAtIndex_NOCAST(0, operator_token);
		EidosValue_Float_SP float_result_SP = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		EidosValue_Float *float_result = float_result_SP->resize_no_initialize(first_child_count);
		
		if (first_child_type == EidosValueType::kValueInt)
		{
			const int64_t *first_child_data = first_child_value->IntData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], singleton_float), value_index);
		}
		else	// (first_child_type == EidosValueType::kValueFloat)
		{
			const double *first_child_data = first_child_value->FloatData();
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
				float_result->set_float_no_check(pow(first_child_data[value_index], singleton_float), value_index);
		}
		
		result_SP = float_result_SP;
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
					logical_result = child_result->LogicalAtIndex_CAST(0, operator_token);
					result_count = 1;
				}
				else if ((child_type == EidosValueType::kValueLogical) && (child_result->UseCount() == 1))
				{
					// child_result is a logical EidosValue owned only by us, so we can just take it over as our initial result
					result_SP = static_pointer_cast<EidosValue_Logical>(child_result);
					result_count = child_count;
				}
				else
				{
					// for other cases, we just clone child_result – but note that it may not be of type logical
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(child_count));
					result_count = child_count;
					
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->set_logical_no_check(child_result->LogicalAtIndex_CAST(value_index, operator_token), value_index);
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
					eidos_logical_t child_logical = child_result->LogicalAtIndex_CAST(0, operator_token);
					
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
						result_logical = result_SP->LogicalAtIndex_CAST(0, operator_token);
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
							result->set_logical_no_check(child_result->LogicalAtIndex_CAST(value_index, operator_token), value_index);
					else
						for (int value_index = 0; value_index < child_count; ++value_index)
							result->set_logical_no_check(false, value_index);
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is F we turn result to F
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (!child_result->LogicalAtIndex_CAST(value_index, operator_token))
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
					logical_result = child_result->LogicalAtIndex_CAST(0, operator_token);
					result_count = 1;
				}
				else if ((child_type == EidosValueType::kValueLogical) && (child_result->UseCount() == 1))
				{
					// child_result is a logical EidosValue owned only by us, so we can just take it over as our initial result
					result_SP = static_pointer_cast<EidosValue_Logical>(child_result);
					result_count = child_count;
				}
				else
				{
					// for other cases, we just clone child_result – but note that it may not be of type logical
					result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(child_count));
					result_count = child_count;
					
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < child_count; ++value_index)
						result->set_logical_no_check(child_result->LogicalAtIndex_CAST(value_index, operator_token), value_index);
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
					eidos_logical_t child_logical = child_result->LogicalAtIndex_CAST(0, operator_token);
					
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
						result_logical = result_SP->LogicalAtIndex_CAST(0, operator_token);
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
							result->set_logical_no_check(child_result->LogicalAtIndex_CAST(value_index, operator_token), value_index);
				}
				else
				{
					// result and child_result are both != 1 length, so we match them one to one, and if child_result is T we turn result to T
					EidosValue_Logical *result = result_SP.get();
					
					for (int value_index = 0; value_index < result_count; ++value_index)
						if (child_result->LogicalAtIndex_CAST(value_index, operator_token))
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
			result_SP = (first_child_value->LogicalAtIndex_CAST(0, operator_token) ? gStaticEidosValue_LogicalF : gStaticEidosValue_LogicalT);
		}
		else
		{
			// for other cases, we just create the negation of first_child_value – but note that it may not be of type logical
			result_SP = EidosValue_Logical_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(first_child_count));
			
			EidosValue_Logical *result = result_SP.get();
			
			if (first_child_type == EidosValueType::kValueLogical)
			{
				const eidos_logical_t *child_data = first_child_value->LogicalData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(!child_data[value_index], value_index);
			}
			else if (first_child_type == EidosValueType::kValueInt)
			{
				const int64_t *child_data = first_child_value->IntData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(child_data[value_index] == 0, value_index);
			}
			else if (first_child_type == EidosValueType::kValueString)
			{
				const std::string *child_vec = first_child_value->StringData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(child_vec[value_index].length() == 0, value_index);
			}
			else
			{
				// General case; hit by type float, since we don't want to duplicate the NAN check here
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					result->set_logical_no_check(!first_child_value->LogicalAtIndex_CAST(value_index, operator_token), value_index);
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
	
#ifdef SLIMGUI
	// When running under SLiMgui, skip the compound assignment optimization if debug points are set
	// This makes compound assignments emit debug logs, while keeping the optimization for testing and most usage
	if (debug_points_ && debug_points_->set.size())
		goto compoundAssignmentSkip;
#endif
	
	if (p_node->cached_compound_assignment_)
	{
		// if _OptimizeAssignments() set this flag, this assignment is of the form "x = x <operator> <number>",
		// where x is a simple identifier and the operator is one of +-/%*^; we try to optimize that case
		EidosASTNode *lvalue_node = p_node->children_[0];
		bool is_const, is_local;
		EidosValue_SP lvalue_SP = global_symbols_->GetValueOrRaiseForASTNode_IsConstIsLocal(lvalue_node, &is_const, &is_local);
		
		// Check for a constant value.  If either the EidosValue or the table it comes from is constant, there is an error.
		if (is_const || lvalue_SP->IsConstant() || lvalue_SP->IsIteratorVariable())
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): identifier '" << lvalue_node->token_->token_string_ << "' cannot be redefined because it is a constant." << EidosTerminate(p_node->token_);
		
		// Check for a non-local value (global, or at any higher scope).  This is an assignment where the
		// rvalue resolves to a non-local variable, but the assignment needs to go into a new local value;
		// see https://github.com/MesserLab/SLiM/issues/430.  This is legal but confusing, and the code
		// here mishandles it, so we skip to the general case, which handles it correctly.
		if (!is_local)
		{
			p_node->cached_compound_assignment_ = false;
			goto compoundAssignmentSkip;
		}
		
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
					int64_t operand2_value = cached_operand2->IntAtIndex_NOCAST(0, nullptr);
					int64_t *int_data = lvalue->IntData_Mutable();
					
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
			else if (lvalue_type == EidosValueType::kValueFloat)
			{
				// if the lvalue is a float, we do not require the rvalue to be a float also; the integer will promote to float seamlessly
				EidosASTNode *rvalue_node = p_node->children_[1];										// the operator node
				EidosValue *cached_operand2 = rvalue_node->children_[1]->cached_literal_value_.get();	// the numeric constant
				EidosTokenType compound_operator = rvalue_node->token_->token_type_;
				double operand2_value = cached_operand2->NumericAtIndex_NOCAST(0, nullptr);				// might be an int64_t and get converted
				double *float_data = lvalue->FloatData_Mutable();
				
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
				
				goto compoundAssignmentSuccess;
			}
		}
		
		// maybe we should flip our flag so we don't waste time trying this again for this node
		p_node->cached_compound_assignment_ = false;
		
		// and then we drop through to be handled normally by the standard assign operator code
	}
	else if (p_node->cached_append_assignment_)
	{
		// we have an assignment statement of the form x = c(x, y), where x is a simple identifier and y is any single expression node
		// as above, we will try to modify the value of x in place if we can, which should be safe in this context
		EidosASTNode *lvalue_node = p_node->children_[0];
		bool is_const, is_local;	// is_const is needed by the API but unused; the const case gets caught by the code below
		EidosValue_SP lvalue_SP = global_symbols_->GetValueOrRaiseForASTNode_IsConstIsLocal(lvalue_node, &is_const, &is_local);
		
		// Check for an lvalue that is a loop iterator, which cannot be changed
		if (lvalue_SP->IsIteratorVariable())
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Assign): identifier '" << lvalue_node->token_->token_string_ << "' cannot be redefined because it is a constant." << EidosTerminate(p_node->token_);
		
		// Check for a non-local value (global, or at any higher scope).  This is an assignment where the
		// rvalue resolves to a non-local variable, but the assignment needs to go into a new local value;
		// see https://github.com/MesserLab/SLiM/issues/430.  This is legal but confusing, and the code
		// here mishandles it, so we skip to the general case, which handles it correctly.
		if (!is_local)
		{
			p_node->cached_append_assignment_ = false;
			goto compoundAssignmentSkip;
		}
		
		EidosASTNode *call_node = p_node->children_[1];
		EidosASTNode *rvalue_node = call_node->children_[2];	// "c" is [0], "x" is [1], "y" is [2]
		EidosValue_SP rvalue_SP = FastEvaluateNode(rvalue_node);
		
		EidosValue_SP result_SP = AppendEidosValues(lvalue_SP, rvalue_SP);
		
		if (!result_SP)
		{
			// a nullptr return means the append was successful, so we're done
			goto compoundAssignmentSuccess;
		}
		else
		{
			// a non-nullptr return means that a new value had to be created for x = c(x,y), so we need to replace
			// the value of x with that new value; std::swap(lvalue_SP, result_SP) does not do it because lvalue_SP
			// is not the EidosValue_SP that is inside the symbol table, it just points to the same EidosValue!
			EidosErrorPosition error_pos_save = PushErrorPositionFromToken(p_node->token_);
			
			global_symbols_->SetValueForSymbolNoCopy(lvalue_node->cached_stringID_, std::move(result_SP));
			
			RestoreErrorPosition(error_pos_save);
			goto compoundAssignmentSuccess;
		}
	}
	
	// we can drop through to here even if cached_compound_assignment_ or cached_append_assignment_ is set, if the code above bailed for some reason
compoundAssignmentSkip:
	
	{
		EidosToken *operator_token = p_node->token_;
		EidosASTNode *lvalue_node = p_node->children_[0];
		EidosValue_SP rvalue = FastEvaluateNode(p_node->children_[1]);
		
#if DEBUG_POINTS_ENABLED
		// SLiMgui debugging point
		EidosDebugPointIndent indenter;
		
		if (debug_points_ && debug_points_->set.size() && (operator_token->token_line_ != -1) &&
			(debug_points_->set.find(operator_token->token_line_) != debug_points_->set.end()))
		{
			std::ostream &output_stream = ErrorOutputStream();
			
			output_stream << EidosDebugPointIndent::Indent() << "#DEBUG ASSIGN (line " << (operator_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): ";
			if (lvalue_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
				output_stream << lvalue_node->token_->token_string_ << " = ";
			if (rvalue->Count() <= 1)
			{
				rvalue->PrintStructure(output_stream, 1);
			}
			else {
				// print multiple values on a new line, with indent
				rvalue->PrintStructure(output_stream, 0);
				output_stream << std::endl;
				indenter.indent(2);
				rvalue->Print(output_stream, EidosDebugPointIndent::Indent());
				indenter.outdent(2);
			}
			output_stream << std::endl;
		}
#endif
		
		EidosErrorPosition error_pos_save = PushErrorPositionFromToken(operator_token);
		
		_AssignRValueToLValue(std::move(rvalue), lvalue_node);
		
		RestoreErrorPosition(error_pos_save);
	}
	
compoundAssignmentSuccess:
	
	// by design, assignment does not yield a usable value; instead it produces void – this prevents the bug "if (x = 3) ..."
	// since the condition is void and will raise; the loss of legitimate uses of "if (x = 3)" seems a small price to pay
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Assign()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Assign_R(const EidosASTNode *p_node)
{
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Assign_R()");
	
	// The <- operator is always illegal in Eidos, to safeguard against erroneous accidental usage for users coming from R.
	// It would parse as "a < -b;" and thus do nothing, silently, instead of assigning.  For this reason, we make it a
	// token and make the use of that token illegal.  We won't actually make it here, typically; use of this operator will
	// be a parse error caught in EidosScript::Match(), giving an error message similar to the one here.
	EidosToken *operator_token = p_node->token_;
	
	EIDOS_TERMINATION << R"V0G0N(ERROR (EidosInterpreter::Evaluate_Assign_R): the R-style assignment operator <- is not legal in Eidos.  For assignment, use operator =, like "a = b;".  For comparison to a negative quantity, use spaces to fix the tokenization, like "a < -b;".)V0G0N" << EidosTerminate(operator_token);
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
		EidosValueType promotion_type = EidosTypeForPromotion(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Eq): non-conformable array operands to the '==' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if ((first_child_count == 1) && (!result_dim_source))
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				bool equal;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: equal = (first_child_value->LogicalAtIndex_CAST(0, operator_token) == second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		equal = (first_child_value->IntAtIndex_CAST(0, operator_token) == second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	equal = (first_child_value->FloatAtIndex_CAST(0, operator_token) == second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	equal = (first_child_value->StringAtIndex_CAST(0, operator_token) == second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	equal = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) == second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: equal = false; break;		// never hit
				}
				
				return (equal ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					// Direct float-to-float compare can be optimized through vector access
					const double *float1_data = first_child_value->FloatData();
					const double *float2_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(float1_data[value_index] == float2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
				{
					// Direct int-to-int compare can be optimized through vector access
					const int64_t *int1_data = first_child_value->IntData();
					const int64_t *int2_data = second_child_value->IntData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(int1_data[value_index] == int2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
				{
					// Direct object-to-object compare can be optimized through vector access
					EidosObject * const *obj1_vec = first_child_value->ObjectData();
					EidosObject * const *obj2_vec = second_child_value->ObjectData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(obj1_vec[value_index] == obj2_vec[value_index], value_index);
				}
				else
				{
					// General case
					for (int value_index = 0; value_index < first_child_count; ++value_index)
					{
						bool equal;
						
						switch (promotion_type)
						{
							case EidosValueType::kValueLogical: equal = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) == second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueInt:		equal = (first_child_value->IntAtIndex_CAST(value_index, operator_token) == second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueFloat:	equal = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) == second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueString:	equal = (first_child_value->StringAtIndex_CAST(value_index, operator_token) == second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueObject:	equal = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) == second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
							default: equal = false; break;		// never hit
						}
						
						logical_result->set_logical_no_check(equal, value_index);
					}
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			if ((promotion_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float1 = first_child_value->FloatAtIndex_CAST(0, operator_token);
				const double *float_data = second_child_value->FloatData();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(float1 == float_data[value_index], value_index);
			}
			else if ((promotion_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int1 = first_child_value->IntAtIndex_CAST(0, operator_token);
				const int64_t *int_data = second_child_value->IntData();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(int1 == int_data[value_index], value_index);
			}
			else if ((promotion_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObject *obj1 = first_child_value->ObjectElementAtIndex_CAST(0, operator_token);
				EidosObject * const *obj_vec = second_child_value->ObjectData();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(obj1 == obj_vec[value_index], value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					bool equal;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: equal = (first_child_value->LogicalAtIndex_CAST(0, operator_token) == second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueInt:		equal = (first_child_value->IntAtIndex_CAST(0, operator_token) == second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueFloat:	equal = (first_child_value->FloatAtIndex_CAST(0, operator_token) == second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueString:	equal = (first_child_value->StringAtIndex_CAST(0, operator_token) == second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueObject:	equal = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) == second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
						default: equal = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(equal, value_index);
				}
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			if ((promotion_type == EidosValueType::kValueFloat) && (first_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float2 = second_child_value->FloatAtIndex_CAST(0, operator_token);
				const double *float_data = first_child_value->FloatData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(float_data[value_index] == float2, value_index);
			}
			else if ((promotion_type == EidosValueType::kValueInt) && (first_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int2 = second_child_value->IntAtIndex_CAST(0, operator_token);
				const int64_t *int_data = first_child_value->IntData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(int_data[value_index] == int2, value_index);
			}
			else if ((promotion_type == EidosValueType::kValueObject) && (first_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObject *obj2 = second_child_value->ObjectElementAtIndex_CAST(0, operator_token);
				EidosObject * const *obj_vec = first_child_value->ObjectData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(obj_vec[value_index] == obj2, value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					bool equal;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: equal = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) == second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueInt:		equal = (first_child_value->IntAtIndex_CAST(value_index, operator_token) == second_child_value->IntAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueFloat:	equal = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) == second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueString:	equal = (first_child_value->StringAtIndex_CAST(value_index, operator_token) == second_child_value->StringAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueObject:	equal = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) == second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
						default: equal = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(equal, value_index);
				}
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
		EidosValueType promotion_type = EidosTypeForPromotion(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Lt): non-conformable array operands to the '<' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if ((first_child_count == 1) && (!result_dim_source))
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				bool lt;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: lt = (first_child_value->LogicalAtIndex_CAST(0, operator_token) < second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		lt = (first_child_value->IntAtIndex_CAST(0, operator_token) < second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	lt = (first_child_value->FloatAtIndex_CAST(0, operator_token) < second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	lt = (first_child_value->StringAtIndex_CAST(0, operator_token) < second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	lt = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) < second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: lt = false; break;		// never hit
				}
				
				return (lt ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					bool lt;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: lt = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) < second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueInt:		lt = (first_child_value->IntAtIndex_CAST(value_index, operator_token) < second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueFloat:	lt = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) < second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueString:	lt = (first_child_value->StringAtIndex_CAST(value_index, operator_token) < second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueObject:	lt = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) < second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
						default: lt = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(lt, value_index);
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
				bool lt;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: lt = (first_child_value->LogicalAtIndex_CAST(0, operator_token) < second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueInt:		lt = (first_child_value->IntAtIndex_CAST(0, operator_token) < second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueFloat:	lt = (first_child_value->FloatAtIndex_CAST(0, operator_token) < second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueString:	lt = (first_child_value->StringAtIndex_CAST(0, operator_token) < second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueObject:	lt = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) < second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
					default: lt = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(lt, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				bool lt;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: lt = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) < second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		lt = (first_child_value->IntAtIndex_CAST(value_index, operator_token) < second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	lt = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) < second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	lt = (first_child_value->StringAtIndex_CAST(value_index, operator_token) < second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	lt = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) < second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: lt = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(lt, value_index);
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
		EidosValueType promotion_type = EidosTypeForPromotion(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_LtEq): non-conformable array operands to the '<=' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if ((first_child_count == 1) && (!result_dim_source))
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				bool lteq;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: lteq = (first_child_value->LogicalAtIndex_CAST(0, operator_token) <= second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		lteq = (first_child_value->IntAtIndex_CAST(0, operator_token) <= second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	lteq = (first_child_value->FloatAtIndex_CAST(0, operator_token) <= second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	lteq = (first_child_value->StringAtIndex_CAST(0, operator_token) <= second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	lteq = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) <= second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: lteq = false; break;		// never hit
				}
				
				return (lteq ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					bool lteq;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: lteq = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) <= second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueInt:		lteq = (first_child_value->IntAtIndex_CAST(value_index, operator_token) <= second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueFloat:	lteq = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) <= second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueString:	lteq = (first_child_value->StringAtIndex_CAST(value_index, operator_token) <= second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueObject:	lteq = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) <= second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
						default: lteq = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(lteq, value_index);
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
				bool lteq;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: lteq = (first_child_value->LogicalAtIndex_CAST(0, operator_token) <= second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueInt:		lteq = (first_child_value->IntAtIndex_CAST(0, operator_token) <= second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueFloat:	lteq = (first_child_value->FloatAtIndex_CAST(0, operator_token) <= second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueString:	lteq = (first_child_value->StringAtIndex_CAST(0, operator_token) <= second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueObject:	lteq = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) <= second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
					default: lteq = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(lteq, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				bool lteq;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: lteq = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) <= second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		lteq = (first_child_value->IntAtIndex_CAST(value_index, operator_token) <= second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	lteq = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) <= second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	lteq = (first_child_value->StringAtIndex_CAST(value_index, operator_token) <= second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	lteq = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) <= second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: lteq = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(lteq, value_index);
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
		EidosValueType promotion_type = EidosTypeForPromotion(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_Gt): non-conformable array operands to the '>' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if ((first_child_count == 1) && (!result_dim_source))
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				bool gt;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: gt = (first_child_value->LogicalAtIndex_CAST(0, operator_token) > second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		gt = (first_child_value->IntAtIndex_CAST(0, operator_token) > second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	gt = (first_child_value->FloatAtIndex_CAST(0, operator_token) > second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	gt = (first_child_value->StringAtIndex_CAST(0, operator_token) > second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	gt = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) > second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: gt = false; break;		// never hit
				}
				
				return (gt ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					bool gt;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: gt = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) > second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueInt:		gt = (first_child_value->IntAtIndex_CAST(value_index, operator_token) > second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueFloat:	gt = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) > second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueString:	gt = (first_child_value->StringAtIndex_CAST(value_index, operator_token) > second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueObject:	gt = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) > second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
						default: gt = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(gt, value_index);
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
				bool gt;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: gt = (first_child_value->LogicalAtIndex_CAST(0, operator_token) > second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueInt:		gt = (first_child_value->IntAtIndex_CAST(0, operator_token) > second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueFloat:	gt = (first_child_value->FloatAtIndex_CAST(0, operator_token) > second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueString:	gt = (first_child_value->StringAtIndex_CAST(0, operator_token) > second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueObject:	gt = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) > second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
					default: gt = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(gt, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				bool gt;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: gt = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) > second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		gt = (first_child_value->IntAtIndex_CAST(value_index, operator_token) > second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	gt = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) > second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	gt = (first_child_value->StringAtIndex_CAST(value_index, operator_token) > second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	gt = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) > second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: gt = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(gt, value_index);
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
		EidosValueType promotion_type = EidosTypeForPromotion(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_GtEq): non-conformable array operands to the '>=' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if ((first_child_count == 1) && (!result_dim_source))
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				bool gteq;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: gteq = (first_child_value->LogicalAtIndex_CAST(0, operator_token) >= second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		gteq = (first_child_value->IntAtIndex_CAST(0, operator_token) >= second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	gteq = (first_child_value->FloatAtIndex_CAST(0, operator_token) >= second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	gteq = (first_child_value->StringAtIndex_CAST(0, operator_token) >= second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	gteq = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) >= second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: gteq = false; break;		// never hit
				}
				
				return (gteq ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					bool gteq;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: gteq = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) >= second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueInt:		gteq = (first_child_value->IntAtIndex_CAST(value_index, operator_token) >= second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueFloat:	gteq = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) >= second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueString:	gteq = (first_child_value->StringAtIndex_CAST(value_index, operator_token) >= second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueObject:	gteq = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) >= second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
						default: gteq = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(gteq, value_index);
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
				bool gteq;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: gteq = (first_child_value->LogicalAtIndex_CAST(0, operator_token) >= second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueInt:		gteq = (first_child_value->IntAtIndex_CAST(0, operator_token) >= second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueFloat:	gteq = (first_child_value->FloatAtIndex_CAST(0, operator_token) >= second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueString:	gteq = (first_child_value->StringAtIndex_CAST(0, operator_token) >= second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
					case EidosValueType::kValueObject:	gteq = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) >= second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
					default: gteq = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(gteq, value_index);
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			for (int value_index = 0; value_index < first_child_count; ++value_index)
			{
				bool gteq;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: gteq = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) >= second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		gteq = (first_child_value->IntAtIndex_CAST(value_index, operator_token) >= second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	gteq = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) >= second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	gteq = (first_child_value->StringAtIndex_CAST(value_index, operator_token) >= second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	gteq = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) >= second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: gteq = false; break;		// never hit
				}
				
				logical_result->set_logical_no_check(gteq, value_index);
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
		EidosValueType promotion_type = EidosTypeForPromotion(first_child_type, second_child_type, operator_token);
		
		// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
		int first_child_dimcount = first_child_value->DimensionCount();
		int second_child_dimcount = second_child_value->DimensionCount();
		EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(first_child_value.get(), second_child_value.get()));
		
		if ((first_child_dimcount > 1) && (second_child_dimcount > 1) && !EidosValue::MatchingDimensions(first_child_value.get(), second_child_value.get()))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_NotEq): non-conformable array operands to the '!=' operator." << EidosTerminate(operator_token);
		
		if (first_child_count == second_child_count)
		{
			if ((first_child_count == 1) && (!result_dim_source))
			{
				// special-case the 1-to-1 comparison to return a statically allocated logical value, for speed
				bool notequal;
				
				switch (promotion_type)
				{
					case EidosValueType::kValueLogical: notequal = (first_child_value->LogicalAtIndex_CAST(0, operator_token) != second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueInt:		notequal = (first_child_value->IntAtIndex_CAST(0, operator_token) != second_child_value->IntAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueFloat:	notequal = (first_child_value->FloatAtIndex_CAST(0, operator_token) != second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueString:	notequal = (first_child_value->StringAtIndex_CAST(0, operator_token) != second_child_value->StringAtIndex_CAST(0, operator_token)); break;
					case EidosValueType::kValueObject:	notequal = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) != second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
					default: notequal = false; break;		// never hit
				}
				
				return (notequal ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
				
				if ((first_child_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
				{
					// Direct float-to-float compare can be optimized through vector access
					const double *float1_data = first_child_value->FloatData();
					const double *float2_data = second_child_value->FloatData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(float1_data[value_index] != float2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
				{
					// Direct int-to-int compare can be optimized through vector access
					const int64_t *int1_data = first_child_value->IntData();
					const int64_t *int2_data = second_child_value->IntData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(int1_data[value_index] != int2_data[value_index], value_index);
				}
				else if ((first_child_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
				{
					// Direct object-to-object compare can be optimized through vector access
					EidosObject * const *obj1_vec = first_child_value->ObjectData();
					EidosObject * const *obj2_vec = second_child_value->ObjectData();
					
					for (int value_index = 0; value_index < first_child_count; ++value_index)
						logical_result->set_logical_no_check(obj1_vec[value_index] != obj2_vec[value_index], value_index);
				}
				else
				{
					// General case
					for (int value_index = 0; value_index < first_child_count; ++value_index)
					{
						bool notequal;
						
						switch (promotion_type)
						{
							case EidosValueType::kValueLogical: notequal = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) != second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueInt:		notequal = (first_child_value->IntAtIndex_CAST(value_index, operator_token) != second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueFloat:	notequal = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) != second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueString:	notequal = (first_child_value->StringAtIndex_CAST(value_index, operator_token) != second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
							case EidosValueType::kValueObject:	notequal = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) != second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
							default: notequal = false; break;		// never hit
						}
						
						logical_result->set_logical_no_check(notequal, value_index);
					}
				}
				
				result_SP = std::move(logical_result_SP);
			}
		}
		else if (first_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(second_child_count);
			
			if ((promotion_type == EidosValueType::kValueFloat) && (second_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float1 = first_child_value->FloatAtIndex_CAST(0, operator_token);
				const double *float_data = second_child_value->FloatData();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(float1 != float_data[value_index], value_index);
			}
			else if ((promotion_type == EidosValueType::kValueInt) && (second_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int1 = first_child_value->IntAtIndex_CAST(0, operator_token);
				const int64_t *int_data = second_child_value->IntData();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(int1 != int_data[value_index], value_index);
			}
			else if ((promotion_type == EidosValueType::kValueObject) && (second_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObject *obj1 = first_child_value->ObjectElementAtIndex_CAST(0, operator_token);
				EidosObject * const *obj_vec = second_child_value->ObjectData();
				
				for (int value_index = 0; value_index < second_child_count; ++value_index)
					logical_result->set_logical_no_check(obj1 != obj_vec[value_index], value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < second_child_count; ++value_index)
				{
					bool notequal;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: notequal = (first_child_value->LogicalAtIndex_CAST(0, operator_token) != second_child_value->LogicalAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueInt:		notequal = (first_child_value->IntAtIndex_CAST(0, operator_token) != second_child_value->IntAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueFloat:	notequal = (first_child_value->FloatAtIndex_CAST(0, operator_token) != second_child_value->FloatAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueString:	notequal = (first_child_value->StringAtIndex_CAST(0, operator_token) != second_child_value->StringAtIndex_CAST(value_index, operator_token)); break;
						case EidosValueType::kValueObject:	notequal = (first_child_value->ObjectElementAtIndex_CAST(0, operator_token) != second_child_value->ObjectElementAtIndex_CAST(value_index, operator_token)); break;
						default: notequal = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(notequal, value_index);
				}
			}
			
			result_SP = std::move(logical_result_SP);
		}
		else if (second_child_count == 1)
		{
			EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
			EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(first_child_count);
			
			if ((promotion_type == EidosValueType::kValueFloat) && (first_child_type == EidosValueType::kValueFloat))
			{
				// Direct float-to-float compare can be optimized through vector access; note the singleton might get promoted to float
				double float2 = second_child_value->FloatAtIndex_CAST(0, operator_token);
				const double *float_data = first_child_value->FloatData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(float_data[value_index] != float2, value_index);
			}
			else if ((promotion_type == EidosValueType::kValueInt) && (first_child_type == EidosValueType::kValueInt))
			{
				// Direct int-to-int compare can be optimized through vector access; note the singleton might get promoted to int
				int64_t int2 = second_child_value->IntAtIndex_CAST(0, operator_token);
				const int64_t *int_data = first_child_value->IntData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(int_data[value_index] != int2, value_index);
			}
			else if ((promotion_type == EidosValueType::kValueObject) && (first_child_type == EidosValueType::kValueObject))
			{
				// Direct object-to-object compare can be optimized through vector access
				EidosObject *obj2 = second_child_value->ObjectElementAtIndex_CAST(0, operator_token);
				EidosObject * const *obj_vec = first_child_value->ObjectData();
				
				for (int value_index = 0; value_index < first_child_count; ++value_index)
					logical_result->set_logical_no_check(obj_vec[value_index] != obj2, value_index);
			}
			else
			{
				// General case
				for (int value_index = 0; value_index < first_child_count; ++value_index)
				{
					bool notequal;
					
					switch (promotion_type)
					{
						case EidosValueType::kValueLogical: notequal = (first_child_value->LogicalAtIndex_CAST(value_index, operator_token) != second_child_value->LogicalAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueInt:		notequal = (first_child_value->IntAtIndex_CAST(value_index, operator_token) != second_child_value->IntAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueFloat:	notequal = (first_child_value->FloatAtIndex_CAST(value_index, operator_token) != second_child_value->FloatAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueString:	notequal = (first_child_value->StringAtIndex_CAST(value_index, operator_token) != second_child_value->StringAtIndex_CAST(0, operator_token)); break;
						case EidosValueType::kValueObject:	notequal = (first_child_value->ObjectElementAtIndex_CAST(value_index, operator_token) != second_child_value->ObjectElementAtIndex_CAST(0, operator_token)); break;
						default: notequal = false; break;		// never hit
					}
					
					logical_result->set_logical_no_check(notequal, value_index);
				}
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): '" << p_number_string << "' could not be represented as an integer (decimal or negative exponent)." << EidosTerminate(p_blame_token);
		return 0;
	}
	else if ((p_number_string.find('e') != std::string::npos) || (p_number_string.find('E') != std::string::npos))	// has an exponent
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): '" << p_number_string << "' could not be represented as an integer (strtod conversion error)." << EidosTerminate(p_blame_token);
		
		// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
		if ((converted_value < (double)INT64_MIN) || (converted_value >= (double)INT64_MAX))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): '" << p_number_string << "' could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
		
		return static_cast<int64_t>(converted_value);
	}
	else																								// plain integer
	{
		int64_t converted_value = strtoll(c_str, &last_used_char, 10);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NonnegativeIntegerForString): '" << p_number_string << "' could not be represented as an integer (strtoll conversion error)." << EidosTerminate(p_blame_token);
		
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
		EIDOS_TERMINATION << "ERROR (EidosInterpreter::FloatForString): '" << p_number_string << "' could not be represented as a float (strtod conversion error)." << EidosTerminate(p_blame_token);
	
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
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): '" << p_number_string << "' could not be represented as a float (strtod conversion error)." << EidosTerminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(converted_value));
	}
	else if ((p_number_string.find('e') != std::string::npos) || (p_number_string.find('E') != std::string::npos))		// has an exponent
	{
		double converted_value = strtod(c_str, &last_used_char);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): '" << p_number_string << "' could not be represented as an integer (strtod conversion error)." << EidosTerminate(p_blame_token);
		
		// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
		if ((converted_value < (double)INT64_MIN) || (converted_value >= (double)INT64_MAX))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): '" << p_number_string << "' could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(static_cast<int64_t>(converted_value)));
	}
	else																										// plain integer
	{
		int64_t converted_value = strtoll(c_str, &last_used_char, 10);
		
		if (errno || (last_used_char == c_str))
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::NumericValueForString): '" << p_number_string << "' could not be represented as an integer (strtoll conversion error)." << EidosTerminate(p_blame_token);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(converted_value));
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
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(p_node->token_->token_string_));
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
	{
		if (use_custom_undefined_identifier_raise_)
		{
			// This branch raises a special exception if the identifier is undefined in the symbol table.
			// This facility is used by Community::_EvaluateTickRangeNode() for tolerant evaluation.
			result_SP = global_symbols_->GetValueOrRaiseForASTNode_SpecialRaise(p_node);	// raises if undefined
		}
		else
		{
			result_SP = global_symbols_->GetValueOrRaiseForASTNode(p_node);	// raises if undefined
		}
	}
	
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
	
#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	EidosDebugPointIndent indenter;
	
	if (debug_points_ && debug_points_->set.size() && (operator_token->token_line_ != -1) &&
		(debug_points_->set.find(operator_token->token_line_) != debug_points_->set.end()) &&
		(condition_result->Count() == 1))
	{
		eidos_logical_t condition_logical = condition_result->LogicalAtIndex_CAST(0, operator_token);
		
		// the above might raise, but if it does, it will be the same error as produced below
		ErrorOutputStream() << EidosDebugPointIndent::Indent() << "#DEBUG IF (line " << (operator_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): condition == " <<
			(condition_logical ? "T" : "F") << std::endl;
		indenter.indent();
	}
#endif
	
	if (condition_result == gStaticEidosValue_LogicalT)
	{
		// Handle a static singleton logical true super fast; no need for type check, count, etc
		EidosASTNode *true_node = p_node->children_[1];
		
#if (SLIMPROFILING == 1)
		// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
		SLIM_PROFILE_BLOCK_START_CONDITION(true_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
		
		result_SP = FastEvaluateNode(true_node);
		
#if (SLIMPROFILING == 1)
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
			
#if (SLIMPROFILING == 1)
			// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
			SLIM_PROFILE_BLOCK_START_CONDITION(false_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
			
			result_SP = FastEvaluateNode(false_node);
			
#if (SLIMPROFILING == 1)
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
		eidos_logical_t condition_logical = condition_result->LogicalAtIndex_CAST(0, operator_token);
		
		if (condition_logical)
		{
			EidosASTNode *true_node = p_node->children_[1];
			
#if (SLIMPROFILING == 1)
			// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
			SLIM_PROFILE_BLOCK_START_CONDITION(true_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
			
			result_SP = FastEvaluateNode(true_node);
			
#if (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END_CONDITION(true_node->profile_total_);
#endif
		}
		else if (children_size == 3)		// has an 'else' node
		{
			EidosASTNode *false_node = p_node->children_[2];
			
#if (SLIMPROFILING == 1)
			// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
			SLIM_PROFILE_BLOCK_START_CONDITION(false_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
			
			result_SP = FastEvaluateNode(false_node);
			
#if (SLIMPROFILING == 1)
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
	
#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	EidosDebugPointIndent indenter;
	
	if (debug_points_ && debug_points_->set.size() && (operator_token->token_line_ != -1) &&
		(debug_points_->set.find(operator_token->token_line_) != debug_points_->set.end()))
	{
		ErrorOutputStream() << EidosDebugPointIndent::Indent() << "#DEBUG DO (line " << (operator_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): entering loop" << std::endl;
		indenter.indent();
	}
#endif
	
	do
	{
		// execute the do...while loop's statement by evaluating its node; evaluation values get thrown away
		EidosASTNode *statement_node = p_node->children_[0];
		
#if (SLIMPROFILING == 1)
		// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
		SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
		
		EidosValue_SP statement_value = FastEvaluateNode(statement_node);
		
#if (SLIMPROFILING == 1)
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
		
#if DEBUG_POINTS_ENABLED
		// SLiMgui debugging point
		if (debug_points_ && debug_points_->set.size() && (operator_token->token_line_ != -1) &&
			(debug_points_->set.find(operator_token->token_line_) != debug_points_->set.end()) &&
			(condition_result->Count() == 1))
		{
			eidos_logical_t condition_logical = condition_result->LogicalAtIndex_CAST(0, operator_token);
			
			// the above might raise, but if it does, it will be the same error as produced below
			indenter.outdent();
			ErrorOutputStream() << EidosDebugPointIndent::Indent() << "#DEBUG DO (line " << (operator_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): condition == " <<
			(condition_logical ? "T" : "F") << std::endl;
			indenter.indent();
		}
#endif
		
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
			eidos_logical_t condition_logical = condition_result->LogicalAtIndex_CAST(0, operator_token);
			
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
		
#if DEBUG_POINTS_ENABLED
		// SLiMgui debugging point
		EidosDebugPointIndent indenter;
		
		if (debug_points_ && debug_points_->set.size() && (operator_token->token_line_ != -1) &&
			(debug_points_->set.find(operator_token->token_line_) != debug_points_->set.end()) &&
			(condition_result->Count() == 1))
		{
			eidos_logical_t condition_logical = condition_result->LogicalAtIndex_CAST(0, operator_token);
			
			// the above might raise, but if it does, it will be the same error as produced below
			ErrorOutputStream() << EidosDebugPointIndent::Indent() << "#DEBUG WHILE (line " << (operator_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): condition == " <<
			(condition_logical ? "T" : "F") << std::endl;
			indenter.indent();
		}
#endif
		
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
			eidos_logical_t condition_logical = condition_result->LogicalAtIndex_CAST(0, operator_token);
			
			if (!condition_logical)
				break;
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_While): condition for while loop has size() != 1." << EidosTerminate(p_node->token_);
		}
		
		// execute the while loop's statement by evaluating its node; evaluation values get thrown away
		EidosASTNode *statement_node = p_node->children_[1];
		
#if (SLIMPROFILING == 1)
		// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
		SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
		
		EidosValue_SP statement_value = FastEvaluateNode(statement_node);
		
#if (SLIMPROFILING == 1)
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
	
	// We now allow multiple "in" clauses, so the number of children is just required to be >= 3 and odd
	// Each "in" clause is two children (identifier and expression), and the statement is the last child.
	EIDOS_ASSERT_CHILD_COUNT_GTEQ("EidosInterpreter::Evaluate_For", 3);
#if DEBUG
	if (p_node->children_.size() % 2 != 1) EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): (internal error) expected an odd number of children." << EidosTerminate(p_node->token_);
#endif
	
	EidosToken *operator_token = p_node->token_;
	int in_clause_count = (int)((p_node->children_.size() - 1) / 2);
	
	// This structure is used to keep track of the range and the index variable for each "in" clause
	typedef struct {
		EidosGlobalStringID identifier_name;
		int64_t iteration_count;
		
		// we handle simple integer ranges more efficiently
		bool simpleIntegerRange;
		int64_t start_int, end_int;
		
		// otherwise we keep an EidosValue for the range of the "in" clause
		EidosValue_SP range_value;
		
		// we keep a reference to the loop iterator variable if we need one
		EidosValue_SP iterator_variable;
		EidosValueType iterator_type;
		void *iterator_data;
	} ForLoopHandler;
	
	// start by evaluating each "in" clause range and caching information about each clause
	std::vector<ForLoopHandler> loop_handlers;
	loop_handlers.resize(in_clause_count);
	
	for (int in_clause_index = 0; in_clause_index < in_clause_count; ++in_clause_index)
	{
		EidosASTNode *identifier_child = p_node->children_[in_clause_index * 2];			// guaranteed by the parser to be an identifier token
		const EidosASTNode *range_node = p_node->children_[in_clause_index * 2 + 1];
		EidosGlobalStringID identifier_name = identifier_child->cached_stringID_;
		bool is_const = false;
		
		// check for a constant up front, to give a better error message with the token highlighted
		if (global_symbols_->ContainsSymbol_IsConstant(identifier_name, &is_const))
		{
			EidosValue *existing_value = global_symbols_->GetValueRawOrRaiseForSymbol(identifier_name);
			
			if (is_const || existing_value->IsIteratorVariable())
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): identifier '" << identifier_child->token_->token_string_ << "' cannot be redefined because it is a constant." << EidosTerminate(identifier_child->token_);
		}
		
		// In some cases we do not need to actually construct the range that we are going to iterate over; we check for those cases first
		bool range_setup_handled = false;
		ForLoopHandler &loop_handler = loop_handlers[in_clause_index];
		loop_handler.identifier_name = identifier_name;
		loop_handler.simpleIntegerRange = false;
		
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
					loop_handler.simpleIntegerRange = true;
					loop_handler.start_int = range_start_value->IntAtIndex_NOCAST(0, nullptr);
					loop_handler.end_int = range_end_value->IntAtIndex_NOCAST(0, nullptr);
					
					if (loop_handler.start_int < loop_handler.end_int)
						loop_handler.iteration_count = loop_handler.end_int - loop_handler.start_int + 1;
					else
						loop_handler.iteration_count = loop_handler.start_int - loop_handler.end_int + 1;
					
					range_setup_handled = true;
				}
				else
				{
					// If we are not using the general case, we have a bit of a problem now, because we have evaluated the child nodes
					// of the range expression.  Because that might have side effects, we can't let the code below do it again.
					// We therefore have to construct the range here that will be used below.  No good deed goes unpunished.
					
					// Note that this call to Evaluate_RangeExpr_Internal() gives ownership of the child values; it deletes them for us
					loop_handler.range_value = _Evaluate_RangeExpr_Internal(range_node, *range_start_value, *range_end_value);
					loop_handler.iteration_count = loop_handler.range_value->Count();
					range_setup_handled = true;
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
							
							loop_handler.simpleIntegerRange = true;
							
							EidosValue_SP argument_value = FastEvaluateNode(argument_node);
							
							loop_handler.iteration_count = argument_value->Count();
							loop_handler.start_int = 0;
							loop_handler.end_int = loop_handler.iteration_count - 1;
							range_setup_handled = true;
						}
					}
					else if (signature->internal_function_ == &Eidos_ExecuteFunction_seqLen)
					{
						if (range_node->children_.size() == 2)
						{
							// We have a qualifying seqLen() call, so evaluate its argument and set up our simple integer sequence
							const EidosASTNode *argument_node = range_node->children_[1];
							
							loop_handler.simpleIntegerRange = true;
							
							EidosValue_SP argument_value = FastEvaluateNode(argument_node);
							EidosValueType arg_type = argument_value->Type();
							
							// check the argument; could call CheckArguments() except that it would use the wrong error position
							if (arg_type != EidosValueType::kValueInt)
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): argument 1 (length) cannot be type " << arg_type << " for function seqLen()." << EidosTerminate(call_name_node->token_);
							if (argument_value->Count() != 1)
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): argument 1 (length) must be a singleton (size() == 1) for function seqLen(), but size() == " << argument_value->Count() << "." << EidosTerminate(call_name_node->token_);
							
							int64_t length = argument_value->IntAtIndex_NOCAST(0, call_name_node->token_);
							
							if (length < 0)
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): function seqLen() requires length to be greater than or equal to 0 (" << length << " supplied)." << EidosTerminate(call_name_node->token_);
							
							loop_handler.iteration_count = length;
							loop_handler.start_int = 0;
							loop_handler.end_int = length - 1;
							range_setup_handled = true;
						}
					}
				}
			}
		}
		
		if (!range_setup_handled)
		{
			// We have something other than a simple integer range, so we have to do more work to figure out the range
			if (!loop_handler.range_value)
				loop_handler.range_value = FastEvaluateNode(range_node);
			
			loop_handler.iteration_count = loop_handler.range_value->Count();
		}
	}
	
	// check that all "in" clauses have the same iteration count
	int64_t iteration_count = 0;
	
	for (int in_clause_index = 0; in_clause_index < in_clause_count; ++in_clause_index)
	{
		ForLoopHandler &loop_handler = loop_handlers[in_clause_index];
		int64_t clause_count = loop_handler.iteration_count;
		
		if (in_clause_index == 0)
			iteration_count = clause_count;
		else if (iteration_count != clause_count)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): all 'in' clauses of a for loop must have the same number of iterations." << EidosTerminate(p_node->token_);
	}
	
	// short-circuit if we have no work to do
	EidosValue_SP result_SP;
	EidosASTNode *statement_node = p_node->children_[in_clause_count * 2];
	
	if (iteration_count == 0)
	{
		for (int in_clause_index = 0; in_clause_index < in_clause_count; ++in_clause_index)
		{
			ForLoopHandler &loop_handler = loop_handlers[in_clause_index];
			
			if (!loop_handler.simpleIntegerRange)
			{
				EidosValueType range_type = loop_handler.range_value->Type();
				
				if (range_type == EidosValueType::kValueVOID)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword does not allow void for its right operand (the range to be iterated over)." << EidosTerminate(p_node->token_);
				if (range_type == EidosValueType::kValueNULL)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): the 'for' keyword does not allow NULL for its right operand (the range to be iterated over)." << EidosTerminate(p_node->token_);
			}
		}
		
		goto for_exit;
	}
	
	// any loop index variables that already exist should be removed so that we can redefine them below
	for (int in_clause_index = 0; in_clause_index < in_clause_count; ++in_clause_index)
	{
		ForLoopHandler &loop_handler = loop_handlers[in_clause_index];
		
		if (global_symbols_->ContainsSymbol(loop_handler.identifier_name))
			global_symbols_->RemoveValueForSymbol(loop_handler.identifier_name);
	}
	
	// actually run the for loop
	try
	{
		for (int range_index = 0; range_index < iteration_count; ++range_index)
		{
#if DEBUG_POINTS_ENABLED
			// SLiMgui debugging point
			EidosDebugPointIndent indenter;
			bool log_debug_point = false;
			
			if (debug_points_ && debug_points_->set.size() && (operator_token->token_line_ != -1) &&
				(debug_points_->set.find(operator_token->token_line_) != debug_points_->set.end()))
			{
				std::ostream &output_stream = ErrorOutputStream();
				
				output_stream << EidosDebugPointIndent::Indent() << "#DEBUG FOR (line " << (operator_token->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): ";
				log_debug_point = true;
			}
#endif
			
			// set up each iterator variable
			for (int in_clause_index = 0; in_clause_index < in_clause_count; ++in_clause_index)
			{
				ForLoopHandler &loop_handler = loop_handlers[in_clause_index];
				
				if (loop_handler.simpleIntegerRange)
				{
					bool counting_up = (loop_handler.start_int < loop_handler.end_int);
					int64_t iterator_int_value = (counting_up ? loop_handler.start_int + range_index : loop_handler.start_int - range_index);
					
					if (range_index == 0)
					{
						EidosValue_Int *index_value = new (gEidosValuePool->AllocateChunk()) EidosValue_Int(iterator_int_value);
						
						loop_handler.iterator_variable = EidosValue_Int_SP(index_value);
						loop_handler.iterator_type = EidosValueType::kValueInt;
						loop_handler.iterator_data = index_value->data_mutable();
						
						// make a constant for the loop index variable; but note that we have a mutable pointer to its data
						// the MarkAsConstant() call marks the *value* as constant, so it can't be modified with, e.g.,
						// x[0] = 7, whereas the MarkAsIteratorVariable() call marks the value as not being allowed to be
						// replaced by a new value with, e.g., x = 7; making an actual constant in the constants symbol table
						// is a no-go because it would prevent user-defined functions from using the same variable name
						global_symbols_->SetValueForSymbolNoCopy(loop_handler.identifier_name, loop_handler.iterator_variable);
						loop_handler.iterator_variable->MarkAsConstant();
						loop_handler.iterator_variable->MarkAsIteratorVariable();
					}
					else
					{
						int64_t *int_data = (int64_t *)loop_handler.iterator_data;
						
						*int_data = iterator_int_value;
					}

#if DEBUG_POINTS_ENABLED
					// SLiMgui debugging point
					if (log_debug_point)
					{
						std::ostream &output_stream = ErrorOutputStream();
						
						output_stream << EidosStringRegistry::StringForGlobalStringID(loop_handler.identifier_name) << " = integer$ " << iterator_int_value;
					}
#endif
				}
				else
				{
					if (range_index == 0)
					{
						// Start out with GetValueAtIndex(), which will return the correct type, handle retain/release, etc. for us
						loop_handler.iterator_type = loop_handler.range_value->Type();
						loop_handler.iterator_variable = loop_handler.range_value->GetValueAtIndex(range_index, operator_token);
						
						if (loop_handler.iterator_variable->IsConstant())
							loop_handler.iterator_variable = loop_handler.iterator_variable->CopyValues();
						
						EidosValue *index_value = loop_handler.iterator_variable.get();
						
						switch (loop_handler.iterator_type)
						{
							case EidosValueType::kValueLogical:		loop_handler.iterator_data = ((EidosValue_Logical *)index_value)->data_mutable(); break;
							case EidosValueType::kValueInt:			loop_handler.iterator_data = ((EidosValue_Int *)index_value)->data_mutable(); break;
							case EidosValueType::kValueFloat:		loop_handler.iterator_data = ((EidosValue_Float *)index_value)->data_mutable(); break;
							case EidosValueType::kValueString:		loop_handler.iterator_data = ((EidosValue_String *)index_value)->StringData_Mutable(); break;
							case EidosValueType::kValueObject:		loop_handler.iterator_data = ((EidosValue_Object *)index_value)->data_mutable(); break;
							default:
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): (internal error) unexpected range value type in for loop." << EidosTerminate(p_node->token_);
						}
						
						// make a constant for the loop index variable; but note that we have a mutable pointer to its data
						// the MarkAsConstant() call marks the *value* as constant, so it can't be modified with, e.g.,
						// x[0] = 7, whereas the MarkAsIteratorVariable() call marks the value as not being allowed to be
						// replaced by a new value with, e.g., x = 7; making an actual constant in the constants symbol table
						// is a no-go because it would prevent user-defined functions from using the same variable name
						global_symbols_->SetValueForSymbolNoCopy(loop_handler.identifier_name, loop_handler.iterator_variable);
						loop_handler.iterator_variable->MarkAsConstant();
						loop_handler.iterator_variable->MarkAsIteratorVariable();
					}
					else
					{
						switch (loop_handler.iterator_type)
						{
							case EidosValueType::kValueLogical:
							{
								EidosValue_Logical *logical_range_value = (EidosValue_Logical *)(loop_handler.range_value.get());
								eidos_logical_t iterator_logical_value = logical_range_value->data()[range_index];
								
								*(eidos_logical_t *)loop_handler.iterator_data = iterator_logical_value;
								break;
							}
							case EidosValueType::kValueInt:
							{
								EidosValue_Int *int_range_value = (EidosValue_Int *)(loop_handler.range_value.get());
								int64_t iterator_int_value = int_range_value->data()[range_index];
								
								*(int64_t *)loop_handler.iterator_data = iterator_int_value;
								break;
							}
							case EidosValueType::kValueFloat:
							{
								EidosValue_Float *float_range_value = (EidosValue_Float *)(loop_handler.range_value.get());
								double iterator_float_value = float_range_value->data()[range_index];
								
								*(double *)loop_handler.iterator_data = iterator_float_value;
								break;
							}
							case EidosValueType::kValueString:
							{
								EidosValue_String *string_range_value = (EidosValue_String *)(loop_handler.range_value.get());
								std::string &iterator_logical_value = string_range_value->StringData_Mutable()[range_index];
								
								*(std::string *)loop_handler.iterator_data = iterator_logical_value;
								break;
							}
							case EidosValueType::kValueObject:
							{
								EidosValue_Object *float_range_value = (EidosValue_Object *)(loop_handler.range_value.get());
								EidosObject *iterator_object_value = float_range_value->data()[range_index];
								EidosObject **iterator_variable_data = (EidosObject **)loop_handler.iterator_data;
								
								if (iterator_object_value->Class()->UsesRetainRelease())
								{
									((EidosDictionaryRetained *)iterator_object_value)->Retain();
									((EidosDictionaryRetained *)*iterator_variable_data)->Release();
								}
								
								*iterator_variable_data = iterator_object_value;
								break;
							}
							default:
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_For): (internal error) unexpected range value type in for loop." << EidosTerminate(p_node->token_);
						}
					}
					
#if DEBUG_POINTS_ENABLED
					// SLiMgui debugging point
					if (log_debug_point)
					{
						std::ostream &output_stream = ErrorOutputStream();
						
						output_stream << EidosStringRegistry::StringForGlobalStringID(loop_handler.identifier_name) << " = " << loop_handler.iterator_type;
						if (loop_handler.iterator_type == EidosValueType::kValueObject)
							output_stream << "<" << loop_handler.range_value->ElementType() << ">";
						output_stream << "$ " << *loop_handler.iterator_variable;
					}
#endif
				}
				
#if DEBUG_POINTS_ENABLED
				// SLiMgui debugging point
				if (log_debug_point)
				{
					std::ostream &output_stream = ErrorOutputStream();
					
					if (in_clause_index == in_clause_count - 1)
					{
						output_stream << std::endl;
						indenter.indent();
					}
					else
					{
						output_stream << ", ";
					}
				}
#endif
			}
			
#if (SLIMPROFILING == 1)
			// PROFILING: profile child statement unless it is a compound statement (which does its own profiling)
			SLIM_PROFILE_BLOCK_START_CONDITION(statement_node->token_->token_type_ != EidosTokenType::kTokenLBrace);
#endif
			
			EidosValue_SP statement_value = FastEvaluateNode(statement_node);
			
#if (SLIMPROFILING == 1)
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
	catch (...)
	{
		// transmute our iterator variables to no longer be constants; the user is allowed to modify them after the loop finishes
		for (int in_clause_index = 0; in_clause_index < in_clause_count; ++in_clause_index)
		{
			ForLoopHandler &loop_handler = loop_handlers[in_clause_index];
			
			if (loop_handler.iterator_variable)
			{
				loop_handler.iterator_variable->MarkAsMutable();
				loop_handler.iterator_variable->MarkAsNonIteratorVariable();
			}
		}
		
		throw;
	}
	
	// transmute our iterator variables to no longer be constants; the user is allowed to modify them after the loop finishes
	for (int in_clause_index = 0; in_clause_index < in_clause_count; ++in_clause_index)
	{
		ForLoopHandler &loop_handler = loop_handlers[in_clause_index];
		
		if (loop_handler.iterator_variable)
		{
			loop_handler.iterator_variable->MarkAsMutable();
			loop_handler.iterator_variable->MarkAsNonIteratorVariable();
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
	
#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	if (debug_points_ && debug_points_->set.size() && (p_node->token_->token_line_ != -1) &&
		(debug_points_->set.find(p_node->token_->token_line_) != debug_points_->set.end()))
	{
		ErrorOutputStream() << EidosDebugPointIndent::Indent() << "#DEBUG NEXT (line " << (p_node->token_->token_line_ + 1) << eidos_context_->DebugPointInfo() << ")" << std::endl;
	}
#endif
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	next_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	PushErrorPositionFromToken(p_node->token_);
	
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Next()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Break(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Break()");
	EIDOS_ASSERT_CHILD_COUNT("EidosInterpreter::Evaluate_Break", 0);

#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	if (debug_points_ && debug_points_->set.size() && (p_node->token_->token_line_ != -1) &&
		(debug_points_->set.find(p_node->token_->token_line_) != debug_points_->set.end()))
	{
		ErrorOutputStream() << EidosDebugPointIndent::Indent() << "#DEBUG BREAK (line " << (p_node->token_->token_line_ + 1) << eidos_context_->DebugPointInfo() << ")" << std::endl;
	}
#endif
	
	// just like a null statement, except that we set a flag in the interpreter, which will be seen by the eval
	// methods and will cause them to return up to the for loop immediately; Evaluate_For will handle the flag.
	break_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	PushErrorPositionFromToken(p_node->token_);
	
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_Break()");
	return result_SP;
}

EidosValue_SP EidosInterpreter::Evaluate_Return(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EIDOS_ENTRY_EXECUTION_LOG("Evaluate_Return()");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"	// the number of children is unsigned and cannot be less than 0, which some compilers warn about...
	EIDOS_ASSERT_CHILD_RANGE("EidosInterpreter::Evaluate_Return", 0, 1);
#pragma GCC diagnostic pop
	
	// set a flag in the interpreter, which will be seen by the eval methods and will cause them to return up to the top-level block immediately
	return_statement_hit_ = true;
	
	// We set up the error state on our token so that if we don't get handled properly above, we are highlighted.
	PushErrorPositionFromToken(p_node->token_);
	
	EidosValue_SP result_SP;
	
	if (p_node->children_.size() == 0)
		result_SP = gStaticEidosValueVOID;	// no return value; note that "return;" is semantically different from "return NULL;" because it returns void
	else
		result_SP = FastEvaluateNode(p_node->children_[0]);
	
#if DEBUG_POINTS_ENABLED
	// SLiMgui debugging point
	EidosDebugPointIndent indenter;
	
	if (debug_points_ && debug_points_->set.size() && (p_node->token_->token_line_ != -1) &&
		(debug_points_->set.find(p_node->token_->token_line_) != debug_points_->set.end()))
	{
		std::ostream &output_stream = ErrorOutputStream();
		
		output_stream << EidosDebugPointIndent::Indent() << "#DEBUG RETURN (line " << (p_node->token_->token_line_ + 1) << eidos_context_->DebugPointInfo() << "): ";
		if (result_SP->Count() <= 1)
		{
			result_SP->PrintStructure(output_stream, 1);
		}
		else {
			// print multiple values on a new line, with indent
			result_SP->PrintStructure(output_stream, 0);
			output_stream << std::endl;
			indenter.indent(2);
			result_SP->Print(output_stream, EidosDebugPointIndent::Indent());
			indenter.outdent(2);
		}
		output_stream << std::endl;
	}
#endif
	
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
	const std::vector<EidosASTNode *> &param_nodes = param_list_node->children_;
	std::vector<std::string> used_param_names;
	
	{
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
		
		try {
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
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionDecl): invalid default value for parameter '" << param_name << "'; a default value must be a numeric constant, a string constant, or a built-in Eidos constant (T, F, NULL, PI, E, INF, or NAN)." << EidosTerminate(default_node->token_);
							}
						}
						else
						{
							default_value = FastEvaluateNode(default_node);
						}
						
						sig->AddArgWithDefault(param_type.type_mask, param_name, param_type.object_class, std::move(default_value));
					}
					
					used_param_names.emplace_back(param_name);
				}
			}
			
			// Add the function body to the function signature.  We can't just use body_node, because we don't know what its
			// lifetime will be; we need our own private copy.  The best thing, really, is to make our own EidosScript object
			// with the appropriate substring, and re-tokenize and re-parse.  This is somewhat wasteful, but it's one-time
			// overhead so it shouldn't matter.  It will smooth out all of the related issues with error reporting, etc.,
			// since we won't be dependent upon somebody else's script object.  Errors in tokenization/parsing should never
			// occur, since the code for the function body already passed through that process once.
			
			// BCH 2/8/2021: Note that we now base this EidosScript on body_node->token_->token_line_ for purposes of debug
			// point detection, so Eidos knows that this script is a substring of the original user script.
			
			// BCH 3/12/2025: For error-tracking, we now need to locate this function declaration within the user's script.
			// Finding the user's script here is a bit gross, though!  We get it from the error position, which generally
			// ought to be set up now with the user script whenever we hit a function declaration.  Maybe in a place like a
			// lambda it would not be set up, which is OK; then we are unmoored from the user's script, and use nullptr.
			// It feels weird to use the error info here to find the user script, but maybe it is OK; the error info is the
			// thing that says "you are interpreting in this script, which is located right here in this other script", and
			// that's exactly what we need to know.  If using it here proves problematic, the alternative seems to me to be
			// for EidosInterpreter to have a pointer to the user script within which it is interpreting; but that would
			// be a big change, and it wouldn't use the information anywhere but here.
			EidosScript *userScript = nullptr;
			
			if (gEidosErrorContext.currentScript)
			{
				EidosScript *error_userScript = gEidosErrorContext.currentScript->UserScript();
				int32_t error_offset = gEidosErrorContext.currentScript->UserScriptUTF16Offset();
				
				if (error_userScript)
				{
					// the current script is derived from the user script
					userScript = error_userScript;
				}
				else if (error_offset == 0)
				{
					// the current script *is* the user script
					userScript = gEidosErrorContext.currentScript;
				}
			}
			
			EidosScript *script;
			
			if (userScript)
			{
#if EIDOS_DEBUG_ERROR_POSITIONS
				std::cout << "=== User-defined function definition found user script " << userScript << "; using that with char offset " << body_node->token_->token_start_ << ", UTF offset " << body_node->token_->token_UTF16_start_ << std::endl;
#endif
				
				script = new EidosScript(body_node->token_->token_string_, userScript, body_node->token_->token_line_, body_node->token_->token_start_, body_node->token_->token_UTF16_start_);
			}
			else
			{
#if EIDOS_DEBUG_ERROR_POSITIONS
				std::cout << "=== User-defined function definition did not find user script (gEidosErrorContext.currentScript == " << gEidosErrorContext.currentScript << ")" << std::endl;
#endif
				
				script = new EidosScript(body_node->token_->token_string_);
			}
			
#if EIDOS_DEBUG_ERROR_POSITIONS
			std::cout << "    script object for the user-defined function == " << script << "" << std::endl;
#endif
			
			script->Tokenize();
			script->ParseInterpreterBlockToAST(false);
			
			sig->body_script_ = script;
			sig->user_defined_ = true;
			
			// this is the line that `function` is on, so we want to use p_node->token_; note this is different
			// from the line offset kept by the script, which refers to the line offset to the starting brace
			// of the body; we want the offset to `function` for debug points, so this is not redundant
			sig->user_definition_line_ = p_node->token_->token_line_;
			
			//std::cout << *sig << std::endl;
			
			// Check that a built-in function is not already defined with this name; no replacing the built-ins.
			auto signature_iter = function_map_.find(function_name);
			
			if (signature_iter != function_map_.end())
			{
				const EidosFunctionSignature *prior_sig = signature_iter->second.get();
				
				if (prior_sig->internal_function_ || !prior_sig->delegate_name_.empty() || !prior_sig->user_defined_)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::Evaluate_FunctionDecl): cannot replace built-in function " << function_name << "()." << EidosTerminate(p_node->token_);
			}
			
			// Add the user-defined function to our function map (possibly replacing a previous version)
			auto found_iter = function_map_.find(sig->call_name_);
			
			if (found_iter != function_map_.end())
				function_map_.erase(found_iter);
			
			function_map_.insert(EidosFunctionMapPair(sig->call_name_, EidosFunctionSignature_CSP(sig)));
			
			// the signature is now under shared_ptr, or deleted, and so variable sig falls out of scope here
		}
		catch (...) {
			delete sig;
			throw;
		}
	}
	
	// Always return void
	EidosValue_SP result_SP = gStaticEidosValueVOID;
	
	EIDOS_EXIT_EXECUTION_LOG("Evaluate_FunctionDecl()");
	return result_SP;
}



























































