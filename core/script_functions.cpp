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

FunctionSignature::FunctionSignature(std::string p_function_name,
									 FunctionIdentifier p_function_id,
									 ScriptValueType p_return_type,
									 bool p_uses_var_args_,
									 int p_minimum_arg_count,
									 std::vector<ScriptValueMask> p_arg_types) :
function_name_(p_function_name), function_id_(p_function_id), return_type_(p_return_type), uses_var_args_(p_uses_var_args_), minimum_arg_count_(p_minimum_arg_count), arg_types_(p_arg_types)
{
}

void RegisterSignature(FunctionMap &p_map, FunctionSignature p_signature)
{
	p_map.insert(FunctionMapPair(p_signature.function_name_, p_signature));
}

FunctionMap BuiltInFunctionMap(void)
{
	FunctionMap map;
	
	
	// construct some shorthands
	
	vector<ScriptValueMask> type_unspecified;
	
	vector<ScriptValueMask> type_1numeric(1, ScriptValueMask::kMaskNumeric);
	vector<ScriptValueMask> type_2numeric(2, ScriptValueMask::kMaskNumeric);
	vector<ScriptValueMask> type_3numeric(3, ScriptValueMask::kMaskNumeric);
	
	vector<ScriptValueMask> type_1string(1, ScriptValueMask::kMaskString);
	
	vector<ScriptValueMask> type_1any(1, ScriptValueMask::kMaskAny);
	vector<ScriptValueMask> type_2any(2, ScriptValueMask::kMaskAny);
	vector<ScriptValueMask> type_3any(3, ScriptValueMask::kMaskAny);
	
	vector<ScriptValueMask> type_any_int;
	type_any_int.push_back(ScriptValueMask::kMaskAny);
	type_any_int.push_back(ScriptValueMask::kMaskInt);
	
	
	// data construction functions
	
	RegisterSignature(map, FunctionSignature("rep",			FunctionIdentifier::repFunction,		ScriptValueType::kValueNULL, false, 2, type_any_int));
	RegisterSignature(map, FunctionSignature("repEach",		FunctionIdentifier::repEachFunction,	ScriptValueType::kValueNULL, false, 2, type_any_int));
	RegisterSignature(map, FunctionSignature("seq",			FunctionIdentifier::seqFunction,		ScriptValueType::kValueNULL, false, 3, type_3numeric));
	RegisterSignature(map, FunctionSignature("seqAlong",	FunctionIdentifier::seqAlongFunction,	ScriptValueType::kValueInt, false, 1, type_1any));
	RegisterSignature(map, FunctionSignature("c",			FunctionIdentifier::cFunction,			ScriptValueType::kValueNULL, true, 0, type_unspecified));
	
	
	// data inspection/manipulation functions
	 
	RegisterSignature(map, FunctionSignature("print",		FunctionIdentifier::printFunction,		ScriptValueType::kValueNULL, false, 1, type_unspecified));
	RegisterSignature(map, FunctionSignature("cat",			FunctionIdentifier::catFunction,		ScriptValueType::kValueNULL, false, 1, type_unspecified));
	RegisterSignature(map, FunctionSignature("size",		FunctionIdentifier::sizeFunction,		ScriptValueType::kValueInt, false, 1, type_1any));
	
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
	 
	RegisterSignature(map, FunctionSignature("class",		FunctionIdentifier::classFunction,		ScriptValueType::kValueString, false, 1, type_1any));
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
	
	RegisterSignature(map, FunctionSignature("acos",		FunctionIdentifier::acosFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("asin",		FunctionIdentifier::asinFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("atan",		FunctionIdentifier::atanFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("cos",			FunctionIdentifier::cosFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("sin",			FunctionIdentifier::sinFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("tan",			FunctionIdentifier::tanFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("exp",			FunctionIdentifier::expFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("log",			FunctionIdentifier::logFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("log10",		FunctionIdentifier::log10Function,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("log2",		FunctionIdentifier::log2Function,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("sqrt",		FunctionIdentifier::sqrtFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("ceil",		FunctionIdentifier::ceilFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("floor",		FunctionIdentifier::floorFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("round",		FunctionIdentifier::roundFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("trunc",		FunctionIdentifier::truncFunction,		ScriptValueType::kValueFloat, false, 1, type_1numeric));
	RegisterSignature(map, FunctionSignature("abs",			FunctionIdentifier::absFunction,		ScriptValueType::kValueNULL, false, 1, type_1numeric));
	
	// bookkeeping functions

	RegisterSignature(map, FunctionSignature("version",		FunctionIdentifier::versionFunction,	ScriptValueType::kValueNULL, false, 0, type_unspecified));
	RegisterSignature(map, FunctionSignature("license",		FunctionIdentifier::licenseFunction,	ScriptValueType::kValueNULL, false, 0, type_unspecified));
	RegisterSignature(map, FunctionSignature("help",		FunctionIdentifier::helpFunction,		ScriptValueType::kValueNULL, false, 0, type_unspecified));
	RegisterSignature(map, FunctionSignature("ls",			FunctionIdentifier::lsFunction,			ScriptValueType::kValueNULL, false, 0, type_unspecified));
	
	// proxy instantiation
	
	RegisterSignature(map, FunctionSignature("Path",		FunctionIdentifier::PathFunction,		ScriptValueType::kValueProxy, true, 0, type_unspecified));
	
	return map;
}

FunctionMap gBuiltInFunctionMap = BuiltInFunctionMap();


//
//	Executing function calls
//

void CheckArgumentsAgainstSignature(std::string const &p_call_type, FunctionSignature const &p_signature, std::vector<ScriptValue*> const &p_arguments)
{
	// Check the number of arguments supplied
	int n_args = (int)p_arguments.size();
	
	if (p_signature.uses_var_args_)
	{
		if (n_args < p_signature.minimum_arg_count_)
			SLIM_TERMINATION << "ERROR (CheckArgumentsAgainstSignature): " << p_call_type << " " << p_signature.function_name_ << "() requires at least " << p_signature.minimum_arg_count_ << " argument(s), " << n_args << " supplied." << endl << slim_terminate();
	}
	else
	{
		if (n_args != p_signature.minimum_arg_count_)
			SLIM_TERMINATION << "ERROR (CheckArgumentsAgainstSignature): " << p_call_type << " " << p_signature.function_name_ << "() requires " << p_signature.minimum_arg_count_ << " argument(s), " << n_args << " supplied." << endl << slim_terminate();
	}
	
	// Check the types of all arguments specified in the signature
	for (int arg_index = 0; arg_index < p_signature.arg_types_.size(); ++arg_index)
	{
		ScriptValueMask type_mask = p_signature.arg_types_[arg_index];
		ScriptValueType arg_type = p_arguments[arg_index]->Type();
		
		if (type_mask != ScriptValueMask::kMaskAny)
		{
			bool type_ok = true;
			uint32_t uint_mask = static_cast<uint32_t>(type_mask);		// the static_casts are annoying but I like scoped enums
			
			switch (arg_type)
			{
				case ScriptValueType::kValueNULL:		type_ok = !!(uint_mask & static_cast<uint32_t>(ScriptValueMask::kMaskNULL)); break;
				case ScriptValueType::kValueLogical:	type_ok = !!(uint_mask & static_cast<uint32_t>(ScriptValueMask::kMaskLogical)); break;
				case ScriptValueType::kValueString:		type_ok = !!(uint_mask & static_cast<uint32_t>(ScriptValueMask::kMaskString)); break;
				case ScriptValueType::kValueInt:		type_ok = !!(uint_mask & static_cast<uint32_t>(ScriptValueMask::kMaskInt)); break;
				case ScriptValueType::kValueFloat:		type_ok = !!(uint_mask & static_cast<uint32_t>(ScriptValueMask::kMaskFloat)); break;
				case ScriptValueType::kValueProxy:		type_ok = !!(uint_mask & static_cast<uint32_t>(ScriptValueMask::kMaskProxy)); break;
			}
			
			if (!type_ok)
				SLIM_TERMINATION << "ERROR (CheckArgumentsAgainstSignature): argument " << arg_index << " cannot be type " << arg_type << " for " << p_call_type << " " << p_signature.function_name_ << "()." << endl << slim_terminate();
		}
	}
}

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
	
	FunctionSignature &signature = signature_iter->second;
	
	CheckArgumentsAgainstSignature("function", signature, p_arguments);
	
	// We predefine variables for the return types, and preallocate them here if possible.  This is for code brevity below.
	ScriptValue_Logical *logical_result = nullptr;
	ScriptValue_Float *float_result = nullptr;
	ScriptValue_Int *int_result = nullptr;
	ScriptValue_String *string_result = nullptr;
	
	switch (signature.return_type_)
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
	switch (signature.function_id_)
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
	CheckArgumentsAgainstSignature("method", method_object->SignatureForMethod(p_method_name), p_arguments);
	
	// Make the method call
	result = method_object->ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
	
	return result;
}




































































