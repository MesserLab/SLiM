//
//  eidos_functions.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15.
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


#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_path_element.h"
#include "eidos_interpreter.h"
#include "eidos_rng.h"

#include "math.h"

#include <ctime>


using std::string;
using std::vector;
using std::map;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


//
//	Construct our built-in function map
//

// We allocate all of our function signatures once and keep them forever, for faster EidosInterpreter startup
vector<const EidosFunctionSignature *> &EidosInterpreter::BuiltInFunctions(void)
{
	static vector<const EidosFunctionSignature *> *signatures = nullptr;
	
	if (!signatures)
	{
		signatures = new vector<const EidosFunctionSignature *>;
		
		// ************************************************************************************
		//
		//	math functions
		//
		
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("abs",				EidosFunctionIdentifier::absFunction,			kValueMaskNumeric))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("acos",				EidosFunctionIdentifier::acosFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("asin",				EidosFunctionIdentifier::asinFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan",				EidosFunctionIdentifier::atanFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan2",			EidosFunctionIdentifier::atan2Function,			kValueMaskFloat))->AddNumeric("x")->AddNumeric("y"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("ceil",				EidosFunctionIdentifier::ceilFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("cos",				EidosFunctionIdentifier::cosFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("exp",				EidosFunctionIdentifier::expFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("floor",			EidosFunctionIdentifier::floorFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFinite",			EidosFunctionIdentifier::isFiniteFunction,		kValueMaskLogical))->AddFloat("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInfinite",		EidosFunctionIdentifier::isInfiniteFunction,	kValueMaskLogical))->AddFloat("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNAN",			EidosFunctionIdentifier::isNaNFunction,			kValueMaskLogical))->AddFloat("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("log",				EidosFunctionIdentifier::logFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("log10",			EidosFunctionIdentifier::log10Function,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("log2",				EidosFunctionIdentifier::log2Function,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("product",			EidosFunctionIdentifier::productFunction,		kValueMaskNumeric | kValueMaskSingleton))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("round",			EidosFunctionIdentifier::roundFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("sin",				EidosFunctionIdentifier::sinFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("sqrt",				EidosFunctionIdentifier::sqrtFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("sum",				EidosFunctionIdentifier::sumFunction,			kValueMaskNumeric | kValueMaskSingleton))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("tan",				EidosFunctionIdentifier::tanFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("trunc",			EidosFunctionIdentifier::truncFunction,			kValueMaskFloat))->AddNumeric("x"));
		
		
		// ************************************************************************************
		//
		//	summary statistics functions
		//
		
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("max",				EidosFunctionIdentifier::maxFunction,			kValueMaskAnyBase | kValueMaskSingleton))->AddAnyBase("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("mean",				EidosFunctionIdentifier::meanFunction,			kValueMaskFloat))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("min",				EidosFunctionIdentifier::minFunction,			kValueMaskAnyBase | kValueMaskSingleton))->AddAnyBase("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("range",			EidosFunctionIdentifier::rangeFunction,			kValueMaskNumeric))->AddNumeric("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("sd",				EidosFunctionIdentifier::sdFunction,			kValueMaskFloat | kValueMaskSingleton))->AddNumeric("x"));
		
		
		// ************************************************************************************
		//
		//	vector construction functions
		//
		
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("c",				EidosFunctionIdentifier::cFunction,				kValueMaskAny))->AddEllipsis());
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_float,			EidosFunctionIdentifier::floatFunction,			kValueMaskFloat))->AddInt_S("length"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_integer,		EidosFunctionIdentifier::integerFunction,		kValueMaskInt))->AddInt_S("length"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_logical,		EidosFunctionIdentifier::logicalFunction,		kValueMaskLogical))->AddInt_S("length"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_object,		EidosFunctionIdentifier::objectFunction,		kValueMaskObject)));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbinom",			EidosFunctionIdentifier::rbinomFunction,		kValueMaskInt))->AddInt_S("n")->AddInt("size")->AddFloat("prob"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("rep",				EidosFunctionIdentifier::repFunction,			kValueMaskAny))->AddAny("x")->AddInt_S("count"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("repEach",			EidosFunctionIdentifier::repEachFunction,		kValueMaskAny))->AddAny("x")->AddInt("count"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("rexp",				EidosFunctionIdentifier::rexpFunction,			kValueMaskFloat))->AddInt_S("n")->AddNumeric_O("rate"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("rnorm",			EidosFunctionIdentifier::rnormFunction,			kValueMaskFloat))->AddInt_S("n")->AddNumeric_O("mean")->AddNumeric_O("sd"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("rpois",			EidosFunctionIdentifier::rpoisFunction,			kValueMaskInt))->AddInt_S("n")->AddNumeric("lambda"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("runif",			EidosFunctionIdentifier::runifFunction,			kValueMaskFloat))->AddInt_S("n")->AddNumeric_O("min")->AddNumeric_O("max"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("sample",			EidosFunctionIdentifier::sampleFunction,		kValueMaskAny))->AddAny("x")->AddInt("size")->AddLogical_OS("replace")->AddNumeric_O("weights"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("seq",				EidosFunctionIdentifier::seqFunction,			kValueMaskNumeric))->AddNumeric_S("from")->AddNumeric_S("to")->AddNumeric_OS("by"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("seqAlong",			EidosFunctionIdentifier::seqAlongFunction,		kValueMaskInt))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_string,		EidosFunctionIdentifier::stringFunction,		kValueMaskString))->AddInt_S("length"));
		
		
		// ************************************************************************************
		//
		//	value inspection/manipulation functions
		//
		
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("all",				EidosFunctionIdentifier::allFunction,			kValueMaskLogical | kValueMaskSingleton))->AddLogical("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("any",				EidosFunctionIdentifier::anyFunction,			kValueMaskLogical | kValueMaskSingleton))->AddLogical("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("cat",				EidosFunctionIdentifier::catFunction,			kValueMaskNULL))->AddAny("x")->AddString_OS("sep"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("ifelse",			EidosFunctionIdentifier::ifelseFunction,		kValueMaskAny))->AddLogical("test")->AddAny("trueValues")->AddAny("falseValues"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("nchar",			EidosFunctionIdentifier::ncharFunction,			kValueMaskInt))->AddString("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("paste",			EidosFunctionIdentifier::pasteFunction,			kValueMaskString | kValueMaskSingleton))->AddAny("x")->AddString_OS("sep"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("print",			EidosFunctionIdentifier::printFunction,			kValueMaskNULL))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("rev",				EidosFunctionIdentifier::revFunction,			kValueMaskAny))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_size,			EidosFunctionIdentifier::sizeFunction,			kValueMaskInt | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("sort",				EidosFunctionIdentifier::sortFunction,			kValueMaskAnyBase))->AddAnyBase("x")->AddLogical_OS("ascending"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("sortBy",			EidosFunctionIdentifier::sortByFunction,		kValueMaskObject))->AddObject("x")->AddString_S("property")->AddLogical_OS("ascending"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_str,			EidosFunctionIdentifier::strFunction,			kValueMaskNULL))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("strsplit",			EidosFunctionIdentifier::strsplitFunction,		kValueMaskString))->AddString_S("x")->AddString_OS("sep"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("substr",			EidosFunctionIdentifier::substrFunction,		kValueMaskString))->AddString("x")->AddInt("first")->AddInt_O("last"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("unique",			EidosFunctionIdentifier::uniqueFunction,		kValueMaskAny))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("which",			EidosFunctionIdentifier::whichFunction,			kValueMaskInt))->AddLogical("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMax",			EidosFunctionIdentifier::whichMaxFunction,		kValueMaskInt))->AddAnyBase("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMin",			EidosFunctionIdentifier::whichMinFunction,		kValueMaskInt))->AddAnyBase("x"));
		
		
		// ************************************************************************************
		//
		//	value type testing/coercion functions
		//
		
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("asFloat",			EidosFunctionIdentifier::asFloatFunction,		kValueMaskFloat))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("asInteger",		EidosFunctionIdentifier::asIntegerFunction,		kValueMaskInt))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("asLogical",		EidosFunctionIdentifier::asLogicalFunction,		kValueMaskLogical))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("asString",			EidosFunctionIdentifier::asStringFunction,		kValueMaskString))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("element",			EidosFunctionIdentifier::elementFunction,		kValueMaskString | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFloat",			EidosFunctionIdentifier::isFloatFunction,		kValueMaskLogical | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInteger",		EidosFunctionIdentifier::isIntegerFunction,		kValueMaskLogical | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isLogical",		EidosFunctionIdentifier::isLogicalFunction,		kValueMaskLogical | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNULL",			EidosFunctionIdentifier::isNULLFunction,		kValueMaskLogical | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isObject",			EidosFunctionIdentifier::isObjectFunction,		kValueMaskLogical | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("isString",			EidosFunctionIdentifier::isStringFunction,		kValueMaskLogical | kValueMaskSingleton))->AddAny("x"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_type,			EidosFunctionIdentifier::typeFunction,			kValueMaskString | kValueMaskSingleton))->AddAny("x"));
		
		
		// ************************************************************************************
		//
		//	bookkeeping functions
		//
		
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("date",				EidosFunctionIdentifier::dateFunction,			kValueMaskString | kValueMaskSingleton)));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("executeLambda",	EidosFunctionIdentifier::executeLambdaFunction,	kValueMaskAny))->AddString_S("lambdaSource"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("function",			EidosFunctionIdentifier::functionFunction,		kValueMaskNULL))->AddString_OS("functionName"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_globals,		EidosFunctionIdentifier::globalsFunction,		kValueMaskNULL)));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("help",				EidosFunctionIdentifier::helpFunction,			kValueMaskNULL))->AddString_OS("topic"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("license",			EidosFunctionIdentifier::licenseFunction,		kValueMaskNULL)));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("rm",				EidosFunctionIdentifier::rmFunction,			kValueMaskNULL))->AddString_O("variableNames"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("setSeed",			EidosFunctionIdentifier::setSeedFunction,		kValueMaskNULL))->AddInt_S("seed"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("getSeed",			EidosFunctionIdentifier::getSeedFunction,		kValueMaskInt | kValueMaskSingleton)));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("stop",				EidosFunctionIdentifier::stopFunction,			kValueMaskNULL))->AddString_OS("message"));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("time",				EidosFunctionIdentifier::timeFunction,			kValueMaskString | kValueMaskSingleton)));
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature("version",			EidosFunctionIdentifier::versionFunction,		kValueMaskNULL)));
		
		
		// ************************************************************************************
		//
		//	object instantiation
		//
		
		signatures->push_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_Path,		EidosFunctionIdentifier::PathFunction,			kValueMaskObject | kValueMaskSingleton))->AddString_OS("path"));
		
		
		// alphabetize, mostly to be nice to the auto-completion feature
		std::sort(signatures->begin(), signatures->end(), CompareEidosCallSignatures);
	}
	
	return *signatures;
}

