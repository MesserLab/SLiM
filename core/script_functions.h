//
//  script_functions.h
//  SLiM
//
//  Created by Ben Haller on 4/6/15.
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
 
 This file contains most of the code for processing function calls in the SLiMScript interpreter.
 
 */

#ifndef __SLiM__script_functions__
#define __SLiM__script_functions__

#include "script_interpreter.h"

#include <iostream>


void CheckArgumentsAgainstSignature(std::string const &p_call_type, FunctionSignature const &p_signature, std::vector<ScriptValue*> const &p_arguments);

ScriptValue *ExecuteFunctionCall(std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter);

ScriptValue *ExecuteMethodCall(ScriptValue_Proxy *method_object, std::string const &_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter);


// A numeric identifier for a function once its name has been looked up; just for efficiency, to allow switch()
enum class FunctionIdentifier {
	kNoFunction = 0,
	
	// data construction functions
	repFunction,
	repEachFunction,
	seqFunction,
	seqAlongFunction,
	cFunction,
	
	// data inspection/manipulation functions
	printFunction,
	catFunction,
	sizeFunction,
	strFunction,
	sumFunction,
	prodFunction,
	rangeFunction,
	minFunction,
	maxFunction,
	whichMinFunction,
	whichMaxFunction,
	whichFunction,
	meanFunction,
	sdFunction,
	revFunction,
	sortFunction,
	anyFunction,
	allFunction,
	strsplitFunction,
	pasteFunction,
	
	// data class testing/coercion functions
	classFunction,
	isLogicalFunction,
	isStringFunction,
	isIntegerFunction,
	isFloatFunction,
	isObjectFunction,
	asLogicalFunction,
	asStringFunction,
	asIntegerFunction,
	asFloatFunction,
	isFiniteFunction,
	isNaNFunction,
	
	// math functions, all implemented using the standard C++ function of the same name
	acosFunction,
	asinFunction,
	atanFunction,
	atan2Function,
	cosFunction,
	sinFunction,
	tanFunction,
	expFunction,
	logFunction,
	log10Function,
	log2Function,
	sqrtFunction,
	absFunction,
	ceilFunction,
	floorFunction,
	roundFunction,
	truncFunction,
	
	// bookkeeping functions
	versionFunction,
	licenseFunction,
	helpFunction,
	lsFunction,
	dateFunction,
	timeFunction,
	
	// proxy instantiation
	PathFunction
};

struct FunctionSignature {
	std::string function_name_;
	FunctionIdentifier function_id_;
	ScriptValueType return_type_;					// if kValueNULL, no assumptions are made about the return type
	bool uses_var_args_;							// if true, the function accepts a variable number of arguments
	int minimum_arg_count_;							// if var_args_function == false, this is also the maximum arg count
	std::vector<ScriptValueMask> arg_types_;		// the expected types for each argument, as a mask
	
	FunctionSignature(std::string p_function_name,
					  FunctionIdentifier p_function_id,
					  ScriptValueType p_return_type,
					  bool p_uses_var_args_,
					  int p_minimum_arg_count,
					  std::vector<ScriptValueMask> p_arg_types);
};


typedef std::pair<std::string, FunctionSignature> FunctionMapPair;
typedef std::map<std::string, FunctionSignature> FunctionMap;

void RegisterSignature(FunctionMap &p_map, FunctionSignature p_signature);
FunctionMap BuiltInFunctionMap(void);


#endif /* defined(__SLiM__script_functions__) */






























































