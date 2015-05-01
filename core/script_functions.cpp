//
//  script_functions.cpp
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


#include "script_functions.h"
#include "script_pathproxy.h"

#include "math.h"


using std::string;
using std::vector;
using std::map;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


ScriptValue *Execute_c(string p_function_name, vector<ScriptValue*> p_arguments);
ScriptValue *Execute_rep(string p_function_name, vector<ScriptValue*> p_arguments);
ScriptValue *Execute_repEach(string p_function_name, vector<ScriptValue*> p_arguments);


//
//	Construct our built-in function map
//

void RegisterSignature(FunctionMap &p_map, const FunctionSignature *p_signature)
{
	p_map.insert(FunctionMapPair(p_signature->function_name_, p_signature));
}

FunctionMap BuiltInFunctionMap(void)
{
	FunctionMap map;
	
	
	// data construction functions
	
	RegisterSignature(map, (new FunctionSignature("rep",		FunctionIdentifier::repFunction,		ScriptValueType::kValueNULL))->AddAny()->AddInt());
	RegisterSignature(map, (new FunctionSignature("repEach",	FunctionIdentifier::repEachFunction,	ScriptValueType::kValueNULL))->AddAny()->AddInt());
	RegisterSignature(map, (new FunctionSignature("seq",		FunctionIdentifier::seqFunction,		ScriptValueType::kValueNULL))->AddNumeric()->AddNumeric()->AddNumeric_O());
	RegisterSignature(map, (new FunctionSignature("seqAlong",	FunctionIdentifier::seqAlongFunction,	ScriptValueType::kValueInt))->AddAny());
	RegisterSignature(map, (new FunctionSignature("c",			FunctionIdentifier::cFunction,			ScriptValueType::kValueNULL))->AddEllipsis());
	
	
	// data inspection/manipulation functions
	 
	RegisterSignature(map, (new FunctionSignature("print",		FunctionIdentifier::printFunction,		ScriptValueType::kValueNULL))->AddAny());
	RegisterSignature(map, (new FunctionSignature("cat",		FunctionIdentifier::catFunction,		ScriptValueType::kValueNULL))->AddAny());
	RegisterSignature(map, (new FunctionSignature("size",		FunctionIdentifier::sizeFunction,		ScriptValueType::kValueInt))->AddAny());
	
	 /*
	strFunction,
	sumFunction,
	prodFunction,
	meanFunction,
	sdFunction,
	revFunction,
	sortFunction,
	*/
	
	// data class testing/coercion functions
	 
	RegisterSignature(map, (new FunctionSignature("class",		FunctionIdentifier::classFunction,		ScriptValueType::kValueString))->AddAny());
	/*
	isLogicalFunction,
	isStringFunction,
	isIntegerFunction,
	isFloatFunction,
	asLogicalFunction,
	asStringFunction,
	asIntegerFunction,
	asFloatFunction,
	*/
	
	// math functions
	
	RegisterSignature(map, (new FunctionSignature("acos",		FunctionIdentifier::acosFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("asin",		FunctionIdentifier::asinFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("atan",		FunctionIdentifier::atanFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("cos",		FunctionIdentifier::cosFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("sin",		FunctionIdentifier::sinFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("tan",		FunctionIdentifier::tanFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("exp",		FunctionIdentifier::expFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("log",		FunctionIdentifier::logFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("log10",		FunctionIdentifier::log10Function,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("log2",		FunctionIdentifier::log2Function,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("sqrt",		FunctionIdentifier::sqrtFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("ceil",		FunctionIdentifier::ceilFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("floor",		FunctionIdentifier::floorFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("round",		FunctionIdentifier::roundFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("trunc",		FunctionIdentifier::truncFunction,		ScriptValueType::kValueFloat))->AddNumeric());
	RegisterSignature(map, (new FunctionSignature("abs",		FunctionIdentifier::absFunction,		ScriptValueType::kValueNULL))->AddNumeric());
	
	// bookkeeping functions

	RegisterSignature(map, (new FunctionSignature("version",	FunctionIdentifier::versionFunction,	ScriptValueType::kValueNULL)));
	RegisterSignature(map, (new FunctionSignature("license",	FunctionIdentifier::licenseFunction,	ScriptValueType::kValueNULL)));
	RegisterSignature(map, (new FunctionSignature("help",		FunctionIdentifier::helpFunction,		ScriptValueType::kValueNULL))->AddString_OS());
	RegisterSignature(map, (new FunctionSignature("ls",			FunctionIdentifier::lsFunction,			ScriptValueType::kValueNULL)));
	
	// proxy instantiation
	
	RegisterSignature(map, (new FunctionSignature("Path",		FunctionIdentifier::PathFunction,		ScriptValueType::kValueProxy))->AddString_OS());
	
	return map;
}

FunctionMap gBuiltInFunctionMap = BuiltInFunctionMap();


//
//	Executing function calls
//

ScriptValue *Execute_c(string p_function_name, vector<ScriptValue*> p_arguments)
{
#pragma unused(p_function_name)
	ScriptValueType highest_type = ScriptValueType::kValueNULL;
	
	// First figure out our return type, which is the highest-promotion type among all our arguments
	for (ScriptValue *arg_value : p_arguments)
	{
		ScriptValueType arg_type = arg_value->Type();
		
		if (arg_type > highest_type)
			highest_type = arg_type;
	}
	
	// If we've got nothing but NULL, then return NULL
	if (highest_type == ScriptValueType::kValueNULL)
		return new ScriptValue_NULL();
	
	// Create an object of the right return type, concatenate all the arguments together, and return it
	if (highest_type == ScriptValueType::kValueLogical)
	{
		ScriptValue_Logical *result = new ScriptValue_Logical();
		
		for (ScriptValue *arg_value : p_arguments)
			if (arg_value->Type() != ScriptValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushLogical(arg_value->LogicalAtIndex(value_index));
		
		return result;
	}
	else if (highest_type == ScriptValueType::kValueInt)
	{
		ScriptValue_Int *result = new ScriptValue_Int();
		
		for (ScriptValue *arg_value : p_arguments)
			if (arg_value->Type() != ScriptValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushInt(arg_value->IntAtIndex(value_index));
		
		return result;
	}
	else if (highest_type == ScriptValueType::kValueFloat)
	{
		ScriptValue_Float *result = new ScriptValue_Float();
		
		for (ScriptValue *arg_value : p_arguments)
			if (arg_value->Type() != ScriptValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushFloat(arg_value->FloatAtIndex(value_index));
		
		return result;
	}
	else if (highest_type == ScriptValueType::kValueString)
	{
		ScriptValue_String *result = new ScriptValue_String();
		
		for (ScriptValue *arg_value : p_arguments)
			if (arg_value->Type() != ScriptValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushString(arg_value->StringAtIndex(value_index));
		
		return result;
	}
	else
	{
		SLIM_TERMINATION << "ERROR (Execute_c): type '" << highest_type << "' cannot be used with c()." << endl << slim_terminate();
	}
	
	return nullptr;
}

ScriptValue *Execute_rep(string p_function_name, vector<ScriptValue*> p_arguments)
{
	ScriptValue *arg1_value = p_arguments[0];
	int arg1_count = arg1_value->Count();
	ScriptValue *arg2_value = p_arguments[1];
	int arg2_count = arg2_value->Count();
	
	// the return type depends on the type of the first argument, which will get replicated
	ScriptValue *result = arg1_value->NewMatchingType();
	
	if (arg2_count == 1)
	{
		int64_t rep_count = arg2_value->IntAtIndex(0);
		
		for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
			for (int value_idx = 0; value_idx < arg1_count; value_idx++)
				result->PushValueFromIndexOfScriptValue(value_idx, arg1_value);
	}
	else
	{
		SLIM_TERMINATION << "ERROR (Execute_rep): function " << p_function_name << "() requires that its second argument's size() == 1." << endl << slim_terminate();
	}
	
	return result;
}

ScriptValue *Execute_repEach(string p_function_name, vector<ScriptValue*> p_arguments)
{
	ScriptValue *arg1_value = p_arguments[0];
	int arg1_count = arg1_value->Count();
	ScriptValue *arg2_value = p_arguments[1];
	int arg2_count = arg2_value->Count();
	
	// the return type depends on the type of the first argument, which will get replicated
	ScriptValue *result = arg1_value->NewMatchingType();
	
	if (arg2_count == 1)
	{
		int64_t rep_count = arg2_value->IntAtIndex(0);
		
		for (int value_idx = 0; value_idx < arg1_count; value_idx++)
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfScriptValue(value_idx, arg1_value);
	}
	else if (arg2_count == arg1_count)
	{
		for (int value_idx = 0; value_idx < arg1_count; value_idx++)
		{
			int64_t rep_count = arg2_value->IntAtIndex(value_idx);
			
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfScriptValue(value_idx, arg1_value);
		}
	}
	else
	{
		SLIM_TERMINATION << "ERROR (Execute_repEach): function " << p_function_name << "() requires that its second argument's size() either (1) be equal to 1, or (2) be equal to the size() of its first argument." << endl << slim_terminate();
	}
	
	return result;
}

ScriptValue *ExecuteFunctionCall(std::string const &p_function_name, vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	ScriptValue *result = nullptr;
	
	// Get the function signature and check our arguments against it
	auto signature_iter = gBuiltInFunctionMap.find(p_function_name);
	
	if (signature_iter == gBuiltInFunctionMap.end())
		SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): unrecognized function name " << p_function_name << "." << endl << slim_terminate();
	
	const FunctionSignature *signature = signature_iter->second;
	
	signature->CheckArguments("function", p_arguments);
	
	// We predefine variables for the return types, and preallocate them here if possible.  This is for code brevity below.
	ScriptValue_Logical *logical_result = nullptr;
	ScriptValue_Float *float_result = nullptr;
	ScriptValue_Int *int_result = nullptr;
	ScriptValue_String *string_result = nullptr;
	
	switch (signature->return_type_)
	{
		case ScriptValueType::kValueNULL:		break;		// no assumptions made
		case ScriptValueType::kValueLogical:	logical_result = new ScriptValue_Logical();		result = logical_result;	break;
		case ScriptValueType::kValueFloat:		float_result = new ScriptValue_Float();			result = float_result;		break;
		case ScriptValueType::kValueInt:		int_result = new ScriptValue_Int();				result = int_result;		break;
		case ScriptValueType::kValueString:		string_result = new ScriptValue_String();		result = string_result;		break;
		case ScriptValueType::kValueProxy:		break;		// can't premake a proxy object, the function will have to do it
	}
	
	// Prefetch arguments to allow greater brevity in the code below
	int n_args = (int)p_arguments.size();
	ScriptValue *arg1_value = (n_args >= 1) ? p_arguments[0] : nullptr;
	ScriptValueType arg1_type = (n_args >= 1) ? arg1_value->Type() : ScriptValueType::kValueNULL;
	int arg1_count = (n_args >= 1) ? arg1_value->Count() : 0;
	
	/*
	ScriptValue *arg2_value = (n_args >= 2) ? p_arguments[1] : nullptr;
	ScriptValueType arg2_type = (n_args >= 2) ? arg2_value->Type() : ScriptValueType::kValueNULL;
	int arg2_count = (n_args >= 2) ? arg2_value->Count() : 0;
	
	ScriptValue *arg3_value = (n_args >= 3) ? p_arguments[2] : nullptr;
	ScriptValueType arg3_type = (n_args >= 3) ? arg3_value->Type() : ScriptValueType::kValueNULL;
	int arg3_count = (n_args >= 3) ? arg3_value->Count() : 0;
	*/
	
	// Now we look up the function again and actually execute it
	switch (signature->function_id_)
	{
		case FunctionIdentifier::kNoFunction:
			SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): internal logic error." << endl << slim_terminate();
			break;
			
			// data construction functions
			
		case FunctionIdentifier::repFunction:			result = Execute_rep(p_function_name, p_arguments);	break;
		case FunctionIdentifier::repEachFunction:		result = Execute_repEach(p_function_name, p_arguments);	break;
			
		case FunctionIdentifier::seqFunction:
		case FunctionIdentifier::seqAlongFunction:
			SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function unimplemented." << endl << slim_terminate();
			break;
			
		case FunctionIdentifier::cFunction:				result = Execute_c(p_function_name, p_arguments); break;
			
			// data inspection/manipulation functions
			
		case FunctionIdentifier::printFunction:
			p_output_stream << *arg1_value << endl;
			result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
			break;
			
		case FunctionIdentifier::catFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
			{
				if (value_index > 0)
					p_output_stream << " ";
				
				p_output_stream << arg1_value->StringAtIndex(value_index);
			}
			result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
			break;
			
		case FunctionIdentifier::sizeFunction:
			int_result->PushInt(arg1_value->Count());
			break;
			
		case FunctionIdentifier::strFunction:
		case FunctionIdentifier::sumFunction:
		case FunctionIdentifier::prodFunction:
		case FunctionIdentifier::rangeFunction:
		case FunctionIdentifier::minFunction:
		case FunctionIdentifier::maxFunction:
		case FunctionIdentifier::whichMinFunction:
		case FunctionIdentifier::whichMaxFunction:
		case FunctionIdentifier::whichFunction:
		case FunctionIdentifier::meanFunction:
		case FunctionIdentifier::sdFunction:
		case FunctionIdentifier::revFunction:
		case FunctionIdentifier::sortFunction:
		case FunctionIdentifier::anyFunction:
		case FunctionIdentifier::allFunction:
		case FunctionIdentifier::strsplitFunction:
		case FunctionIdentifier::pasteFunction:
			SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function unimplemented." << endl << slim_terminate();
			break;
			
			// data class testing/coercion functions
			
		case FunctionIdentifier::classFunction:
			string_result->PushString(StringForScriptValueType(arg1_value->Type()));
			break;
			
		case FunctionIdentifier::isLogicalFunction:
		case FunctionIdentifier::isStringFunction:
		case FunctionIdentifier::isIntegerFunction:
		case FunctionIdentifier::isFloatFunction:
		case FunctionIdentifier::isObjectFunction:
		case FunctionIdentifier::asLogicalFunction:
		case FunctionIdentifier::asStringFunction:
		case FunctionIdentifier::asIntegerFunction:
		case FunctionIdentifier::asFloatFunction:
		case FunctionIdentifier::isFiniteFunction:
		case FunctionIdentifier::isNaNFunction:
			SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function unimplemented." << endl << slim_terminate();
			break;
			
			// math functions, all implemented using the standard C++ function of the same name
			
		case FunctionIdentifier::acosFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(acos(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::asinFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(asin(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::atanFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(atan(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::atan2Function:
			SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function unimplemented." << endl << slim_terminate();
			break;
			
		case FunctionIdentifier::cosFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(cos(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::sinFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(sin(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::tanFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(tan(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::expFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(exp(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::logFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(log(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::log10Function:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(log10(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::log2Function:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(log2(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::sqrtFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(sqrt(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::absFunction:
			if (arg1_type == ScriptValueType::kValueInt)
			{
				int_result = new ScriptValue_Int();
				result = int_result;
				
				for (int value_index = 0; value_index < arg1_count; ++value_index)
					int_result->PushInt(llabs(arg1_value->IntAtIndex(value_index)));
			}
			else if (arg1_type == ScriptValueType::kValueFloat)
			{
				float_result = new ScriptValue_Float();
				result = float_result;
				
				for (int value_index = 0; value_index < arg1_count; ++value_index)
					float_result->PushFloat(fabs(arg1_value->FloatAtIndex(value_index)));
			}
			break;
			
		case FunctionIdentifier::ceilFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(ceil(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::floorFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(floor(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::roundFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(round(arg1_value->FloatAtIndex(value_index)));
			break;
			
		case FunctionIdentifier::truncFunction:
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				float_result->PushFloat(trunc(arg1_value->FloatAtIndex(value_index)));
			break;
			
			// bookkeeping functions
			
		case FunctionIdentifier::versionFunction:
			p_output_stream << "SLiMscript version 2.0a1" << endl;
			result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
			break;
			
		case FunctionIdentifier::licenseFunction:
			p_output_stream << "SLiM is free software: you can redistribute it and/or" << endl;
			p_output_stream << "modify it under the terms of the GNU General Public" << endl;
			p_output_stream << "License as published by the Free Software Foundation," << endl;
			p_output_stream << "either version 3 of the License, or (at your option)" << endl;
			p_output_stream << "any later version." << endl << endl;
			
			p_output_stream << "SLiM is distributed in the hope that it will be" << endl;
			p_output_stream << "useful, but WITHOUT ANY WARRANTY; without even the" << endl;
			p_output_stream << "implied warranty of MERCHANTABILITY or FITNESS FOR" << endl;
			p_output_stream << "A PARTICULAR PURPOSE.  See the GNU General Public" << endl;
			p_output_stream << "License for more details." << endl << endl;
			
			p_output_stream << "You should have received a copy of the GNU General" << endl;
			p_output_stream << "Public License along with SLiM.  If not, see" << endl;
			p_output_stream << "<http://www.gnu.org/licenses/>." << endl;
			result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
			break;
			
		case FunctionIdentifier::helpFunction:
			p_output_stream << "Help for SLiMscript is currently unimplemented." << endl;
			result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
			break;
			
		case FunctionIdentifier::lsFunction:
			p_output_stream << p_interpreter.BorrowSymbolTable();
			result = ScriptValue_NULL::ScriptValue_NULL_Invisible();
			break;
			
		case FunctionIdentifier::dateFunction:
		case FunctionIdentifier::timeFunction:
			SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function unimplemented." << endl << slim_terminate();
			break;
			
			// proxy instantiation
			
		case FunctionIdentifier::PathFunction:
			if (n_args > 1)
				SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function " << p_function_name << "() requires 0 or 1 arguments." << endl << slim_terminate();
			
			if (n_args == 1)
			{
				if (arg1_type != ScriptValueType::kValueString)
					SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function " << p_function_name << "() requires a string argument." << endl << slim_terminate();
				if (arg1_count != 1)
					SLIM_TERMINATION << "ERROR (ExecuteFunctionCall): function " << p_function_name << "() requires that its first argument's size() == 1." << endl << slim_terminate();
				
				result = new ScriptValue_PathProxy(arg1_value->StringAtIndex(0));
			}
			else
			{
				result = new ScriptValue_PathProxy();
			}
			break;
	}
	
	return result;
}

ScriptValue *ExecuteMethodCall(ScriptValue_Proxy *method_object, std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	ScriptValue *result = nullptr;
	
	// Get the function signature and check our arguments against it
	const FunctionSignature *method_signature = method_object->SignatureForMethod(p_method_name);
	
	method_signature->CheckArguments("method", p_arguments);
	
	// Make the method call
	result = method_object->ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
	
	return result;
}


//
//	FunctionSignature
//
#pragma mark FunctionSignature

FunctionSignature::FunctionSignature(std::string p_function_name, FunctionIdentifier p_function_id, ScriptValueType p_return_type) :
function_name_(p_function_name), function_id_(p_function_id), return_type_(p_return_type)
{
}

FunctionSignature *FunctionSignature::AddArg(ScriptValueMask p_arg_mask)
{
	bool is_optional = (((uint32_t)p_arg_mask & (uint32_t)ScriptValueMask::kMaskOptional) == (uint32_t)ScriptValueMask::kMaskOptional);
	
	if (has_optional_args_ && !is_optional)
		SLIM_TERMINATION << "ERROR (FunctionSignature::AddArg): cannot add a required argument after an optional argument has been added." << endl << slim_terminate();
	
	if (has_ellipsis_)
		SLIM_TERMINATION << "ERROR (FunctionSignature::AddArg): cannot add an argument after an ellipsis." << endl << slim_terminate();
	
	arg_masks_.push_back(p_arg_mask);
	
	if (is_optional)
		has_optional_args_ = true;
	
	return this;
}

FunctionSignature *FunctionSignature::AddEllipsis()
{
	if (has_optional_args_)
		SLIM_TERMINATION << "ERROR (FunctionSignature::AddEllipsis): cannot add an ellipsis after an optional argument has been added." << endl << slim_terminate();
	
	if (has_ellipsis_)
		SLIM_TERMINATION << "ERROR (FunctionSignature::AddEllipsis): cannot add more than one ellipsis." << endl << slim_terminate();
	
	has_ellipsis_ = true;
	
	return this;
}

FunctionSignature *FunctionSignature::AddLogical()			{ return AddArg(ScriptValueMask::kMaskLogical); }
FunctionSignature *FunctionSignature::AddInt()				{ return AddArg(ScriptValueMask::kMaskInt); }
FunctionSignature *FunctionSignature::AddFloat()			{ return AddArg(ScriptValueMask::kMaskFloat); }
FunctionSignature *FunctionSignature::AddString()			{ return AddArg(ScriptValueMask::kMaskString); }
FunctionSignature *FunctionSignature::AddProxy()			{ return AddArg(ScriptValueMask::kMaskProxy); }
FunctionSignature *FunctionSignature::AddNumeric()			{ return AddArg(ScriptValueMask::kMaskNumeric); }
FunctionSignature *FunctionSignature::AddLogicalEquiv()		{ return AddArg(ScriptValueMask::kMaskLogicalEquiv); }
FunctionSignature *FunctionSignature::AddAnyBase()			{ return AddArg(ScriptValueMask::kMaskAnyBase); }
FunctionSignature *FunctionSignature::AddAny()				{ return AddArg(ScriptValueMask::kMaskAny); }

FunctionSignature *FunctionSignature::AddLogical_O()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskLogical | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddInt_O()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskInt | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddFloat_O()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskFloat | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddString_O()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskString | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddProxy_O()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskProxy | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddNumeric_O()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskNumeric | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddLogicalEquiv_O()	{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskLogicalEquiv | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddAnyBase_O()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskAnyBase | ScriptValueMask::kMaskOptional)); }
FunctionSignature *FunctionSignature::AddAny_O()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskAny | ScriptValueMask::kMaskOptional)); }

FunctionSignature *FunctionSignature::AddLogical_S()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskLogical | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddInt_S()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskInt | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddFloat_S()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskFloat | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddString_S()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskString | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddProxy_S()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskProxy | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddNumeric_S()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskNumeric | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddLogicalEquiv_S()	{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskLogicalEquiv | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddAnyBase_S()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskAnyBase | ScriptValueMask::kMaskSingleton)); }
FunctionSignature *FunctionSignature::AddAny_S()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskAny | ScriptValueMask::kMaskSingleton)); }

FunctionSignature *FunctionSignature::AddLogical_OS()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskLogical | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddInt_OS()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskInt | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddFloat_OS()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskFloat | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddString_OS()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskString | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddProxy_OS()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskProxy | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddNumeric_OS()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskNumeric | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddLogicalEquiv_OS()	{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskLogicalEquiv | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddAnyBase_OS()		{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskAnyBase | ScriptValueMask::kMaskOptSingleton)); }
FunctionSignature *FunctionSignature::AddAny_OS()			{ return AddArg(ScriptValueMask(ScriptValueMask::kMaskAny | ScriptValueMask::kMaskOptSingleton)); }

void FunctionSignature::CheckArguments(std::string const &p_call_type, std::vector<ScriptValue*> const &p_arguments) const
{
	// Check the number of arguments supplied
	int n_args = (int)p_arguments.size();
	
	if (!has_ellipsis_)
	{
		if (n_args > arg_masks_.size())
			SLIM_TERMINATION << "ERROR (FunctionSignature::CheckArguments): " << p_call_type << " " << function_name_ << "() requires at most " << arg_masks_.size() << " argument(s), " << n_args << " supplied." << endl << slim_terminate();
	}
	
	// Check the types of all arguments specified in the signature
	for (int arg_index = 0; arg_index < arg_masks_.size(); ++arg_index)
	{
		uint32_t type_mask = static_cast<uint32_t>(arg_masks_[arg_index]);		// the static_casts are annoying but I like scoped enums
		bool is_optional = false;
		bool requires_singleton = false;
		
		if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskOptional))
		{
			is_optional = true;
			type_mask = type_mask - static_cast<uint32_t>(ScriptValueMask::kMaskOptional);
		}
		
		if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskSingleton))
		{
			requires_singleton = true;
			type_mask = type_mask - static_cast<uint32_t>(ScriptValueMask::kMaskSingleton);
		}
		
		// if no argument was passed for this slot, it needs to be an optional slot
		if (n_args <= arg_index)
		{
			if (is_optional)
				break;			// all the rest of the arguments must be optional, so we're done checking
			else
				SLIM_TERMINATION << "ERROR (FunctionSignature::CheckArguments): missing required argument for " << p_call_type << " " << function_name_ << "()." << endl << slim_terminate();
		}
		
		// an argument was passed, so check its type
		ScriptValueType arg_type = p_arguments[arg_index]->Type();
		
		if (type_mask != static_cast<uint32_t>(ScriptValueMask::kMaskAny))
		{
			bool type_ok = true;
			
			switch (arg_type)
			{
				case ScriptValueType::kValueNULL:		type_ok = !!(type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskNULL)); break;
				case ScriptValueType::kValueLogical:	type_ok = !!(type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskLogical)); break;
				case ScriptValueType::kValueString:		type_ok = !!(type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskString)); break;
				case ScriptValueType::kValueInt:		type_ok = !!(type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskInt)); break;
				case ScriptValueType::kValueFloat:		type_ok = !!(type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskFloat)); break;
				case ScriptValueType::kValueProxy:		type_ok = !!(type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskProxy)); break;
			}
			
			if (!type_ok)
				SLIM_TERMINATION << "ERROR (FunctionSignature::CheckArguments): argument " << arg_index + 1 << " cannot be type " << arg_type << " for " << p_call_type << " " << function_name_ << "()." << endl << slim_terminate();
		}
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const FunctionSignature &p_signature)
{
	ScriptValueType ret_type = p_signature.return_type_;
	
	if (ret_type == ScriptValueType::kValueNULL)
		p_outstream << "- (void)" << p_signature.function_name_ << "(";
	else
		p_outstream << "- (" << p_signature.return_type_ << ")" << p_signature.function_name_ << "(";
	
	int arg_mask_count = (int)p_signature.arg_masks_.size();
	
	if (arg_mask_count == 0)
	{
		p_outstream << "void";
	}
	else
	{
		for (int arg_index = 0; arg_index < arg_mask_count; ++arg_index)
		{
			uint32_t type_mask = static_cast<uint32_t>(p_signature.arg_masks_[arg_index]);		// the static_casts are annoying but I like scoped enums
			bool is_optional = false;
			bool requires_singleton = false;
			
			if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskOptional))
			{
				is_optional = true;
				type_mask = type_mask - static_cast<uint32_t>(ScriptValueMask::kMaskOptional);
			}
			
			if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskSingleton))
			{
				requires_singleton = true;
				type_mask = type_mask - static_cast<uint32_t>(ScriptValueMask::kMaskSingleton);
			}
			
			if (arg_index > 0)
				p_outstream << ", ";
			
			if (is_optional)
				p_outstream << "[";
			
			if (type_mask == static_cast<uint32_t>(ScriptValueMask::kMaskAny))
			{
				p_outstream << "*";
			}
			else
			{
				if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskNULL)) p_outstream << "N";
				if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskLogical)) p_outstream << "l";
				if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskString)) p_outstream << "s";
				if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskInt)) p_outstream << "i";
				if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskFloat)) p_outstream << "f";
				if (type_mask & static_cast<uint32_t>(ScriptValueMask::kMaskProxy)) p_outstream << "o";
			}
			
			if (requires_singleton)
				p_outstream << "$";
			
			if (is_optional)
				p_outstream << "]";
		}
	}
	
	if (p_signature.has_ellipsis_)
		p_outstream << ((arg_mask_count > 0) ? ", ..." : "...");
	
	p_outstream << ")";
	
	return p_outstream;
}


































































