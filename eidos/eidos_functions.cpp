//
//  eidos_functions.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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


#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_test_element.h"
#include "eidos_interpreter.h"
#include "eidos_rng.h"
#include "eidos_beep.h"

#include <ctime>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <utility>
#include <numeric>
#include <sys/stat.h>

#include "time.h"

// BCH 20 October 2016: continuing to try to fix problems with gcc 5.4.0 on Linux without breaking other
// builds.  We will switch to including <cmath> and using the std:: namespace math functions, since on
// that platform <cmath> is included as a side effect even if we don't include it ourselves, and on
// that platform that actually (incorrectly) undefines the global functions defined by math.h.  On other
// platforms, we get the global math.h functions defined as well, so we can't use using to select the
// <cmath> functions, we have to specify them explicitly.
#include <cmath>


// From stackoverflow: http://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf/
// I chose to use iFreilicht's answer, which requires C++11.  BCH 26 April 2016
#include <memory>
#include <iostream>
#include <string>
#include <cstdio>
#include <cinttypes>

template<typename ... Args>
std::string EidosStringFormat(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1;	// Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]); 
	snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1);				// We don't want the '\0' inside
}


using std::string;
using std::vector;
using std::map;
using std::endl;
using std::istringstream;
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
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("abs",				Eidos_ExecuteFunction_abs,			kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("acos",				Eidos_ExecuteFunction_acos,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asin",				Eidos_ExecuteFunction_asin,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan",				Eidos_ExecuteFunction_atan,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan2",			Eidos_ExecuteFunction_atan2,			kEidosValueMaskFloat))->AddNumeric("x")->AddNumeric("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ceil",				Eidos_ExecuteFunction_ceil,			kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cos",				Eidos_ExecuteFunction_cos,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cumProduct",		Eidos_ExecuteFunction_cumProduct,	kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cumSum",			Eidos_ExecuteFunction_cumSum,		kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("exp",				Eidos_ExecuteFunction_exp,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("floor",			Eidos_ExecuteFunction_floor,			kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("integerDiv",		Eidos_ExecuteFunction_integerDiv,	kEidosValueMaskInt))->AddInt("x")->AddInt("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("integerMod",		Eidos_ExecuteFunction_integerMod,	kEidosValueMaskInt))->AddInt("x")->AddInt("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFinite",			Eidos_ExecuteFunction_isFinite,		kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInfinite",		Eidos_ExecuteFunction_isInfinite,	kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNAN",			Eidos_ExecuteFunction_isNAN,			kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log",				Eidos_ExecuteFunction_log,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log10",			Eidos_ExecuteFunction_log10,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log2",				Eidos_ExecuteFunction_log2,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("product",			Eidos_ExecuteFunction_product,		kEidosValueMaskNumeric | kEidosValueMaskSingleton))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("round",			Eidos_ExecuteFunction_round,			kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setUnion",			Eidos_ExecuteFunction_setUnion,		kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setIntersection",	Eidos_ExecuteFunction_setIntersection,		kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setDifference",		Eidos_ExecuteFunction_setDifference,		kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setSymmetricDifference",		Eidos_ExecuteFunction_setSymmetricDifference,		kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sin",				Eidos_ExecuteFunction_sin,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sqrt",				Eidos_ExecuteFunction_sqrt,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sum",				Eidos_ExecuteFunction_sum,			kEidosValueMaskNumeric | kEidosValueMaskSingleton))->AddLogicalEquiv("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("tan",				Eidos_ExecuteFunction_tan,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("trunc",			Eidos_ExecuteFunction_trunc,			kEidosValueMaskFloat))->AddFloat("x"));
		
		
		// ************************************************************************************
		//
		//	summary statistics functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("max",				Eidos_ExecuteFunction_max,			kEidosValueMaskAnyBase | kEidosValueMaskSingleton))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mean",				Eidos_ExecuteFunction_mean,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("min",				Eidos_ExecuteFunction_min,			kEidosValueMaskAnyBase | kEidosValueMaskSingleton))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pmax",				Eidos_ExecuteFunction_pmax,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddAnyBase("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pmin",				Eidos_ExecuteFunction_pmin,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddAnyBase("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("range",			Eidos_ExecuteFunction_range,			kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sd",				Eidos_ExecuteFunction_sd,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		
		
		// ************************************************************************************
		//
		//	distribution draw / density functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dnorm",			Eidos_ExecuteFunction_dnorm,			kEidosValueMaskFloat))->AddFloat("x")->AddNumeric_O("mean", gStaticEidosValue_Float0)->AddNumeric_O("sd", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbinom",			Eidos_ExecuteFunction_rbinom,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddInt("size")->AddFloat("prob"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rexp",				Eidos_ExecuteFunction_rexp,			kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("mu", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgamma",			Eidos_ExecuteFunction_rgamma,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("mean")->AddNumeric("shape"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rlnorm",			Eidos_ExecuteFunction_rlnorm,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("meanlog", gStaticEidosValue_Float0)->AddNumeric_O("sdlog", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rnorm",			Eidos_ExecuteFunction_rnorm,			kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("mean", gStaticEidosValue_Float0)->AddNumeric_O("sd", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rpois",			Eidos_ExecuteFunction_rpois,			kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddNumeric("lambda"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("runif",			Eidos_ExecuteFunction_runif,			kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("min", gStaticEidosValue_Float0)->AddNumeric_O("max", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rweibull",			Eidos_ExecuteFunction_rweibull,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("lambda")->AddNumeric("k"));
		
		
		// ************************************************************************************
		//
		//	vector construction functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("c",				Eidos_ExecuteFunction_c,				kEidosValueMaskAny))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_float,	Eidos_ExecuteFunction_float,			kEidosValueMaskFloat))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_integer,	Eidos_ExecuteFunction_integer,		kEidosValueMaskInt))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_logical,	Eidos_ExecuteFunction_logical,		kEidosValueMaskLogical))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_object,	Eidos_ExecuteFunction_object,		kEidosValueMaskObject, gEidos_UndefinedClassObject)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rep",				Eidos_ExecuteFunction_rep,			kEidosValueMaskAny))->AddAny("x")->AddInt_S("count"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("repEach",			Eidos_ExecuteFunction_repEach,		kEidosValueMaskAny))->AddAny("x")->AddInt("count"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sample",			Eidos_ExecuteFunction_sample,		kEidosValueMaskAny))->AddAny("x")->AddInt_S("size")->AddLogical_OS("replace", gStaticEidosValue_LogicalF)->AddNumeric_ON(gEidosStr_weights, gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("seq",				Eidos_ExecuteFunction_seq,			kEidosValueMaskNumeric))->AddNumeric_S("from")->AddNumeric_S("to")->AddNumeric_OSN("by", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("seqAlong",			Eidos_ExecuteFunction_seqAlong,		kEidosValueMaskInt))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_string,	Eidos_ExecuteFunction_string,		kEidosValueMaskString))->AddInt_S("length"));
		
		
		// ************************************************************************************
		//
		//	value inspection/manipulation functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("all",				Eidos_ExecuteFunction_all,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("any",				Eidos_ExecuteFunction_any,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cat",				Eidos_ExecuteFunction_cat,			kEidosValueMaskNULL))->AddAny("x")->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("format",			Eidos_ExecuteFunction_format,		kEidosValueMaskString))->AddString_S("format")->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("identical",		Eidos_ExecuteFunction_identical,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ifelse",			Eidos_ExecuteFunction_ifelse,		kEidosValueMaskAny))->AddLogical("test")->AddAny("trueValues")->AddAny("falseValues"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("match",			Eidos_ExecuteFunction_match,			kEidosValueMaskInt))->AddAny("x")->AddAny("table"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nchar",			Eidos_ExecuteFunction_nchar,			kEidosValueMaskInt))->AddString("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("order",				Eidos_ExecuteFunction_order,			kEidosValueMaskInt))->AddAnyBase("x")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("paste",			Eidos_ExecuteFunction_paste,			kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x")->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("print",			Eidos_ExecuteFunction_print,			kEidosValueMaskNULL))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rev",				Eidos_ExecuteFunction_rev,			kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_size,		Eidos_ExecuteFunction_size,			kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sort",				Eidos_ExecuteFunction_sort,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sortBy",			Eidos_ExecuteFunction_sortBy,		kEidosValueMaskObject))->AddObject("x", nullptr)->AddString_S("property")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_str,		Eidos_ExecuteFunction_str,			kEidosValueMaskNULL))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strsplit",			Eidos_ExecuteFunction_strsplit,		kEidosValueMaskString))->AddString_S("x")->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("substr",			Eidos_ExecuteFunction_substr,		kEidosValueMaskString))->AddString("x")->AddInt("first")->AddInt_ON("last", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("unique",			Eidos_ExecuteFunction_unique,		kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("which",			Eidos_ExecuteFunction_which,			kEidosValueMaskInt))->AddLogical("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMax",			Eidos_ExecuteFunction_whichMax,		kEidosValueMaskInt))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMin",			Eidos_ExecuteFunction_whichMin,		kEidosValueMaskInt))->AddAnyBase("x"));
		
		
		// ************************************************************************************
		//
		//	value type testing/coercion functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asFloat",			Eidos_ExecuteFunction_asFloat,		kEidosValueMaskFloat))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asInteger",		Eidos_ExecuteFunction_asInteger,		kEidosValueMaskInt))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asLogical",		Eidos_ExecuteFunction_asLogical,		kEidosValueMaskLogical))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asString",			Eidos_ExecuteFunction_asString,		kEidosValueMaskString))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("elementType",		Eidos_ExecuteFunction_elementType,	kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFloat",			Eidos_ExecuteFunction_isFloat,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInteger",		Eidos_ExecuteFunction_isInteger,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isLogical",		Eidos_ExecuteFunction_isLogical,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNULL",			Eidos_ExecuteFunction_isNULL,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isObject",			Eidos_ExecuteFunction_isObject,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isString",			Eidos_ExecuteFunction_isString,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("type",				Eidos_ExecuteFunction_type,			kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x"));
		
		
		// ************************************************************************************
		//
		//	miscellaneous functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_apply,	Eidos_ExecuteFunction_apply,			kEidosValueMaskAny))->AddAny("x")->AddString_S("lambdaSource"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("beep",				Eidos_ExecuteFunction_beep,			kEidosValueMaskNULL))->AddString_OSN("soundName", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("citation",			Eidos_ExecuteFunction_citation,		kEidosValueMaskNULL)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("clock",				Eidos_ExecuteFunction_clock,		kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("date",				Eidos_ExecuteFunction_date,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("defineConstant",	Eidos_ExecuteFunction_defineConstant,	kEidosValueMaskNULL))->AddString_S("symbol")->AddAnyBase("value"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_doCall,	Eidos_ExecuteFunction_doCall,		kEidosValueMaskAny))->AddString_S("function")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_executeLambda,	Eidos_ExecuteFunction_executeLambda,	kEidosValueMaskAny))->AddString_S("lambdaSource")->AddLogical_OS("timed", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("exists",			Eidos_ExecuteFunction_exists,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("symbol"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("function",			Eidos_ExecuteFunction_function,		kEidosValueMaskNULL))->AddString_OSN("functionName", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_ls,		Eidos_ExecuteFunction_ls,			kEidosValueMaskNULL)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("license",			Eidos_ExecuteFunction_license,		kEidosValueMaskNULL)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_rm,		Eidos_ExecuteFunction_rm,			kEidosValueMaskNULL))->AddString_ON("variableNames", gStaticEidosValueNULL)->AddLogical_OS("removeConstants", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setSeed",			Eidos_ExecuteFunction_setSeed,		kEidosValueMaskNULL))->AddInt_S("seed"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("getSeed",			Eidos_ExecuteFunction_getSeed,		kEidosValueMaskInt | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("stop",				Eidos_ExecuteFunction_stop,			kEidosValueMaskNULL))->AddString_OSN("message", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("time",				Eidos_ExecuteFunction_time,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("version",			Eidos_ExecuteFunction_version,		kEidosValueMaskNULL)));
		
		
		// ************************************************************************************
		//
		//	filesystem access functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("createDirectory",	Eidos_ExecuteFunction_createDirectory,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("path"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("filesAtPath",		Eidos_ExecuteFunction_filesAtPath,	kEidosValueMaskString))->AddString_S("path")->AddLogical_OS("fullPaths", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("deleteFile",		Eidos_ExecuteFunction_deleteFile,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("filePath"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("readFile",			Eidos_ExecuteFunction_readFile,		kEidosValueMaskString))->AddString_S("filePath"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("writeFile",		Eidos_ExecuteFunction_writeFile,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("filePath")->AddString("contents")->AddLogical_OS("append", gStaticEidosValue_LogicalF));

		
		// ************************************************************************************
		//
		//	object instantiation
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_Test",			Eidos_ExecuteFunction__Test,			kEidosValueMaskObject | kEidosValueMaskSingleton, gEidos_TestElementClass))->AddInt_S("yolk"));
		
		
		// alphabetize, mostly to be nice to the auto-completion feature
		std::sort(signatures->begin(), signatures->end(), CompareEidosCallSignatures);
	}
	
	return *signatures;
}

EidosFunctionMap *EidosInterpreter::built_in_function_map_ = nullptr;

void EidosInterpreter::CacheBuiltInFunctionMap(void)
{
	// The built-in function map is statically allocated for faster EidosInterpreter startup
	
	if (!built_in_function_map_)
	{
		vector<const EidosFunctionSignature *> &built_in_functions = EidosInterpreter::BuiltInFunctions();
		
		built_in_function_map_ = new EidosFunctionMap;
		
		for (auto sig : built_in_functions)
			built_in_function_map_->insert(EidosFunctionMapPair(sig->call_name_, sig));
	}
}


//
//	Executing function calls
//

EidosValue_SP ConcatenateEidosValues(const EidosValue_SP *const p_arguments, int p_argument_count, bool p_allow_null)
{
	// This function expects an error range to be set bracketing it externally,
	// so no blame token is needed here.
	
	EidosValueType highest_type = EidosValueType::kValueNULL;	// start at the lowest
	bool has_object_type = false, has_nonobject_type = false, all_invisible = true;
	const EidosObjectClass *element_class = gEidos_UndefinedClassObject;
	int reserve_size = 0;
	
	// First figure out our return type, which is the highest-promotion type among all our arguments
	for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		EidosValueType arg_type = arg_value->Type();
		int arg_value_count = arg_value->Count();
		
		reserve_size += arg_value_count;
		
		if (!p_allow_null && (arg_type == EidosValueType::kValueNULL))
			EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): NULL is not allowed to be used in this context." << eidos_terminate(nullptr);
		
		// The highest type includes arguments of zero length; doing c(3, 7, string(0)) should produce a string vector
		if (arg_type > highest_type)
			highest_type = arg_type;
		
		if (!arg_value->Invisible())
			all_invisible = false;
		
		if (arg_type == EidosValueType::kValueObject)
		{
			const EidosObjectClass *this_element_class = ((EidosValue_Object *)arg_value)->Class();
			
			if (this_element_class != gEidos_UndefinedClassObject)	// undefined objects do not conflict with other object types
			{
				if (element_class == gEidos_UndefinedClassObject)
				{
					// we haven't seen a (defined) object type yet, so remember what type we're dealing with
					element_class = this_element_class;
				}
				else
				{
					// we've already seen a object type, so check that this one is the same type
					if (element_class != this_element_class)
						EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): objects of different types cannot be mixed." << eidos_terminate(nullptr);
				}
			}
			
			has_object_type = true;
		}
		else if (arg_type != EidosValueType::kValueNULL)
			has_nonobject_type = true;
	}
	
	if (has_object_type && has_nonobject_type)
		EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): object and non-object types cannot be mixed." << eidos_terminate(nullptr);
	
	// If we've got nothing but NULL, then return NULL; preserve invisibility
	if (highest_type == EidosValueType::kValueNULL)
		return (all_invisible ? gStaticEidosValueNULLInvisible : gStaticEidosValueNULL);
	
	// Create an object of the right return type, concatenate all the arguments together, and return it
	// Note that NULLs here concatenate away silently because their Count()==0; a bit dangerous!
	if (highest_type == EidosValueType::kValueLogical)
	{
		EidosValue_Logical *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(reserve_size);
		EidosValue_Logical_SP result_SP = EidosValue_Logical_SP(result);
		std::vector<eidos_logical_t> *result_vec = result->LogicalVector_Mutable();
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				result_vec->emplace_back(arg_value->LogicalAtIndex(0, nullptr));
			}
			else if (arg_value_count)
			{
				if (arg_value->Type() == EidosValueType::kValueLogical)
				{
					// Speed up logical arguments, which are probably common since our result is logical
					const std::vector<eidos_logical_t> &arg_vec = *arg_value->LogicalVector();
					
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result_vec->emplace_back(arg_vec[value_index]);
				}
				else
				{
					// CODE COVERAGE: This is dead code
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result_vec->emplace_back(arg_value->LogicalAtIndex(value_index, nullptr));
				}
			}
		}
		
		return result_SP;
	}
	else if (highest_type == EidosValueType::kValueInt)
	{
		EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(reserve_size);
		EidosValue_Int_vector_SP result_SP = EidosValue_Int_vector_SP(result);
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				result->PushInt(arg_value->IntAtIndex(0, nullptr));
			}
			else if (arg_value_count)
			{
				if (arg_value->Type() == EidosValueType::kValueInt)
				{
					// Speed up integer arguments, which are probably common since our result is integer
					const std::vector<int64_t> &arg_vec = *arg_value->IntVector();
					
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushInt(arg_vec[value_index]);
				}
				else
				{
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushInt(arg_value->IntAtIndex(value_index, nullptr));
				}
			}
		}
		
		return result_SP;
	}
	else if (highest_type == EidosValueType::kValueFloat)
	{
		EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(reserve_size);
		EidosValue_Float_vector_SP result_SP = EidosValue_Float_vector_SP(result);
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				result->PushFloat(arg_value->FloatAtIndex(0, nullptr));
			}
			else if (arg_value_count)
			{
				if (arg_value->Type() == EidosValueType::kValueFloat)
				{
					// Speed up float arguments, which are probably common since our result is float
					const std::vector<double> &arg_vec = *arg_value->FloatVector();
					
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushFloat(arg_vec[value_index]);
				}
				else
				{
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushFloat(arg_value->FloatAtIndex(value_index, nullptr));
				}
			}
		}
		
		return result_SP;
	}
	else if (highest_type == EidosValueType::kValueString)
	{
		EidosValue_String_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(reserve_size);
		EidosValue_String_vector_SP result_SP = EidosValue_String_vector_SP(result);
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				result->PushString(arg_value->StringAtIndex(0, nullptr));
			}
			else if (arg_value_count)
			{
				if (arg_value->Type() == EidosValueType::kValueString)
				{
					// Speed up string arguments, which are probably common since our result is string
					const std::vector<std::string> &arg_vec = *arg_value->StringVector();
					
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushString(arg_vec[value_index]);
				}
				else
				{
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushString(arg_value->StringAtIndex(value_index, nullptr));
				}
			}
		}
		
		return result_SP;
	}
	else if (has_object_type)
	{
		EidosValue_Object_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(element_class))->Reserve(reserve_size);
		EidosValue_Object_vector_SP result_SP = EidosValue_Object_vector_SP(result);
		
		for (int arg_index = 0; arg_index < p_argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				result->PushObjectElement(arg_value->ObjectElementAtIndex(0, nullptr));
			}
			else if (arg_value_count)
			{
				if (arg_value->Type() == EidosValueType::kValueObject)
				{
					// Speed up object arguments, which are probably common since our result is object
					const std::vector<EidosObjectElement *> &arg_vec = *arg_value->ObjectElementVector();
					
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushObjectElement(arg_vec[value_index]);
				}
				else
				{
					// CODE COVERAGE: This is dead code
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->PushObjectElement(arg_value->ObjectElementAtIndex(value_index, nullptr));
				}
			}
		}
		
		return result_SP;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): type '" << highest_type << "' is not supported by ConcatenateEidosValues()." << eidos_terminate(nullptr);
	}
	
	return EidosValue_SP(nullptr);
}

