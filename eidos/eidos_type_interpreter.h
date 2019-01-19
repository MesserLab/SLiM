//
//  eidos_type_interpreter.h
//  Eidos
//
//  Created by Ben Haller on 5/8/16.
//  Copyright (c) 2016-2019 Philipp Messer.  All rights reserved.
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
 
 The class EidosTypeInterpreter interprets a script solely for the purpose of producing type information about variables defined
 by the script, for code completion.  It is parallel to EidosInterpreter in many ways, and uses EidosTypeTable in place of
 EidosSymbolTable.  Unlike EidosInterpreter, it is designed not to raise with malformed scripts; it is error-tolerant and just
 does the best it can.  The worst that happens is that code completion has incorrect or missing information about some variables.
 
 */

#ifndef __Eidos__eidos_type_interpreter__
#define __Eidos__eidos_type_interpreter__

#include "eidos_script.h"
#include "eidos_type_table.h"
#include "eidos_ast_node.h"
#include "eidos_interpreter.h"


// This is used to record the object class returned by function calls encountered during type interpreting.  This is used to recall
// the return type of a function at the beginning of a key path for code completion, in cases where the function signature is not
// sufficient to determine that, such as sample(), rep(), etc.  See -[EidosTextView completionsForKeyPathEndingInTokenIndex:...].
typedef std::pair<int32_t, const EidosObjectClass *> EidosCallTypeEntry;
typedef std::map<int32_t, const EidosObjectClass *> EidosCallTypeTable;


class EidosTypeInterpreter
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
protected:
	const EidosASTNode *root_node_;				// not owned
	EidosTypeTable *global_symbols_;			// NOT OWNED: whoever creates us must give us a reference to a type table, which we use
	
	EidosFunctionMap &function_map_;			// NOT OWNED: a map table of EidosFunctionSignature objects, keyed by function name

	EidosCallTypeTable &call_type_map_;			// NOT OWNED: a map table of types for function calls encountered, keyed by position
	bool defines_only_;							// if true, we add symbols only for defineConstant() calls, not for assignments
	
	// for autocompletion of argument names, set up by TypeEvaluateInterpreterBlock_AddArgumentCompletions()
	std::vector<std::string> *argument_completions_ = nullptr;
	size_t script_length_ = 0;
	
public:
	
	EidosTypeInterpreter(const EidosTypeInterpreter&) = delete;					// no copying
	EidosTypeInterpreter& operator=(const EidosTypeInterpreter&) = delete;		// no copying
	EidosTypeInterpreter(void) = delete;										// no null construction
	
	EidosTypeInterpreter(const EidosScript &p_script, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only = false);			// we use the passed symbol table but do not own it
	EidosTypeInterpreter(const EidosASTNode *p_root_node_, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only = false);		// we use the passed symbol table but do not own it
	
	virtual ~EidosTypeInterpreter(void);										// destructor
	
	inline __attribute__((always_inline)) EidosTypeTable &SymbolTable(void) { return *global_symbols_; };	// the returned reference is to the symbol table that the interpreter has borrowed
	inline __attribute__((always_inline)) EidosFunctionMap &FunctionMap(void) { return function_map_; };	// the returned reference is to the function map that the interpreter has borrowed
	inline __attribute__((always_inline)) EidosCallTypeTable &CallTypeMap(void) { return call_type_map_; };	// the returned reference is to the call type map that the interpreter has borrowed
	
	// Evaluation methods; the caller owns the returned EidosValue object
	EidosTypeSpecifier TypeEvaluateInterpreterBlock();	// the starting point for executed blocks in Eidos, which do not require braces
	EidosTypeSpecifier TypeEvaluateInterpreterBlock_AddArgumentCompletions(std::vector<std::string> *p_argument_completions, size_t p_script_length);	// for autocompletion of argument names
	
	EidosTypeSpecifier TypeEvaluateNode(const EidosASTNode *p_node);
	
	EidosTypeSpecifier TypeEvaluate_NullStatement(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_CompoundStatement(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_RangeExpr(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Call(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Subset(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_MemberRef(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Plus(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Minus(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Mod(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Mult(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Div(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Conditional(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Exp(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_And(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Or(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Assign(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Eq(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Lt(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_LtEq(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Gt(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_GtEq(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Not(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_NotEq(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Number(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_String(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Identifier(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_If(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Do(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_While(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_For(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Next(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Break(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_Return(const EidosASTNode *p_node);
	EidosTypeSpecifier TypeEvaluate_FunctionDecl(const EidosASTNode *p_node);
	
	// Function/method processing.  These can implement type-defining side effects of calls; for example,
	// defineConstant() has the side effect of defining a new symbol of a particular type.  These are
	// virtual so the Context can subclass this class to define additional functions/methods with such
	// side effects.
	virtual EidosTypeSpecifier _TypeEvaluate_FunctionCall_Internal(std::string const &p_function_name, const EidosFunctionSignature *p_function_signature, const std::vector<EidosASTNode *> &p_arguments);
	
	virtual EidosTypeSpecifier _TypeEvaluate_MethodCall_Internal(const EidosObjectClass *p_target, const EidosMethodSignature *p_method_signature, const std::vector<EidosASTNode *> &p_arguments);
	
	// Argument processing; handles default arguments and named arguments
	void _ProcessArgumentListTypes(const EidosASTNode *p_node, const EidosCallSignature *p_call_signature, std::vector<EidosASTNode *> &p_arguments);
};


#endif /* __Eidos__eidos_type_interpreter__ */











































































