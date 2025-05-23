//
//  eidos_interpreter.h
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

/*
 
 The class EidosInterpreter embodies an interpreter for a script, and handles symbol lookup, operation execution,
 etc., using classes that are, for simplicity, also defined in this header.
 
 */

#ifndef __Eidos__eidos_interpreter__
#define __Eidos__eidos_interpreter__

#include <vector>
#include <string>
#include <map>
#include <cassert>

#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_functions.h"
#include "eidos_symbol_table.h"
#include "eidos_ast_node.h"

class EidosCallSignature;


// EidosInterpreter keeps track of the EidosContext object that is in charge of the whole show.  This should
// be an object of type EidosObject; this allows dynamic_cast to work, whereas void* would not.  This
// is optional; you can pass nullptr if you don't want to register a Context.  Eidos itself does nothing
// with the Context except keep track of it for you; the purpose of it is to allow your Eidos method
// implementations, function implementations, etc., to get up to the big picture without every object in
// your object graph having a back pointer of some kind.  If you think this is gross, don't use it.  :->
typedef EidosObject EidosContext;

// typedefs used to set up our map table of EidosFunctionSignature objects; std::map is used instead of
// std::unordered_map mostly for convenience, speed should not matter much since signatures get cached anyway
typedef std::pair<std::string, EidosFunctionSignature_CSP> EidosFunctionMapPair;
typedef std::map<std::string, EidosFunctionSignature_CSP> EidosFunctionMap;

// utility functions
bool TypeCheckAssignmentOfEidosValueIntoEidosValue(const EidosValue &p_base_value, const EidosValue &p_destination_value);	// codifies what promotions can occur in assignment


// This typedef is used to represent the line numbers that have "debug points" associated with them in SLiMgui.
// This is a bit convoluted so that we can forward-declare it in other headers even though it's a typedef of a
// templated class; I couldn't figure out a better way to do it than wrapping it in a struct, ugh.
#ifdef SLIMGUI
#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
struct EidosInterpreterDebugPointsSet_struct {
	robin_hood::unordered_set<int> set;
};
typedef EidosInterpreterDebugPointsSet_struct EidosInterpreterDebugPointsSet;
#elif STD_UNORDERED_MAP_HASHING
#include <unordered_set>
struct EidosInterpreterDebugPointsSet_struct {
	std::unordered_set<int> set;
};
typedef EidosInterpreterDebugPointsSet_struct EidosInterpreterDebugPointsSet;
#endif
#endif


// A class representing a script interpretation context with all associated symbol table state
class EidosInterpreter
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	EidosContext *eidos_context_;				// not owned
	const EidosASTNode *root_node_;				// not owned
	EidosSymbolTable *global_symbols_;			// NOT OWNED: whoever creates us must give us a reference to a symbol table, which we use
	
	EidosFunctionMap &function_map_;			// a map table of EidosFunctionSignature objects, keyed by function name
	
	// flags to handle next/break statements in do...while, while, and for loops
	bool next_statement_hit_ = false;
	bool break_statement_hit_ = false;
	bool return_statement_hit_ = false;
	
	// flags and streams for execution logging – a trace of the DFS of the parse tree
	bool logging_execution_ = false;
	int execution_log_indent_ = 0;
	std::ostringstream *execution_log_ = nullptr;		// allocated lazily; for internal tokenization/parse/execution traces
	
	// output streams for standard and error output from executed nodes and functions; these go into the user's console
	std::ostream &execution_output_;
	std::ostream &error_output_;
	
	// a flag controlling the special use of SLiMUndefinedIdentifierException by
	// EidosSymbolTable::_GetValue(); see Community::_EvaluateTickRangeNode() for
	// the use of this facility.
	bool use_custom_undefined_identifier_raise_ = false;
	
	// similarly, a flag controlling the special use of SLiMUndefinedFunctionException
	// by EidosInterpreter::Evaluate_Call(); see Community::_EvaluateTickRangeNode() for
	// the use of this facility.
	bool use_custom_undefined_function_raise_ = false;
	
	// The standard built-in function map, set up by CacheBuiltInFunctionMap()
	static EidosFunctionMap *s_built_in_function_map_;
	