EidosValue_SP UniqueEidosValue(const EidosValue *p_arg0_value, bool p_force_new_vector)
{
	EidosValue_SP result_SP(nullptr);
	
	const EidosValue *arg0_value = p_arg0_value;
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		result_SP = arg0_value->NewMatchingType();
	}
	else if (arg0_count == 1)
	{
		if (p_force_new_vector)
			result_SP = arg0_value->VectorBasedCopy();
		else
			result_SP = arg0_value->CopyValues();
	}
	else if (arg0_type == EidosValueType::kValueLogical)
	{
		const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
		bool containsF = false, containsT = false;
		
		if (logical_vec[0])
		{
			// We have a true, look for a false
			containsT = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (!logical_vec[value_index])
				{
					containsF = true;
					break;
				}
		}
		else
		{
			// We have a false, look for a true
			containsF = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (logical_vec[value_index])
				{
					containsT = true;
					break;
				}
		}
		
		if (containsF && !containsT)
			result_SP = (p_force_new_vector ? EidosValue_SP(gStaticEidosValue_LogicalF->VectorBasedCopy()) : (EidosValue_SP)gStaticEidosValue_LogicalF);
		else if (containsT && !containsF)
			result_SP = (p_force_new_vector ? EidosValue_SP(gStaticEidosValue_LogicalT->VectorBasedCopy()) : (EidosValue_SP)gStaticEidosValue_LogicalT);
		else if (!containsT && !containsF)
			result_SP = (p_force_new_vector ? EidosValue_SP(gStaticEidosValue_Logical_ZeroVec->VectorBasedCopy()) : (EidosValue_SP)gStaticEidosValue_Logical_ZeroVec);
		else	// containsT && containsF
		{
			// In this case, we need to be careful to preserve the order of occurrence
			EidosValue_Logical *logical_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Logical();
			result_SP = EidosValue_SP(logical_result);
			
			if (logical_vec[0])
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
		// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
		const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
		EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			int64_t value = int_vec[value_index];
			int scan_index;
			
			for (scan_index = 0; scan_index < value_index; ++scan_index)
			{
				if (value == int_vec[scan_index])
					break;
			}
			
			if (scan_index == value_index)
				int_result->PushInt(value);
		}
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			double value = float_vec[value_index];
			int scan_index;
			
			for (scan_index = 0; scan_index < value_index; ++scan_index)
			{
				if (value == float_vec[scan_index])
					break;
			}
			
			if (scan_index == value_index)
				float_result->PushFloat(value);
		}
	}
	else if (arg0_type == EidosValueType::kValueString)
	{
		// We have arg0_count != 1, so the type of arg0_value must be EidosValue_String_vector; we can use the fast API
		const std::vector<std::string> &string_vec = *arg0_value->StringVector();
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			string value = string_vec[value_index];
			int scan_index;
			
			for (scan_index = 0; scan_index < value_index; ++scan_index)
			{
				if (value == string_vec[scan_index])
					break;
			}
			
			if (scan_index == value_index)
				string_result->PushString(value);
		}
	}
	else if (arg0_type == EidosValueType::kValueObject)
	{
		// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Object_vector; we can use the fast API
		const std::vector<EidosObjectElement*> &object_vec = *arg0_value->ObjectElementVector();
		EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)arg0_value)->Class());
		result_SP = EidosValue_SP(object_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			EidosObjectElement *value = object_vec[value_index];
			int scan_index;
			
			for (scan_index = 0; scan_index < value_index; ++scan_index)
			{
				if (value == object_vec[scan_index])
					break;
			}
			
			if (scan_index == value_index)
				object_result->PushObjectElement(value);
		}
	}
	
	return result_SP;
}



// ************************************************************************************
//
//	math functions
//
#pragma mark -
#pragma mark Math functions
#pragma mark -


//	(numeric)abs(numeric x)
EidosValue_SP Eidos_ExecuteFunction_abs(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_type == EidosValueType::kValueInt)
	{
		if (arg0_count == 1)
		{
			// This is an overflow-safe version of:
			//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(llabs(arg0_value->IntAtIndex(0, nullptr)));
			
			int64_t operand = arg0_value->IntAtIndex(0, nullptr);
			
			// the absolute value of INT64_MIN cannot be represented in int64_t
			if (operand == INT64_MIN)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_abs): function abs() cannot take the absolute value of the most negative integer." << eidos_terminate(nullptr);
			
			int64_t abs_result = llabs(operand);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(abs_result));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				// This is an overflow-safe version of:
				//int_result->PushInt(llabs(int_vec[value_index]));
				
				int64_t operand = int_vec[value_index];
				
				// the absolute value of INT64_MIN cannot be represented in int64_t
				if (operand == INT64_MIN)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_abs): function abs() cannot take the absolute value of the most negative integer." << eidos_terminate(nullptr);
				
				int64_t abs_result = llabs(operand);
				
				int_result->PushInt(abs_result);
			}
		}
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fabs(arg0_value->FloatAtIndex(0, nullptr))));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				float_result->PushFloat(fabs(float_vec[value_index]));
		}
	}
	
	return result_SP;
}

//	(float)acos(numeric x)
EidosValue_SP Eidos_ExecuteFunction_acos(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(acos(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(acos(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)asin(numeric x)
EidosValue_SP Eidos_ExecuteFunction_asin(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(asin(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(asin(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)atan(numeric x)
EidosValue_SP Eidos_ExecuteFunction_atan(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(atan(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(atan(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)atan2(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_atan2(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	int arg1_count = arg1_value->Count();
	
	if (arg0_count != arg1_count)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_atan2): function atan2() requires arguments of equal length." << eidos_terminate(nullptr);
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(atan2(arg0_value->FloatAtIndex(0, nullptr), arg1_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(atan2(arg0_value->FloatAtIndex(value_index, nullptr), arg1_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)ceil(float x)
EidosValue_SP Eidos_ExecuteFunction_ceil(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(ceil(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(ceil(float_vec[value_index]));
	}
	
	return result_SP;
}

//	(float)cos(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cos(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cos(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(cos(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(numeric)cumProduct(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cumProduct(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_type == EidosValueType::kValueInt)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			int64_t product = 1;
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				int64_t operand = int_vec[value_index];
				
				bool overflow = Eidos_mul_overflow(product, operand, &product);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cumProduct): integer multiplication overflow in function cumProduct()." << eidos_terminate(nullptr);
				
				int_result->PushInt(product);
			}
		}
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(arg0_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			double product = 1.0;
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				product *= float_vec[value_index];
				float_result->PushFloat(product);
			}
		}
	}
	
	return result_SP;
}

//	(numeric)cumSum(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cumSum(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_type == EidosValueType::kValueInt)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			int64_t sum = 0;
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				int64_t operand = int_vec[value_index];
				
				bool overflow = Eidos_add_overflow(sum, operand, &sum);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cumSum): integer addition overflow in function cumSum()." << eidos_terminate(nullptr);
				
				int_result->PushInt(sum);
			}
		}
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(arg0_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			double sum = 0.0;
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				sum += float_vec[value_index];
				float_result->PushFloat(sum);
			}
		}
	}
	
	return result_SP;
}

//	(float)exp(numeric x)
EidosValue_SP Eidos_ExecuteFunction_exp(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(exp(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(exp(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)floor(float x)
EidosValue_SP Eidos_ExecuteFunction_floor(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(floor(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(floor(float_vec[value_index]));
	}
	
	return result_SP;
}

//	(integer)integerDiv(integer x, integer y)
EidosValue_SP Eidos_ExecuteFunction_integerDiv(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	int arg1_count = arg1_value->Count();
	
	if ((arg0_count == 1) && (arg1_count == 1))
	{
		int64_t int1 = arg0_value->IntAtIndex(0, nullptr);
		int64_t int2 = arg1_value->IntAtIndex(0, nullptr);
		
		if (int2 == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int1 / int2));
	}
	else
	{
		if (arg0_count == arg1_count)
		{
			const std::vector<int64_t> &int1_vec = *arg0_value->IntVector();
			const std::vector<int64_t> &int2_vec = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				int64_t int1 = int1_vec[value_index];
				int64_t int2 = int2_vec[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
				
				int_result->PushInt(int1 / int2);
			}
		}
		else if (arg0_count == 1)
		{
			int64_t int1 = arg0_value->IntAtIndex(0, nullptr);
			const std::vector<int64_t> &int2_vec = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg1_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
			{
				int64_t int2 = int2_vec[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
				
				int_result->PushInt(int1 / int2);
			}
		}
		else if (arg1_count == 1)
		{
			const std::vector<int64_t> &int1_vec = *arg0_value->IntVector();
			int64_t int2 = arg1_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			if (int2 == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(int1_vec[value_index] / int2);
		}
		else	// if ((arg0_count != arg1_count) && (arg0_count != 1) && (arg1_count != 1))
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(nullptr);
		}
	}
	
	return result_SP;
}

//	(integer)integerMod(integer x, integer y)
EidosValue_SP Eidos_ExecuteFunction_integerMod(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	int arg1_count = arg1_value->Count();
	
	if ((arg0_count == 1) && (arg1_count == 1))
	{
		int64_t int1 = arg0_value->IntAtIndex(0, nullptr);
		int64_t int2 = arg1_value->IntAtIndex(0, nullptr);
		
		if (int2 == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int1 % int2));
	}
	else
	{
		if (arg0_count == arg1_count)
		{
			const std::vector<int64_t> &int1_vec = *arg0_value->IntVector();
			const std::vector<int64_t> &int2_vec = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				int64_t int1 = int1_vec[value_index];
				int64_t int2 = int2_vec[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
				
				int_result->PushInt(int1 % int2);
			}
		}
		else if (arg0_count == 1)
		{
			int64_t int1 = arg0_value->IntAtIndex(0, nullptr);
			const std::vector<int64_t> &int2_vec = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg1_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
			{
				int64_t int2 = int2_vec[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
				
				int_result->PushInt(int1 % int2);
			}
		}
		else if (arg1_count == 1)
		{
			const std::vector<int64_t> &int1_vec = *arg0_value->IntVector();
			int64_t int2 = arg1_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			if (int2 == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(int1_vec[value_index] % int2);
		}
		else	// if ((arg0_count != arg1_count) && (arg0_count != 1) && (arg1_count != 1))
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(nullptr);
		}
	}
	
	return result_SP;
}

//	(logical)isFinite(float x)
EidosValue_SP Eidos_ExecuteFunction_isFinite(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = (std::isfinite(arg0_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
		std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			logical_result_vec.emplace_back(std::isfinite(float_vec[value_index]));
	}
	
	return result_SP;
}

//	(logical)isInfinite(float x)
EidosValue_SP Eidos_ExecuteFunction_isInfinite(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = (std::isinf(arg0_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
		std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			logical_result_vec.emplace_back(std::isinf(float_vec[value_index]));
	}
	
	return result_SP;
}

//	(logical)isNAN(float x)
EidosValue_SP Eidos_ExecuteFunction_isNAN(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = (std::isnan(arg0_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
		std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			logical_result_vec.emplace_back(std::isnan(float_vec[value_index]));
	}
	
	return result_SP;
}

//	(float)log(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(log(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)log10(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log10(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log10(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(log10(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)log2(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log2(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log2(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(log2(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(numeric$)product(numeric x)
EidosValue_SP Eidos_ExecuteFunction_product(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_type == EidosValueType::kValueInt)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			int64_t product = 1;
			double product_d = 1.0;
			bool fits_in_integer = true;
			
			// We do a tricky thing here.  We want to try to compute in integer, but switch to float if we overflow.
			// If we do overflow, we want to minimize numerical error by accumulating in integer for as long as we
			// can, and then throwing the integer accumulator over into the float accumulator only when it is about
			// to overflow.  We perform both computations in parallel, and use integer for the result if we can.
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				int64_t old_product = product;
				int64_t temp = int_vec[value_index];
				
				bool overflow = Eidos_mul_overflow(old_product, temp, &product);
				
				// switch to float computation on overflow, and accumulate in the float product just before overflow
				if (overflow)
				{
					fits_in_integer = false;
					product_d *= old_product;
					product = temp;
				}
			}
			
			product_d *= product;		// multiply in whatever integer accumulation has not overflowed
			
			if (fits_in_integer)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(product));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(product_d));
		}
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(arg0_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			double product = 1;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				product *= float_vec[value_index];
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(product));
		}
	}
	
	return result_SP;
}

//	(*)setUnion(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setUnion(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if (arg0_type != arg1_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setUnion): function setUnion() requires that both operands have the same type." << eidos_terminate(nullptr);
	
	EidosValueType arg_type = arg0_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)arg0_value)->Class();
		class1 = ((EidosValue_Object *)arg1_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setUnion): function setUnion() requires that both operands of object type have the same class (or undefined class)." << eidos_terminate(nullptr);
	}
	
	if (arg0_count + arg1_count == 0)
	{
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = arg1_value->NewMatchingType();
		else
			result_SP = arg0_value->NewMatchingType();
	}
	else if ((arg0_count == 1) && (arg1_count == 0))
	{
		result_SP = arg0_value->CopyValues();
	}
	else if ((arg0_count == 0) && (arg1_count == 1))
	{
		result_SP = arg1_value->CopyValues();
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const std::vector<eidos_logical_t> &logical_vec0 = *arg0_value->LogicalVector();
		const std::vector<eidos_logical_t> &logical_vec1 = *arg1_value->LogicalVector();
		bool containsF = false, containsT = false;
		
		if (((arg0_count > 0) && logical_vec0[0]) || ((arg1_count > 0) && logical_vec1[0]))
		{
			// We have a true value; look for a false value
			containsT = true;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (!logical_vec0[value_index])
				{
					containsF = true;
					break;
				}
			
			if (!containsF)
				for (int value_index = 0; value_index < arg1_count; ++value_index)
					if (!logical_vec1[value_index])
					{
						containsF = true;
						break;
					}
		}
		else
		{
			// We have a false value; look for a true value
			containsF = true;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (logical_vec0[value_index])
				{
					containsT = true;
					break;
				}
			
			if (!containsT)
				for (int value_index = 0; value_index < arg1_count; ++value_index)
					if (logical_vec1[value_index])
					{
						containsT = true;
						break;
					}
		}
		
		if (containsF && !containsT)
			result_SP = gStaticEidosValue_LogicalF;
		else if (containsT && !containsF)
			result_SP = gStaticEidosValue_LogicalT;
		else if (!containsT && !containsF)
			result_SP = gStaticEidosValue_Logical_ZeroVec;		// CODE COVERAGE: This is dead code
		else	// containsT && containsF
		{
			EidosValue_Logical *logical_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Logical();
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->PushLogical(false);
			logical_result->PushLogical(true);
		}
	}
	else if (arg0_count == 0)
	{
		// arg0 is zero-length, arg1 is >1, so we just need to unique arg1
		result_SP = UniqueEidosValue(arg1_value, false);
	}
	else if (arg1_count == 0)
	{
		// arg1 is zero-length, arg0 is >1, so we just need to unique arg0
		result_SP = UniqueEidosValue(arg0_value, false);
	}
	else if ((arg0_count == 1) && (arg1_count == 1))
	{
		// Make a bit of an effort to produce a singleton result, while handling the singleton/singleton case
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = arg0_value->IntAtIndex(0, nullptr), int1 = arg1_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{int0, int1});
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = arg0_value->FloatAtIndex(0, nullptr), float1 = arg1_value->FloatAtIndex(0, nullptr);
			
			if (float0 == float1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{float0, float1});
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = arg0_value->StringAtIndex(0, nullptr), string1 = arg1_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{string0, string1});
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = arg0_value->ObjectElementAtIndex(0, nullptr), *obj1 = arg1_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)arg0_value)->Class()));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector({obj0, obj1}, ((EidosValue_Object *)arg0_value)->Class()));
		}
	}
	else if ((arg0_count == 1) || (arg1_count == 1))
	{
		// We have one value that is a singleton, one that is a vector.  We'd like this case to be fast,
		// so that addition of a single element to a set is a fast operation.
		if (arg0_count == 1)
		{
			std::swap(arg0_count, arg1_count);
			std::swap(arg0_value, arg1_value);
		}
		
		// now arg0_count > 1, arg1_count == 1
		result_SP = UniqueEidosValue(arg0_value, true);
		
		int result_count = result_SP->Count();
		
		// result_SP is modifiable and is guaranteed to be a vector, so now add arg1 if necessary using the fast APIs
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t value = arg1_value->IntAtIndex(0, nullptr);
			const std::vector<int64_t> &int_vec = *result_SP->IntVector();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == int_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				((EidosValue_Int_vector *)(result_SP.get()))->PushInt(value);
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double value = arg1_value->FloatAtIndex(0, nullptr);
			const std::vector<double> &float_vec = *result_SP->FloatVector();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == float_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				((EidosValue_Float_vector *)(result_SP.get()))->PushFloat(value);
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string value = arg1_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *result_SP->StringVector();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == string_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				((EidosValue_String_vector *)(result_SP.get()))->PushString(value);
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value = arg1_value->ObjectElementAtIndex(0, nullptr);
			const std::vector<EidosObjectElement *> &object_vec = *result_SP->ObjectElementVector();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == object_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				((EidosValue_Object_vector *)(result_SP.get()))->PushObjectElement(value);
		}
	}
	else
	{
		// We have two arguments which are both vectors of >1 value, so this is the base case.  We construct
		// a new EidosValue containing all elements from both arguments, and then call UniqueEidosValue() to unique it.
		// This code might look slow, but really the uniquing is O(N^2) and everything else is O(N), so since
		// we are in the vector/vector case here, it really isn't worth worrying about optimizing the O(N) part.
		result_SP = ConcatenateEidosValues(p_arguments, 2, false);
		result_SP = UniqueEidosValue(result_SP.get(), false);
	}
	
	return result_SP;
}

