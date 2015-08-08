//
//  eidos_interpreter.h
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

/*
 
 The class EidosInterpreter embodies an interpreter for a script, and handles symbol lookup, operation execution,
 etc., using classes that are, for simplicity, also defined in this header.
 
 */

#ifndef __Eidos__eidos_interpreter__
#define __Eidos__eidos_interpreter__

#include <vector>
#include <string>
#include <map>

#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_functions.h"
#include "eidos_symbol_table.h"


// typedefs used to set up our map table of EidosFunctionSignature objects
typedef std::pair<std::string, const EidosFunctionSignature*> EidosFunctionMapPair;
typedef std::map<std::string, const EidosFunctionSignature*> EidosFunctionMap;

// utility functions
bool TypeCheckAssignmentOfEidosValueIntoEidosValue(EidosValue *base_value, EidosValue *destination_value);	// codifies what promotions can occur in assignment


// A class representing a script interpretation context with all associated symbol table state
class EidosInterpreter
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	const EidosASTNode *root_node_;				// not owned
	EidosSymbolTable &global_symbols_;				// NOT OWNED: whoever creates us must give us a reference to a symbol table, which we use
	EidosFunctionMap *function_map_;			// NOT OWNED: a map table of EidosFunctionSignature objects, keyed by function name
												// the function_map_ pointer itself is owned, and will be deleted unless it is the static built-in map
	
	// flags to handle next/break statements in do...while, while, and for loops
	bool next_statement_hit_ = false;
	bool break_statement_hit_ = false;
	bool return_statement_hit_ = false;
	
	// flags and streams for execution logging – a trace of the DFS of the parse tree
	bool logging_execution_ = false;
	int execution_log_indent_ = 0;
	std::ostringstream *execution_log_ = nullptr;		// allocated lazily
	
	// an output stream for output from executed nodes and functions; this goes into the user's console
	std::ostringstream *execution_output_ = nullptr;	// allocated lazily
	
public:
	
	EidosInterpreter(const EidosInterpreter&) = delete;					// no copying
	EidosInterpreter& operator=(const EidosInterpreter&) = delete;		// no copying
	EidosInterpreter(void) = delete;										// no null construction
	
	EidosInterpreter(const EidosScript &p_script, EidosSymbolTable &p_symbols);				// we use the passed symbol table but do not own it
	EidosInterpreter(const EidosASTNode *p_root_node_, EidosSymbolTable &p_symbols);		// we use the passed symbol table but do not own it
	
	~EidosInterpreter(void);												// destructor
	
	void *context_pointer_ = nullptr;		// a pointer to the Context object that owns this interpreter; what this means and is used for is Context-dependent
	
	inline std::string IndentString(int p_indent_level) { return std::string(p_indent_level * 2, ' '); };
	
	void SetShouldLogExecution(bool p_log);
	bool ShouldLogExecution(void);
	std::string ExecutionLog(void);
	
	std::ostringstream &ExecutionOutputStream(void);			// lazy allocation; all use of execution_output_ should get it through this accessor
	std::string ExecutionOutput(void);
	
	EidosSymbolTable &GetSymbolTable(void);						// the returned reference is to the symbol table that the interpreter has borrowed
	
	// Evaluation methods; the caller owns the returned EidosValue object
	EidosValue *EvaluateInternalBlock(void);					// the starting point for internally executed blocks, which require braces and suppress output
	EidosValue *EvaluateInterpreterBlock(bool p_print_output);	// the starting point for executed blocks in Eidos, which do not require braces
	
	void _ProcessSubscriptAssignment(EidosValue **p_base_value_ptr, EidosGlobalStringID *p_property_string_id_ptr, std::vector<int> *p_indices_ptr, const EidosASTNode *p_parent_node);
	void _AssignRValueToLValue(EidosValue *rvalue, const EidosASTNode *p_lvalue_node);
	
	EidosValue *EvaluateNode(const EidosASTNode *p_node);
	
	EidosValue *Evaluate_NullStatement(const EidosASTNode *p_node);
	EidosValue *Evaluate_CompoundStatement(const EidosASTNode *p_node);
	EidosValue *Evaluate_RangeExpr(const EidosASTNode *p_node);
	EidosValue *Evaluate_FunctionCall(const EidosASTNode *p_node);
	EidosValue *Evaluate_Subset(const EidosASTNode *p_node);
	EidosValue *Evaluate_MemberRef(const EidosASTNode *p_node);
	EidosValue *Evaluate_Plus(const EidosASTNode *p_node);
	EidosValue *Evaluate_Minus(const EidosASTNode *p_node);
	EidosValue *Evaluate_Mod(const EidosASTNode *p_node);
	EidosValue *Evaluate_Mult(const EidosASTNode *p_node);
	EidosValue *Evaluate_Div(const EidosASTNode *p_node);
	EidosValue *Evaluate_Exp(const EidosASTNode *p_node);
	EidosValue *Evaluate_And(const EidosASTNode *p_node);
	EidosValue *Evaluate_Or(const EidosASTNode *p_node);
	EidosValue *Evaluate_Assign(const EidosASTNode *p_node);
	EidosValue *Evaluate_Eq(const EidosASTNode *p_node);
	EidosValue *Evaluate_Lt(const EidosASTNode *p_node);
	EidosValue *Evaluate_LtEq(const EidosASTNode *p_node);
	EidosValue *Evaluate_Gt(const EidosASTNode *p_node);
	EidosValue *Evaluate_GtEq(const EidosASTNode *p_node);
	EidosValue *Evaluate_Not(const EidosASTNode *p_node);
	EidosValue *Evaluate_NotEq(const EidosASTNode *p_node);
	EidosValue *Evaluate_Number(const EidosASTNode *p_node);
	EidosValue *Evaluate_String(const EidosASTNode *p_node);
	EidosValue *Evaluate_Identifier(const EidosASTNode *p_node);
	EidosValue *Evaluate_If(const EidosASTNode *p_node);
	EidosValue *Evaluate_Do(const EidosASTNode *p_node);
	EidosValue *Evaluate_While(const EidosASTNode *p_node);
	EidosValue *Evaluate_For(const EidosASTNode *p_node);
	EidosValue *Evaluate_Next(const EidosASTNode *p_node);
	EidosValue *Evaluate_Break(const EidosASTNode *p_node);
	EidosValue *Evaluate_Return(const EidosASTNode *p_node);
	
	// Function and method dispatch/execution; these are implemented in script_functions.cpp
	static std::vector<const EidosFunctionSignature *> &BuiltInFunctions(void);
	static EidosFunctionMap *BuiltInFunctionMap(void);
	
	inline void RegisterFunctionMap(EidosFunctionMap *p_function_map) { function_map_ = p_function_map; };
	
	EidosValue *ExecuteFunctionCall(const std::string &p_function_name, const EidosFunctionSignature *p_function_signature, EidosValue *const *const p_arguments, int p_argument_count);
	EidosValue *ExecuteMethodCall(EidosValue_Object *method_object, EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count);
	
	// Utility static methods
	static int64_t IntForNumberToken(const EidosToken *p_token);
};


#endif /* defined(__Eidos__eidos_interpreter__) */



































