#ifdef SLIMGUI
	EidosInterpreterDebugPointsSet *debug_points_ = nullptr;		// NOT OWNED; line numbers for all lines with debugging points set
#endif
	
public:
	
	EidosInterpreter(const EidosInterpreter&) = delete;					// no copying
	EidosInterpreter& operator=(const EidosInterpreter&) = delete;		// no copying
	EidosInterpreter(void) = delete;										// no null construction
	
	EidosInterpreter(const EidosScript &p_script, EidosSymbolTable &p_symbols, EidosFunctionMap &p_functions, EidosContext *p_eidos_context, std::ostream &p_outstream, std::ostream &p_errstream);			// we use the passed symbol table but do not own it
	EidosInterpreter(const EidosASTNode *p_root_node_, EidosSymbolTable &p_symbols, EidosFunctionMap &p_functions, EidosContext *p_eidos_context, std::ostream &p_outstream, std::ostream &p_errstream);	// we use the passed symbol table but do not own it
	
	inline ~EidosInterpreter(void)
	{
		if (execution_log_)
			delete execution_log_;
		
#ifdef SLIMGUI
		debug_points_ = nullptr;
#endif
	}
	
	inline __attribute__((always_inline)) std::string IndentString(int p_indent_level) { return std::string(p_indent_level * 2, ' '); };
	
	void SetShouldLogExecution(bool p_log);
	bool ShouldLogExecution(void);
	std::string ExecutionLog(void);
	
	inline __attribute__((always_inline)) std::ostream &ExecutionOutputStream(void) { return execution_output_; }
	inline __attribute__((always_inline)) std::ostream &ErrorOutputStream(void) { return error_output_; }
	
	inline __attribute__((always_inline)) EidosSymbolTable &SymbolTable(void) { return *global_symbols_; };			// the returned reference is to the symbol table that the interpreter has borrowed
	inline __attribute__((always_inline)) EidosFunctionMap &FunctionMap(void) { return function_map_; };				// the returned reference is to the function map that the interpreter has borrowed
	inline __attribute__((always_inline)) EidosContext *Context(void) const { return eidos_context_; };
	
	// Evaluation methods; the caller owns the returned EidosValue object
	EidosValue_SP EvaluateInternalBlock(EidosScript *p_script_for_block);		// the starting point for internally executed blocks, which require braces and suppress output
	EidosValue_SP EvaluateInterpreterBlock(bool p_print_output, bool p_return_last_value);		// the starting point for executed blocks in Eidos, which do not require braces
	
	void _ProcessSubsetAssignment(EidosValue_SP *p_base_value_ptr, EidosGlobalStringID *p_property_string_id_ptr, std::vector<int> *p_indices_ptr, const EidosASTNode *p_parent_node);
	void _AssignRValueToLValue(EidosValue_SP p_rvalue, const EidosASTNode *p_lvalue_node);
	EidosValue_SP _Evaluate_RangeExpr_Internal(const EidosASTNode *p_node, const EidosValue &p_first_child_value, const EidosValue &p_second_child_value);
	
	// These process the arguments for a call node (either a function call or a method call), ignoring the first child
	// node since that is the call name node, not an argument node.  Named arguments and default arguments are handled
	// here, and a final argument list is placed into the argument_cache_'s argument_buffer_, in the node, for use.
	// Note that recursion in Eidos code requires the use of separately allocated argument buffers for the recursive
	// calls; slow, but presumably an edge case for typical code.  If Eidos interpretation is ever made multithreaded,
	// the argument_buffer_in_use_ flag will need to be governed by a lock.
	void _CreateArgumentList(const EidosASTNode *p_node, const EidosCallSignature *p_call_signature);
	
	std::vector<EidosValue_SP> *_ProcessArgumentList(const EidosASTNode *p_node, const EidosCallSignature *p_call_signature)
	{
		EidosASTNode_ArgumentCache *argument_cache = p_node->argument_cache_;	// the argument cache lives on the call node itself, conventionally
		std::vector<EidosValue_SP> *argument_buffer = nullptr;
		
		// Allocate and configure an argument cache struct if we don't already have one
		if (!argument_cache)
		{
			// We don't already have an argument cache, so create one and use it
			_CreateArgumentList(p_node, p_call_signature);
			argument_cache = p_node->argument_cache_;
			assert(argument_cache);		// static analyzer doesn't understand that _CreateArgumentList() created the cache
			argument_buffer = &argument_cache->argument_buffer_;
			argument_cache->argument_buffer_in_use_ = true;
		}
		else if (argument_cache->argument_buffer_in_use_)
		{
			// We have an argument cache, but its argument buffer is already in use, so make a new temporary one
			argument_buffer = new std::vector<EidosValue_SP>;
			argument_buffer->resize(argument_cache->argument_buffer_.size());	// fill with nullptr up to the required size
			
			// Copy the non-filled (default/constant) arguments into the temporary argument buffer
			for (uint8_t no_fill_index : argument_cache->no_fill_index_)
				(*argument_buffer)[no_fill_index] = argument_cache->argument_buffer_[no_fill_index];
		}
		else
		{
			// We have an argument cache, and its argument buffer is not already in use, so use it now
			argument_buffer = &argument_cache->argument_buffer_;
			argument_cache->argument_buffer_in_use_ = true;
		}
		
		std::vector<EidosASTNode_ArgumentFill> &fill_info = argument_cache->fill_info_;
		
		// Now our argument cache is all ready to use; we just need to fill and type-check arguments
		for (const EidosASTNode_ArgumentFill &fill : fill_info)
		{
			// Get the argument value by evaluating the AST node responsible for providing it
			EidosValue_SP arg_value = FastEvaluateNode(fill.fill_node_);
			
			// Type-check the value; note that default/constant arguments are type-checked during signature construction
			p_call_signature->CheckArgument(arg_value.get(), fill.signature_index_);
			
			// Move the argument value into the argument buffer
			(*argument_buffer)[fill.fill_index_] = std::move(arg_value);
		}
		
		// call CheckArguments() to double-check for errors when in DEBUG; this can be removed eventually, it's just for the transition to the new argument buffers
#if DEBUG
		p_call_signature->CheckArguments(*argument_buffer);
#endif
		
		return argument_buffer;
	}
	
	inline void _DeprocessArgumentList(const EidosASTNode *p_node, std::vector<EidosValue_SP> *p_argument_buffer)
	{
		EidosASTNode_ArgumentCache *argument_cache = p_node->argument_cache_;
		
#if DEBUG
		if (!argument_cache)
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::_DeprocessArgumentList): (internal) missing argument cache." << EidosTerminate(nullptr);
#endif
		
		if (p_argument_buffer == &argument_cache->argument_buffer_)
		{
			// We're deprocessing the argument list inside the argument cache, so we just want to nil out the filled entries
			std::vector<EidosASTNode_ArgumentFill> &fill_info = argument_cache->fill_info_;
			
			// Loop through the fill indices and reset the shared pointers so the EidosValues get released in a timely fashion
			// Note that we leave the non-fill indices alone; they are default values and constants that will get reused next time
			for (const EidosASTNode_ArgumentFill &fill : fill_info)
				(*p_argument_buffer)[fill.fill_index_].reset();
			
			// The argument cache's argument buffer is no longer in use
			argument_cache->argument_buffer_in_use_ = false;
		}
		else
		{
			// We're deprocessing a heap-allocated argument buffer created due to recursion; free everything
			// The argument cache's argument buffer remains in use, by the root call of the recursion
			delete p_argument_buffer;
		}
	}
	