//	(*)setIntersection(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setIntersection(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if (arg0_type != arg1_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setIntersection): function setIntersection() requires that both operands have the same type." << eidos_terminate(nullptr);
	
	EidosValueType arg_type = arg0_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)arg0_value)->Class();
		class1 = ((EidosValue_Object *)arg1_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setIntersection): function setIntersection() requires that both operands of object type have the same class (or undefined class)." << eidos_terminate(nullptr);
	}
	
	if ((arg0_count == 0) || (arg1_count == 0))
	{
		// If either argument is empty, the intersection is the empty set
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = arg1_value->NewMatchingType();
		else
			result_SP = arg0_value->NewMatchingType();
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const std::vector<eidos_logical_t> &logical_vec0 = *arg0_value->LogicalVector();
		const std::vector<eidos_logical_t> &logical_vec1 = *arg1_value->LogicalVector();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_vec0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (!logical_vec0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (logical_vec0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_vec1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < arg1_count; ++value_index)
				if (!logical_vec1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < arg1_count; ++value_index)
				if (logical_vec1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		if (containsF0 && containsT0 && containsF1 && containsT1)
		{
			EidosValue_Logical *logical_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Logical();
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->PushLogical(false);
			logical_result->PushLogical(true);
		}
		else if (containsF0 && containsF1)
			result_SP = gStaticEidosValue_LogicalF;
		else if (containsT0 && containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else
			result_SP = gStaticEidosValue_Logical_ZeroVec;
	}
	else if ((arg0_count == 1) && (arg1_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = arg0_value->IntAtIndex(0, nullptr), int1 = arg1_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
			else
				result_SP = gStaticEidosValue_Integer_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = arg0_value->FloatAtIndex(0, nullptr), float1 = arg1_value->FloatAtIndex(0, nullptr);
			
			if (float0 == float1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
			else
				result_SP = gStaticEidosValue_Float_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = arg0_value->StringAtIndex(0, nullptr), string1 = arg1_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
			else
				result_SP = gStaticEidosValue_String_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = arg0_value->ObjectElementAtIndex(0, nullptr), *obj1 = arg1_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)arg0_value)->Class()));
			else
				result_SP = arg0_value->NewMatchingType();
		}
	}
	else if ((arg0_count == 1) || (arg1_count == 1))
	{
		// If either argument is singleton (but not both), handle that case with a fast check
		if (arg0_count == 1)
		{
			std::swap(arg0_count, arg1_count);
			std::swap(arg0_value, arg1_value);
		}
		
		// now arg0_count > 1, arg1_count == 1
		bool found_match = false;
		
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t value = arg1_value->IntAtIndex(0, nullptr);
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			
			for (int scan_index = 0; scan_index < arg0_count; ++scan_index)
				if (value == int_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double value = arg1_value->FloatAtIndex(0, nullptr);
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			
			for (int scan_index = 0; scan_index < arg0_count; ++scan_index)
				if (value == float_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string value = arg1_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *arg0_value->StringVector();
			
			for (int scan_index = 0; scan_index < arg0_count; ++scan_index)
				if (value == string_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value = arg1_value->ObjectElementAtIndex(0, nullptr);
			const std::vector<EidosObjectElement *> &object_vec = *arg0_value->ObjectElementVector();
			
			for (int scan_index = 0; scan_index < arg0_count; ++scan_index)
				if (value == object_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		
		if (found_match)
			result_SP = arg1_value->CopyValues();
		else
			result_SP = arg0_value->NewMatchingType();
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both
		if (arg0_type == EidosValueType::kValueInt)
		{
			const std::vector<int64_t> &int_vec0 = *arg0_value->IntVector();
			const std::vector<int64_t> &int_vec1 = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				int64_t value = int_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (int value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == int_vec1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == int_vec0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							int_result->PushInt(value);
						break;
					}
			}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			const std::vector<double> &float_vec0 = *arg0_value->FloatVector();
			const std::vector<double> &float_vec1 = *arg1_value->FloatVector();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				double value = float_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (int value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == float_vec1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == float_vec0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							float_result->PushFloat(value);
						break;
					}
			}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *arg0_value->StringVector();
			const std::vector<std::string> &string_vec1 = *arg1_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (int value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == string_vec1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == string_vec0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							string_result->PushString(value);
						break;
					}
			}
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			const std::vector<EidosObjectElement*> &object_vec0 = *arg0_value->ObjectElementVector();
			const std::vector<EidosObjectElement*> &object_vec1 = *arg1_value->ObjectElementVector();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)arg0_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				EidosObjectElement *value = object_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (int value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == object_vec1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == object_vec0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							object_result->PushObjectElement(value);
						break;
					}
			}
		}
	}
	
	return result_SP;
}

//	(*)setDifference(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setDifference(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if (arg0_type != arg1_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setDifference): function setDifference() requires that both operands have the same type." << eidos_terminate(nullptr);
	
	EidosValueType arg_type = arg0_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)arg0_value)->Class();
		class1 = ((EidosValue_Object *)arg1_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setDifference): function setDifference() requires that both operands of object type have the same class (or undefined class)." << eidos_terminate(nullptr);
	}
	
	if (arg0_count == 0)
	{
		// If arg0 is empty, the difference is the empty set
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = arg1_value->NewMatchingType();
		else
			result_SP = arg0_value->NewMatchingType();
	}
	else if (arg1_count == 0)
	{
		// If arg1 is empty, the difference is arg0, uniqued
		result_SP = UniqueEidosValue(arg0_value, false);
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const std::vector<eidos_logical_t> &logical_vec0 = *arg0_value->LogicalVector();
		const std::vector<eidos_logical_t> &logical_vec1 = *arg1_value->LogicalVector();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_vec0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (!logical_vec0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (logical_vec0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_vec1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < arg1_count; ++value_index)
				if (!logical_vec1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < arg1_count; ++value_index)
				if (logical_vec1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		if (containsF1 && containsT1)
			result_SP = gStaticEidosValue_Logical_ZeroVec;
		else if (containsT0 && containsF0 && !containsT1 && !containsF1)
		{
			// CODE COVERAGE: This is dead code
			EidosValue_Logical *logical_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Logical();
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->PushLogical(false);
			logical_result->PushLogical(true);
		}
		else if (containsT0 && !containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else if (containsF0 && !containsF1)
			result_SP = gStaticEidosValue_LogicalF;
		else
			result_SP = gStaticEidosValue_Logical_ZeroVec;
	}
	else if ((arg0_count == 1) && (arg1_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = arg0_value->IntAtIndex(0, nullptr), int1 = arg1_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = gStaticEidosValue_Integer_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = arg0_value->FloatAtIndex(0, nullptr), float1 = arg1_value->FloatAtIndex(0, nullptr);
			
			if (float0 == float1)
				result_SP = gStaticEidosValue_Float_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = arg0_value->StringAtIndex(0, nullptr), string1 = arg1_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = gStaticEidosValue_String_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = arg0_value->ObjectElementAtIndex(0, nullptr), *obj1 = arg1_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = arg0_value->NewMatchingType();
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)arg0_value)->Class()));
		}
	}
	else if (arg0_count == 1)
	{
		// If any element in arg1 matches the element in arg0, the result is an empty vector
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = arg0_value->IntAtIndex(0, nullptr);
			const std::vector<int64_t> &int_vec = *arg1_value->IntVector();
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				if (int0 == int_vec[value_index])
					return gStaticEidosValue_Integer_ZeroVec;
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = arg0_value->FloatAtIndex(0, nullptr);
			const std::vector<double> &float_vec = *arg1_value->FloatVector();
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				if (float0 == float_vec[value_index])
					return gStaticEidosValue_Float_ZeroVec;
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = arg0_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *arg1_value->StringVector();
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				if (string0 == string_vec[value_index])
					return gStaticEidosValue_String_ZeroVec;
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = arg0_value->ObjectElementAtIndex(0, nullptr);
			const std::vector<EidosObjectElement *> &object_vec = *arg1_value->ObjectElementVector();
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				if (obj0 == object_vec[value_index])
					return arg0_value->NewMatchingType();
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)arg0_value)->Class()));
		}
	}
	else if (arg1_count == 1)
	{
		// The result is arg0 uniqued, minus the element in arg1 if it matches
		result_SP = UniqueEidosValue(arg0_value, true);
		
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int1 = arg1_value->IntAtIndex(0, nullptr);
			std::vector<int64_t> &int_vec = *result_SP->IntVector_Mutable();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (int1 == int_vec[value_index])
				{
					int_vec.erase(int_vec.begin() + value_index);
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float1 = arg1_value->FloatAtIndex(0, nullptr);
			std::vector<double> &float_vec = *result_SP->FloatVector_Mutable();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (float1 == float_vec[value_index])
				{
					float_vec.erase(float_vec.begin() + value_index);
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string1 = arg1_value->StringAtIndex(0, nullptr);
			std::vector<std::string> &string_vec = *result_SP->StringVector_Mutable();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (string1 == string_vec[value_index])
				{
					string_vec.erase(string_vec.begin() + value_index);
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj1 = arg1_value->ObjectElementAtIndex(0, nullptr);
			std::vector<EidosObjectElement *> &object_vec = *result_SP->ObjectElementVector_Mutable();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (obj1 == object_vec[value_index])
				{
					obj1->Release();
					object_vec.erase(object_vec.begin() + value_index);
					break;
				}
		}
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both
		if (arg0_type == EidosValueType::kValueInt)
		{
			const std::vector<int64_t> &int_vec0 = *arg0_value->IntVector();
			const std::vector<int64_t> &int_vec1 = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				int64_t value = int_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == int_vec1[value_index1])
						break;
				
				if (value_index1 < arg1_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == int_vec0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				int_result->PushInt(value);
			}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			const std::vector<double> &float_vec0 = *arg0_value->FloatVector();
			const std::vector<double> &float_vec1 = *arg1_value->FloatVector();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				double value = float_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == float_vec1[value_index1])
						break;
				
				if (value_index1 < arg1_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == float_vec0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				float_result->PushFloat(value);
			}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *arg0_value->StringVector();
			const std::vector<std::string> &string_vec1 = *arg1_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == string_vec1[value_index1])
						break;
				
				if (value_index1 < arg1_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == string_vec0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				string_result->PushString(value);
			}
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			const std::vector<EidosObjectElement*> &object_vec0 = *arg0_value->ObjectElementVector();
			const std::vector<EidosObjectElement*> &object_vec1 = *arg1_value->ObjectElementVector();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)arg0_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (int value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				EidosObjectElement *value = object_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == object_vec1[value_index1])
						break;
				
				if (value_index1 < arg1_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == object_vec0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				object_result->PushObjectElement(value);
			}
		}
	}
	
	return result_SP;
}

//	(*)setSymmetricDifference(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setSymmetricDifference(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if (arg0_type != arg1_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setSymmetricDifference): function setSymmetricDifference() requires that both operands have the same type." << eidos_terminate(nullptr);
	
	EidosValueType arg_type = arg0_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)arg0_value)->Class();
		class1 = ((EidosValue_Object *)arg1_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setSymmetricDifference): function setSymmetricDifference() requires that both operands of object type have the same class (or undefined class)." << eidos_terminate(nullptr);
	}
	
	if (arg0_count + arg1_count == 0)
	{
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = arg1_value->NewMatchingType();
		else
			result_SP = arg0_value->NewMatchingType();
	}
	else if ((arg0_count == 1) && (arg1_count == 0))
	{
		result_SP = arg0_value->CopyValues();
	}
	else if ((arg0_count == 0) && (arg1_count == 1))
	{
		result_SP = arg1_value->CopyValues();
	}
	else if (arg0_count == 0)
	{
		result_SP = UniqueEidosValue(arg1_value, false);
	}
	else if (arg1_count == 0)
	{
		result_SP = UniqueEidosValue(arg0_value, false);
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const std::vector<eidos_logical_t> &logical_vec0 = *arg0_value->LogicalVector();
		const std::vector<eidos_logical_t> &logical_vec1 = *arg1_value->LogicalVector();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_vec0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (!logical_vec0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
				if (logical_vec0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_vec1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < arg1_count; ++value_index)
				if (!logical_vec1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < arg1_count; ++value_index)
				if (logical_vec1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		if ((containsF0 != containsF1) && (containsT0 != containsT1))
		{
			EidosValue_Logical *logical_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Logical();
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->PushLogical(false);
			logical_result->PushLogical(true);
		}
		else if ((containsF0 == containsF1) && (containsT0 == containsT1))
			result_SP = gStaticEidosValue_Logical_ZeroVec;
		else if (containsT0 != containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else // (containsF0 != containsF1)
			result_SP = gStaticEidosValue_LogicalF;
	}
	else if ((arg0_count == 1) && (arg1_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = arg0_value->IntAtIndex(0, nullptr), int1 = arg1_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = gStaticEidosValue_Integer_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{int0, int1});
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = arg0_value->FloatAtIndex(0, nullptr), float1 = arg1_value->FloatAtIndex(0, nullptr);
			
			if (float0 == float1)
				result_SP = gStaticEidosValue_Float_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{float0, float1});
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = arg0_value->StringAtIndex(0, nullptr), string1 = arg1_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = gStaticEidosValue_String_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{string0, string1});
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = arg0_value->ObjectElementAtIndex(0, nullptr), *obj1 = arg1_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = arg0_value->NewMatchingType();
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector({obj0, obj1}, ((EidosValue_Object *)arg0_value)->Class()));
		}
	}
	else if ((arg0_count == 1) || (arg1_count == 1))
	{
		// We have one value that is a singleton, one that is a vector.  We'd like this case to be fast,
		// so that addition of a single element to a set is a fast operation.
		if (arg0_count == 1)
		{
			std::swap(arg0_count, arg1_count);
			std::swap(arg0_value, arg1_value);
		}
		
		// now arg0_count > 1, arg1_count == 1
		result_SP = UniqueEidosValue(arg0_value, true);
		
		int result_count = result_SP->Count();
		
		// result_SP is modifiable and is guaranteed to be a vector, so now the result is arg0 uniqued,
		// minus the element in arg1 if it matches, but plus the element in arg1 if it does not match
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int1 = arg1_value->IntAtIndex(0, nullptr);
			std::vector<int64_t> &int_vec = *result_SP->IntVector_Mutable();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (int1 == int_vec[value_index])
					break;
			
			if (value_index == result_count)
				int_vec.push_back(int1);
			else
				int_vec.erase(int_vec.begin() + value_index);
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float1 = arg1_value->FloatAtIndex(0, nullptr);
			std::vector<double> &float_vec = *result_SP->FloatVector_Mutable();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (float1 == float_vec[value_index])
					break;
			
			if (value_index == result_count)
				float_vec.push_back(float1);
			else
				float_vec.erase(float_vec.begin() + value_index);
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string1 = arg1_value->StringAtIndex(0, nullptr);
			std::vector<std::string> &string_vec = *result_SP->StringVector_Mutable();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (string1 == string_vec[value_index])
					break;
			
			if (value_index == result_count)
				string_vec.push_back(string1);
			else
				string_vec.erase(string_vec.begin() + value_index);
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj1 = arg1_value->ObjectElementAtIndex(0, nullptr);
			std::vector<EidosObjectElement *> &object_vec = *result_SP->ObjectElementVector_Mutable();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (obj1 == object_vec[value_index])
					break;
			
			if (value_index == result_count)
			{
				obj1->Retain();
				object_vec.push_back(obj1);
			}
			else
			{
				obj1->Release();
				object_vec.erase(object_vec.begin() + value_index);
			}
		}
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both.  Loop through arg0 adding
		// unique values not in arg1, then loop through arg1 adding unique values not in arg0.
		int value_index0, value_index1, scan_index;
		
		if (arg0_type == EidosValueType::kValueInt)
		{
			const std::vector<int64_t> &int_vec0 = *arg0_value->IntVector();
			const std::vector<int64_t> &int_vec1 = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				int64_t value = int_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == int_vec1[value_index1])
						break;
				
				if (value_index1 == arg1_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == int_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						int_result->PushInt(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
			{
				int64_t value = int_vec1[value_index1];
				
				// First check that the value also exists in arg1
				for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
					if (value == int_vec0[value_index0])
						break;
				
				if (value_index0 == arg0_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == int_vec1[scan_index])
							break;
					
					if (scan_index == value_index1)
						int_result->PushInt(value);
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			const std::vector<double> &float_vec0 = *arg0_value->FloatVector();
			const std::vector<double> &float_vec1 = *arg1_value->FloatVector();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				double value = float_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == float_vec1[value_index1])
						break;
				
				if (value_index1 == arg1_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == float_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						float_result->PushFloat(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
			{
				double value = float_vec1[value_index1];
				
				// First check that the value also exists in arg1
				for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
					if (value == float_vec0[value_index0])
						break;
				
				if (value_index0 == arg0_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == float_vec1[scan_index])
							break;
					
					if (scan_index == value_index1)
						float_result->PushFloat(value);
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *arg0_value->StringVector();
			const std::vector<std::string> &string_vec1 = *arg1_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == string_vec1[value_index1])
						break;
				
				if (value_index1 == arg1_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == string_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						string_result->PushString(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
			{
				std::string value = string_vec1[value_index1];
				
				// First check that the value also exists in arg1
				for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
					if (value == string_vec0[value_index0])
						break;
				
				if (value_index0 == arg0_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == string_vec1[scan_index])
							break;
					
					if (scan_index == value_index1)
						string_result->PushString(value);
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			const std::vector<EidosObjectElement*> &object_vec0 = *arg0_value->ObjectElementVector();
			const std::vector<EidosObjectElement*> &object_vec1 = *arg1_value->ObjectElementVector();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)arg0_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
			{
				EidosObjectElement *value = object_vec0[value_index0];
				
				// First check that the value also exists in arg1
				for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
					if (value == object_vec1[value_index1])
						break;
				
				if (value_index1 == arg1_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == object_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						object_result->PushObjectElement(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < arg1_count; ++value_index1)
			{
				EidosObjectElement *value = object_vec1[value_index1];
				
				// First check that the value also exists in arg1
				for (value_index0 = 0; value_index0 < arg0_count; ++value_index0)
					if (value == object_vec0[value_index0])
						break;
				
				if (value_index0 == arg0_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == object_vec1[scan_index])
							break;
					
					if (scan_index == value_index1)
						object_result->PushObjectElement(value);
				}
			}
		}
	}
	
	return result_SP;
}

//	(numeric$)sum(lif x)
EidosValue_SP Eidos_ExecuteFunction_sum(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_type == EidosValueType::kValueInt)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			int64_t sum = 0;
			double sum_d = 0;
			bool fits_in_integer = true;
			
			// We do a tricky thing here.  We want to try to compute in integer, but switch to float if we overflow.
			// If we do overflow, we want to minimize numerical error by accumulating in integer for as long as we
			// can, and then throwing the integer accumulator over into the float accumulator only when it is about
			// to overflow.  We perform both computations in parallel, and use integer for the result if we can.
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				int64_t old_sum = sum;
				int64_t temp = int_vec[value_index];
				
				bool overflow = Eidos_add_overflow(old_sum, temp, &sum);
				
				// switch to float computation on overflow, and accumulate in the float sum just before overflow
				if (overflow)
				{
					fits_in_integer = false;
					sum_d += old_sum;
					sum = temp;		// start integer accumulation again from 0 until it overflows again
				}
			}
			
			sum_d += sum;			// add in whatever integer accumulation has not overflowed
			
			if (fits_in_integer)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(sum));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum_d));
		}
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		if (arg0_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(arg0_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			double sum = 0;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				sum += float_vec[value_index];
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum));
		}
	}
	else if (arg0_type == EidosValueType::kValueLogical)
	{
		// EidosValue_Logical does not have a singleton subclass, so we can always use the fast API
		const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
		int64_t sum = 0;
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			sum += logical_vec[value_index];
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(sum));
	}
	
	return result_SP;
}

