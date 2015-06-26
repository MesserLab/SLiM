//
//  script_interpreter.h
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

/*
 
 The class ScriptInterpreter embodies an interpreter for a script, and handles symbol lookup, operation execution,
 etc., using classes that are, for simplicity, also defined in this header.
 
 */

#ifndef __SLiM__script_interpreter__
#define __SLiM__script_interpreter__

#include <vector>
#include <string>
#include <map>

#include "mutation.h"
#include "script.h"
#include "script_value.h"
#include "script_functions.h"
#include "script_symbols.h"


// typedefs used to set up our map table of FunctionSignature objects
typedef std::pair<std::string, const FunctionSignature*> FunctionMapPair;
typedef std::map<std::string, const FunctionSignature*> FunctionMap;

// utility functions
bool TypeCheckAssignmentOfValueIntoValue(ScriptValue *base_value, ScriptValue *destination_value);	// codifies what promotions can occur in assignment


// A class representing a script interpretation context with all associated symbol table state
class ScriptInterpreter
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	const ScriptASTNode *root_node_;				// not owned
	SymbolTable &global_symbols_;					// NOT OWNED: whoever creates us must give us a reference to a symbol table, which we use
	FunctionMap *function_map_;						// NOT OWNED: a map table of FunctionSignature objects, keyed by function name
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
	
	ScriptInterpreter(const ScriptInterpreter&) = delete;					// no copying
	ScriptInterpreter& operator=(const ScriptInterpreter&) = delete;		// no copying
	ScriptInterpreter(void) = delete;										// no null construction
	
	ScriptInterpreter(const Script &p_script, SymbolTable &p_symbols);					// we use the passed symbol table but do not own it
	ScriptInterpreter(const ScriptASTNode *p_root_node_, SymbolTable &p_symbols);		// we use the passed symbol table but do not own it
	void SharedInitialization(void);
	
	~ScriptInterpreter(void);												// destructor
	
	inline std::string IndentString(int p_indent_level) { return std::string(p_indent_level * 2, ' '); };
	
	void SetShouldLogExecution(bool p_log);
	bool ShouldLogExecution(void);
	std::string ExecutionLog(void);
	
	std::ostringstream &ExecutionOutputStream(void);	// lazy allocation; all use of execution_output_ should get it through this accessor
	std::string ExecutionOutput(void);
	
	SymbolTable &GetSymbolTable(void);						// the returned reference is to the symbol table that the interpreter has borrowed
	
	// Evaluation methods; the caller owns the returned ScriptValue object
	ScriptValue *EvaluateScriptBlock(void);			// the starting point for script blocks in SLiM simulations, which require braces
	ScriptValue *EvaluateInterpreterBlock(void);	// the starting point for executed blocks in SLiMscript, which do not require braces
	
	void _ProcessSubscriptAssignment(ScriptValue **p_base_value_ptr, std::string *p_member_name_ptr, std::vector<int> *p_indices_ptr, const ScriptASTNode *p_parent_node);
	void _AssignRValueToLValue(ScriptValue *rvalue, const ScriptASTNode *p_lvalue_node);
	
	ScriptValue *EvaluateNode(const ScriptASTNode *p_node);
	
	ScriptValue *Evaluate_NullStatement(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_CompoundStatement(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_RangeExpr(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_FunctionCall(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Subset(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_MemberRef(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Plus(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Minus(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Mod(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Mult(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Div(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Exp(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_And(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Or(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Assign(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Eq(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Lt(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_LtEq(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Gt(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_GtEq(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Not(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_NotEq(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Number(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_String(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Identifier(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_If(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Do(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_While(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_For(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Next(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Break(const ScriptASTNode *p_node);
	ScriptValue *Evaluate_Return(const ScriptASTNode *p_node);
	
	// Function and method dispatch/execution; these are implemented in script_functions.cpp
	static std::vector<const FunctionSignature *> &BuiltInFunctions(void);
	static FunctionMap *BuiltInFunctionMap(void);
	
	inline void RegisterFunctionMap(FunctionMap *p_function_map) { function_map_ = p_function_map; };
	
	ScriptValue *ExecuteFunctionCall(std::string const &p_function_name, ScriptValue *const *const p_arguments, int p_argument_count);
	ScriptValue *ExecuteMethodCall(ScriptValue_Object *method_object, std::string const &_method_name, ScriptValue *const *const p_arguments, int p_argument_count);
	
	// Utility static methods
	static int64_t IntForNumberToken(const ScriptToken *p_token);
};


#endif /* defined(__SLiM__script_interpreter__) */



































