EidosFunctionMap *EidosInterpreter::BuiltInFunctionMap(void)
{
	// The built-in function map is statically allocated for faster EidosInterpreter startup
	static EidosFunctionMap *built_in_function_map = nullptr;
	
	if (!built_in_function_map)
	{
		vector<const EidosFunctionSignature *> &built_in_functions = EidosInterpreter::BuiltInFunctions();
		
		built_in_function_map = new EidosFunctionMap;
		
		for (auto sig : built_in_functions)
			built_in_function_map->insert(EidosFunctionMapPair(sig->function_name_, sig));
	}
	
	return built_in_function_map;
}


//
//	Executing function calls
//

EidosValue *ConcatenateEidosValues(const std::string &p_function_name, EidosValue *const *const p_arguments, int p_argument_count)
{
#pragma unused(p_function_name)
	EidosValueType highest_type = EidosValueType::kValueNULL;
	bool has_object_type = false, has_nonobject_type = false, all_invisible = true;
	const std::string *element_type = nullptr;
	
	// First figure out our return type, which is the highest-promotion type among all our arguments
	for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index];
		EidosValueType arg_type = arg_value->Type();
		
		if (arg_type > highest_type)
			highest_type = arg_type;
		
		if (!arg_value->Invisible())
			all_invisible = false;
		
		if (arg_type == EidosValueType::kValueObject)
		{
			if (arg_value->Count() > 0)		// object(0) parameters do not conflict with other object types
			{
				const std::string *this_element_type = arg_value->ElementType();
				
				if (!element_type)
				{
					// we haven't seen a (non-empty) object type yet, so remember what type we're dealing with
					element_type = this_element_type;
				}
				else
				{
					// we've already seen a object type, so check that this one is the same type
					if (element_type != this_element_type)
						EIDOS_TERMINATION << "ERROR (" << p_function_name << "): objects of different types cannot be mixed." << eidos_terminate();
				}
			}
			
			has_object_type = true;
		}
		else
			has_nonobject_type = true;
	}
	
	if (has_object_type && has_nonobject_type)
		EIDOS_TERMINATION << "ERROR (" << p_function_name << "): object and non-object types cannot be mixed." << eidos_terminate();
	
	// If we've got nothing but NULL, then return NULL; preserve invisibility
	if (highest_type == EidosValueType::kValueNULL)
		return (all_invisible ? gStaticEidosValueNULLInvisible : gStaticEidosValueNULL);
	
	// Create an object of the right return type, concatenate all the arguments together, and return it
	if (highest_type == EidosValueType::kValueLogical)
	{
		EidosValue_Logical *result = new EidosValue_Logical();
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index];
			
			if (arg_value->Type() != EidosValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushLogical(arg_value->LogicalAtIndex(value_index));
		}
		
		return result;
	}
	else if (highest_type == EidosValueType::kValueInt)
	{
		EidosValue_Int_vector *result = new EidosValue_Int_vector();
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index];
			
			if (arg_value->Type() != EidosValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushInt(arg_value->IntAtIndex(value_index));
		}
		
		return result;
	}
	else if (highest_type == EidosValueType::kValueFloat)
	{
		EidosValue_Float_vector *result = new EidosValue_Float_vector();
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index];
			
			if (arg_value->Type() != EidosValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushFloat(arg_value->FloatAtIndex(value_index));
		}
		
		return result;
	}
	else if (highest_type == EidosValueType::kValueString)
	{
		EidosValue_String *result = new EidosValue_String();
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index];
			
			if (arg_value->Type() != EidosValueType::kValueNULL)
				for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
					result->PushString(arg_value->StringAtIndex(value_index));
		}
		
		return result;
	}
	else if (has_object_type)
	{
		EidosValue_Object_vector *result = new EidosValue_Object_vector();
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index];
			
			for (int value_index = 0; value_index < arg_value->Count(); ++value_index)
				result->PushElement(arg_value->ElementAtIndex(value_index));
		}
		
		return result;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (" << p_function_name << "): type '" << highest_type << "' is not supported by ConcatenateEidosValues()." << eidos_terminate();
	}
	
	return nullptr;
}

EidosValue *EidosInterpreter::ExecuteFunctionCall(string const &p_function_name, const EidosFunctionSignature *p_function_signature, EidosValue *const *const p_arguments, int p_argument_count)
{
	EidosValue *result = nullptr;
	
	// If the function call is a built-in Eidos function, we might already have a pointer to its signature cached; if not, we'll have to look it up
	if (!p_function_signature)
	{
		// Get the function signature and check our arguments against it
		auto signature_iter = function_map_->find(p_function_name);
		
		if (signature_iter == function_map_->end())
			EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): unrecognized function name " << p_function_name << "." << eidos_terminate();
		
		p_function_signature = signature_iter->second;
	}
	
	p_function_signature->CheckArguments(p_arguments, p_argument_count);
	
	// We predefine variables for the return types, and preallocate them here if possible.  This is for code brevity below.
	EidosValue_NULL *null_result = nullptr;
	EidosValue_String *string_result = nullptr;
	EidosValueMask return_type_mask = p_function_signature->return_mask_ & kValueMaskFlagStrip;
	
	if (return_type_mask == kValueMaskNULL)
	{
		null_result = gStaticEidosValueNULLInvisible;	// assumed that invisible is correct when the return type is NULL
		result = null_result;
	}
	else if (return_type_mask == kValueMaskString)
	{
		string_result = new EidosValue_String();
		result = string_result;
	}
	// else the return type is not predictable and thus cannot be set up beforehand; the function implementation will have to do it
	
	// Prefetch arguments to allow greater brevity in the code below
	EidosValue *arg0_value = (p_argument_count >= 1) ? p_arguments[0] : nullptr;
	EidosValueType arg0_type = (p_argument_count >= 1) ? arg0_value->Type() : EidosValueType::kValueNULL;
	int arg0_count = (p_argument_count >= 1) ? arg0_value->Count() : 0;
	
	/*
	EidosValue *arg1_value = (p_argument_count >= 2) ? p_arguments[1] : nullptr;
	EidosValueType arg1_type = (p_argument_count >= 2) ? arg1_value->Type() : EidosValueType::kValueNULL;
	int arg1_count = (p_argument_count >= 2) ? arg1_value->Count() : 0;
	
	EidosValue *arg2_value = (p_argument_count >= 3) ? p_arguments[2] : nullptr;
	EidosValueType arg2_type = (p_argument_count >= 3) ? arg2_value->Type() : EidosValueType::kValueNULL;
	int arg2_count = (p_argument_count >= 3) ? arg2_value->Count() : 0;
	*/
	
	// Now we look up the function again and actually execute it
	switch (p_function_signature->function_id_)
	{
		case EidosFunctionIdentifier::kNoFunction:
		{
			EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): internal logic error." << eidos_terminate();
			break;
		}
			
		case EidosFunctionIdentifier::kDelegatedFunction:
		{
			result = p_function_signature->delegate_function_(p_function_signature->delegate_object_, p_function_name, p_arguments, p_argument_count, *this);
			break;
		}
			
			
		// ************************************************************************************
		//
		//	math functions
		//