//	(float)round(float x)
EidosValue_SP Eidos_ExecuteFunction_round(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(round(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(round(float_vec[value_index]));
	}
	
	return result_SP;
}

//	(float)sin(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sin(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sin(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(sin(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)sqrt(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sqrt(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sqrt(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(sqrt(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)tan(numeric x)
EidosValue_SP Eidos_ExecuteFunction_tan(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(tan(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(tan(arg0_value->FloatAtIndex(value_index, nullptr)));
	}
	
	return result_SP;
}

//	(float)trunc(float x)
EidosValue_SP Eidos_ExecuteFunction_trunc(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(trunc(arg0_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const std::vector<double> &float_vec = *arg0_value->FloatVector();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(trunc(float_vec[value_index]));
	}
	
	return result_SP;
}


// ************************************************************************************
//
//	summary statistics functions
//
#pragma mark -
#pragma mark Summary statistics functions
#pragma mark -


//	(+$)max(+ x)
EidosValue_SP Eidos_ExecuteFunction_max(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if (arg0_type == EidosValueType::kValueLogical)
	{
		eidos_logical_t max = arg0_value->LogicalAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				eidos_logical_t temp = logical_vec[value_index];
				if (max < temp)
					max = temp;
			}
		}
		
		result_SP = (max ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else if (arg0_type == EidosValueType::kValueInt)
	{
		int64_t max = arg0_value->IntAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				int64_t temp = int_vec[value_index];
				if (max < temp)
					max = temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(max));
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		double max = arg0_value->FloatAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				double temp = float_vec[value_index];
				if (max < temp)
					max = temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(max));
	}
	else if (arg0_type == EidosValueType::kValueString)
	{
		string max = arg0_value->StringAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_String_vector; we can use the fast API
			const std::vector<std::string> &string_vec = *arg0_value->StringVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				const string &temp = string_vec[value_index];
				if (max < temp)
					max = temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(max));
	}
	
	return result_SP;
}

//	(float$)mean(numeric x)
EidosValue_SP Eidos_ExecuteFunction_mean(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		double sum = 0;
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			sum += arg0_value->FloatAtIndex(value_index, nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum / arg0_count));
	}
	
	return result_SP;
}

//	(+$)min(+ x)
EidosValue_SP Eidos_ExecuteFunction_min(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if (arg0_type == EidosValueType::kValueLogical)
	{
		eidos_logical_t min = arg0_value->LogicalAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				eidos_logical_t temp = logical_vec[value_index];
				if (min > temp)
					min = temp;
			}
		}
		
		result_SP = (min ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else if (arg0_type == EidosValueType::kValueInt)
	{
		int64_t min = arg0_value->IntAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				int64_t temp = int_vec[value_index];
				if (min > temp)
					min = temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(min));
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		double min = arg0_value->FloatAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				double temp = float_vec[value_index];
				if (min > temp)
					min = temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(min));
	}
	else if (arg0_type == EidosValueType::kValueString)
	{
		string min = arg0_value->StringAtIndex(0, nullptr);
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_String_vector; we can use the fast API
			const std::vector<std::string> &string_vec = *arg0_value->StringVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				const string &temp = string_vec[value_index];
				if (min > temp)
					min = temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(min));
	}
	
	return result_SP;
}

//	(+)pmax(+ x, + y)
EidosValue_SP Eidos_ExecuteFunction_pmax(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if (arg0_type != arg1_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmax): function pmax() requires arguments x and y to be the same type." << eidos_terminate(nullptr);
	if (arg0_count != arg1_count)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmax): function pmax() requires arguments x and y to be of equal length." << eidos_terminate(nullptr);
	
	if (arg0_type == EidosValueType::kValueNULL)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if (arg0_count == 1)
	{
		// Handle the singleton case separately so we can handle the vector case quickly
		if (CompareEidosValues(*arg0_value, 0, *arg1_value, 0, nullptr) == -1)
			result_SP = arg1_value->CopyValues();
		else
			result_SP = arg0_value->CopyValues();
	}
	else
	{
		// We know the type is not NULL or object, and that arg0_count != 1; we split up by type and handle fast
		if (arg0_type == EidosValueType::kValueLogical)
		{
			const std::vector<eidos_logical_t> &logical0_vec = *arg0_value->LogicalVector();
			const std::vector<eidos_logical_t> &logical1_vec = *arg1_value->LogicalVector();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
			std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				logical_result_vec.emplace_back(logical0_vec[value_index] || logical1_vec[value_index]); // || is logical max
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			const std::vector<int64_t> &int0_vec = *arg0_value->IntVector();
			const std::vector<int64_t> &int1_vec = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(std::max(int0_vec[value_index], int1_vec[value_index]));
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			const std::vector<double> &float0_vec = *arg0_value->FloatVector();
			const std::vector<double> &float1_vec = *arg1_value->FloatVector();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				float_result->PushFloat(std::max(float0_vec[value_index], float1_vec[value_index]));
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string0_vec = *arg0_value->StringVector();
			const std::vector<std::string> &string1_vec = *arg1_value->StringVector();
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				string_result->PushString(std::max(string0_vec[value_index], string1_vec[value_index]));
		}
	}
	
	return result_SP;
}

//	(+)pmin(+ x, + y)
EidosValue_SP Eidos_ExecuteFunction_pmin(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if (arg0_type != arg1_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmin): function pmin() requires arguments x and y to be the same type." << eidos_terminate(nullptr);
	if (arg0_count != arg1_count)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmin): function pmin() requires arguments x and y to be of equal length." << eidos_terminate(nullptr);
	
	if (arg0_type == EidosValueType::kValueNULL)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if (arg0_count == 1)
	{
		// Handle the singleton case separately so we can handle the vector case quickly
		if (CompareEidosValues(*arg0_value, 0, *arg1_value, 0, nullptr) == 1)
			result_SP = arg1_value->CopyValues();
		else
			result_SP = arg0_value->CopyValues();
	}
	else
	{
		// We know the type is not NULL or object, and that arg0_count != 1; we split up by type and handle fast
		if (arg0_type == EidosValueType::kValueLogical)
		{
			const std::vector<eidos_logical_t> &logical0_vec = *arg0_value->LogicalVector();
			const std::vector<eidos_logical_t> &logical1_vec = *arg1_value->LogicalVector();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
			std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				logical_result_vec.emplace_back(logical0_vec[value_index] && logical1_vec[value_index]); // && is logical min
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			const std::vector<int64_t> &int0_vec = *arg0_value->IntVector();
			const std::vector<int64_t> &int1_vec = *arg1_value->IntVector();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(std::min(int0_vec[value_index], int1_vec[value_index]));
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			const std::vector<double> &float0_vec = *arg0_value->FloatVector();
			const std::vector<double> &float1_vec = *arg1_value->FloatVector();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				float_result->PushFloat(std::min(float0_vec[value_index], float1_vec[value_index]));
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string0_vec = *arg0_value->StringVector();
			const std::vector<std::string> &string1_vec = *arg1_value->StringVector();
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				string_result->PushString(std::min(string0_vec[value_index], string1_vec[value_index]));
		}
	}
	
	return result_SP;
}

//	(numeric)range(numeric x)
EidosValue_SP Eidos_ExecuteFunction_range(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if (arg0_type == EidosValueType::kValueInt)
	{
		EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
		result_SP = EidosValue_SP(int_result);
		
		int64_t max = arg0_value->IntAtIndex(0, nullptr);
		int64_t min = max;
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
			const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				int64_t temp = int_vec[value_index];
				if (max < temp)
					max = temp;
				else if (min > temp)
					min = temp;
			}
		}
		
		int_result->PushInt(min);
		int_result->PushInt(max);
	}
	else if (arg0_type == EidosValueType::kValueFloat)
	{
		EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
		result_SP = EidosValue_SP(float_result);
		
		double max = arg0_value->FloatAtIndex(0, nullptr);
		double min = max;
		
		if (arg0_count > 1)
		{
			// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
			const std::vector<double> &float_vec = *arg0_value->FloatVector();
			
			for (int value_index = 1; value_index < arg0_count; ++value_index)
			{
				double temp = float_vec[value_index];
				if (max < temp)
					max = temp;
				else if (min > temp)
					min = temp;
			}
		}
		
		float_result->PushFloat(min);
		float_result->PushFloat(max);
	}
	
	return result_SP;
}

//	(float$)sd(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sd(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count > 1)
	{
		double mean = 0;
		double sd = 0;
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			mean += arg0_value->FloatAtIndex(value_index, nullptr);
		
		mean /= arg0_count;
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			double temp = (arg0_value->FloatAtIndex(value_index, nullptr) - mean);
			sd += temp * temp;
		}
		
		sd = sqrt(sd / (arg0_count - 1));
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sd));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}


// ************************************************************************************
//
//	distribution draw / density functions
//
#pragma mark -
#pragma mark Distribution draw/density functions
#pragma mark -


//	(float)dnorm(float x, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_dnorm(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_quantiles = arg_quantile->Count();
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires mean to be of length 1 or equal in length to x." << eidos_terminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd to be of length 1 or equal in length to x." << eidos_terminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd > 0.0 (" << sigma0 << " supplied)." << eidos_terminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gaussian_pdf(arg_quantile->FloatAtIndex(0, nullptr) - mu0, sigma0)));
		}
		else
		{
			const std::vector<double> &float_vec = *arg_quantile->FloatVector();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->PushFloat(gsl_ran_gaussian_pdf(float_vec[value_index] - mu0, sigma0));
		}
	}
	else
	{
		const std::vector<double> &float_vec = *arg_quantile->FloatVector();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(value_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(value_index, nullptr));
			
			if (sigma <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd > 0.0 (" << sigma << " supplied)." << eidos_terminate(nullptr);
			
			float_result->PushFloat(gsl_ran_gaussian_pdf(float_vec[value_index] - mu, sigma));
		}
	}
	
	return result_SP;
}

