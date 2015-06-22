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
#include "script_value.h"

#include <iostream>


// Utility functions usable by everybody
ScriptValue *ConcatenateScriptValues(std::string p_function_name, std::vector<ScriptValue*> p_arguments);


// A numeric identifier for a function once its name has been looked up; just for efficiency, to allow switch()
enum class FunctionIdentifier {
	kNoFunction = 0,
	kDelegatedFunction,		// implemented through a delegate, such as SLiM
	
	// math functions
	absFunction,
	acosFunction,
	asinFunction,
	atanFunction,
	atan2Function,
	ceilFunction,
	cosFunction,
	expFunction,
	floorFunction,
	isFiniteFunction,
	isInfiniteFunction,
	isNaNFunction,
	logFunction,
	log10Function,
	log2Function,
	productFunction,
	roundFunction,
	sinFunction,
	sqrtFunction,
	sumFunction,
	tanFunction,
	truncFunction,
	
	// summary statistics functions
	maxFunction,
	meanFunction,
	minFunction,
	rangeFunction,
	sdFunction,
	
	// vector construction functions
	cFunction,
	floatFunction,
	integerFunction,
	logicalFunction,
	objectFunction,
	rbinomFunction,
	repFunction,
	repEachFunction,
	rexpFunction,
	rnormFunction,
	rpoisFunction,
	runifFunction,
	sampleFunction,
	seqFunction,
	seqAlongFunction,
	stringFunction,
	
	// value inspection/manipulation functions
	allFunction,
	anyFunction,
	catFunction,
	ifelseFunction,
	ncharFunction,
	pasteFunction,
	printFunction,
	revFunction,
	sizeFunction,
	sortFunction,
	sortByFunction,
	strFunction,
	strsplitFunction,
	substrFunction,
	uniqueFunction,
	whichFunction,
	whichMaxFunction,
	whichMinFunction,
	
	// value type testing/coercion functions
	asFloatFunction,
	asIntegerFunction,
	asLogicalFunction,
	asStringFunction,
	elementFunction,
	isFloatFunction,
	isIntegerFunction,
	isLogicalFunction,
	isNULLFunction,
	isObjectFunction,
	isStringFunction,
	typeFunction,
	
	// bookkeeping functions
	dateFunction,
	functionFunction,
	globalsFunction,
	helpFunction,
	licenseFunction,
	rmFunction,
	setSeedFunction,
	stopFunction,
	timeFunction,
	versionFunction,
	
	// object instantiation
	PathFunction
};

// Functions that are built into SLiMscript are handled internally.  However, it is also possible for external agents to register their own
// functions with SLiMscript; this is how SLiM gets its functions into SLiMscript.  This registration is done with a delegate object and
// a delegate function pointer (NOT a pointer to method).  This is the prototype for a function implementation delegate.  The function
// pointed to will be called, and passed the delegate object along with other parameters for the function call.
typedef ScriptValue *(*SLiMDelegateFunctionPtr)(void *delegate, std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter);


#endif /* defined(__SLiM__script_functions__) */






























