#pragma mark *** Math functions
			
#pragma mark abs
		case EidosFunctionIdentifier::absFunction:
		{
			if (arg0_type == EidosValueType::kValueInt)
			{
				if (arg0_count == 1)
				{
					result = new EidosValue_Int_singleton_const(llabs(arg0_value->IntAtIndex(0)));
				}
				else
				{
					EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
					result = int_result;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						int_result->PushInt(llabs(arg0_value->IntAtIndex(value_index)));
				}
			}
			else if (arg0_type == EidosValueType::kValueFloat)
			{
				if (arg0_count == 1)
				{
					result = new EidosValue_Float_singleton_const(fabs(arg0_value->FloatAtIndex(0)));
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						float_result->PushFloat(fabs(arg0_value->FloatAtIndex(value_index)));
				}
			}
			break;
		}
			
#pragma mark acos
		case EidosFunctionIdentifier::acosFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(acos(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(acos(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark asin
		case EidosFunctionIdentifier::asinFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(asin(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(asin(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark atan
		case EidosFunctionIdentifier::atanFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(atan(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(atan(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark atan2
		case EidosFunctionIdentifier::atan2Function:
		{
			EidosValue *arg1_value = p_arguments[1];
			int arg1_count = arg1_value->Count();
			
			if (arg0_count != arg1_count)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function atan2() requires arguments of equal length." << eidos_terminate();
			
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(atan2(arg0_value->FloatAtIndex(0), arg1_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(atan2(arg0_value->FloatAtIndex(value_index), arg1_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark ceil
		case EidosFunctionIdentifier::ceilFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(ceil(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(ceil(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark cos
		case EidosFunctionIdentifier::cosFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(cos(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(cos(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark exp
		case EidosFunctionIdentifier::expFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(exp(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(exp(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark floor
		case EidosFunctionIdentifier::floorFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(floor(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(floor(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark isFinite
		case EidosFunctionIdentifier::isFiniteFunction:
		{
			if (arg0_count == 1)
			{
				result = (isfinite(arg0_value->FloatAtIndex(0)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical *logical_result = new EidosValue_Logical();
				result = logical_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					logical_result->PushLogical(isfinite(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark isInfinite
		case EidosFunctionIdentifier::isInfiniteFunction:
		{
			if (arg0_count == 1)
			{
				result = (isinf(arg0_value->FloatAtIndex(0)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical *logical_result = new EidosValue_Logical();
				result = logical_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					logical_result->PushLogical(isinf(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark isNAN
		case EidosFunctionIdentifier::isNaNFunction:
		{
			if (arg0_count == 1)
			{
				result = (isnan(arg0_value->FloatAtIndex(0)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical *logical_result = new EidosValue_Logical();
				result = logical_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					logical_result->PushLogical(isnan(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark log
		case EidosFunctionIdentifier::logFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(log(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(log(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark log10
		case EidosFunctionIdentifier::log10Function:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(log10(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(log10(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark log2
		case EidosFunctionIdentifier::log2Function:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(log2(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(log2(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark product
		case EidosFunctionIdentifier::productFunction:
		{
			if (arg0_type == EidosValueType::kValueInt)
			{
				if (arg0_count == 1)
				{
					result = new EidosValue_Int_singleton_const(arg0_value->IntAtIndex(0));
				}
				else
				{
					EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
					result = int_result;
					
					int64_t product = 1;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
					{
						int64_t old_product = product;
						int64_t temp = arg0_value->IntAtIndex(value_index);
						
						product *= arg0_value->IntAtIndex(value_index);
						
						// raise on overflow; test after doing the multiplication
						if (product / temp != old_product)
							EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): overflow in product() with integer argument; use asFloat() to convert the argument." << eidos_terminate();
					}
					
					int_result->PushInt(product);
				}
			}
			else if (arg0_type == EidosValueType::kValueFloat)
			{
				if (arg0_count == 1)
				{
					result = new EidosValue_Float_singleton_const(arg0_value->FloatAtIndex(0));
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					double product = 1;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						product *= arg0_value->FloatAtIndex(value_index);
					
					float_result->PushFloat(product);
				}
			}
			break;
		}
			
#pragma mark sum
		case EidosFunctionIdentifier::sumFunction:
		{
			if (arg0_type == EidosValueType::kValueInt)
			{
				if (arg0_count == 1)
				{
					result = new EidosValue_Int_singleton_const(arg0_value->IntAtIndex(0));
				}
				else
				{
					EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
					result = int_result;
					
					int64_t sum = 0;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
					{
						int64_t temp = arg0_value->IntAtIndex(value_index);
						
						// raise on overflow; test prior to doing the addition
						if (((temp > 0) && (sum > INT64_MAX - temp)) || ((temp < 0) && (sum < INT64_MIN - temp)))
							EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): overflow in sum() with integer argument; use asFloat() to convert the argument." << eidos_terminate();
						
						sum += arg0_value->IntAtIndex(value_index);
					}
					
					int_result->PushInt(sum);
				}
			}
			else if (arg0_type == EidosValueType::kValueFloat)
			{
				if (arg0_count == 1)
				{
					result = new EidosValue_Float_singleton_const(arg0_value->FloatAtIndex(0));
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					double sum = 0;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						sum += arg0_value->FloatAtIndex(value_index);
					
					float_result->PushFloat(sum);
				}
			}
			break;
		}
			
#pragma mark round
		case EidosFunctionIdentifier::roundFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(round(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(round(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark sin
		case EidosFunctionIdentifier::sinFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(sin(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(sin(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark sqrt
		case EidosFunctionIdentifier::sqrtFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(sqrt(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(sqrt(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark tan
		case EidosFunctionIdentifier::tanFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(tan(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(tan(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
#pragma mark trunc
		case EidosFunctionIdentifier::truncFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(trunc(arg0_value->FloatAtIndex(0)));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(trunc(arg0_value->FloatAtIndex(value_index)));
			}
			break;
		}
			
			
		// ************************************************************************************
		//
		//	summary statistics functions
		//
#pragma mark *** Summary statistics functions
			
#pragma mark max
		case EidosFunctionIdentifier::maxFunction:
		{
			if (arg0_count == 0)
			{
				result = gStaticEidosValueNULL;
			}
			else if (arg0_type == EidosValueType::kValueLogical)
			{
				bool max = arg0_value->LogicalAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					bool temp = arg0_value->LogicalAtIndex(value_index);
					if (max < temp)
						max = temp;
				}
				result = (max ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else if (arg0_type == EidosValueType::kValueInt)
			{
				int64_t max = arg0_value->IntAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					int64_t temp = arg0_value->IntAtIndex(value_index);
					if (max < temp)
						max = temp;
				}
				result = new EidosValue_Int_singleton_const(max);
			}
			else if (arg0_type == EidosValueType::kValueFloat)
			{
				double max = arg0_value->FloatAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					double temp = arg0_value->FloatAtIndex(value_index);
					if (max < temp)
						max = temp;
				}
				result = new EidosValue_Float_singleton_const(max);
			}
			else if (arg0_type == EidosValueType::kValueString)
			{
				string_result = new EidosValue_String();
				result = string_result;
				
				string max = arg0_value->StringAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					string &&temp = arg0_value->StringAtIndex(value_index);
					if (max < temp)
						max = temp;
				}
				string_result->PushString(max);
			}
			break;
		}
			
#pragma mark mean
		case EidosFunctionIdentifier::meanFunction:
		{
			double sum = 0;
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				sum += arg0_value->FloatAtIndex(value_index);
			
			result = new EidosValue_Float_singleton_const(sum / arg0_count);
			break;
		}
			
#pragma mark min
		case EidosFunctionIdentifier::minFunction:
		{
			if (arg0_count == 0)
			{
				result = gStaticEidosValueNULL;
			}
			else if (arg0_type == EidosValueType::kValueLogical)
			{
				bool min = arg0_value->LogicalAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					bool temp = arg0_value->LogicalAtIndex(value_index);
					if (min > temp)
						min = temp;
				}
				result = (min ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else if (arg0_type == EidosValueType::kValueInt)
			{
				int64_t min = arg0_value->IntAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					int64_t temp = arg0_value->IntAtIndex(value_index);
					if (min > temp)
						min = temp;
				}
				result = new EidosValue_Int_singleton_const(min);
			}
			else if (arg0_type == EidosValueType::kValueFloat)
			{
				double min = arg0_value->FloatAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					double temp = arg0_value->FloatAtIndex(value_index);
					if (min > temp)
						min = temp;
				}
				result = new EidosValue_Float_singleton_const(min);
			}
			else if (arg0_type == EidosValueType::kValueString)
			{
				string_result = new EidosValue_String();
				result = string_result;
				
				string min = arg0_value->StringAtIndex(0);
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					string &&temp = arg0_value->StringAtIndex(value_index);
					if (min > temp)
						min = temp;
				}
				string_result->PushString(min);
			}
			break;
		}
			
#pragma mark range
		case EidosFunctionIdentifier::rangeFunction:
		{
			if (arg0_count == 0)
			{
				result = gStaticEidosValueNULL;
			}
			else if (arg0_type == EidosValueType::kValueInt)
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				int64_t max = arg0_value->IntAtIndex(0);
				int64_t min = max;

				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					int64_t temp = arg0_value->IntAtIndex(value_index);
					if (max < temp)
						max = temp;
					else if (min > temp)
						min = temp;
				}
				int_result->PushInt(min);
				int_result->PushInt(max);
			}
			else if (arg0_type == EidosValueType::kValueFloat)
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				double max = arg0_value->FloatAtIndex(0);
				double min = max;

				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					double temp = arg0_value->FloatAtIndex(value_index);
					if (max < temp)
						max = temp;
					else if (min > temp)
						min = temp;
				}
				float_result->PushFloat(min);
				float_result->PushFloat(max);
			}
			break;
		}
			
#pragma mark sd
		case EidosFunctionIdentifier::sdFunction:
		{
			if (arg0_count > 1)
			{
				double mean = 0;
				double sd = 0;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					mean += arg0_value->FloatAtIndex(value_index);
				
				mean /= arg0_count;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					double temp = (arg0_value->FloatAtIndex(value_index) - mean);
					sd += temp * temp;
				}
				
				sd = sqrt(sd / (arg0_count - 1));
				result = new EidosValue_Float_singleton_const(sd);
			}
			else
			{
				result = gStaticEidosValueNULL;
			}
			break;
		}
			
		// ************************************************************************************
		//
		//	vector construction functions
		//
#pragma mark *** Vector conversion functions
			
#pragma mark c
		case EidosFunctionIdentifier::cFunction:
		{
			result = ConcatenateEidosValues(p_function_name, p_arguments, p_argument_count);
			break;
		}
			
#pragma mark float
		case EidosFunctionIdentifier::floatFunction:
		{
			EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
			result = float_result;
			
			for (int64_t value_index = arg0_value->IntAtIndex(0); value_index > 0; --value_index)
				float_result->PushFloat(0.0);
			break;
		}
			
#pragma mark integer
		case EidosFunctionIdentifier::integerFunction:
		{
			EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
			result = int_result;
			
			for (int64_t value_index = arg0_value->IntAtIndex(0); value_index > 0; --value_index)
				int_result->PushInt(0);
			break;
		}
			
#pragma mark logical
		case EidosFunctionIdentifier::logicalFunction:
		{
			EidosValue_Logical *logical_result = new EidosValue_Logical();
			result = logical_result;
			
			for (int64_t value_index = arg0_value->IntAtIndex(0); value_index > 0; --value_index)
				logical_result->PushLogical(false);
			break;
		}
			
#pragma mark object
		case EidosFunctionIdentifier::objectFunction:
		{
			result = new EidosValue_Object_vector();
			break;
		}
			
#pragma mark rbinom
		case EidosFunctionIdentifier::rbinomFunction:
		{
			int64_t num_draws = arg0_value->IntAtIndex(0);
			EidosValue *arg_size = p_arguments[1];
			EidosValue *arg_prob = p_arguments[2];
			int arg_size_count = arg_size->Count();
			int arg_prob_count = arg_prob->Count();
			bool size_singleton = (arg_size_count == 1);
			bool prob_singleton = (arg_prob_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rbinom() requires n to be greater than or equal to 0." << eidos_terminate();
			if (!size_singleton && (arg_size_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rbinom() requires size to be of length 1 or n." << eidos_terminate();
			if (!prob_singleton && (arg_prob_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rbinom() requires prob to be of length 1 or n." << eidos_terminate();
			
			int size0 = (int)arg_size->IntAtIndex(0);
			double probability0 = arg_prob->FloatAtIndex(0);
			
			if (size_singleton && prob_singleton)
			{
				if (size0 < 0)
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rbinom() requires size >= 0." << eidos_terminate();
				if ((probability0 < 0.0) || (probability0 > 1.0))
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rbinom() requires probability in [0.0, 1.0]." << eidos_terminate();
				
				if (num_draws == 1)
				{
					result = new EidosValue_Int_singleton_const(gsl_ran_binomial(gEidos_rng, probability0, size0));
				}
				else
				{
					EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
					result = int_result;
					
					for (int draw_index = 0; draw_index < num_draws; ++draw_index)
						int_result->PushInt(gsl_ran_binomial(gEidos_rng, probability0, size0));
				}
			}
			else
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				{
					int size = (size_singleton ? size0 : (int)arg_size->IntAtIndex(draw_index));
					double probability = (prob_singleton ? probability0 : arg_prob->FloatAtIndex(draw_index));
					
					if (size < 0)
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rbinom() requires size >= 0." << eidos_terminate();
					if ((probability < 0.0) || (probability > 1.0))
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rbinom() requires probability in [0.0, 1.0]." << eidos_terminate();
					
					int_result->PushInt(gsl_ran_binomial(gEidos_rng, probability, size));
				}
			}
			
			break;
		}
			
#pragma mark rep
		case EidosFunctionIdentifier::repFunction:
		{
			EidosValue *arg1_value = p_arguments[1];
			int arg1_count = arg1_value->Count();
			
			// the return type depends on the type of the first argument, which will get replicated
			result = arg0_value->NewMatchingType();
			
			if (arg1_count == 1)
			{
				int64_t rep_count = arg1_value->IntAtIndex(0);
				
				for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
					for (int value_idx = 0; value_idx < arg0_count; value_idx++)
						result->PushValueFromIndexOfEidosValue(value_idx, arg0_value);
			}
			
			break;
		}
			
#pragma mark repEach
		case EidosFunctionIdentifier::repEachFunction:
		{
			EidosValue *arg1_value = p_arguments[1];
			int arg1_count = arg1_value->Count();
			
			// the return type depends on the type of the first argument, which will get replicated
			result = arg0_value->NewMatchingType();
			
			if (arg1_count == 1)
			{
				int64_t rep_count = arg1_value->IntAtIndex(0);
				
				for (int value_idx = 0; value_idx < arg0_count; value_idx++)
					for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
						result->PushValueFromIndexOfEidosValue(value_idx, arg0_value);
			}
			else if (arg1_count == arg0_count)
			{
				for (int value_idx = 0; value_idx < arg0_count; value_idx++)
				{
					int64_t rep_count = arg1_value->IntAtIndex(value_idx);
					
					for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
						result->PushValueFromIndexOfEidosValue(value_idx, arg0_value);
				}
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function repEach() requires that its second argument's size() either (1) be equal to 1, or (2) be equal to the size() of its first argument." << eidos_terminate();
			}
			
			break;
		}
			
#pragma mark rexp
		case EidosFunctionIdentifier::rexpFunction:
		{
			int64_t num_draws = arg0_value->IntAtIndex(0);
			EidosValue *arg_rate = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
			int arg_rate_count = (arg_rate ? arg_rate->Count() : 1);
			bool rate_singleton = (arg_rate_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rexp() requires n to be greater than or equal to 0." << eidos_terminate();
			if (!rate_singleton && (arg_rate_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rexp() requires rate to be of length 1 or n." << eidos_terminate();
			
			if (rate_singleton)
			{
				double rate0 = (arg_rate ? arg_rate->FloatAtIndex(0) : 1.0);
				double mu0 = 1.0 / rate0;
				
				if (rate0 <= 0.0)
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rexp() requires rate > 0.0." << eidos_terminate();
				
				if (num_draws == 1)
				{
					result = new EidosValue_Float_singleton_const(gsl_ran_exponential(gEidos_rng, mu0));
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					for (int draw_index = 0; draw_index < num_draws; ++draw_index)
						float_result->PushFloat(gsl_ran_exponential(gEidos_rng, mu0));
				}
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				{
					double rate = arg_rate->FloatAtIndex(draw_index);
					
					if (rate <= 0.0)
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rexp() requires rate > 0.0." << eidos_terminate();
					
					float_result->PushFloat(gsl_ran_exponential(gEidos_rng, 1.0 / rate));
				}
			}
			
			break;
		}
			
#pragma mark rnorm
		case EidosFunctionIdentifier::rnormFunction:
		{
			int64_t num_draws = arg0_value->IntAtIndex(0);
			EidosValue *arg_mu = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
			EidosValue *arg_sigma = ((p_argument_count >= 3) ? p_arguments[2] : nullptr);
			int arg_mu_count = (arg_mu ? arg_mu->Count() : 1);
			int arg_sigma_count = (arg_sigma ? arg_sigma->Count() : 1);
			bool mu_singleton = (arg_mu_count == 1);
			bool sigma_singleton = (arg_sigma_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rnorm() requires n to be greater than or equal to 0." << eidos_terminate();
			if (!mu_singleton && (arg_mu_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rnorm() requires mean to be of length 1 or n." << eidos_terminate();
			if (!sigma_singleton && (arg_sigma_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rnorm() requires sd to be of length 1 or n." << eidos_terminate();
			
			double mu0 = (arg_mu ? arg_mu->FloatAtIndex(0) : 0.0);
			double sigma0 = (arg_sigma ? arg_sigma->FloatAtIndex(0) : 1.0);
			
			if (mu_singleton && sigma_singleton)
			{
				if (sigma0 < 0.0)
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rnorm() requires sd >= 0.0." << eidos_terminate();
				
				if (num_draws == 1)
				{
					result = new EidosValue_Float_singleton_const(gsl_ran_gaussian(gEidos_rng, sigma0) + mu0);
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					for (int draw_index = 0; draw_index < num_draws; ++draw_index)
						float_result->PushFloat(gsl_ran_gaussian(gEidos_rng, sigma0) + mu0);
				}
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				{
					double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(draw_index));
					double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(draw_index));
					
					if (sigma < 0.0)
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rnorm() requires sd >= 0.0." << eidos_terminate();
					
					float_result->PushFloat(gsl_ran_gaussian(gEidos_rng, sigma) + mu);
				}
			}
			
			break;
		}
			
#pragma mark rpois
		case EidosFunctionIdentifier::rpoisFunction:
		{
			int64_t num_draws = arg0_value->IntAtIndex(0);
			EidosValue *arg_lambda = p_arguments[1];
			int arg_lambda_count = arg_lambda->Count();
			bool lambda_singleton = (arg_lambda_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rpois() requires n to be greater than or equal to 0." << eidos_terminate();
			if (!lambda_singleton && (arg_lambda_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rpois() requires lambda to be of length 1 or n." << eidos_terminate();
			
			if (lambda_singleton)
			{
				double lambda0 = arg_lambda->FloatAtIndex(0);
				
				if (lambda0 <= 0.0)
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rpois() requires lambda > 0.0." << eidos_terminate();
				
				if (num_draws == 1)
				{
					result = new EidosValue_Int_singleton_const(gsl_ran_poisson(gEidos_rng, lambda0));
				}
				else
				{
					EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
					result = int_result;
					
					for (int draw_index = 0; draw_index < num_draws; ++draw_index)
						int_result->PushInt(gsl_ran_poisson(gEidos_rng, lambda0));		// use the GSL, not eidos_fast_ran_poisson, to give the user high accuracy
				}
			}
			else
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				{
					double lambda = arg_lambda->FloatAtIndex(draw_index);
					
					if (lambda <= 0.0)
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function rpois() requires lambda > 0.0." << eidos_terminate();
					
					int_result->PushInt(gsl_ran_poisson(gEidos_rng, lambda));
				}
			}
			
			break;
		}
			
#pragma mark runif
		case EidosFunctionIdentifier::runifFunction:
		{
			int64_t num_draws = arg0_value->IntAtIndex(0);
			EidosValue *arg_min = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
			EidosValue *arg_max = ((p_argument_count >= 3) ? p_arguments[2] : nullptr);
			int arg_min_count = (arg_min ? arg_min->Count() : 1);
			int arg_max_count = (arg_max ? arg_max->Count() : 1);
			bool min_singleton = (arg_min_count == 1);
			bool max_singleton = (arg_max_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function runif() requires n to be greater than or equal to 0." << eidos_terminate();
			if (!min_singleton && (arg_min_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function runif() requires min to be of length 1 or n." << eidos_terminate();
			if (!max_singleton && (arg_max_count != num_draws))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function runif() requires max to be of length 1 or n." << eidos_terminate();
			
			double min_value0 = (arg_min ? arg_min->FloatAtIndex(0) : 0.0);
			double max_value0 = (arg_max ? arg_max->FloatAtIndex(0) : 1.0);
			double range0 = max_value0 - min_value0;
			
			if (min_singleton && max_singleton)
			{
				if (range0 < 0.0)
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function runif() requires min < max." << eidos_terminate();
				
				if (num_draws == 1)
				{
					result = new EidosValue_Float_singleton_const(gsl_rng_uniform(gEidos_rng) * range0 + min_value0);
				}
				else
				{
					EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
					result = float_result;
					
					for (int draw_index = 0; draw_index < num_draws; ++draw_index)
						float_result->PushFloat(gsl_rng_uniform(gEidos_rng) * range0 + min_value0);
				}
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				{
					double min_value = (min_singleton ? min_value0 : arg_min->FloatAtIndex(draw_index));
					double max_value = (max_singleton ? max_value0 : arg_max->FloatAtIndex(draw_index));
					double range = max_value - min_value;
					
					if (range < 0.0)
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function runif() requires min < max." << eidos_terminate();
					
					float_result->PushFloat(gsl_rng_uniform(gEidos_rng) * range + min_value);
				}
			}
			
			break;
		}
			
#pragma mark sample
		case EidosFunctionIdentifier::sampleFunction:
		{
			int64_t sample_size = p_arguments[1]->IntAtIndex(0);
			bool replace = ((p_argument_count >= 3) ? p_arguments[2]->LogicalAtIndex(0) : false);
			
			result = arg0_value->NewMatchingType();
			
			if (sample_size < 0)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function sample() requires a sample size >= 0." << eidos_terminate();
			if (sample_size == 0)
				break;
			
			// the algorithm used depends on whether weights were supplied
			if (p_argument_count >= 4)
			{
				// weights supplied
				vector<double> weights_vector;
				double weights_sum = 0.0;
				EidosValue *arg3_value = p_arguments[3];
				int arg3_count = arg3_value->Count();
				
				if (arg3_count != arg0_count)
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function sample() requires x and weights to be the same length." << eidos_terminate();
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					double weight = arg3_value->FloatAtIndex(value_index);
					
					weights_vector.push_back(weight);
					weights_sum += weight;
				}
				
				// get indices of x; we sample from this vector and then look up the corresponding EidosValue element
				vector<int> index_vector;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					index_vector.push_back(value_index);
				
				// do the sampling
				int64_t contender_count = arg0_count;
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					if (contender_count <= 0)
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function sample() ran out of eligible elements from which to sample." << eidos_terminate();
					if (weights_sum <= 0.0)
						EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function sample() encountered weights summing to <= 0." << eidos_terminate();
					
					double rose = gsl_rng_uniform(gEidos_rng) * weights_sum;
					double rose_sum = 0.0;
					int rose_index;
					
					for (rose_index = 0; rose_index < contender_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_vector[rose_index];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(index_vector[rose_index], arg0_value);
					
					if (!replace)
					{
						weights_sum -= weights_vector[rose_index];
						
						index_vector.erase(index_vector.begin() + rose_index);
						weights_vector.erase(weights_vector.begin() + rose_index);
						--contender_count;
					}
				}
			}
			else
			{
				// weights not supplied; use equal weights
				if (replace)
				{
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
						result->PushValueFromIndexOfEidosValue((int)gsl_rng_uniform_int(gEidos_rng, arg0_count), arg0_value);
				}
				else
				{
					// get indices of x; we sample from this vector and then look up the corresponding EidosValue element
					vector<int> index_vector;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						index_vector.push_back(value_index);
					
					// do the sampling
					int64_t contender_count = arg0_count;
					
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						if (contender_count <= 0)
							EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function sample() ran out of eligible elements from which to sample." << eidos_terminate();
						
						int rose_index = (int)gsl_rng_uniform_int(gEidos_rng, contender_count);
						
						result->PushValueFromIndexOfEidosValue(index_vector[rose_index], arg0_value);
						
						index_vector.erase(index_vector.begin() + rose_index);
						--contender_count;
					}
				}
			}
			
			break;
		}
			
#pragma mark seq
		case EidosFunctionIdentifier::seqFunction:
		{
			EidosValue *arg1_value = p_arguments[1];
			EidosValueType arg1_type = arg1_value->Type();
			EidosValue *arg2_value = ((p_argument_count == 3) ? p_arguments[2] : nullptr);
			EidosValueType arg2_type = (arg2_value ? arg2_value->Type() : EidosValueType::kValueInt);
			
			if ((arg0_type == EidosValueType::kValueFloat) || (arg1_type == EidosValueType::kValueFloat) || (arg2_type == EidosValueType::kValueFloat))
			{
				// float return case
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				double first_value = arg0_value->FloatAtIndex(0);
				double second_value = arg1_value->FloatAtIndex(0);
				double default_by = ((first_value < second_value) ? 1 : -1);
				double by_value = (arg2_value ? arg2_value->FloatAtIndex(0) : default_by);
				
				if (by_value == 0.0)
					EIDOS_TERMINATION << "ERROR (Execute_seq): function " << p_function_name << " requires a by argument != 0." << eidos_terminate();
				if (((first_value < second_value) && (by_value < 0)) || ((first_value > second_value) && (by_value > 0)))
					EIDOS_TERMINATION << "ERROR (Execute_seq): function " << p_function_name << " by argument has incorrect sign." << eidos_terminate();
				
				if (by_value > 0)
					for (double seq_value = first_value; seq_value <= second_value; seq_value += by_value)
						float_result->PushFloat(seq_value);
				else
					for (double seq_value = first_value; seq_value >= second_value; seq_value += by_value)
						float_result->PushFloat(seq_value);
			}
			else
			{
				// int return case
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				int64_t first_value = arg0_value->IntAtIndex(0);
				int64_t second_value = arg1_value->IntAtIndex(0);
				int64_t default_by = ((first_value < second_value) ? 1 : -1);
				int64_t by_value = (arg2_value ? arg2_value->IntAtIndex(0) : default_by);
				
				if (by_value == 0)
					EIDOS_TERMINATION << "ERROR (Execute_seq): function " << p_function_name << " requires a by argument != 0." << eidos_terminate();
				if (((first_value < second_value) && (by_value < 0)) || ((first_value > second_value) && (by_value > 0)))
					EIDOS_TERMINATION << "ERROR (Execute_seq): function " << p_function_name << " by argument has incorrect sign." << eidos_terminate();
				
				if (by_value > 0)
					for (int64_t seq_value = first_value; seq_value <= second_value; seq_value += by_value)
						int_result->PushInt(seq_value);
				else
					for (int64_t seq_value = first_value; seq_value >= second_value; seq_value += by_value)
						int_result->PushInt(seq_value);
			}
			
			break;
		}
			
#pragma mark seqAlong
		case EidosFunctionIdentifier::seqAlongFunction:
		{
			EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
			result = int_result;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(value_index);
			break;
		}
			
#pragma mark string
		case EidosFunctionIdentifier::stringFunction:
		{
			for (int64_t value_index = arg0_value->IntAtIndex(0); value_index > 0; --value_index)
				string_result->PushString(gStr_empty_string);
			break;
		}
			

		// ************************************************************************************
		//
		//	value inspection/manipulation functions
		//
#pragma mark *** Value inspection/manipulation functions
			
#pragma mark all
		case EidosFunctionIdentifier::allFunction:
		{
			result = gStaticEidosValue_LogicalT;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (!arg0_value->LogicalAtIndex(value_index))
				{
					result = gStaticEidosValue_LogicalF;
					break;
				}
			
			break;
		}
			
#pragma mark any
		case EidosFunctionIdentifier::anyFunction:
		{
			result = gStaticEidosValue_LogicalF;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (arg0_value->LogicalAtIndex(value_index))
				{
					result = gStaticEidosValue_LogicalT;
					break;
				}
			
			break;
		}
			
#pragma mark cat
		case EidosFunctionIdentifier::catFunction:
		{
			std::ostringstream &output_stream = ExecutionOutputStream();
			string separator = ((p_argument_count >= 2) ? p_arguments[1]->StringAtIndex(0) : gStr_space_string);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				if (value_index > 0)
					output_stream << separator;
				
				output_stream << arg0_value->StringAtIndex(value_index);
			}
			break;
		}
			
#pragma mark ifelse
		case EidosFunctionIdentifier::ifelseFunction:
		{
			EidosValue *arg1_value = p_arguments[1];
			EidosValueType arg1_type = arg1_value->Type();
			int arg1_count = arg1_value->Count();
			
			EidosValue *arg2_value = p_arguments[2];
			EidosValueType arg2_type = arg2_value->Type();
			int arg2_count = arg2_value->Count();
			
			if (arg0_count != arg1_count || arg0_count != arg2_count)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function ifelse() requires arguments of equal length." << eidos_terminate();
			if (arg1_type != arg2_type)
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function ifelse() requires arguments 2 and 3 to be the same type." << eidos_terminate();
				
			result = arg1_value->NewMatchingType();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				if (arg0_value->LogicalAtIndex(value_index))
					result->PushValueFromIndexOfEidosValue(value_index, arg1_value);
				else
					result->PushValueFromIndexOfEidosValue(value_index, arg2_value);
			}
			break;
		}
			
#pragma mark nchar
		case EidosFunctionIdentifier::ncharFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Int_singleton_const(arg0_value->StringAtIndex(0).length());
			}
			else
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					int_result->PushInt(arg0_value->StringAtIndex(value_index).length());
			}
			break;
		}
			
#pragma mark paste
		case EidosFunctionIdentifier::pasteFunction:
		{
			string separator = ((p_argument_count >= 2) ? p_arguments[1]->StringAtIndex(0) : gStr_space_string);
			string result_string;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				if (value_index > 0)
					result_string.append(separator);
				
				result_string.append(arg0_value->StringAtIndex(value_index));
			}
			
			string_result->PushString(result_string);
			break;
		}
			
#pragma mark print
		case EidosFunctionIdentifier::printFunction:
		{
			ExecutionOutputStream() << *arg0_value << endl;
			break;
		}
			
#pragma mark rev
		case EidosFunctionIdentifier::revFunction:
		{
			result = arg0_value->NewMatchingType();
			
			for (int value_index = arg0_count - 1; value_index >= 0; --value_index)
				result->PushValueFromIndexOfEidosValue(value_index, arg0_value);
			break;
		}
			
#pragma mark size
		case EidosFunctionIdentifier::sizeFunction:
		{
			result = new EidosValue_Int_singleton_const(arg0_value->Count());
			break;
		}
			
#pragma mark sort
		case EidosFunctionIdentifier::sortFunction:
		{
			result = arg0_value->NewMatchingType();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				result->PushValueFromIndexOfEidosValue(value_index, arg0_value);
			
			result->Sort((p_argument_count == 1) ? true : p_arguments[1]->LogicalAtIndex(0));
			break;
		}
			
#pragma mark sortBy
		case EidosFunctionIdentifier::sortByFunction:
		{
			EidosValue_Object_vector *object_result = new EidosValue_Object_vector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				object_result->PushElement(arg0_value->ElementAtIndex(value_index));
			
			object_result->SortBy(p_arguments[1]->StringAtIndex(0), (p_argument_count == 2) ? true : p_arguments[2]->LogicalAtIndex(0));
			
			result = object_result;
			break;
		}
			
#pragma mark str
		case EidosFunctionIdentifier::strFunction:
		{
			std::ostringstream &output_stream = ExecutionOutputStream();
			
			output_stream << "(" << arg0_type << ") ";
			
			if (arg0_count <= 2)
				output_stream << *arg0_value << endl;
			else
			{
				EidosValue *first_value = arg0_value->GetValueAtIndex(0);
				EidosValue *second_value = arg0_value->GetValueAtIndex(1);
				
				output_stream << *first_value << gStr_space_string << *second_value << " ... (" << arg0_count << " values)" << endl;
				
				if (first_value->IsTemporary()) delete first_value;
				if (second_value->IsTemporary()) delete second_value;
			}
			break;
		}
			
#pragma mark strsplit
		case EidosFunctionIdentifier::strsplitFunction:
		{
			string joined_string = arg0_value->StringAtIndex(0);
			string separator = ((p_argument_count >= 2) ? p_arguments[1]->StringAtIndex(0) : gStr_space_string);
			string::size_type start_idx = 0, sep_idx;
			
			while (true)
			{
				sep_idx = joined_string.find(separator, start_idx);
				
				if (sep_idx == string::npos)
				{
					string_result->PushString(joined_string.substr(start_idx));
					break;
				}
				else
				{
					string_result->PushString(joined_string.substr(start_idx, sep_idx - start_idx));
					start_idx = sep_idx + separator.length();
				}
			}
			
			break;
		}
			
#pragma mark substr
		case EidosFunctionIdentifier::substrFunction:
		{
			EidosValue *arg_first = p_arguments[1];
			int arg_first_count = arg_first->Count();
			bool first_singleton = (arg_first_count == 1);
			
			if (!first_singleton && (arg_first_count != arg0_count))
				EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function substr() requires the size of first to be 1, or equal to the size of x." << eidos_terminate();
			
			int64_t first0 = arg_first->IntAtIndex(0);
			
			if (p_argument_count >= 3)
			{
				// last supplied
				EidosValue *arg_last = p_arguments[2];
				int arg_last_count = arg_last->Count();
				bool last_singleton = (arg_last_count == 1);
				
				if (!last_singleton && (arg_last_count != arg0_count))
					EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): function substr() requires the size of last to be 1, or equal to the size of x." << eidos_terminate();
				
				int64_t last0 = arg_last->IntAtIndex(0);
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					std::string str = arg0_value->StringAtIndex(value_index);
					string::size_type len = str.length();
					int clamped_first = (int)(first_singleton ? first0 : arg_first->IntAtIndex(value_index));
					int clamped_last = (int)(last_singleton ? last0 : arg_last->IntAtIndex(value_index));
					
					if (clamped_first < 0) clamped_first = 0;
					if (clamped_last >= len) clamped_last = (int)len - 1;
					
					if ((clamped_first >= len) || (clamped_last < 0) || (clamped_first > clamped_last))
						string_result->PushString(gStr_empty_string);
					else
						string_result->PushString(str.substr(clamped_first, clamped_last - clamped_first + 1));
				}
			}
			else
			{
				// last not supplied; take substrings to the end of each string
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					std::string str = arg0_value->StringAtIndex(value_index);
					string::size_type len = str.length();
					int clamped_first = (int)(first_singleton ? first0 : arg_first->IntAtIndex(value_index));
					
					if (clamped_first < 0) clamped_first = 0;
					
					if (clamped_first >= len)						
						string_result->PushString(gStr_empty_string);
					else
						string_result->PushString(str.substr(clamped_first, len));
				}
			}
			
			break;
		}
			
#pragma mark unique
		case EidosFunctionIdentifier::uniqueFunction:
		{
			if (arg0_count == 0)
			{
				result = arg0_value->NewMatchingType();
			}
			else if (arg0_type == EidosValueType::kValueLogical)
			{
				bool containsF = false, containsT = false;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					if (arg0_value->LogicalAtIndex(value_index))
						containsT = true;
					else
						containsF = true;
				}
				
				if (containsF && !containsT)
					result = gStaticEidosValue_LogicalF;
				else if (containsT && !containsF)
					result = gStaticEidosValue_LogicalT;
				else if (!containsT && !containsF)
					result = new EidosValue_Logical();
				else	// containsT && containsF
				{
					// In this case, we need to be careful to preserve the order of occurrence
					EidosValue_Logical *logical_result = new EidosValue_Logical();
					result = logical_result;
					
					if (arg0_value->LogicalAtIndex(0))
					{
						logical_result->PushLogical(true);
						logical_result->PushLogical(false);
					}
					else
					{
						logical_result->PushLogical(false);
						logical_result->PushLogical(true);
					}
				}
			}
			else if (arg0_type == EidosValueType::kValueInt)
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					int64_t value = arg0_value->IntAtIndex(value_index);
					int scan_index;
					
					for (scan_index = 0; scan_index < value_index; ++scan_index)
					{
						if (value == arg0_value->IntAtIndex(scan_index))
							break;
					}
					
					if (scan_index == value_index)
						int_result->PushInt(value);
				}
			}
			else if (arg0_type == EidosValueType::kValueFloat)
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					double value = arg0_value->FloatAtIndex(value_index);
					int scan_index;
					
					for (scan_index = 0; scan_index < value_index; ++scan_index)
					{
						if (value == arg0_value->FloatAtIndex(scan_index))
							break;
					}
					
					if (scan_index == value_index)
						float_result->PushFloat(value);
				}
			}
			else if (arg0_type == EidosValueType::kValueString)
			{
				string_result = new EidosValue_String();
				result = string_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					string value = arg0_value->StringAtIndex(value_index);
					int scan_index;
					
					for (scan_index = 0; scan_index < value_index; ++scan_index)
					{
						if (value == arg0_value->StringAtIndex(scan_index))
							break;
					}
					
					if (scan_index == value_index)
						string_result->PushString(value);
				}
			}
			else if (arg0_type == EidosValueType::kValueObject)
			{
				EidosValue_Object_vector *object_result = new EidosValue_Object_vector();
				result = object_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					EidosObjectElement *value = arg0_value->ElementAtIndex(value_index);
					int scan_index;
					
					for (scan_index = 0; scan_index < value_index; ++scan_index)
					{
						if (value == arg0_value->ElementAtIndex(scan_index))
							break;
					}
					
					if (scan_index == value_index)
						object_result->PushElement(value);
				}
			}
			break;
		}
			
#pragma mark which
		case EidosFunctionIdentifier::whichFunction:
		{
			EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
			result = int_result;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (arg0_value->LogicalAtIndex(value_index))
					int_result->PushInt(value_index);
			break;
		}
			
#pragma mark whichMax
		case EidosFunctionIdentifier::whichMaxFunction:
		{
			if (arg0_count == 0)
			{
				result = gStaticEidosValueNULL;
			}
			else
			{
				int first_index = 0;
				
				if (arg0_type == EidosValueType::kValueLogical)
				{
					bool max = arg0_value->LogicalAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						bool temp = arg0_value->LogicalAtIndex(value_index);
						if (max < temp) { max = temp; first_index = value_index; }
					}
				}
				else if (arg0_type == EidosValueType::kValueInt)
				{
					int64_t max = arg0_value->IntAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						int64_t temp = arg0_value->IntAtIndex(value_index);
						if (max < temp) { max = temp; first_index = value_index; }
					}
				}
				else if (arg0_type == EidosValueType::kValueFloat)
				{
					double max = arg0_value->FloatAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						double temp = arg0_value->FloatAtIndex(value_index);
						if (max < temp) { max = temp; first_index = value_index; }
					}
				}
				else if (arg0_type == EidosValueType::kValueString)
				{
					string max = arg0_value->StringAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						string &&temp = arg0_value->StringAtIndex(value_index);
						if (max < temp) { max = temp; first_index = value_index; }
					}
				}
				
				result = new EidosValue_Int_singleton_const(first_index);
			}
			break;
		}
			
#pragma mark whichMin
		case EidosFunctionIdentifier::whichMinFunction:
		{
			if (arg0_count == 0)
			{
				result = gStaticEidosValueNULL;
			}
			else
			{
				int first_index = 0;
				
				if (arg0_type == EidosValueType::kValueLogical)
				{
					bool min = arg0_value->LogicalAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						bool temp = arg0_value->LogicalAtIndex(value_index);
						if (min > temp) { min = temp; first_index = value_index; }
					}
				}
				else if (arg0_type == EidosValueType::kValueInt)
				{
					int64_t min = arg0_value->IntAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						int64_t temp = arg0_value->IntAtIndex(value_index);
						if (min > temp) { min = temp; first_index = value_index; }
					}
				}
				else if (arg0_type == EidosValueType::kValueFloat)
				{
					double min = arg0_value->FloatAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						double temp = arg0_value->FloatAtIndex(value_index);
						if (min > temp) { min = temp; first_index = value_index; }
					}
				}
				else if (arg0_type == EidosValueType::kValueString)
				{
					string min = arg0_value->StringAtIndex(0);
					
					for (int value_index = 1; value_index < arg0_count; ++value_index)
					{
						string &&temp = arg0_value->StringAtIndex(value_index);
						if (min > temp) { min = temp; first_index = value_index; }
					}
				}
				
				result = new EidosValue_Int_singleton_const(first_index);
			}
			break;
		}
			
			
		// ************************************************************************************
		//
		//	value type testing/coercion functions
		//
#pragma mark *** Value type testing/coercion functions
			
#pragma mark asFloat
		case EidosFunctionIdentifier::asFloatFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Float_singleton_const(arg0_value->FloatAtIndex(0));
			}
			else
			{
				EidosValue_Float_vector *float_result = new EidosValue_Float_vector();
				result = float_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					float_result->PushFloat(arg0_value->FloatAtIndex(value_index));
			}
            break;
		}
			
#pragma mark asInteger
		case EidosFunctionIdentifier::asIntegerFunction:
		{
			if (arg0_count == 1)
			{
				result = new EidosValue_Int_singleton_const(arg0_value->IntAtIndex(0));
			}
			else
			{
				EidosValue_Int_vector *int_result = new EidosValue_Int_vector();
				result = int_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					int_result->PushInt(arg0_value->IntAtIndex(value_index));
			}
            break;
		}
			
#pragma mark asLogical
		case EidosFunctionIdentifier::asLogicalFunction:
		{
			if (arg0_count == 1)
			{
				result = (arg0_value->LogicalAtIndex(0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical *logical_result = new EidosValue_Logical();
				result = logical_result;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					logical_result->PushLogical(arg0_value->LogicalAtIndex(value_index));
				break;
			}
		}
			
#pragma mark asString
		case EidosFunctionIdentifier::asStringFunction:
		{
            for (int value_index = 0; value_index < arg0_count; ++value_index)
                string_result->PushString(arg0_value->StringAtIndex(value_index));
            break;
		}
			
#pragma mark element
		case EidosFunctionIdentifier::elementFunction:
		{
			string_result->PushString(*arg0_value->ElementType());
			break;
		}
			
#pragma mark isFloat
		case EidosFunctionIdentifier::isFloatFunction:
		{
			result = (arg0_type == EidosValueType::kValueFloat) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
#pragma mark isInteger
		case EidosFunctionIdentifier::isIntegerFunction:
		{
			result = (arg0_type == EidosValueType::kValueInt) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
#pragma mark isLogical
		case EidosFunctionIdentifier::isLogicalFunction:
		{
			result = (arg0_type == EidosValueType::kValueLogical) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
#pragma mark isNULL
		case EidosFunctionIdentifier::isNULLFunction:
		{
			result = (arg0_type == EidosValueType::kValueNULL) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
#pragma mark isObject
		case EidosFunctionIdentifier::isObjectFunction:
		{
			result = (arg0_type == EidosValueType::kValueObject) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
#pragma mark isString
		case EidosFunctionIdentifier::isStringFunction:
		{
			result = (arg0_type == EidosValueType::kValueString) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
#pragma mark type
		case EidosFunctionIdentifier::typeFunction:
		{
			string_result->PushString(StringForEidosValueType(arg0_value->Type()));
			break;
		}
			
			
		// ************************************************************************************
		//
		//	bookkeeping functions
		//
#pragma mark *** Bookkeeping functions
			
#pragma mark date
		case EidosFunctionIdentifier::dateFunction:
		{
			time_t rawtime;
			struct tm *timeinfo;
			char buffer[25];	// should never be more than 10, in fact, plus a null
			
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(buffer, 25, "%d-%m-%Y", timeinfo);
			
			string_result->PushString(string(buffer));
			break;
		}
			
#pragma mark executeLambda
		case EidosFunctionIdentifier::executeLambdaFunction:
		{
			EidosScript script(arg0_value->StringAtIndex(0), 0);
			
			script.Tokenize();
			script.ParseInterpreterBlockToAST();
			
			EidosSymbolTable &symbols = GetSymbolTable();			// get our own symbol table
			EidosInterpreter interpreter(script, symbols);		// give the interpreter the symbol table
			
			result = interpreter.EvaluateInterpreterBlock(false);
			
			ExecutionOutputStream() << interpreter.ExecutionOutput();
			
			break;
		}
			
#pragma mark function
		case EidosFunctionIdentifier::functionFunction:
		{
			std::ostringstream &output_stream = ExecutionOutputStream();
			string match_string = (arg0_value ? arg0_value->StringAtIndex(0) : gStr_empty_string);
			bool signature_found = false;
			
			// function_map_ is already alphebetized since maps keep sorted order
			for (auto functionPairIter = function_map_->begin(); functionPairIter != function_map_->end(); ++functionPairIter)
			{
				const EidosFunctionSignature *iter_signature = functionPairIter->second;
				
				if (arg0_value && (iter_signature->function_name_.compare(match_string) != 0))
					continue;
				
				output_stream << *iter_signature << endl;
				signature_found = true;
			}
			
			if (arg0_value && !signature_found)
				output_stream << "No function signature found for \"" << match_string << "\"." << endl;
			
			break;
		}
			
#pragma mark globals
		case EidosFunctionIdentifier::globalsFunction:
		{
			ExecutionOutputStream() << global_symbols_;
			break;
		}
			
#pragma mark help
		case EidosFunctionIdentifier::helpFunction:
		{
			ExecutionOutputStream() << "Help for Eidos is currently unimplemented." << endl;
			break;
		}
			
#pragma mark license
		case EidosFunctionIdentifier::licenseFunction:
		{
			std::ostringstream &output_stream = ExecutionOutputStream();
			
			output_stream << "Eidos is free software: you can redistribute it and/or" << endl;
			output_stream << "modify it under the terms of the GNU General Public" << endl;
			output_stream << "License as published by the Free Software Foundation," << endl;
			output_stream << "either version 3 of the License, or (at your option)" << endl;
			output_stream << "any later version." << endl << endl;
			
			output_stream << "Eidos is distributed in the hope that it will be" << endl;
			output_stream << "useful, but WITHOUT ANY WARRANTY; without even the" << endl;
			output_stream << "implied warranty of MERCHANTABILITY or FITNESS FOR" << endl;
			output_stream << "A PARTICULAR PURPOSE.  See the GNU General Public" << endl;
			output_stream << "License for more details." << endl << endl;
			
			output_stream << "You should have received a copy of the GNU General" << endl;
			output_stream << "Public License along with Eidos.  If not, see" << endl;
			output_stream << "<http://www.gnu.org/licenses/>." << endl;
			
			if (gEidosContextLicense.length())
			{
				output_stream << endl << "------------------------------------------------------" << endl << endl;
				output_stream << gEidosContextLicense << endl;
			}
			
			break;
		}
			
#pragma mark rm
		case EidosFunctionIdentifier::rmFunction:
		{
			vector<string> symbols_to_remove;
			
			if (p_argument_count == 0)
				symbols_to_remove = global_symbols_.ReadWriteSymbols();
			else
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					symbols_to_remove.push_back(arg0_value->StringAtIndex(value_index));
			
			for (string &symbol : symbols_to_remove)
				global_symbols_.RemoveValueForSymbol(symbol, false);
			
			break;
		}
			
#pragma mark setSeed
		case EidosFunctionIdentifier::setSeedFunction:
		{
			EidosInitializeRNGFromSeed((int)(arg0_value->IntAtIndex(0)));
			break;
		}
			
#pragma mark getSeed
		case EidosFunctionIdentifier::getSeedFunction:
		{
			result = new EidosValue_Int_singleton_const(gEidos_rng_last_seed);
			break;
		}
			
#pragma mark stop
		case EidosFunctionIdentifier::stopFunction:
		{
			if (arg0_value)
				ExecutionOutputStream() << arg0_value->StringAtIndex(0) << endl;
			
			EIDOS_TERMINATION << "ERROR (ExecuteFunctionCall): stop() called." << eidos_terminate();
			break;
		}
			
#pragma mark time
		case EidosFunctionIdentifier::timeFunction:
		{
			time_t rawtime;
			struct tm *timeinfo;
			char buffer[20];		// should never be more than 8, in fact, plus a null
			
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(buffer, 20, "%H:%M:%S", timeinfo);
			
			string_result->PushString(string(buffer));
			break;
		}
			
#pragma mark version
		case EidosFunctionIdentifier::versionFunction:
		{
			std::ostringstream &output_stream = ExecutionOutputStream();
			
			output_stream << "Eidos version 1.0a1" << endl;
			
			if (gEidosContextVersion.length())
				output_stream << gEidosContextVersion << endl;
			break;
		}
			
			
		// ************************************************************************************
		//
		//	object instantiation
		//
			
#pragma mark Path
		case EidosFunctionIdentifier::PathFunction:
		{
			Eidos_PathElement *pathElement = (p_argument_count == 1) ? (new Eidos_PathElement(arg0_value->StringAtIndex(0))) : (new Eidos_PathElement());
			result = new EidosValue_Object_singleton_const(pathElement);
			pathElement->Release();
			break;
		}
	}
	
	// Deallocate any unused result pointers
	//if (null_result && (null_result != result)) delete null_result;				// null_result is a static
	//if (logical_result && (logical_result != result)) delete logical_result;		// we don't use logical_result because of static logicals
	//if (float_result && (float_result != result)) delete float_result;			// we don't use float_result because of singleton floats
	//if (int_result && (int_result != result)) delete int_result;					// we don't use int_result because of singleton ints
	if (string_result && (string_result != result)) delete string_result;
	
	// Check the return value against the signature
	p_function_signature->CheckReturn(result);
	
	return result;
}

EidosValue *EidosInterpreter::ExecuteMethodCall(EidosValue_Object *method_object, EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count)
{
	EidosValue *result = nullptr;
	
	// Get the function signature and check our arguments against it
	const EidosMethodSignature *method_signature = method_object->SignatureForMethodOfElements(p_method_id);
	
	method_signature->CheckArguments(p_arguments, p_argument_count);
	
	// Make the method call
	if (method_signature->is_class_method)
		result = method_object->ExecuteClassMethodOfElements(p_method_id, p_arguments, p_argument_count, *this);
	else
		result = method_object->ExecuteInstanceMethodOfElements(p_method_id, p_arguments, p_argument_count, *this);
	
	// Check the return value against the signature
	method_signature->CheckReturn(result);
	
	return result;
}


































