//	(integer)rbinom(integer$ n, integer size, float prob)
EidosValue_SP Eidos_ExecuteFunction_rbinom(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	EidosValue *arg_size = p_arguments[1].get();
	EidosValue *arg_prob = p_arguments[2].get();
	int arg_size_count = arg_size->Count();
	int arg_prob_count = arg_prob->Count();
	bool size_singleton = (arg_size_count == 1);
	bool prob_singleton = (arg_prob_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!size_singleton && (arg_size_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size to be of length 1 or n." << eidos_terminate(nullptr);
	if (!prob_singleton && (arg_prob_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires prob to be of length 1 or n." << eidos_terminate(nullptr);
	
	int size0 = (int)arg_size->IntAtIndex(0, nullptr);
	double probability0 = arg_prob->FloatAtIndex(0, nullptr);
	
	if (size_singleton && prob_singleton)
	{
		if (size0 < 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size >= 0 (" << size0 << " supplied)." << eidos_terminate(nullptr);
		if ((probability0 < 0.0) || (probability0 > 1.0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires probability in [0.0, 1.0] (" << probability0 << " supplied)." << eidos_terminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_binomial(gEidos_rng, probability0, size0)));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(int_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				int_result->PushInt(gsl_ran_binomial(gEidos_rng, probability0, size0));
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			int size = (size_singleton ? size0 : (int)arg_size->IntAtIndex(draw_index, nullptr));
			double probability = (prob_singleton ? probability0 : arg_prob->FloatAtIndex(draw_index, nullptr));
			
			if (size < 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size >= 0 (" << size << " supplied)." << eidos_terminate(nullptr);
			if ((probability < 0.0) || (probability > 1.0))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires probability in [0.0, 1.0] (" << probability << " supplied)." << eidos_terminate(nullptr);
			
			int_result->PushInt(gsl_ran_binomial(gEidos_rng, probability, size));
		}
	}
	
	return result_SP;
}

//	(float)rexp(integer$ n, [numeric mu = 1])
EidosValue_SP Eidos_ExecuteFunction_rexp(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	int arg_mu_count = arg_mu->Count();
	bool mu_singleton = (arg_mu_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rexp): function rexp() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!mu_singleton && (arg_mu_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rexp): function rexp() requires mu to be of length 1 or n." << eidos_terminate(nullptr);
	
	if (mu_singleton)
	{
		double mu0 = arg_mu->FloatAtIndex(0, nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_exponential(gEidos_rng, mu0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->PushFloat(gsl_ran_exponential(gEidos_rng, mu0));
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double mu = arg_mu->FloatAtIndex(draw_index, nullptr);
			
			float_result->PushFloat(gsl_ran_exponential(gEidos_rng, mu));
		}
	}
	
	return result_SP;
}

//	(float)rgamma(integer$ n, numeric mean, numeric shape)
EidosValue_SP Eidos_ExecuteFunction_rgamma(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	EidosValue *arg_mean = p_arguments[1].get();
	EidosValue *arg_shape = p_arguments[2].get();
	int arg_mean_count = arg_mean->Count();
	int arg_shape_count = arg_shape->Count();
	bool mean_singleton = (arg_mean_count == 1);
	bool shape_singleton = (arg_shape_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!mean_singleton && (arg_mean_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires mean to be of length 1 or n." << eidos_terminate(nullptr);
	if (!shape_singleton && (arg_shape_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape to be of length 1 or n." << eidos_terminate(nullptr);
	
	double mean0 = (arg_mean_count ? arg_mean->FloatAtIndex(0, nullptr) : 1.0);
	double shape0 = (arg_shape_count ? arg_shape->FloatAtIndex(0, nullptr) : 0.0);
	
	if (mean_singleton && shape_singleton)
	{
		if (shape0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape > 0.0 (" << shape0 << " supplied)." << eidos_terminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gamma(gEidos_rng, shape0, mean0/shape0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			double scale = mean0 / shape0;
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->PushFloat(gsl_ran_gamma(gEidos_rng, shape0, scale));
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double mean = (mean_singleton ? mean0 : arg_mean->FloatAtIndex(draw_index, nullptr));
			double shape = (shape_singleton ? shape0 : arg_shape->FloatAtIndex(draw_index, nullptr));
			
			if (shape <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape > 0.0 (" << shape << " supplied)." << eidos_terminate(nullptr);
			
			float_result->PushFloat(gsl_ran_gamma(gEidos_rng, shape, mean / shape));
		}
	}
	
	return result_SP;
}

//	(float)rlnorm(integer$ n, [numeric meanlog = 0], [numeric sdlog = 1])
EidosValue_SP Eidos_ExecuteFunction_rlnorm(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg_meanlog = p_arguments[1].get();
	EidosValue *arg_sdlog = p_arguments[2].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	int arg_meanlog_count = arg_meanlog->Count();
	int arg_sdlog_count = arg_sdlog->Count();
	bool meanlog_singleton = (arg_meanlog_count == 1);
	bool sdlog_singleton = (arg_sdlog_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!meanlog_singleton && (arg_meanlog_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires meanlog to be of length 1 or n." << eidos_terminate(nullptr);
	if (!sdlog_singleton && (arg_sdlog_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires sdlog to be of length 1 or n." << eidos_terminate(nullptr);
	
	double meanlog0 = (arg_meanlog_count ? arg_meanlog->FloatAtIndex(0, nullptr) : 0.0);
	double sdlog0 = (arg_sdlog_count ? arg_sdlog->FloatAtIndex(0, nullptr) : 1.0);
	
	if (meanlog_singleton && sdlog_singleton)
	{
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_lognormal(gEidos_rng, meanlog0, sdlog0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->PushFloat(gsl_ran_lognormal(gEidos_rng, meanlog0, sdlog0));
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double meanlog = (meanlog_singleton ? meanlog0 : arg_meanlog->FloatAtIndex(draw_index, nullptr));
			double sdlog = (sdlog_singleton ? sdlog0 : arg_sdlog->FloatAtIndex(draw_index, nullptr));
			
			float_result->PushFloat(gsl_ran_lognormal(gEidos_rng, meanlog, sdlog));
		}
	}
	
	return result_SP;
}

//	(float)rnorm(integer$ n, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_rnorm(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!mu_singleton && (arg_mu_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires mean to be of length 1 or n." << eidos_terminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd to be of length 1 or n." << eidos_terminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 < 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd >= 0.0 (" << sigma0 << " supplied)." << eidos_terminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gaussian(gEidos_rng, sigma0) + mu0));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->PushFloat(gsl_ran_gaussian(gEidos_rng, sigma0) + mu0);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(draw_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(draw_index, nullptr));
			
			if (sigma < 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd >= 0.0 (" << sigma << " supplied)." << eidos_terminate(nullptr);
			
			float_result->PushFloat(gsl_ran_gaussian(gEidos_rng, sigma) + mu);
		}
	}
	
	return result_SP;
}

//	(integer)rpois(integer$ n, numeric lambda)
EidosValue_SP Eidos_ExecuteFunction_rpois(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	EidosValue *arg_lambda = p_arguments[1].get();
	int arg_lambda_count = arg_lambda->Count();
	bool lambda_singleton = (arg_lambda_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!lambda_singleton && (arg_lambda_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda to be of length 1 or n." << eidos_terminate(nullptr);
	
	// Here we ignore USE_GSL_POISSON and always use the GSL.  This is because we don't know whether lambda (otherwise known as mu) is
	// small or large, and because we don't know what level of accuracy is demanded by whatever the user is doing with the deviates,
	// and so forth; it makes sense to just rely on the GSL for maximal accuracy and reliability.
	
	if (lambda_singleton)
	{
		double lambda0 = arg_lambda->FloatAtIndex(0, nullptr);
		
		if (lambda0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda > 0.0 (" << lambda0 << " supplied)." << eidos_terminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_poisson(gEidos_rng, lambda0)));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(int_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				int_result->PushInt(gsl_ran_poisson(gEidos_rng, lambda0));
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double lambda = arg_lambda->FloatAtIndex(draw_index, nullptr);
			
			if (lambda <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda > 0.0 (" << lambda << " supplied)." << eidos_terminate(nullptr);
			
			int_result->PushInt(gsl_ran_poisson(gEidos_rng, lambda));
		}
	}
	
	return result_SP;
}

//	(float)runif(integer$ n, [numeric min = 0], [numeric max = 1])
EidosValue_SP Eidos_ExecuteFunction_runif(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue *arg_min = p_arguments[1].get();
	EidosValue *arg_max = p_arguments[2].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	int arg_min_count = arg_min->Count();
	int arg_max_count = arg_max->Count();
	bool min_singleton = (arg_min_count == 1);
	bool max_singleton = (arg_max_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!min_singleton && (arg_min_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min to be of length 1 or n." << eidos_terminate(nullptr);
	if (!max_singleton && (arg_max_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires max to be of length 1 or n." << eidos_terminate(nullptr);
	
	double min_value0 = (arg_min_count ? arg_min->FloatAtIndex(0, nullptr) : 0.0);
	double max_value0 = (arg_max_count ? arg_max->FloatAtIndex(0, nullptr) : 1.0);
	
	if (min_singleton && max_singleton && (min_value0 == 0.0) && (max_value0 == 1.0))
	{
		// With the default min and max, we can streamline quite a bit
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_rng_uniform(gEidos_rng)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->PushFloat(gsl_rng_uniform(gEidos_rng));
		}
	}
	else
	{
		double range0 = max_value0 - min_value0;
		
		if (min_singleton && max_singleton)
		{
			if (range0 < 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min < max." << eidos_terminate(nullptr);
			
			if (num_draws == 1)
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_rng_uniform(gEidos_rng) * range0 + min_value0));
			}
			else
			{
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
				result_SP = EidosValue_SP(float_result);
				
				for (int draw_index = 0; draw_index < num_draws; ++draw_index)
					float_result->PushFloat(gsl_rng_uniform(gEidos_rng) * range0 + min_value0);
			}
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				double min_value = (min_singleton ? min_value0 : arg_min->FloatAtIndex(draw_index, nullptr));
				double max_value = (max_singleton ? max_value0 : arg_max->FloatAtIndex(draw_index, nullptr));
				double range = max_value - min_value;
				
				if (range < 0.0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min < max." << eidos_terminate(nullptr);
				
				float_result->PushFloat(gsl_rng_uniform(gEidos_rng) * range + min_value);
			}
		}
	}
	
	return result_SP;
}

//	(float)rweibull(integer$ n, numeric lambda, numeric k)
EidosValue_SP Eidos_ExecuteFunction_rweibull(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
	EidosValue *arg_lambda = p_arguments[1].get();
	EidosValue *arg_k = p_arguments[2].get();
	int arg_lambda_count = arg_lambda->Count();
	int arg_k_count = arg_k->Count();
	bool lambda_singleton = (arg_lambda_count == 1);
	bool k_singleton = (arg_k_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
	if (!lambda_singleton && (arg_lambda_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda to be of length 1 or n." << eidos_terminate(nullptr);
	if (!k_singleton && (arg_k_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k to be of length 1 or n." << eidos_terminate(nullptr);
	
	double lambda0 = (arg_lambda_count ? arg_lambda->FloatAtIndex(0, nullptr) : 0.0);
	double k0 = (arg_k_count ? arg_k->FloatAtIndex(0, nullptr) : 0.0);
	
	if (lambda_singleton && k_singleton)
	{
		if (lambda0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda > 0.0 (" << lambda0 << " supplied)." << eidos_terminate(nullptr);
		if (k0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k > 0.0 (" << k0 << " supplied)." << eidos_terminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_weibull(gEidos_rng, lambda0, k0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->PushFloat(gsl_ran_weibull(gEidos_rng, lambda0, k0));
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double lambda = (lambda_singleton ? lambda0 : arg_lambda->FloatAtIndex(draw_index, nullptr));
			double k = (k_singleton ? k0 : arg_k->FloatAtIndex(draw_index, nullptr));
			
			if (lambda <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda > 0.0 (" << lambda << " supplied)." << eidos_terminate(nullptr);
			if (k <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k > 0.0 (" << k << " supplied)." << eidos_terminate(nullptr);
			
			float_result->PushFloat(gsl_ran_weibull(gEidos_rng, lambda, k));
		}
	}
	
	return result_SP;
}

// ************************************************************************************
//
//	vector construction functions
//
#pragma mark -
#pragma mark Vector conversion functions
#pragma mark -


//	(*)c(...)
EidosValue_SP Eidos_ExecuteFunction_c(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	result_SP = ConcatenateEidosValues(p_arguments, p_argument_count, true);
	
	return result_SP;
}

//	(float)float(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_float(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_float): function float() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)element_count);
	result_SP = EidosValue_SP(float_result);
	
	for (int64_t value_index = element_count; value_index > 0; --value_index)
		float_result->PushFloat(0.0);
	
	return result_SP;
}

//	(integer)integer(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_integer(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integer): function integer() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)element_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int64_t value_index = element_count; value_index > 0; --value_index)
		int_result->PushInt(0);
	
	return result_SP;
}

//	(logical)logical(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_logical(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_logical): function logical() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
	
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve((int)element_count);
	std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
	result_SP = EidosValue_SP(logical_result);
	
	for (int64_t value_index = element_count; value_index > 0; --value_index)
		logical_result_vec.emplace_back(false);
	
	return result_SP;
}

//	(object<undefined>)object(void)
EidosValue_SP Eidos_ExecuteFunction_object(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	result_SP = gStaticEidosValue_Object_ZeroVec;
	
	return result_SP;
}

//	(*)rep(* x, integer$ count)
EidosValue_SP Eidos_ExecuteFunction_rep(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	
	int64_t rep_count = arg1_value->IntAtIndex(0, nullptr);
	
	if (rep_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rep): function rep() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << eidos_terminate(nullptr);
	
	// the return type depends on the type of the first argument, which will get replicated
	result_SP = arg0_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
		for (int value_idx = 0; value_idx < arg0_count; value_idx++)
			result->PushValueFromIndexOfEidosValue(value_idx, *arg0_value, nullptr);
	
	return result_SP;
}

//	(*)repEach(* x, integer count)
EidosValue_SP Eidos_ExecuteFunction_repEach(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	int arg1_count = arg1_value->Count();
	
	// the return type depends on the type of the first argument, which will get replicated
	result_SP = arg0_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	if (arg1_count == 1)
	{
		int64_t rep_count = arg1_value->IntAtIndex(0, nullptr);
		
		if (rep_count < 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << eidos_terminate(nullptr);
		
		for (int value_idx = 0; value_idx < arg0_count; value_idx++)
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfEidosValue(value_idx, *arg0_value, nullptr);
	}
	else if (arg1_count == arg0_count)
	{
		for (int value_idx = 0; value_idx < arg0_count; value_idx++)
		{
			int64_t rep_count = arg1_value->IntAtIndex(value_idx, nullptr);
			
			if (rep_count < 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires all elements of count to be greater than or equal to 0 (" << rep_count << " supplied)." << eidos_terminate(nullptr);
			
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfEidosValue(value_idx, *arg0_value, nullptr);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires that parameter count's size() either (1) be equal to 1, or (2) be equal to the size() of its first argument." << eidos_terminate(nullptr);
	}
	
	return result_SP;
}

//	(*)sample(* x, integer$ size, [logical$ replace = F], [Nif weights = NULL])
EidosValue_SP Eidos_ExecuteFunction_sample(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t sample_size = p_arguments[1]->IntAtIndex(0, nullptr);
	bool replace = p_arguments[2]->LogicalAtIndex(0, nullptr);
	EidosValue *weights_value = p_arguments[3].get();
	int arg0_count = arg0_value->Count();
	
	if (sample_size < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires a sample size >= 0 (" << sample_size << " supplied)." << eidos_terminate(nullptr);
	if (sample_size == 0)
	{
		result_SP = arg0_value->NewMatchingType();
		return result_SP;
	}
	
	if (arg0_count == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() provided with insufficient elements (0 supplied)." << eidos_terminate(nullptr);
	
	if (!replace && (arg0_count < sample_size))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() provided with insufficient elements (" << arg0_count << " supplied, " << sample_size << " needed)." << eidos_terminate(nullptr);
	
	result_SP = arg0_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	// the algorithm used depends on whether weights were supplied
	if (weights_value->Type() != EidosValueType::kValueNULL)
	{
		// weights supplied
		vector<double> weights_vector;
		double weights_sum = 0.0;
		int arg3_count = weights_value->Count();
		
		if (arg3_count != arg0_count)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires x and weights to be the same length." << eidos_terminate(nullptr);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			double weight = weights_value->FloatAtIndex(value_index, nullptr);
			
			if (weight < 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << weight << " supplied)." << eidos_terminate(nullptr);
			
			weights_vector.emplace_back(weight);
			weights_sum += weight;
		}
		
		// get indices of x; we sample from this vector and then look up the corresponding EidosValue element
		vector<int> index_vector;
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			index_vector.emplace_back(value_index);
		
		// do the sampling
		int64_t contender_count = arg0_count;
		
		for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
		{
			if (contender_count <= 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() ran out of eligible elements from which to sample." << eidos_terminate(nullptr);		// CODE COVERAGE: This is dead code
			if (weights_sum <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << eidos_terminate(nullptr);
			
			double rose = gsl_rng_uniform(gEidos_rng) * weights_sum;
			double rose_sum = 0.0;
			int rose_index;
			
			for (rose_index = 0; rose_index < contender_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
			{
				rose_sum += weights_vector[rose_index];
				
				if (rose <= rose_sum)
					break;
			}
			
			result->PushValueFromIndexOfEidosValue(index_vector[rose_index], *arg0_value, nullptr);
			
			if (!replace)
			{
				weights_sum -= weights_vector[rose_index];	// possible source of numerical error
				
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
				result->PushValueFromIndexOfEidosValue((int)gsl_rng_uniform_int(gEidos_rng, arg0_count), *arg0_value, nullptr);
		}
		else
		{
			// get indices of x; we sample from this vector and then look up the corresponding EidosValue element
			vector<int> index_vector;
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				index_vector.emplace_back(value_index);
			
			// do the sampling
			int64_t contender_count = arg0_count;
			
			for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
			{
				// this error should never occur, since we checked the count above
				if (contender_count <= 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): (internal error) function sample() ran out of eligible elements from which to sample." << eidos_terminate(nullptr);		// CODE COVERAGE: This is dead code
				
				int rose_index = (int)gsl_rng_uniform_int(gEidos_rng, contender_count);
				
				result->PushValueFromIndexOfEidosValue(index_vector[rose_index], *arg0_value, nullptr);
				
				index_vector.erase(index_vector.begin() + rose_index);
				--contender_count;
			}
		}
	}
	
	return result_SP;
}

//	(numeric)seq(numeric$ from, numeric$ to, [Nif$ by = NULL])
EidosValue_SP Eidos_ExecuteFunction_seq(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	EidosValue *arg2_value = p_arguments[2].get();
	EidosValueType arg2_type = arg2_value->Type();
	
	if ((arg0_type == EidosValueType::kValueFloat) || (arg1_type == EidosValueType::kValueFloat) || (arg2_type == EidosValueType::kValueFloat))
	{
		// float return case
		double first_value = arg0_value->FloatAtIndex(0, nullptr);
		double second_value = arg1_value->FloatAtIndex(0, nullptr);
		double default_by = ((first_value < second_value) ? 1 : -1);
		double by_value = ((arg2_type != EidosValueType::kValueNULL) ? arg2_value->FloatAtIndex(0, nullptr) : default_by);
		
		if (by_value == 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires by != 0." << eidos_terminate(nullptr);
		if (((first_value < second_value) && (by_value < 0)) || ((first_value > second_value) && (by_value > 0)))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() by has incorrect sign." << eidos_terminate(nullptr);
		
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(int(1 + ceil((second_value - first_value) / by_value)));	// take a stab at a reserve size; might not be quite right, but no harm
		result_SP = EidosValue_SP(float_result);
		
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
		int64_t first_value = arg0_value->IntAtIndex(0, nullptr);
		int64_t second_value = arg1_value->IntAtIndex(0, nullptr);
		int64_t default_by = ((first_value < second_value) ? 1 : -1);
		int64_t by_value = ((arg2_type != EidosValueType::kValueNULL) ? arg2_value->IntAtIndex(0, nullptr) : default_by);
		
		if (by_value == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires by != 0." << eidos_terminate(nullptr);
		if (((first_value < second_value) && (by_value < 0)) || ((first_value > second_value) && (by_value > 0)))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() by has incorrect sign." << eidos_terminate(nullptr);
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)(1 + (second_value - first_value) / by_value));		// take a stab at a reserve size; might not be quite right, but no harm
		result_SP = EidosValue_SP(int_result);
		
		if (by_value > 0)
			for (int64_t seq_value = first_value; seq_value <= second_value; seq_value += by_value)
				int_result->PushInt(seq_value);
		else
			for (int64_t seq_value = first_value; seq_value >= second_value; seq_value += by_value)
				int_result->PushInt(seq_value);
	}
	
	return result_SP;
}

//	(integer)seqAlong(* x)
EidosValue_SP Eidos_ExecuteFunction_seqAlong(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
		int_result->PushInt(value_index);
	
	return result_SP;
}

//	(string)string(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_string(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_string): function string() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
	
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)element_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int64_t value_index = element_count; value_index > 0; --value_index)
		string_result->PushString(gEidosStr_empty_string);
	
	return result_SP;
}


// ************************************************************************************
//
//	value inspection/manipulation functions
//
#pragma mark -
#pragma mark Value inspection/manipulation functions
#pragma mark -


//	(logical$)all(logical x)
EidosValue_SP Eidos_ExecuteFunction_all(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
	
	result_SP = gStaticEidosValue_LogicalT;
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
		if (!logical_vec[value_index])
		{
			result_SP = gStaticEidosValue_LogicalF;
			break;
		}
	
	return result_SP;
}

//	(logical$)any(logical x)
EidosValue_SP Eidos_ExecuteFunction_any(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
	
	result_SP = gStaticEidosValue_LogicalF;
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
		if (logical_vec[value_index])
		{
			result_SP = gStaticEidosValue_LogicalT;
			break;
		}
	
	return result_SP;
}

//	(void)cat(* x, [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_cat(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValueType arg0_type = arg0_value->Type();
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
	{
		if (value_index > 0)
			output_stream << separator;
		
		if (arg0_type == EidosValueType::kValueObject)
			output_stream << *arg0_value->ObjectElementAtIndex(value_index, nullptr);
		else
			output_stream << arg0_value->StringAtIndex(value_index, nullptr);
	}
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(string)format(string$ format, numeric x)
EidosValue_SP Eidos_ExecuteFunction_format(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	string format = arg0_value->StringAtIndex(0, nullptr);
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	// Check the format string for correct syntax.  We have to be pretty careful about what we pass on to C++, both
	// for robustness and for security.  We allow the standard flags (+- #0), an integer field width (but not *), and
	// an integer precision (but not *).  For integer x we allow %d %i %o %x %X, for float x we allow %f %F %e %E %g %G;
	// other conversion specifiers are not allowed.  We do not allow a length modifier; we supply the correct length
	// modifier ourselves, which is platform-dependent.  We allow the format to be embedded within a longer string,
	// as usual, for convenience, but only one % specifier may exist within the format string.
	int length = (int)format.length();
	int pos = 0;
	int conversion_specifier_pos = -1;
	char conv_ch = ' ';
	bool flag_plus = false, flag_minus = false, flag_space = false, flag_pound = false, flag_zero = false;
	
	while (pos < length)
	{
		if (format[pos] == '%')
		{
			if ((pos + 1 < length) && (format[pos + 1] == '%'))
			{
				// skip over %% escapes
				pos += 2;
			}
			else if (conversion_specifier_pos != -1)
			{
				// we already saw a format specifier
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); only one % escape is allowed." << eidos_terminate(nullptr);
			}
			else
			{
				// other uses of % must be the format specifier, which we now parse
				
				// skip the %
				++pos;
				
				// skip over the optional +- #0 flags
				while (pos < length)
				{
					char flag = format[pos];
					
					if (flag == '+')
					{
						if (flag_plus)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '+' specified more than once." << eidos_terminate(nullptr);
						
						flag_plus = true;
						++pos;	// skip the '+'
					}
					else if (flag == '-')
					{
						if (flag_minus)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '-' specified more than once." << eidos_terminate(nullptr);
						
						flag_minus = true;
						++pos;	// skip the '-'
					}
					else if (flag == ' ')
					{
						if (flag_space)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag ' ' specified more than once." << eidos_terminate(nullptr);
						
						flag_space = true;
						++pos;	// skip the ' '
					}
					else if (flag == '#')
					{
						if (flag_pound)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '#' specified more than once." << eidos_terminate(nullptr);
						
						flag_pound = true;
						++pos;	// skip the '#'
					}
					else if (flag == '0')
					{
						if (flag_zero)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '0' specified more than once." << eidos_terminate(nullptr);
						
						flag_zero = true;
						++pos;	// skip the '0'
					}
					else
					{
						// not a flag character, so we are done with our optional flags
						break;
					}
				}
				
				// skip over the optional field width; eat a [1-9] followed by any number of [0-9]
				if (pos < length)
				{
					char fieldwidth_ch = format[pos];
					
					if ((fieldwidth_ch >= '1') && (fieldwidth_ch <= '9'))
					{
						// skip the leading digit
						++pos;
						
						while (pos < length)
						{
							fieldwidth_ch = format[pos];
							
							if ((fieldwidth_ch >= '0') && (fieldwidth_ch <= '9'))
								++pos;	// skip the digit
							else
								break;
						}
					}
				}
				
				// skip the optional precision specifier, a '.' followed by an integer
				if ((pos < length) && (format[pos] == '.'))
				{
					// skip the leading '.'
					++pos;
					
					while (pos < length)
					{
						char precision_ch = format[pos];
						
						if ((precision_ch >= '0') && (precision_ch <= '9'))
							++pos;	// skip the digit
						else
							break;
					}
				}
				
				// now eat the required conversion specifier
				if (pos < length)
				{
					conv_ch = format[pos];
					
					conversion_specifier_pos = pos;
					++pos;
					
					if ((conv_ch == 'd') || (conv_ch == 'i') || (conv_ch == 'o') || (conv_ch == 'x') || (conv_ch == 'X'))
					{
						if (arg1_type != EidosValueType::kValueInt)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type integer." << eidos_terminate(nullptr);
					}
					else if ((conv_ch == 'f') || (conv_ch == 'F') || (conv_ch == 'e') || (conv_ch == 'E') || (conv_ch == 'g') || (conv_ch == 'G'))
					{
						if (arg1_type != EidosValueType::kValueFloat)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type float." << eidos_terminate(nullptr);
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' not supported." << eidos_terminate(nullptr);
					}
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); missing conversion specifier after '%'." << eidos_terminate(nullptr);
				}
			}
		}
		else
		{
			// Skip over all other characters
			++pos;
		}
	}
	
	// Fix the format string to have the correct length modifier.  This is an issue only for integer; for float, the
	// default is double anyway so we're fine.  For integer, the correct format strings are defined by <cinttypes>:
	// PRId64, PRIi64, PRIo64, PRIx64, and PRIX64.
	if (arg1_type == EidosValueType::kValueInt)
	{
		string new_conv_string;
		
		if (conv_ch == 'd')
			new_conv_string = PRId64;
		else if (conv_ch == 'i')
			new_conv_string = PRIi64;
		else if (conv_ch == 'o')
			new_conv_string = PRIo64;
		else if (conv_ch == 'x')
			new_conv_string = PRIx64;
		else if (conv_ch == 'X')
			new_conv_string = PRIX64;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): (internal error) bad format string in function format(); conversion specifier '" << conv_ch << "' not recognized." << eidos_terminate(nullptr);		// CODE COVERAGE: This is dead code
		
		format.replace(conversion_specifier_pos, 1, new_conv_string);
	}
	
	// Check for possibilities that produce undefined behavior according to the C++11 standard
	if (flag_pound && ((conv_ch == 'd') || (conv_ch == 'i')))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); the flag '#' may not be used with the conversion specifier '" << conv_ch << "'." << eidos_terminate(nullptr);
	
	if (arg1_count == 1)
	{
		// singleton case
		string result_string;
		
		if (arg1_type == EidosValueType::kValueInt)
			result_string = EidosStringFormat(format, arg1_value->IntAtIndex(0, nullptr));
		else if (arg1_type == EidosValueType::kValueFloat)
			result_string = EidosStringFormat(format, arg1_value->FloatAtIndex(0, nullptr));
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
	}
	else
	{
		// non-singleton x vector, with a singleton format vector
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(arg1_count);
		result_SP = EidosValue_SP(string_result);
		
		if (arg1_type == EidosValueType::kValueInt)
		{
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				string_result->PushString(EidosStringFormat(format, arg1_value->IntAtIndex(value_index, nullptr)));
		}
		else if (arg1_type == EidosValueType::kValueFloat)
		{
			for (int value_index = 0; value_index < arg1_count; ++value_index)
				string_result->PushString(EidosStringFormat(format, arg1_value->FloatAtIndex(value_index, nullptr)));
		}
	}
	
	return result_SP;
}

//	(logical$)identical(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_identical(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if ((arg0_type != arg1_type) || (arg0_count != arg1_count))
	{
		result_SP = gStaticEidosValue_LogicalF;
		return result_SP;
	}
	
	result_SP = gStaticEidosValue_LogicalT;
	
	if (arg0_type == EidosValueType::kValueNULL)
		return result_SP;
	
	if (arg0_count == 1)
	{
		// Handle singleton comparison separately, to allow the use of the fast vector API below
		if (arg0_type == EidosValueType::kValueLogical)
		{
			if (arg0_value->LogicalAtIndex(0, nullptr) != arg1_value->LogicalAtIndex(0, nullptr))
				result_SP = gStaticEidosValue_LogicalF;
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			if (arg0_value->IntAtIndex(0, nullptr) != arg1_value->IntAtIndex(0, nullptr))
				result_SP = gStaticEidosValue_LogicalF;
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			if (arg0_value->FloatAtIndex(0, nullptr) != arg1_value->FloatAtIndex(0, nullptr))
				result_SP = gStaticEidosValue_LogicalF;
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			if (arg0_value->StringAtIndex(0, nullptr) != arg1_value->StringAtIndex(0, nullptr))
				result_SP = gStaticEidosValue_LogicalF;
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			if (arg0_value->ObjectElementAtIndex(0, nullptr) != arg1_value->ObjectElementAtIndex(0, nullptr))
				result_SP = gStaticEidosValue_LogicalF;
		}
	}
	else
	{
		// We have arg0_count != 1, so we can use the fast vector API; we want identical() to be very fast since it is a common bottleneck
		if (arg0_type == EidosValueType::kValueLogical)
		{
			const std::vector<eidos_logical_t> &logical_vec0 = *arg0_value->LogicalVector();
			const std::vector<eidos_logical_t> &logical_vec1 = *arg1_value->LogicalVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (logical_vec0[value_index] != logical_vec1[value_index])
				{
					result_SP = gStaticEidosValue_LogicalF;
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			const std::vector<int64_t> &int_vec0 = *arg0_value->IntVector();
			const std::vector<int64_t> &int_vec1 = *arg1_value->IntVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (int_vec0[value_index] != int_vec1[value_index])
				{
					result_SP = gStaticEidosValue_LogicalF;
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			const std::vector<double> &float_vec0 = *arg0_value->FloatVector();
			const std::vector<double> &float_vec1 = *arg1_value->FloatVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (float_vec0[value_index] != float_vec1[value_index])
				{
					result_SP = gStaticEidosValue_LogicalF;
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *arg0_value->StringVector();
			const std::vector<std::string> &string_vec1 = *arg1_value->StringVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (string_vec0[value_index] != string_vec1[value_index])
				{
					result_SP = gStaticEidosValue_LogicalF;
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			const std::vector<EidosObjectElement *> &objelement_vec0 = *arg0_value->ObjectElementVector();
			const std::vector<EidosObjectElement *> &objelement_vec1 = *arg1_value->ObjectElementVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (objelement_vec0[value_index] != objelement_vec1[value_index])
				{
					result_SP = gStaticEidosValue_LogicalF;
					break;
				}
		}
	}
	
	return result_SP;
}

//	(*)ifelse(logical test, * trueValues, * falseValues)
EidosValue_SP Eidos_ExecuteFunction_ifelse(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
	
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	EidosValue *arg2_value = p_arguments[2].get();
	EidosValueType arg2_type = arg2_value->Type();
	int arg2_count = arg2_value->Count();
	
	if (arg1_type != arg2_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): function ifelse() requires arguments 2 and 3 to be the same type (" << arg1_type << " and " << arg2_type << " supplied)." << eidos_terminate(nullptr);
	
	if ((arg1_count == arg0_count) && (arg2_count == arg0_count))
	{
		result_SP = arg1_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(value_index, *arg1_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(value_index, *arg2_value, nullptr);
		}
	}
	else if ((arg1_count == 1) && (arg2_count == 1))
	{
		result_SP = arg1_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(0, *arg1_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(0, *arg2_value, nullptr);
		}
	}
	else if ((arg1_count == arg0_count) && (arg2_count == 1))
	{
		result_SP = arg1_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(value_index, *arg1_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(0, *arg2_value, nullptr);
		}
	}
	else if ((arg1_count == 1) && (arg2_count == arg0_count))
	{
		result_SP = arg1_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(0, *arg1_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(value_index, *arg2_value, nullptr);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): function ifelse() requires arguments of equal length, or trueValues and falseValues most both be of length 1." << eidos_terminate(nullptr);
	}
	
	return result_SP;
}

//	(integer)match(* x, * table)
EidosValue_SP Eidos_ExecuteFunction_match(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValueType arg1_type = arg1_value->Type();
	int arg1_count = arg1_value->Count();
	
	if (arg0_type != arg1_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_match): function match() requires arguments x and table to be the same type." << eidos_terminate(nullptr);
	
	if (arg0_type == EidosValueType::kValueNULL)
	{
		result_SP = gStaticEidosValue_Integer_ZeroVec;
		return result_SP;
	}
	
	if ((arg0_count == 1) && (arg1_count == 1))
	{
		// Handle singleton matching separately, to allow the use of the fast vector API below
		if (arg0_type == EidosValueType::kValueLogical)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->LogicalAtIndex(0, nullptr) == arg1_value->LogicalAtIndex(0, nullptr) ? 0 : -1));
		else if (arg0_type == EidosValueType::kValueInt)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->IntAtIndex(0, nullptr) == arg1_value->IntAtIndex(0, nullptr) ? 0 : -1));
		else if (arg0_type == EidosValueType::kValueFloat)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->FloatAtIndex(0, nullptr) == arg1_value->FloatAtIndex(0, nullptr) ? 0 : -1));
		else if (arg0_type == EidosValueType::kValueString)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->StringAtIndex(0, nullptr) == arg1_value->StringAtIndex(0, nullptr) ? 0 : -1));
		else if (arg0_type == EidosValueType::kValueObject)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->ObjectElementAtIndex(0, nullptr) == arg1_value->ObjectElementAtIndex(0, nullptr) ? 0 : -1));
	}
	else if (arg0_count == 1)	// && (arg1_count != 1)
	{
		int table_index;
		
		if (arg0_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t value0 = arg0_value->LogicalAtIndex(0, nullptr);
			const std::vector<eidos_logical_t> &logical_vec1 = *arg1_value->LogicalVector();
			
			for (table_index = 0; table_index < arg1_count; ++table_index)
				if (value0 == logical_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			int64_t value0 = arg0_value->IntAtIndex(0, nullptr);
			const std::vector<int64_t> &int_vec1 = *arg1_value->IntVector();
			
			for (table_index = 0; table_index < arg1_count; ++table_index)
				if (value0 == int_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			double value0 = arg0_value->FloatAtIndex(0, nullptr);
			const std::vector<double> &float_vec1 = *arg1_value->FloatVector();
			
			for (table_index = 0; table_index < arg1_count; ++table_index)
				if (value0 == float_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			std::string value0 = arg0_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec1 = *arg1_value->StringVector();
			
			for (table_index = 0; table_index < arg1_count; ++table_index)
				if (value0 == string_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value0 = arg0_value->ObjectElementAtIndex(0, nullptr);
			const std::vector<EidosObjectElement *> &objelement_vec1 = *arg1_value->ObjectElementVector();
			
			for (table_index = 0; table_index < arg1_count; ++table_index)
				if (value0 == objelement_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		
		if (table_index == arg1_count)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1));
	}
	else if (arg1_count == 1)	// && (arg0_count != 1)
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(int_result);
		
		if (arg0_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t value1 = arg1_value->LogicalAtIndex(0, nullptr);
			const std::vector<eidos_logical_t> &logical_vec0 = *arg0_value->LogicalVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(logical_vec0[value_index] == value1 ? 0 : -1);
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			int64_t value1 = arg1_value->IntAtIndex(0, nullptr);
			const std::vector<int64_t> &int_vec0 = *arg0_value->IntVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(int_vec0[value_index] == value1 ? 0 : -1);
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			double value1 = arg1_value->FloatAtIndex(0, nullptr);
			const std::vector<double> &float_vec0 = *arg0_value->FloatVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(float_vec0[value_index] == value1 ? 0 : -1);
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			std::string value1 = arg1_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec0 = *arg0_value->StringVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(string_vec0[value_index] == value1 ? 0 : -1);
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value1 = arg1_value->ObjectElementAtIndex(0, nullptr);
			const std::vector<EidosObjectElement *> &objelement_vec0 = *arg0_value->ObjectElementVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(objelement_vec0[value_index] == value1 ? 0 : -1);
		}
	}
	else						// ((arg0_count != 1) && (arg1_count != 1))
	{
		// We can use the fast vector API; we want match() to be very fast since it is a common bottleneck
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(int_result);
		
		int table_index;
		
		if (arg0_type == EidosValueType::kValueLogical)
		{
			const std::vector<eidos_logical_t> &logical_vec0 = *arg0_value->LogicalVector();
			const std::vector<eidos_logical_t> &logical_vec1 = *arg1_value->LogicalVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				for (table_index = 0; table_index < arg1_count; ++table_index)
					if (logical_vec0[value_index] == logical_vec1[table_index])
						break;
				
				int_result->PushInt(table_index == arg1_count ? -1 : table_index);
			}
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			const std::vector<int64_t> &int_vec0 = *arg0_value->IntVector();
			const std::vector<int64_t> &int_vec1 = *arg1_value->IntVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				for (table_index = 0; table_index < arg1_count; ++table_index)
					if (int_vec0[value_index] == int_vec1[table_index])
						break;
				
				int_result->PushInt(table_index == arg1_count ? -1 : table_index);
			}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			const std::vector<double> &float_vec0 = *arg0_value->FloatVector();
			const std::vector<double> &float_vec1 = *arg1_value->FloatVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				for (table_index = 0; table_index < arg1_count; ++table_index)
					if (float_vec0[value_index] == float_vec1[table_index])
						break;
				
				int_result->PushInt(table_index == arg1_count ? -1 : table_index);
			}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *arg0_value->StringVector();
			const std::vector<std::string> &string_vec1 = *arg1_value->StringVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				for (table_index = 0; table_index < arg1_count; ++table_index)
					if (string_vec0[value_index] == string_vec1[table_index])
						break;
				
				int_result->PushInt(table_index == arg1_count ? -1 : table_index);
			}
		}
		else if (arg0_type == EidosValueType::kValueObject)
		{
			const std::vector<EidosObjectElement *> &objelement_vec0 = *arg0_value->ObjectElementVector();
			const std::vector<EidosObjectElement *> &objelement_vec1 = *arg1_value->ObjectElementVector();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				for (table_index = 0; table_index < arg1_count; ++table_index)
					if (objelement_vec0[value_index] == objelement_vec1[table_index])
						break;
				
				int_result->PushInt(table_index == arg1_count ? -1 : table_index);
			}
		}
	}
	
	return result_SP;
}

//	(integer)nchar(string x)
EidosValue_SP Eidos_ExecuteFunction_nchar(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->StringAtIndex(0, nullptr).size()));
	}
	else
	{
		const std::vector<std::string> &string_vec = *arg0_value->StringVector();
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			int_result->PushInt(string_vec[value_index].size());
	}
	
	return result_SP;
}

// Get indexes that would result in sorted ordering of a vector.  This rather nice code is adapted from http://stackoverflow.com/a/12399290/2752221
template <typename T>
vector<int64_t> EidosSortIndexes(const vector<T> &v, bool ascending = true)
{
	// initialize original index locations
	vector<int64_t> idx(v.size());
	std::iota(idx.begin(), idx.end(), 0);
	
	// sort indexes based on comparing values in v
	if (ascending)
		std::sort(idx.begin(), idx.end(), [&v](int64_t i1, int64_t i2) {return v[i1] < v[i2];});
	else
		std::sort(idx.begin(), idx.end(), [&v](int64_t i1, int64_t i2) {return v[i1] > v[i2];});
	
	return idx;
}

//	(integer)order(+ x, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_order(const EidosValue_SP *const p_arguments, int __attribute__((unused)) p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		// This handles all the zero-length cases by returning integer(0)
		result_SP = gStaticEidosValue_Integer_ZeroVec;
	}
	else if (arg0_count == 1)
	{
		// This handles all the singleton cases by returning 0
		result_SP = gStaticEidosValue_Integer0;
	}
	else
	{
		// Here we handle the vector cases, which can be done with direct access
		EidosValueType arg0_type = arg0_value->Type();
		bool ascending = p_arguments[1]->LogicalAtIndex(0, nullptr);
		vector<int64_t> order;
		
		if (arg0_type == EidosValueType::kValueLogical)
			order = EidosSortIndexes(*arg0_value->LogicalVector(), ascending);
		else if (arg0_type == EidosValueType::kValueInt)
			order = EidosSortIndexes(*arg0_value->IntVector(), ascending);
		else if (arg0_type == EidosValueType::kValueFloat)
			order = EidosSortIndexes(*arg0_value->FloatVector(), ascending);
		else if (arg0_type == EidosValueType::kValueString)
			order = EidosSortIndexes(*arg0_value->StringVector(), ascending);
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(order));
		result_SP = EidosValue_SP(int_result);
	}
	
	return result_SP;
}

//	(string$)paste(* x, [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_paste(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValueType arg0_type = arg0_value->Type();
	string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	string result_string;
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
	{
		if (value_index > 0)
			result_string.append(separator);
		
		if (arg0_type == EidosValueType::kValueObject)
		{
			std::ostringstream oss;
			
			oss << *arg0_value->ObjectElementAtIndex(value_index, nullptr);
			
			result_string.append(oss.str());
		}
		else
			result_string.append(arg0_value->StringAtIndex(value_index, nullptr));
	}
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
	
	return result_SP;
}

//	(void)print(* x)
EidosValue_SP Eidos_ExecuteFunction_print(const EidosValue_SP *const p_arguments, int __attribute__((unused)) p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	
	p_interpreter.ExecutionOutputStream() << *arg0_value << endl;
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(*)rev(* x)
EidosValue_SP Eidos_ExecuteFunction_rev(const EidosValue_SP *const p_arguments, int __attribute__((unused)) p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	result_SP = arg0_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int value_index = arg0_count - 1; value_index >= 0; --value_index)
		result->PushValueFromIndexOfEidosValue(value_index, *arg0_value, nullptr);
	
	return result_SP;
}

//	(integer$)size(* x)
EidosValue_SP Eidos_ExecuteFunction_size(const EidosValue_SP *const p_arguments, int __attribute__((unused)) p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->Count()));
	
	return result_SP;
}

//	(+)sort(+ x, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_sort(const EidosValue_SP *const p_arguments, int __attribute__((unused)) p_argument_count, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	result_SP = arg0_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
		result->PushValueFromIndexOfEidosValue(value_index, *arg0_value, nullptr);
	
	result->Sort(p_arguments[1]->LogicalAtIndex(0, nullptr));
	
	return result_SP;
}

//	(object)sortBy(object x, string$ property, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_sortBy(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)arg0_value)->Class()))->Reserve(arg0_count);
	result_SP = EidosValue_SP(object_result);
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
		object_result->PushObjectElement(arg0_value->ObjectElementAtIndex(value_index, nullptr));
	
	object_result->SortBy(p_arguments[1]->StringAtIndex(0, nullptr), p_arguments[2]->LogicalAtIndex(0, nullptr));
	
	return result_SP;
}

//	(void)str(* x)
EidosValue_SP Eidos_ExecuteFunction_str(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "(" << arg0_type << ") ";
	
	if (arg0_count <= 2)
		output_stream << *arg0_value << endl;
	else
	{
		EidosValue_SP first_value = arg0_value->GetValueAtIndex(0, nullptr);
		EidosValue_SP second_value = arg0_value->GetValueAtIndex(1, nullptr);
		
		output_stream << *first_value << gEidosStr_space_string << *second_value << " ... (" << arg0_count << " values)" << endl;
	}
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(string)strsplit(string$ x, [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_strsplit(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
	result_SP = EidosValue_SP(string_result);
	
	string joined_string = arg0_value->StringAtIndex(0, nullptr);
	string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	string::size_type start_idx = 0, sep_idx;
	
	if (separator.length() == 0)
	{
		// special-case a zero-length separator
		for (char &ch : joined_string)
			string_result->PushString(std::string(&ch, 1));
	}
	else
	{
		// non-zero-length separator
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
				start_idx = sep_idx + separator.size();
			}
		}
	}
	
	return result_SP;
}

//	(string)substr(string x, integer first, [Ni last = NULL])
EidosValue_SP Eidos_ExecuteFunction_substr(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue *arg_last = p_arguments[2].get();
	EidosValueType arg_last_type = arg_last->Type();
	
	if (arg0_count == 1)
	{
		const std::string &string_value = arg0_value->StringAtIndex(0, nullptr);
		int64_t len = (int64_t)string_value.size();
		EidosValue *arg_first = p_arguments[1].get();
		int arg_first_count = arg_first->Count();
		
		if (arg_first_count != arg0_count)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of first to be 1, or equal to the size of x." << eidos_terminate(nullptr);
		
		int64_t first0 = arg_first->IntAtIndex(0, nullptr);
		
		if (arg_last_type != EidosValueType::kValueNULL)
		{
			// last supplied
			int arg_last_count = arg_last->Count();
			
			if (arg_last_count != arg0_count)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of last to be 1, or equal to the size of x." << eidos_terminate(nullptr);
			
			int64_t last0 = arg_last->IntAtIndex(0, nullptr);
			
			int64_t clamped_first = (int)first0;
			int64_t clamped_last = (int)last0;
			
			if (clamped_first < 0) clamped_first = 0;
			if (clamped_last >= len) clamped_last = (int)len - 1;
			
			if ((clamped_first >= len) || (clamped_last < 0) || (clamped_first > clamped_last))
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_empty_string));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string_value.substr(clamped_first, clamped_last - clamped_first + 1)));
		}
		else
		{
			// last not supplied; take substrings to the end of each string
			int clamped_first = (int)first0;
			
			if (clamped_first < 0) clamped_first = 0;
			
			if (clamped_first >= len)						
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_empty_string));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string_value.substr(clamped_first, len)));
		}
	}
	else
	{
		const std::vector<std::string> &string_vec = *arg0_value->StringVector();
		EidosValue *arg_first = p_arguments[1].get();
		int arg_first_count = arg_first->Count();
		bool first_singleton = (arg_first_count == 1);
		
		if (!first_singleton && (arg_first_count != arg0_count))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of first to be 1, or equal to the size of x." << eidos_terminate(nullptr);
		
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(string_result);
		
		int64_t first0 = arg_first->IntAtIndex(0, nullptr);
		
		if (arg_last_type != EidosValueType::kValueNULL)
		{
			// last supplied
			int arg_last_count = arg_last->Count();
			bool last_singleton = (arg_last_count == 1);
			
			if (!last_singleton && (arg_last_count != arg0_count))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of last to be 1, or equal to the size of x." << eidos_terminate(nullptr);
			
			int64_t last0 = arg_last->IntAtIndex(0, nullptr);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				std::string str = string_vec[value_index];
				int64_t len = (int64_t)str.size();
				int64_t clamped_first = (first_singleton ? first0 : arg_first->IntAtIndex(value_index, nullptr));
				int64_t clamped_last = (last_singleton ? last0 : arg_last->IntAtIndex(value_index, nullptr));
				
				if (clamped_first < 0) clamped_first = 0;
				if (clamped_last >= len) clamped_last = (int)len - 1;
				
				if ((clamped_first >= len) || (clamped_last < 0) || (clamped_first > clamped_last))
					string_result->PushString(gEidosStr_empty_string);
				else
					string_result->PushString(str.substr(clamped_first, clamped_last - clamped_first + 1));
			}
		}
		else
		{
			// last not supplied; take substrings to the end of each string
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				std::string str = string_vec[value_index];
				int64_t len = (int64_t)str.size();
				int64_t clamped_first = (first_singleton ? first0 : arg_first->IntAtIndex(value_index, nullptr));
				
				if (clamped_first < 0) clamped_first = 0;
				
				if (clamped_first >= len)						
					string_result->PushString(gEidosStr_empty_string);
				else
					string_result->PushString(str.substr(clamped_first, len));
			}
		}
	}
	
	return result_SP;
}