#ifdef SLIMGUI
	void _LogCallArguments(const EidosCallSignature *call_signature, std::vector<EidosValue_SP> *argument_buffer);
#endif
	
	EidosValue_SP DispatchUserDefinedFunction(const EidosFunctionSignature &p_function_signature, const std::vector<EidosValue_SP> &p_arguments);
	
	void NullReturnRaiseForNode(const EidosASTNode *p_node);
	EidosValue_SP EvaluateNode(const EidosASTNode *p_node);
	
	EidosValue_SP Evaluate_NullStatement(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_CompoundStatement(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_RangeExpr(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Call(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Subset(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_MemberRef(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Plus(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Minus(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Mod(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Mult(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Div(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Conditional(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Exp(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_And(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Or(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Assign(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Assign_R(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Eq(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Lt(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_LtEq(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Gt(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_GtEq(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Not(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_NotEq(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Number(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_String(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Identifier(const EidosASTNode *p_node);
	inline __attribute__((always_inline)) EidosValue *Evaluate_Identifier_RAW(const EidosASTNode *p_node)
	{
		// this is a fast inline version of Evaluate_Identifier() that returns a raw EidosValue *
		// it also skips call logging (in DEBUG and SLiMgui), and assumes no value is cached (because
		// this code path is used when the expectation is that we're fetching an object from the symbol table)
		// use a cached value from EidosASTNode::_OptimizeConstants() if present
		
		return global_symbols_->GetValueRawOrRaiseForASTNode(p_node);	// raises if undefined
	}
	EidosValue_SP Evaluate_If(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Do(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_While(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_For(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Next(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Break(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_Return(const EidosASTNode *p_node);
	EidosValue_SP Evaluate_FunctionDecl(const EidosASTNode *p_node);
	
	// Function dispatch/execution; these are implemented in eidos_functions.cpp
	static const std::vector<EidosFunctionSignature_CSP> &BuiltInFunctions(void);
	static inline __attribute__((always_inline)) const EidosFunctionMap *BuiltInFunctionMap(void) { return s_built_in_function_map_; }
	static void CacheBuiltInFunctionMap(void);	// must be called by Eidos_WarmUp() before BuiltInFunctionMap() is called
	
	// Utility static methods for numeric conversions
	static int64_t NonnegativeIntegerForString(const std::string &p_number_string, const EidosToken *p_blame_token);
	static double FloatForString(const std::string &p_number_string, const EidosToken *p_blame_token);
	static EidosValue_SP NumericValueForString(const std::string &p_number_string, const EidosToken *p_blame_token);
	
	// An inline function for super-fast node evaluation, skipping EvaluateNode()
	inline __attribute__((always_inline)) EidosValue_SP FastEvaluateNode(const EidosASTNode *p_node)
	{
		// We should have cached p_node->cached_evaluator.  If not, the call below will simply crash with a zero deref, which
		// is sufficiently diagnostic given that it should never happen.  The if() slows us down quite a bit!
		
		//if (p_node->cached_evaluator)
			return (this->*(p_node->cached_evaluator_))(p_node);
		//else
		//	return EvaluateNode(p_node);
	}
	
	// Support for tick range expression parsing; see Community::_EvaluateTickRangeNode()
	bool UseCustomUndefinedIdentifierRaise(void) { return use_custom_undefined_identifier_raise_; }
	void SetUseCustomUndefinedIdentifierRaise(bool p_flag) { use_custom_undefined_identifier_raise_ = p_flag; }
	
	bool UseCustomUndefinedFunctionRaise(void) { return use_custom_undefined_function_raise_; }
	void SetUseCustomUndefinedFunctionRaise(bool p_flag) { use_custom_undefined_function_raise_ = p_flag; }
};


#endif /* defined(__Eidos__eidos_interpreter__) */



































