//	(*)unique(* x)
EidosValue_SP Eidos_ExecuteFunction_unique(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	return UniqueEidosValue(p_arguments[0].get(), false);
}

//	(integer)which(logical x)
EidosValue_SP Eidos_ExecuteFunction_which(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
	EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < arg0_count; ++value_index)
		if (logical_vec[value_index])
			int_result->PushInt(value_index);
	
	return result_SP;
}

//	(integer)whichMax(+ x)
EidosValue_SP Eidos_ExecuteFunction_whichMax(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		int first_index = 0;
		
		if (arg0_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t max = arg0_value->LogicalAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
				const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					eidos_logical_t temp = logical_vec[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			int64_t max = arg0_value->IntAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
				const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					int64_t temp = int_vec[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			double max = arg0_value->FloatAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
				const std::vector<double> &float_vec = *arg0_value->FloatVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					double temp = float_vec[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			string max = arg0_value->StringAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_String_vector; we can use the fast API
				const std::vector<std::string> &string_vec = *arg0_value->StringVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					const string &temp = string_vec[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_index));
	}
	
	return result_SP;
}

//	(integer)whichMin(+ x)
EidosValue_SP Eidos_ExecuteFunction_whichMin(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		int first_index = 0;
		
		if (arg0_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t min = arg0_value->LogicalAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
				const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					eidos_logical_t temp = logical_vec[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueInt)
		{
			int64_t min = arg0_value->IntAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Int_vector; we can use the fast API
				const std::vector<int64_t> &int_vec = *arg0_value->IntVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					int64_t temp = int_vec[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueFloat)
		{
			double min = arg0_value->FloatAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_Float_vector; we can use the fast API
				const std::vector<double> &float_vec = *arg0_value->FloatVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					double temp = float_vec[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (arg0_type == EidosValueType::kValueString)
		{
			string min = arg0_value->StringAtIndex(0, nullptr);
			
			if (arg0_count > 1)
			{
				// We have arg0_count != 1, so the type of arg0_value must be EidosValue_String_vector; we can use the fast API
				const std::vector<std::string> &string_vec = *arg0_value->StringVector();
				
				for (int value_index = 1; value_index < arg0_count; ++value_index)
				{
					const string &temp = string_vec[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_index));
	}
	
	return result_SP;
}


// ************************************************************************************
//
//	value type testing/coercion functions
//
#pragma mark -
#pragma mark Value type testing/coercion functions
#pragma mark -


//	(float)asFloat(+ x)
EidosValue_SP Eidos_ExecuteFunction_asFloat(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(arg0_value->FloatAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			float_result->PushFloat(arg0_value->FloatAtIndex(value_index, nullptr));
	}
	
	return result_SP;
}

//	(integer)asInteger(+ x)
EidosValue_SP Eidos_ExecuteFunction_asInteger(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->IntAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			int_result->PushInt(arg0_value->IntAtIndex(value_index, nullptr));
	}
	
	return result_SP;
}

//	(logical)asLogical(+ x)
EidosValue_SP Eidos_ExecuteFunction_asLogical(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = (arg0_value->LogicalAtIndex(0, nullptr) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
		std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			logical_result_vec.emplace_back(arg0_value->LogicalAtIndex(value_index, nullptr));
	}
	
	return result_SP;
}

//	(string)asString(+ x)
EidosValue_SP Eidos_ExecuteFunction_asString(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	
	if (arg0_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(arg0_value->StringAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(arg0_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			string_result->PushString(arg0_value->StringAtIndex(value_index, nullptr));
	}
	
	return result_SP;
}

//	(string$)elementType(* x)
EidosValue_SP Eidos_ExecuteFunction_elementType(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(arg0_value->ElementType()));
	
	return result_SP;
}

//	(logical$)isFloat(* x)
EidosValue_SP Eidos_ExecuteFunction_isFloat(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	
	result_SP = (arg0_type == EidosValueType::kValueFloat) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isInteger(* x)
EidosValue_SP Eidos_ExecuteFunction_isInteger(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	
	result_SP = (arg0_type == EidosValueType::kValueInt) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isLogical(* x)
EidosValue_SP Eidos_ExecuteFunction_isLogical(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	
	result_SP = (arg0_type == EidosValueType::kValueLogical) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isNULL(* x)
EidosValue_SP Eidos_ExecuteFunction_isNULL(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	
	result_SP = (arg0_type == EidosValueType::kValueNULL) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isObject(* x)
EidosValue_SP Eidos_ExecuteFunction_isObject(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	
	result_SP = (arg0_type == EidosValueType::kValueObject) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isString(* x)
EidosValue_SP Eidos_ExecuteFunction_isString(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValueType arg0_type = arg0_value->Type();
	
	result_SP = (arg0_type == EidosValueType::kValueString) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(string$)type(* x)
EidosValue_SP Eidos_ExecuteFunction_type(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(StringForEidosValueType(arg0_value->Type())));
	
	return result_SP;
}


// ************************************************************************************
//
//	filesystem access functions
//
#pragma mark -
#pragma mark Filesystem access functions
#pragma mark -


//	(logical$)createDirectory(string$ path)
EidosValue_SP Eidos_ExecuteFunction_createDirectory(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	string base_path = arg0_value->StringAtIndex(0, nullptr);
	int base_path_length = (int)base_path.length();
	bool base_path_ends_in_slash = (base_path_length > 0) && (base_path[base_path_length-1] == '/');
	
	if (base_path_ends_in_slash)
		base_path.pop_back();		// remove the trailing slash, which just confuses stat()
	
	string path = EidosResolvedPath(base_path);
	
	errno = 0;
	
	struct stat file_info;
	bool path_exists = (stat(path.c_str(), &file_info) == 0);
	
	if (path_exists)
	{
		bool is_directory = !!(file_info.st_mode & S_IFDIR);
		
		if (is_directory)
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory at " << path << " because a directory at that path already exists." << endl;
			result_SP = gStaticEidosValue_LogicalT;
		}
		else
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory at " << path << " because a file at that path already exists." << endl;
			result_SP = gStaticEidosValue_LogicalF;
		}
	}
	else if (errno == ENOENT)
	{
		// The path does not exist, so let's try to create it
		errno = 0;
		
		if (mkdir(path.c_str(), 0777) == 0)
		{
			// success
			result_SP = gStaticEidosValue_LogicalT;
		}
		else
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory at " << path << " because of an unspecified filesystem error." << endl;
			result_SP = gStaticEidosValue_LogicalF;
		}
	}
	else
	{
		// The stat() call failed for an unknown reason
		p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory at " << path << " because of an unspecified filesystem error." << endl;
		result_SP = gStaticEidosValue_LogicalF;
	}
	
	return result_SP;
}

//	(string)filesAtPath(string$ path, [logical$ fullPaths = F])
EidosValue_SP Eidos_ExecuteFunction_filesAtPath(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	string base_path = arg0_value->StringAtIndex(0, nullptr);
	int base_path_length = (int)base_path.length();
	bool base_path_ends_in_slash = (base_path_length > 0) && (base_path[base_path_length-1] == '/');
	string path = EidosResolvedPath(base_path);
	bool fullPaths = p_arguments[1]->LogicalAtIndex(0, nullptr);
	
	// this code modified from GNU: http://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html#Simple-Directory-Lister
	// I'm not sure if it works on Windows... sigh...
	DIR *dp;
	struct dirent *ep;
	
	dp = opendir(path.c_str());
	
	if (dp != NULL)
	{
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		while ((ep = readdir(dp)))
		{
			string filename = ep->d_name;
			
			if (fullPaths)
				filename = base_path + (base_path_ends_in_slash ? "" : "/") + filename;
			
			string_result->PushString(filename);
		}
		
		(void)closedir(dp);
	}
	else
	{
		// not a fatal error, just a warning log
		p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_filesAtPath): function filesAtPath() could not open path " << path << "." << endl;
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}

//	(logical$)deleteFile(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_deleteFile(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	string base_path = arg0_value->StringAtIndex(0, nullptr);
	string file_path = EidosResolvedPath(base_path);
	
	result_SP = ((remove(file_path.c_str()) == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	return result_SP;
}

//	(string)readFile(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_readFile(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	string base_path = arg0_value->StringAtIndex(0, nullptr);
	string file_path = EidosResolvedPath(base_path);
	
	// read the contents in
	std::ifstream file_stream(file_path.c_str());
	
	if (!file_stream.is_open())
	{
		// not a fatal error, just a warning log
		p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_readFile): function readFile() could not read file at path " << file_path << "." << endl;
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		string line;
		
		while (getline(file_stream, line))
			string_result->PushString(line);
		
		if (file_stream.bad())
		{
			// not a fatal error, just a warning log
			p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_readFile): function readFile() encountered stream errors while reading file at path " << file_path << "." << endl;
			
			result_SP = gStaticEidosValueNULL;
		}
	}
	
	return result_SP;
}

//	(logical$)writeFile(string$ filePath, string contents, [logical$ append = F])
EidosValue_SP Eidos_ExecuteFunction_writeFile(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	string base_path = arg0_value->StringAtIndex(0, nullptr);
	string file_path = EidosResolvedPath(base_path);
	
	// the second argument is the file contents to write
	EidosValue *arg1_value = p_arguments[1].get();
	int arg1_count = arg1_value->Count();
	
	// the third argument is an optional append flag, F by default
	bool append = p_arguments[2]->LogicalAtIndex(0, nullptr);
	
	// write the contents out
	std::ofstream file_stream(file_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
	
	if (!file_stream.is_open())
	{
		// Not a fatal error, just a warning log
		p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeFile): function writeFile() could not write to file at path " << file_path << "." << endl;
		result_SP = gStaticEidosValue_LogicalF;
	}
	else
	{
		if (arg1_count == 1)
		{
			// BCH 27 January 2017: changed to add a newline after the last line, too, so appending new content to a file produces correct line breaks
			file_stream << arg1_value->StringAtIndex(0, nullptr) << endl;
		}
		else
		{
			const std::vector<std::string> &string_vec = *arg1_value->StringVector();
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
			{
				file_stream << string_vec[value_index];
				
				// Add newlines after all lines but the last
				// BCH 27 January 2017: changed to add a newline after the last line, too, so appending new content to a file produces correct line breaks
				//if (value_index + 1 < arg1_count)
					file_stream << endl;
			}
		}
		
		if (file_stream.bad())
		{
			// Not a fatal error, just a warning log
			p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeFile): function writeFile() encountered stream errors while writing to file at path " << file_path << "." << endl;
			result_SP = gStaticEidosValue_LogicalF;
		}
		else
		{
			result_SP = gStaticEidosValue_LogicalT;
		}
	}
	
	return result_SP;
}


// ************************************************************************************
//
//	miscellaneous functions
//
#pragma mark -
#pragma mark Miscellaneous functions
#pragma mark -


//	(*)apply(* x, string$ lambdaSource)
EidosValue_SP Eidos_ExecuteFunction_apply(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	int arg0_count = arg0_value->Count();
	EidosValue *arg1_value = p_arguments[1].get();
	EidosValue_String_singleton *arg1_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[1].get());
	EidosScript *script = (arg1_value_singleton ? arg1_value_singleton->CachedScript() : nullptr);
	vector<EidosValue_SP> results;
	
	// Errors in lambdas should be reported for the lambda script, not for the calling script,
	// if possible.  In the GUI this does not work well, however; there, errors should be
	// reported as occurring in the call to apply().  Here we save off the current
	// error context and set up the error context for reporting errors inside the lambda,
	// in case that is possible; see how exceptions are handled below.
	int error_start_save = gEidosCharacterStartOfError;
	int error_end_save = gEidosCharacterEndOfError;
	int error_start_save_UTF16 = gEidosCharacterStartOfErrorUTF16;
	int error_end_save_UTF16 = gEidosCharacterEndOfErrorUTF16;
	EidosScript *current_script_save = gEidosCurrentScript;
	bool executing_runtime_script_save = gEidosExecutingRuntimeScript;
	
	// We try to do tokenization and parsing once per script, by caching the script inside the EidosValue_String_singleton instance
	if (!script)
	{
		script = new EidosScript(arg1_value->StringAtIndex(0, nullptr));
		
		gEidosCharacterStartOfError = -1;
		gEidosCharacterEndOfError = -1;
		gEidosCharacterStartOfErrorUTF16 = -1;
		gEidosCharacterEndOfErrorUTF16 = -1;
		gEidosCurrentScript = script;
		gEidosExecutingRuntimeScript = true;
		
		try
		{
			script->Tokenize();
			script->ParseInterpreterBlockToAST();
		}
		catch (std::runtime_error err)
		{
			if (gEidosTerminateThrows)
			{
				gEidosCharacterStartOfError = error_start_save;
				gEidosCharacterEndOfError = error_end_save;
				gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
				gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
				gEidosCurrentScript = current_script_save;
				gEidosExecutingRuntimeScript = executing_runtime_script_save;
			}
			
			delete script;
			
			throw;
		}
		
		if (arg1_value_singleton)
			arg1_value_singleton->SetCachedScript(script);
			}
	
	// Execute inside try/catch so we can handle errors well
	gEidosCharacterStartOfError = -1;
	gEidosCharacterEndOfError = -1;
	gEidosCharacterStartOfErrorUTF16 = -1;
	gEidosCharacterEndOfErrorUTF16 = -1;
	gEidosCurrentScript = script;
	gEidosExecutingRuntimeScript = true;
	
	try
	{
		EidosSymbolTable &symbols = p_interpreter.SymbolTable();									// use our own symbol table
		EidosFunctionMap &function_map = p_interpreter.FunctionMap();								// use our own function map
		EidosInterpreter interpreter(*script, symbols, function_map, p_interpreter.Context());
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
		{
			EidosValue_SP apply_value = arg0_value->GetValueAtIndex(value_index, nullptr);
			
			// Set the iterator variable "applyValue" to the value
			symbols.SetValueForSymbolNoCopy(gEidosID_applyValue, std::move(apply_value));
			
			// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
			// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
			// bugs that would otherwise not manifest.
			results.emplace_back(interpreter.EvaluateInterpreterBlock(false));
		}
		
		// We do not want a leftover applyValue symbol in the symbol table, so we remove it now
		symbols.RemoveValueForSymbol(gEidosID_applyValue);
		
		// Assemble all the individual results together, just as c() does
		p_interpreter.ExecutionOutputStream() << interpreter.ExecutionOutput();
		result_SP = ConcatenateEidosValues(results.data(), (int)results.size(), true);
	}
	catch (std::runtime_error err)
	{
		// If exceptions throw, then we want to set up the error information to highlight the
		// apply() that failed, since we can't highlight the actual error.  (If exceptions
		// don't throw, this catch block will never be hit; exit() will already have been called
		// and the error will have been reported from the context of the lambda script string.)
		if (gEidosTerminateThrows)
		{
			gEidosCharacterStartOfError = error_start_save;
			gEidosCharacterEndOfError = error_end_save;
			gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
			gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
			gEidosCurrentScript = current_script_save;
			gEidosExecutingRuntimeScript = executing_runtime_script_save;
		}
		
		if (!arg1_value_singleton)
			delete script;
		
		throw;
	}
	
	// Restore the normal error context in the event that no exception occurring within the lambda
	gEidosCharacterStartOfError = error_start_save;
	gEidosCharacterEndOfError = error_end_save;
	gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
	gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
	gEidosCurrentScript = current_script_save;
	gEidosExecutingRuntimeScript = executing_runtime_script_save;
	
	if (!arg1_value_singleton)
		delete script;
	
	return result_SP;
}

//	(void)beep([Ns$ soundName = NULL])
EidosValue_SP Eidos_ExecuteFunction_beep(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	string name_string = ((arg0_value->Type() == EidosValueType::kValueString) ? arg0_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	
	string beep_error = EidosBeep(name_string);
	
	if (beep_error.length())
	{
		std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
		
		output_stream << beep_error << std::endl;
	}
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(void)citation(void)
EidosValue_SP Eidos_ExecuteFunction_citation(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "To cite Eidos in publications please use:" << std::endl << std::endl;
	output_stream << "Haller, B.C. (2016). Eidos: A Simple Scripting Language." << std::endl;
	output_stream << "URL: http://benhaller.com/slim/Eidos_Manual.pdf" << std::endl << std::endl;
	
	if (gEidosContextCitation.length())
	{
		output_stream << "---------------------------------------------------------" << endl << endl;
		output_stream << gEidosContextCitation << endl;
	}
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(float$)clock(void)
EidosValue_SP Eidos_ExecuteFunction_clock(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	clock_t cpu_time = clock();
	double cpu_time_d = static_cast<double>(cpu_time) / CLOCKS_PER_SEC;
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cpu_time_d));
	
	return result_SP;
}

//	(string$)date(void)
EidosValue_SP Eidos_ExecuteFunction_date(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	time_t rawtime;
	struct tm *timeinfo;
	char buffer[25];	// should never be more than 10, in fact, plus a null
	
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, 25, "%d-%m-%Y", timeinfo);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string(buffer)));
	
	return result_SP;
}

//	(void)defineConstant(string$ symbol, + x)
EidosValue_SP Eidos_ExecuteFunction_defineConstant(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	std::string symbol_name = p_arguments[0]->StringAtIndex(0, nullptr);
	const EidosValue_SP x_value_sp = p_arguments[1];
	EidosGlobalStringID symbol_id = EidosGlobalStringIDForString(symbol_name);
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	symbols.DefineConstantForSymbol(symbol_id, x_value_sp);
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(*)doCall(string$ function, ...)
EidosValue_SP Eidos_ExecuteFunction_doCall(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	std::string function_name = p_arguments[0]->StringAtIndex(0, nullptr);
	const EidosValue_SP *const arguments = p_arguments + 1;
	int argument_count = p_argument_count - 1;
	
	// Look up the signature for this function dynamically
	EidosFunctionMap &function_map = p_interpreter.FunctionMap();
	auto signature_iter = function_map.find(function_name);
	
	if (signature_iter == function_map.end())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): unrecognized function name " << function_name << "." << eidos_terminate(nullptr);
	
	const EidosFunctionSignature *function_signature = signature_iter->second;
	
	// Check the function's arguments
	function_signature->CheckArguments(arguments, argument_count);
	
	// BEWARE!  Since the function called here could be a function, like executeLambda() or apply(), that
	// causes re-entrancy into the Eidos engine, this call is rather dangerous.  See the comments on those
	// functions for further details.
	if (function_signature->internal_function_)
		result_SP = function_signature->internal_function_(arguments, argument_count, p_interpreter);
	else if (function_signature->delegate_function_)
		result_SP = function_signature->delegate_function_(function_signature->delegate_object_, function_name, arguments, argument_count, p_interpreter);
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): unbound function " << function_name << "." << eidos_terminate(nullptr);
	
	// Check the return value against the signature
	function_signature->CheckReturn(*result_SP);
	
	return result_SP;
}

//	(*)executeLambda(string$ lambdaSource, [logical$ timed = F])
EidosValue_SP Eidos_ExecuteFunction_executeLambda(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	EidosValue_String_singleton *arg0_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[0].get());
	EidosScript *script = (arg0_value_singleton ? arg0_value_singleton->CachedScript() : nullptr);
	
	// Errors in lambdas should be reported for the lambda script, not for the calling script,
	// if possible.  In the GUI this does not work well, however; there, errors should be
	// reported as occurring in the call to executeLambda().  Here we save off the current
	// error context and set up the error context for reporting errors inside the lambda,
	// in case that is possible; see how exceptions are handled below.
	int error_start_save = gEidosCharacterStartOfError;
	int error_end_save = gEidosCharacterEndOfError;
	int error_start_save_UTF16 = gEidosCharacterStartOfErrorUTF16;
	int error_end_save_UTF16 = gEidosCharacterEndOfErrorUTF16;
	EidosScript *current_script_save = gEidosCurrentScript;
	bool executing_runtime_script_save = gEidosExecutingRuntimeScript;
	
	// We try to do tokenization and parsing once per script, by caching the script inside the EidosValue_String_singleton instance
	if (!script)
	{
		script = new EidosScript(arg0_value->StringAtIndex(0, nullptr));
		
		gEidosCharacterStartOfError = -1;
		gEidosCharacterEndOfError = -1;
		gEidosCharacterStartOfErrorUTF16 = -1;
		gEidosCharacterEndOfErrorUTF16 = -1;
		gEidosCurrentScript = script;
		gEidosExecutingRuntimeScript = true;
		
		try
		{
			script->Tokenize();
			script->ParseInterpreterBlockToAST();
		}
		catch (std::runtime_error err)
		{
			if (gEidosTerminateThrows)
			{
				gEidosCharacterStartOfError = error_start_save;
				gEidosCharacterEndOfError = error_end_save;
				gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
				gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
				gEidosCurrentScript = current_script_save;
				gEidosExecutingRuntimeScript = executing_runtime_script_save;
			}
			
			delete script;
			
			throw;
		}
		
		if (arg0_value_singleton)
			arg0_value_singleton->SetCachedScript(script);
	}
	
	// Execute inside try/catch so we can handle errors well
	bool timed = p_arguments[1]->LogicalAtIndex(0, nullptr);
	clock_t begin = 0, end = 0;
	
	gEidosCharacterStartOfError = -1;
	gEidosCharacterEndOfError = -1;
	gEidosCharacterStartOfErrorUTF16 = -1;
	gEidosCharacterEndOfErrorUTF16 = -1;
	gEidosCurrentScript = script;
	gEidosExecutingRuntimeScript = true;
	
	try
	{
		EidosSymbolTable &symbols = p_interpreter.SymbolTable();									// use our own symbol table
		EidosFunctionMap &function_map = p_interpreter.FunctionMap();								// use our own function map
		EidosInterpreter interpreter(*script, symbols, function_map, p_interpreter.Context());
		
		if (timed)
			begin = clock();
		
		// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
		// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
		// bugs that would otherwise not manifest.
		result_SP = interpreter.EvaluateInterpreterBlock(false);
		
		if (timed)
			end = clock();
		
		p_interpreter.ExecutionOutputStream() << interpreter.ExecutionOutput();
	}
	catch (std::runtime_error err)
	{
		// If exceptions throw, then we want to set up the error information to highlight the
		// executeLambda() that failed, since we can't highlight the actual error.  (If exceptions
		// don't throw, this catch block will never be hit; exit() will already have been called
		// and the error will have been reported from the context of the lambda script string.)
		if (gEidosTerminateThrows)
		{
			gEidosCharacterStartOfError = error_start_save;
			gEidosCharacterEndOfError = error_end_save;
			gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
			gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
			gEidosCurrentScript = current_script_save;
			gEidosExecutingRuntimeScript = executing_runtime_script_save;
		}
		
		if (!arg0_value_singleton)
			delete script;
		
		throw;
	}
	
	// Restore the normal error context in the event that no exception occurring within the lambda
	gEidosCharacterStartOfError = error_start_save;
	gEidosCharacterEndOfError = error_end_save;
	gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
	gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
	gEidosCurrentScript = current_script_save;
	gEidosExecutingRuntimeScript = executing_runtime_script_save;
	
	if (timed)
	{
		double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
		
		p_interpreter.ExecutionOutputStream() << "// ********** executeLambda() elapsed time: " << time_spent << std::endl;
	}
	
	if (!arg0_value_singleton)
		delete script;
	
	return result_SP;
}

//	(logical$)exists(string$ symbol)
EidosValue_SP Eidos_ExecuteFunction_exists(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	std::string symbol_name = p_arguments[0]->StringAtIndex(0, nullptr);
	EidosGlobalStringID symbol_id = EidosGlobalStringIDForString(symbol_name);
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	bool exists = symbols.ContainsSymbol(symbol_id);
	
	result_SP = (exists ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	return result_SP;
}

//	(void)function([Ns$ functionName = NULL])
EidosValue_SP Eidos_ExecuteFunction_function(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	bool function_name_specified = (arg0_value->Type() == EidosValueType::kValueString);
	string match_string = (function_name_specified ? arg0_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	bool signature_found = false;
	
	// function_map_ is already alphebetized since maps keep sorted order
	EidosFunctionMap &function_map = p_interpreter.FunctionMap();
	
	for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
	{
		const EidosFunctionSignature *iter_signature = functionPairIter->second;
		
		if (function_name_specified && (iter_signature->call_name_.compare(match_string) != 0))
			continue;
		
		if (!function_name_specified && (iter_signature->call_name_.substr(0, 1).compare("_") == 0))
			continue;	// skip internal functions that start with an underscore, unless specifically requested
		
		output_stream << *iter_signature << endl;
		signature_found = true;
	}
	
	if (function_name_specified && !signature_found)
		output_stream << "No function signature found for \"" << match_string << "\"." << endl;
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(void)ls(void)
EidosValue_SP Eidos_ExecuteFunction_ls(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	p_interpreter.ExecutionOutputStream() << p_interpreter.SymbolTable();
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(void)license(void)
EidosValue_SP Eidos_ExecuteFunction_license(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	
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
	output_stream << "<http://www.gnu.org/licenses/>." << endl << endl;
	
	if (gEidosContextLicense.length())
	{
		output_stream << "---------------------------------------------------------" << endl << endl;
		output_stream << gEidosContextLicense << endl;
	}
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(void)rm([Ns variableNames = NULL], [logical$ removeConstants = F])
EidosValue_SP Eidos_ExecuteFunction_rm(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	bool removeConstants = p_arguments[1]->LogicalAtIndex(0, nullptr);
	vector<string> symbols_to_remove;
	
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	if (arg0_value->Type() == EidosValueType::kValueNULL)
		symbols_to_remove = symbols.ReadWriteSymbols();
	else
	{
		int arg0_count = arg0_value->Count();
		
		for (int value_index = 0; value_index < arg0_count; ++value_index)
			symbols_to_remove.emplace_back(arg0_value->StringAtIndex(value_index, nullptr));
	}
	
	if (removeConstants)
		for (string &symbol : symbols_to_remove)
			symbols.RemoveConstantForSymbol(EidosGlobalStringIDForString(symbol));
	else
		for (string &symbol : symbols_to_remove)
			symbols.RemoveValueForSymbol(EidosGlobalStringIDForString(symbol));
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(void)setSeed(integer$ seed)
EidosValue_SP Eidos_ExecuteFunction_setSeed(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	
	EidosInitializeRNGFromSeed(arg0_value->IntAtIndex(0, nullptr));
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}

//	(integer$)getSeed(void)
EidosValue_SP Eidos_ExecuteFunction_getSeed(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_rng_last_seed));
	
	return result_SP;
}

//	(void)stop([Ns$ message = NULL])
EidosValue_SP Eidos_ExecuteFunction_stop(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	
	if (arg0_value->Type() != EidosValueType::kValueNULL)
		p_interpreter.ExecutionOutputStream() << p_arguments[0]->StringAtIndex(0, nullptr) << endl;
	
	EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_stop): stop() called." << eidos_terminate(nullptr);
	
	return result_SP;
}

//	(string$)time(void)
EidosValue_SP Eidos_ExecuteFunction_time(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	time_t rawtime;
	struct tm *timeinfo;
	char buffer[20];		// should never be more than 8, in fact, plus a null
	
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, 20, "%H:%M:%S", timeinfo);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string(buffer)));
	
	return result_SP;
}

//	(void)version(void)
EidosValue_SP Eidos_ExecuteFunction_version(__attribute__((unused)) const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "Eidos version 1.2" << endl;	// EIDOS VERSION
	
	if (gEidosContextVersion.length())
		output_stream << gEidosContextVersion << endl;
	
	result_SP = gStaticEidosValueNULLInvisible;
	
	return result_SP;
}


// ************************************************************************************
//
//	object instantiation
//
#pragma mark -
#pragma mark Object instantiation
#pragma mark -


//	(object<_TestElement>$)_Test(integer$ yolk)
EidosValue_SP Eidos_ExecuteFunction__Test(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg0_value = p_arguments[0].get();
	Eidos_TestElement *testElement = new Eidos_TestElement(arg0_value->IntAtIndex(0, nullptr));
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(testElement, gEidos_TestElementClass));
	testElement->Release();
	
	return result_SP;
}




































































