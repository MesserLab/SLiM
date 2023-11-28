//
//  eidos_functions.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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
#include "eidos_interpreter.h"

#include <utility>
#include <string>
#include <algorithm>
#include <vector>


// Source code for built-in functions that are implemented in Eidos.  These strings are globals mostly so the
// formatting of the code looks nice in Xcode; they are used only by EidosInterpreter::BuiltInFunctions().

// - (void)source(string$ filePath, [logical$ chdir = F])
const char *gEidosSourceCode_source =
R"V0G0N({
	warn = suppressWarnings(T);
	lines = readFile(filePath);
	suppressWarnings(warn);
	if (isNULL(lines))
		stop("source(): file not found at path '" + filePath + "'");
	
	_changedwd = (chdir & strcontains(filePath, "/"));
	if (_changedwd) {
		components = strsplit(filePath, "/");
		pathComponents = components[seqLen(length(components) - 1)];
		path = paste(pathComponents, sep="/");
		_oldwd = setwd(path);
		_changedwd = T;
	}
	
	_executeLambda_OUTER(paste(lines, sep='\n'));
	
	if (_changedwd)
		setwd(_oldwd);
})V0G0N";


//
//	Construct our built-in function map
//

// We allocate all of our function signatures once and keep them forever, for faster EidosInterpreter startup
const std::vector<EidosFunctionSignature_CSP> &EidosInterpreter::BuiltInFunctions(void)
{
	static std::vector<EidosFunctionSignature_CSP> *signatures = nullptr;
	
	if (!signatures)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosInterpreter::BuiltInFunctions(): not warmed up");
		
		signatures = new std::vector<EidosFunctionSignature_CSP>;
		
		// ************************************************************************************
		//
		//	math functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("abs",				Eidos_ExecuteFunction_abs,			kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("acos",				Eidos_ExecuteFunction_acos,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asin",				Eidos_ExecuteFunction_asin,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan",				Eidos_ExecuteFunction_atan,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan2",				Eidos_ExecuteFunction_atan2,		kEidosValueMaskFloat))->AddNumeric("x")->AddNumeric("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ceil",				Eidos_ExecuteFunction_ceil,			kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cos",				Eidos_ExecuteFunction_cos,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cumProduct",		Eidos_ExecuteFunction_cumProduct,	kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cumSum",			Eidos_ExecuteFunction_cumSum,		kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("exp",				Eidos_ExecuteFunction_exp,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("floor",				Eidos_ExecuteFunction_floor,		kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("integerDiv",		Eidos_ExecuteFunction_integerDiv,	kEidosValueMaskInt))->AddInt("x")->AddInt("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("integerMod",		Eidos_ExecuteFunction_integerMod,	kEidosValueMaskInt))->AddInt("x")->AddInt("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFinite",			Eidos_ExecuteFunction_isFinite,		kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInfinite",		Eidos_ExecuteFunction_isInfinite,	kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNAN",				Eidos_ExecuteFunction_isNAN,		kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log",				Eidos_ExecuteFunction_log,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log10",				Eidos_ExecuteFunction_log10,		kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log2",				Eidos_ExecuteFunction_log2,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("product",			Eidos_ExecuteFunction_product,		kEidosValueMaskNumeric | kEidosValueMaskSingleton))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("round",				Eidos_ExecuteFunction_round,		kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setUnion",			Eidos_ExecuteFunction_setUnion,		kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setIntersection",	Eidos_ExecuteFunction_setIntersection,	kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setDifference",		Eidos_ExecuteFunction_setDifference,	kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setSymmetricDifference",	Eidos_ExecuteFunction_setSymmetricDifference,	kEidosValueMaskAny))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sin",				Eidos_ExecuteFunction_sin,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sqrt",				Eidos_ExecuteFunction_sqrt,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sum",				Eidos_ExecuteFunction_sum,			kEidosValueMaskNumeric | kEidosValueMaskSingleton))->AddLogicalEquiv("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sumExact",			Eidos_ExecuteFunction_sumExact,		kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("tan",				Eidos_ExecuteFunction_tan,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("trunc",				Eidos_ExecuteFunction_trunc,		kEidosValueMaskFloat))->AddFloat("x"));
		
		
		// ************************************************************************************
		//
		//	statistics functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cor",				Eidos_ExecuteFunction_cor,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x")->AddNumeric("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cov",				Eidos_ExecuteFunction_cov,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x")->AddNumeric("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("max",				Eidos_ExecuteFunction_max,			kEidosValueMaskAnyBase | kEidosValueMaskSingleton))->AddAnyBase("x")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mean",				Eidos_ExecuteFunction_mean,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddLogicalEquiv("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("min",				Eidos_ExecuteFunction_min,			kEidosValueMaskAnyBase | kEidosValueMaskSingleton))->AddAnyBase("x")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pmax",				Eidos_ExecuteFunction_pmax,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddAnyBase("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pmin",				Eidos_ExecuteFunction_pmin,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddAnyBase("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("quantile",			Eidos_ExecuteFunction_quantile,		kEidosValueMaskFloat))->AddNumeric("x")->AddFloat_ON("probs", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_range,		Eidos_ExecuteFunction_range,		kEidosValueMaskNumeric))->AddNumeric("x")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sd",				Eidos_ExecuteFunction_sd,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ttest",				Eidos_ExecuteFunction_ttest,		kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddFloat("x")->AddFloat_ON("y", gStaticEidosValueNULL)->AddFloat_OSN("mu", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("var",				Eidos_ExecuteFunction_var,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		
		
		// ************************************************************************************
		//
		//	distribution draw / density functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("findInterval",		Eidos_ExecuteFunction_findInterval,	kEidosValueMaskInt))->AddNumeric("x")->AddNumeric("vec")->AddLogical_OS("rightmostClosed", gStaticEidosValue_LogicalF)->AddLogical_OS("allInside", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dmvnorm",			Eidos_ExecuteFunction_dmvnorm,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric("mu")->AddNumeric("sigma"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dbeta",				Eidos_ExecuteFunction_dbeta,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric("alpha")->AddNumeric("beta"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dexp",				Eidos_ExecuteFunction_dexp,			kEidosValueMaskFloat))->AddFloat("x")->AddNumeric_O("mu", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dgamma",			Eidos_ExecuteFunction_dgamma,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric("mean")->AddNumeric("shape"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dnorm",				Eidos_ExecuteFunction_dnorm,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric_O("mean", gStaticEidosValue_Integer0)->AddNumeric_O("sd", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pnorm",				Eidos_ExecuteFunction_pnorm,		kEidosValueMaskFloat))->AddFloat("q")->AddNumeric_O("mean", gStaticEidosValue_Integer0)->AddNumeric_O("sd", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("qnorm",				Eidos_ExecuteFunction_qnorm,		kEidosValueMaskFloat))->AddFloat("p")->AddNumeric_O("mean", gStaticEidosValue_Integer0)->AddNumeric_O("sd", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbeta",				Eidos_ExecuteFunction_rbeta,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("alpha")->AddNumeric("beta"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbinom",			Eidos_ExecuteFunction_rbinom,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddInt("size")->AddFloat("prob"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rcauchy",			Eidos_ExecuteFunction_rcauchy,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("location", gStaticEidosValue_Integer0)->AddNumeric_O("scale", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rdunif",			Eidos_ExecuteFunction_rdunif,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddInt_O("min", gStaticEidosValue_Integer0)->AddInt_O("max", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rexp",				Eidos_ExecuteFunction_rexp,			kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("mu", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rf",				Eidos_ExecuteFunction_rf,			kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("d1")->AddNumeric("d2"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgamma",			Eidos_ExecuteFunction_rgamma,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("mean")->AddNumeric("shape"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgeom",				Eidos_ExecuteFunction_rgeom,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddFloat("p"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rlnorm",			Eidos_ExecuteFunction_rlnorm,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("meanlog", gStaticEidosValue_Integer0)->AddNumeric_O("sdlog", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rmvnorm",			Eidos_ExecuteFunction_rmvnorm,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("mu")->AddNumeric("sigma"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rnbinom",			Eidos_ExecuteFunction_rnbinom,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddNumeric("size")->AddFloat("prob"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rnorm",				Eidos_ExecuteFunction_rnorm,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("mean", gStaticEidosValue_Integer0)->AddNumeric_O("sd", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rpois",				Eidos_ExecuteFunction_rpois,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddNumeric("lambda"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("runif",				Eidos_ExecuteFunction_runif,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("min", gStaticEidosValue_Integer0)->AddNumeric_O("max", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rweibull",			Eidos_ExecuteFunction_rweibull,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("lambda")->AddNumeric("k"));
		
		
		// ************************************************************************************
		//
		//	vector construction functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_c,			Eidos_ExecuteFunction_c,			kEidosValueMaskAny))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_float,		Eidos_ExecuteFunction_float,		kEidosValueMaskFloat))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_integer,	Eidos_ExecuteFunction_integer,		kEidosValueMaskInt))->AddInt_S("length")->AddInt_OS("fill1", gStaticEidosValue_Integer0)->AddInt_OS("fill2", gStaticEidosValue_Integer1)->AddInt_ON("fill2Indices", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_logical,	Eidos_ExecuteFunction_logical,		kEidosValueMaskLogical))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_object,	Eidos_ExecuteFunction_object,		kEidosValueMaskObject, gEidosObject_Class)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rep",				Eidos_ExecuteFunction_rep,			kEidosValueMaskAny))->AddAny("x")->AddInt_S("count"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("repEach",			Eidos_ExecuteFunction_repEach,		kEidosValueMaskAny))->AddAny("x")->AddInt("count"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sample",			Eidos_ExecuteFunction_sample,		kEidosValueMaskAny))->AddAny("x")->AddInt_S("size")->AddLogical_OS("replace", gStaticEidosValue_LogicalF)->AddNumeric_ON(gEidosStr_weights, gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("seq",				Eidos_ExecuteFunction_seq,			kEidosValueMaskNumeric))->AddNumeric_S("from")->AddNumeric_S("to")->AddNumeric_OSN("by", gStaticEidosValueNULL)->AddInt_OSN("length", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("seqAlong",			Eidos_ExecuteFunction_seqAlong,		kEidosValueMaskInt))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("seqLen",			Eidos_ExecuteFunction_seqLen,		kEidosValueMaskInt))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_string,	Eidos_ExecuteFunction_string,		kEidosValueMaskString))->AddInt_S("length"));
		
		
		// ************************************************************************************
		//
		//	value inspection/manipulation functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("all",				Eidos_ExecuteFunction_all,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical("x")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("any",				Eidos_ExecuteFunction_any,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical("x")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cat",				Eidos_ExecuteFunction_cat,			kEidosValueMaskVOID))->AddAny("x")->AddString_OS("sep", gStaticEidosValue_StringSpace)->AddLogical_OS("error", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("catn",				Eidos_ExecuteFunction_catn,			kEidosValueMaskVOID))->AddAny_O("x", gStaticEidosValue_StringEmpty)->AddString_OS("sep", gStaticEidosValue_StringSpace)->AddLogical_OS("error", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("format",			Eidos_ExecuteFunction_format,		kEidosValueMaskString))->AddString_S("format")->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("identical",			Eidos_ExecuteFunction_identical,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ifelse",			Eidos_ExecuteFunction_ifelse,		kEidosValueMaskAny))->AddLogical("test")->AddAny("trueValues")->AddAny("falseValues"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("match",				Eidos_ExecuteFunction_match,		kEidosValueMaskInt))->AddAny("x")->AddAny("table"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("order",				Eidos_ExecuteFunction_order,		kEidosValueMaskInt))->AddAnyBase("x")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("paste",				Eidos_ExecuteFunction_paste,		kEidosValueMaskString | kEidosValueMaskSingleton))->AddEllipsis()->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("paste0",			Eidos_ExecuteFunction_paste0,		kEidosValueMaskString | kEidosValueMaskSingleton))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("print",				Eidos_ExecuteFunction_print,		kEidosValueMaskVOID))->AddAny("x")->AddLogical_OS("error", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rank",				Eidos_ExecuteFunction_rank,			kEidosValueMaskNumeric))->AddNumeric("x")->AddString_OS("tiesMethod", gStaticEidosValue_String_average));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rev",				Eidos_ExecuteFunction_rev,			kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_size,		Eidos_ExecuteFunction_size_length,	kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_length,	Eidos_ExecuteFunction_size_length,	kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sort",				Eidos_ExecuteFunction_sort,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sortBy",			Eidos_ExecuteFunction_sortBy,		kEidosValueMaskObject))->AddObject("x", nullptr)->AddString_S("property")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_str,		Eidos_ExecuteFunction_str,			kEidosValueMaskVOID))->AddAny("x")->AddLogical_OS("error", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("tabulate",			Eidos_ExecuteFunction_tabulate,		kEidosValueMaskInt))->AddInt("bin")->AddInt_OSN("maxbin", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("unique",			Eidos_ExecuteFunction_unique,		kEidosValueMaskAny))->AddAny("x")->AddLogical_OS("preserveOrder", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("which",				Eidos_ExecuteFunction_which,		kEidosValueMaskInt))->AddLogical("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMax",			Eidos_ExecuteFunction_whichMax,		kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMin",			Eidos_ExecuteFunction_whichMin,		kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAnyBase("x"));
		
		
		// ************************************************************************************
		//
		//	value type testing/coercion functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asFloat",			Eidos_ExecuteFunction_asFloat,		kEidosValueMaskFloat))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asInteger",			Eidos_ExecuteFunction_asInteger,	kEidosValueMaskInt))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asLogical",			Eidos_ExecuteFunction_asLogical,	kEidosValueMaskLogical))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asString",			Eidos_ExecuteFunction_asString,		kEidosValueMaskString))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("elementType",		Eidos_ExecuteFunction_elementType,	kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFloat",			Eidos_ExecuteFunction_isFloat,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInteger",			Eidos_ExecuteFunction_isInteger,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isLogical",			Eidos_ExecuteFunction_isLogical,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNULL",			Eidos_ExecuteFunction_isNULL,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isObject",			Eidos_ExecuteFunction_isObject,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isString",			Eidos_ExecuteFunction_isString,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("type",				Eidos_ExecuteFunction_type,			kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x"));
		
		
		// ************************************************************************************
		//
		//	string manipulation functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("grep",				Eidos_ExecuteFunction_grep,			kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskString))->AddString_S("pattern")->AddString("x")->AddLogical_OS("ignoreCase", gStaticEidosValue_LogicalF)->AddString_OS("grammar", gStaticEidosValue_String_ECMAScript)->AddString_OS("value", gStaticEidosValue_String_indices)->AddLogical_OS("fixed", gStaticEidosValue_LogicalF)->AddLogical_OS("invert", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nchar",				Eidos_ExecuteFunction_nchar,		kEidosValueMaskInt))->AddString("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strcontains",		Eidos_ExecuteFunction_strcontains,	kEidosValueMaskLogical))->AddString("x")->AddString_S("s")->AddInt_OS("pos", gStaticEidosValue_Integer0));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strfind",			Eidos_ExecuteFunction_strfind,		kEidosValueMaskInt))->AddString("x")->AddString_S("s")->AddInt_OS("pos", gStaticEidosValue_Integer0));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strprefix",			Eidos_ExecuteFunction_strprefix,	kEidosValueMaskLogical))->AddString("x")->AddString_S("s"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strsplit",			Eidos_ExecuteFunction_strsplit,		kEidosValueMaskString))->AddString_S("x")->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strsuffix",			Eidos_ExecuteFunction_strsuffix,	kEidosValueMaskLogical))->AddString("x")->AddString_S("s"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("substr",			Eidos_ExecuteFunction_substr,		kEidosValueMaskString))->AddString("x")->AddInt("first")->AddInt_ON("last", gStaticEidosValueNULL));
		
		
		// ************************************************************************************
		//
		//	matrix and array functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("array",				Eidos_ExecuteFunction_array,		kEidosValueMaskAny))->AddAny("data")->AddInt("dim"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cbind",				Eidos_ExecuteFunction_cbind,		kEidosValueMaskAny))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dim",				Eidos_ExecuteFunction_dim,			kEidosValueMaskInt))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("drop",				Eidos_ExecuteFunction_drop,			kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("matrix",			Eidos_ExecuteFunction_matrix,		kEidosValueMaskAny))->AddAny("data")->AddInt_OSN("nrow", gStaticEidosValueNULL)->AddInt_OSN("ncol", gStaticEidosValueNULL)->AddLogical_OS("byrow", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("matrixMult",		Eidos_ExecuteFunction_matrixMult,	kEidosValueMaskNumeric))->AddNumeric("x")->AddNumeric("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ncol",				Eidos_ExecuteFunction_ncol,			kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nrow",				Eidos_ExecuteFunction_nrow,			kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbind",				Eidos_ExecuteFunction_rbind,		kEidosValueMaskAny))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("t",					Eidos_ExecuteFunction_t,			kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("upperTri",			Eidos_ExecuteFunction_upperTri,		kEidosValueMaskLogical))->AddAny("x")->AddLogical_OS("diag", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("lowerTri",			Eidos_ExecuteFunction_lowerTri,		kEidosValueMaskLogical))->AddAny("x")->AddLogical_OS("diag", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("diag",				Eidos_ExecuteFunction_diag,			kEidosValueMaskAny))->AddAny_O("x", gStaticEidosValue_Integer1)->AddInt_OSN("nrow", gStaticEidosValueNULL)->AddInt_OSN("ncol", gStaticEidosValueNULL));

		
		// ************************************************************************************
		//
		//	color manipulation functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cmColors",			Eidos_ExecuteFunction_cmColors,		kEidosValueMaskString))->AddInt_S(gEidosStr_n)->MarkDeprecated());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("colors",			Eidos_ExecuteFunction_colors,		kEidosValueMaskString))->AddNumeric(gEidosStr_x)->AddString_S("name"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("heatColors",		Eidos_ExecuteFunction_heatColors,		kEidosValueMaskString))->AddInt_S(gEidosStr_n)->MarkDeprecated());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rainbow",			Eidos_ExecuteFunction_rainbow,		kEidosValueMaskString))->AddInt_S(gEidosStr_n)->AddFloat_OS(gEidosStr_s, gStaticEidosValue_Float1)->AddFloat_OS("v", gStaticEidosValue_Float1)->AddFloat_OS(gEidosStr_start, gStaticEidosValue_Float0)->AddFloat_OSN(gEidosStr_end, gStaticEidosValueNULL)->AddLogical_OS("ccw", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("terrainColors",		Eidos_ExecuteFunction_terrainColors,	kEidosValueMaskString))->AddInt_S(gEidosStr_n)->MarkDeprecated());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("hsv2rgb",			Eidos_ExecuteFunction_hsv2rgb,		kEidosValueMaskFloat))->AddFloat("hsv"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgb2hsv",			Eidos_ExecuteFunction_rgb2hsv,		kEidosValueMaskFloat))->AddFloat("rgb"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgb2color",			Eidos_ExecuteFunction_rgb2color,	kEidosValueMaskString))->AddFloat("rgb"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("color2rgb",			Eidos_ExecuteFunction_color2rgb,	kEidosValueMaskFloat))->AddString(gEidosStr_color));
		
		
		// ************************************************************************************
		//
		//	miscellaneous functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("assert",				Eidos_ExecuteFunction_assert,			kEidosValueMaskVOID))->AddLogical("assertions")->AddString_OSN("message", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_apply,		Eidos_ExecuteFunction_apply,		kEidosValueMaskAny))->AddAny("x")->AddInt("margin")->AddString_S("lambdaSource"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_sapply,	Eidos_ExecuteFunction_sapply,		kEidosValueMaskAny))->AddAny("x")->AddString_S("lambdaSource")->AddString_OS("simplify", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("vector"))));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("beep",				Eidos_ExecuteFunction_beep,			kEidosValueMaskVOID))->AddString_OSN("soundName", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("citation",			Eidos_ExecuteFunction_citation,		kEidosValueMaskVOID)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("clock",				Eidos_ExecuteFunction_clock,		kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddString_OS("type", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("cpu"))));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("date",				Eidos_ExecuteFunction_date,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("debugIndent",		Eidos_ExecuteFunction_debugIndent,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("defineConstant",	Eidos_ExecuteFunction_defineConstant,	kEidosValueMaskVOID))->AddString_S("symbol")->AddAny("value"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("defineGlobal",		Eidos_ExecuteFunction_defineGlobal,	kEidosValueMaskVOID))->AddString_S("symbol")->AddAny("value"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_doCall,	Eidos_ExecuteFunction_doCall,		kEidosValueMaskAny | kEidosValueMaskVOID))->AddString_S("functionName")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_executeLambda,	Eidos_ExecuteFunction_executeLambda,	kEidosValueMaskAny | kEidosValueMaskVOID))->AddString_S("lambdaSource")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton, "timed", nullptr, gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr__executeLambda_OUTER,	Eidos_ExecuteFunction__executeLambda_OUTER,	kEidosValueMaskAny | kEidosValueMaskVOID))->AddString_S("lambdaSource")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton, "timed", nullptr, gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("exists",			Eidos_ExecuteFunction_exists,		kEidosValueMaskLogical))->AddString("symbol"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("functionSignature",	Eidos_ExecuteFunction_functionSignature,	kEidosValueMaskVOID))->AddString_OSN("functionName", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("functionSource",	Eidos_ExecuteFunction_functionSource,	kEidosValueMaskVOID))->AddString_S("functionName"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_ls,		Eidos_ExecuteFunction_ls,			kEidosValueMaskVOID))->AddLogical_OS("showSymbolTables", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("license",			Eidos_ExecuteFunction_license,		kEidosValueMaskVOID)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("parallelGetNumThreads",			Eidos_ExecuteFunction_parallelGetNumThreads,		kEidosValueMaskInt | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("parallelGetMaxThreads",			Eidos_ExecuteFunction_parallelGetMaxThreads,		kEidosValueMaskInt | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("parallelGetTaskThreadCounts",	Eidos_ExecuteFunction_parallelGetTaskThreadCounts,	kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("parallelSetNumThreads",			Eidos_ExecuteFunction_parallelSetNumThreads,		kEidosValueMaskVOID))->AddInt_OSN("numThreads", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("parallelSetTaskThreadCounts",	Eidos_ExecuteFunction_parallelSetTaskThreadCounts,	kEidosValueMaskVOID))->AddObject_SN("dict", nullptr));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_rm,		Eidos_ExecuteFunction_rm,			kEidosValueMaskVOID))->AddString_ON("variableNames", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setSeed",			Eidos_ExecuteFunction_setSeed,		kEidosValueMaskVOID))->AddInt_S("seed"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("getSeed",			Eidos_ExecuteFunction_getSeed,		kEidosValueMaskInt | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("stop",				Eidos_ExecuteFunction_stop,			kEidosValueMaskVOID))->AddString_OSN("message", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("suppressWarnings",	Eidos_ExecuteFunction_suppressWarnings,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical_S("suppress"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sysinfo",			Eidos_ExecuteFunction_sysinfo,		kEidosValueMaskAny))->AddString_S("key"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("system",			Eidos_ExecuteFunction_system,		kEidosValueMaskString))->AddString_S("command")->AddString_O("args", gStaticEidosValue_StringEmpty)->AddString_O("input", gStaticEidosValue_StringEmpty)->AddLogical_OS("stderr", gStaticEidosValue_LogicalF)->AddLogical_OS("wait", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("time",				Eidos_ExecuteFunction_time,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_usage,		Eidos_ExecuteFunction_usage,		kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton, "type", nullptr, EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("rss"))));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("version",			Eidos_ExecuteFunction_version,		kEidosValueMaskFloat))->AddLogical_OS("print", gStaticEidosValue_LogicalT));
		
		
		// ************************************************************************************
		//
		//	filesystem access functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("createDirectory",	Eidos_ExecuteFunction_createDirectory,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("path"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("filesAtPath",		Eidos_ExecuteFunction_filesAtPath,	kEidosValueMaskString))->AddString_S("path")->AddLogical_OS("fullPaths", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("getwd",				Eidos_ExecuteFunction_getwd,		kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("deleteFile",		Eidos_ExecuteFunction_deleteFile,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S(gEidosStr_filePath));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("fileExists",		Eidos_ExecuteFunction_fileExists,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S(gEidosStr_filePath));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("flushFile",			Eidos_ExecuteFunction_flushFile,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S(gEidosStr_filePath));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("readFile",			Eidos_ExecuteFunction_readFile,		kEidosValueMaskString))->AddString_S(gEidosStr_filePath));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setwd",				Eidos_ExecuteFunction_setwd,		kEidosValueMaskString | kEidosValueMaskSingleton))->AddString_S("path"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("tempdir",			Eidos_ExecuteFunction_tempdir,		kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("writeFile",			Eidos_ExecuteFunction_writeFile,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S(gEidosStr_filePath)->AddString("contents")->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("compress", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("writeTempFile",		Eidos_ExecuteFunction_writeTempFile,	kEidosValueMaskString | kEidosValueMaskSingleton))->AddString_S("prefix")->AddString_S("suffix")->AddString("contents")->AddLogical_OS("compress", gStaticEidosValue_LogicalF));

		
		// ************************************************************************************
		//
		//	built-in user-defined functions
		//
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_source,	gEidosSourceCode_source,	kEidosValueMaskVOID))->AddString_S(gEidosStr_filePath)->AddLogical_OS("chdir", gStaticEidosValue_LogicalF));
		
		
		// ************************************************************************************
		//
		//	object instantiation – delegated to EidosClass subclasses
		//
		for (EidosClass *eidos_class : EidosClass::RegisteredClasses(true, true))
		{
			const std::vector<EidosFunctionSignature_CSP> *class_functions = eidos_class->Functions();
			
			signatures->insert(signatures->end(), class_functions->begin(), class_functions->end());
		}
		
		
		// alphabetize, mostly to be nice to the auto-completion feature
		std::sort(signatures->begin(), signatures->end(), CompareEidosFunctionSignatures);
	}
	
	return *signatures;
}

EidosFunctionMap *EidosInterpreter::s_built_in_function_map_ = nullptr;

void EidosInterpreter::CacheBuiltInFunctionMap(void)
{
	// The built-in function map is statically allocated for faster EidosInterpreter startup
	
	if (!s_built_in_function_map_)
	{
		const std::vector<EidosFunctionSignature_CSP> &built_in_functions = EidosInterpreter::BuiltInFunctions();
		
		s_built_in_function_map_ = new EidosFunctionMap;
		
		for (const EidosFunctionSignature_CSP &sig : built_in_functions)
			s_built_in_function_map_->insert(EidosFunctionMapPair(sig->call_name_, sig));
	}
}


//
//	Executing function calls
//

bool IdenticalEidosValues(EidosValue *x_value, EidosValue *y_value, bool p_compare_dimensions)
{
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if ((x_type != y_type) || (x_count != y_count))
		return false;
	
	if (p_compare_dimensions && !EidosValue::MatchingDimensions(x_value, y_value))
		return false;
	
	if (x_type == EidosValueType::kValueNULL)
		return true;
	
	if (x_count == 1)
	{
		// Handle singleton comparison separately, to allow the use of the fast vector API below
		if (x_type == EidosValueType::kValueLogical)
		{
			if (x_value->LogicalAtIndex(0, nullptr) != y_value->LogicalAtIndex(0, nullptr))
				return false;
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			if (x_value->IntAtIndex(0, nullptr) != y_value->IntAtIndex(0, nullptr))
				return false;
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double xv = x_value->FloatAtIndex(0, nullptr);
			double yv = y_value->FloatAtIndex(0, nullptr);
			
			if (!std::isnan(xv) || !std::isnan(yv))
				if (xv != yv)
					return false;
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string &x_string = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::string &y_string = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			
			if (x_string != y_string)
				return false;
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			if (x_value->ObjectElementAtIndex(0, nullptr) != y_value->ObjectElementAtIndex(0, nullptr))
				return false;
		}
	}
	else
	{
		// We have x_count != 1, so we can use the fast vector API; we want identical() to be very fast since it is a common bottleneck
		if (x_type == EidosValueType::kValueLogical)
		{
			const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
			const eidos_logical_t *logical_data1 = y_value->LogicalVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (logical_data0[value_index] != logical_data1[value_index])
					return false;
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = y_value->IntVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (int_data0[value_index] != int_data1[value_index])
					return false;
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data0 = x_value->FloatVector()->data();
			const double *float_data1 = y_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				double xv = float_data0[value_index];
				double yv = float_data1[value_index];
				
				if (!std::isnan(xv) || !std::isnan(yv))
					if (xv != yv)
						return false;
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *y_value->StringVector();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (string_vec0[value_index] != string_vec1[value_index])
					return false;
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObject * const *objelement_vec0 = x_value->ObjectElementVector()->data();
			EidosObject * const *objelement_vec1 = y_value->ObjectElementVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (objelement_vec0[value_index] != objelement_vec1[value_index])
					return false;
		}
	}
	
	return true;
}

EidosValue_SP ConcatenateEidosValues(const std::vector<EidosValue_SP> &p_arguments, bool p_allow_null, bool p_allow_void)
{
	int argument_count = (int)p_arguments.size();
	
	// This function expects an error range to be set bracketing it externally,
	// so no blame token is needed here.
	
	EidosValueType highest_type = EidosValueType::kValueVOID;	// start at the lowest
	bool has_object_type = false, has_nonobject_type = false, all_invisible = true;
	const EidosClass *element_class = gEidosObject_Class;
	int reserve_size = 0;
	
	// First figure out our return type, which is the highest-promotion type among all our arguments
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		EidosValueType arg_type = arg_value->Type();
		int arg_value_count = arg_value->Count();
		
		reserve_size += arg_value_count;
		
		if (arg_type == EidosValueType::kValueVOID && !p_allow_void)
			EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): void is not allowed to be used in this context." << EidosTerminate(nullptr);
		if (arg_type == EidosValueType::kValueNULL && !p_allow_null)
			EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): NULL is not allowed to be used in this context." << EidosTerminate(nullptr);
		
		// The highest type includes arguments of zero length; doing c(3, 7, string(0)) should produce a string vector
		if (arg_type > highest_type)
			highest_type = arg_type;
		
		if (!arg_value->Invisible())
			all_invisible = false;
		
		if (arg_type == EidosValueType::kValueObject)
		{
			const EidosClass *this_element_class = ((EidosValue_Object *)arg_value)->Class();
			
			if (this_element_class != gEidosObject_Class)	// undefined objects do not conflict with other object types
			{
				if (element_class == gEidosObject_Class)
				{
					// we haven't seen a (defined) object type yet, so remember what type we're dealing with
					element_class = this_element_class;
				}
				else
				{
					// we've already seen a object type, so check that this one is the same type
					if (element_class != this_element_class)
						EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): objects of different types cannot be mixed." << EidosTerminate(nullptr);
				}
			}
			
			has_object_type = true;
		}
		else if ((arg_type != EidosValueType::kValueNULL) && (arg_type != EidosValueType::kValueVOID))
			has_nonobject_type = true;
	}
	
	if (has_object_type && has_nonobject_type)
		EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): object and non-object types cannot be mixed." << EidosTerminate(nullptr);
	
	if (highest_type == EidosValueType::kValueVOID)
	{
		// If VOID is disallowed by p_allow_void, we will not return it.  Otherwise, if all the values we concatenated are VOID,
		// we return VOID; the result of a vectorized method call where every call returns VOID is VOID, for instance.
		return gStaticEidosValueVOID;
	}
	if (highest_type == EidosValueType::kValueNULL)
	{
		// If we've got nothing but NULL, then return NULL; preserve invisibility
		if (all_invisible)
			return gStaticEidosValueNULLInvisible;
		else
			return gStaticEidosValueNULL;
	}
	
	// Create an object of the right return type, concatenate all the arguments together, and return it
	// Note that NULLs here concatenate away silently because their Count()==0; a bit dangerous!
	if (highest_type == EidosValueType::kValueLogical)
	{
		EidosValue_Logical *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(reserve_size);
		EidosValue_Logical_SP result_SP = EidosValue_Logical_SP(result);
		int result_set_index = 0;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			
			if (arg_value == gStaticEidosValue_LogicalF)
			{
				result->set_logical_no_check(false, result_set_index++);
			}
			else if (arg_value == gStaticEidosValue_LogicalT)
			{
				result->set_logical_no_check(true, result_set_index++);
			}
			else
			{
				int arg_value_count = arg_value->Count();
				
				if (arg_value_count)	// weeds out NULL; otherwise we must have a logical vector
				{
					const eidos_logical_t *arg_data = arg_value->LogicalVector()->data();
					
					// Unlike the integer and float cases below, memcpy() is much faster for logical values
					// on OS X 10.12.6, Xcode 8.3; about 1.5 times faster, in fact.  So it is a win here.
					
					//for (int value_index = 0; value_index < arg_value_count; ++value_index)
					//	result->set_logical_no_check(arg_data[value_index], result_set_index++);
					memcpy(result->data() + result_set_index, arg_data, arg_value_count * sizeof(eidos_logical_t));
					result_set_index += arg_value_count;
				}
			}
		}
		
		return result_SP;
	}
	else if (highest_type == EidosValueType::kValueInt)
	{
		EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(reserve_size);
		EidosValue_Int_vector_SP result_SP = EidosValue_Int_vector_SP(result);
		int result_set_index = 0;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				result->set_int_no_check(arg_value->IntAtIndex(0, nullptr), result_set_index++);
			}
			else if (arg_value_count)
			{
				if (arg_value->Type() == EidosValueType::kValueInt)
				{
					// Speed up integer arguments, which are probably common since our result is integer
					const int64_t *arg_data = arg_value->IntVector()->data();
					
					// Annoyingly, memcpy() is actually *slower* here on OS X 10.12.6, Xcode 8.3; a test of the
					// memcpy() version runs in ~53.3 seconds versus ~48.4 seconds for the loop.  So the Clang
					// optimizer is smarter than the built-in memcpy() implementation, I guess.
					
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->set_int_no_check(arg_data[value_index], result_set_index++);
					//memcpy(result->data() + result_set_index, arg_data, arg_value_count * sizeof(int64_t));
					//result_set_index += arg_value_count;
				}
				else
				{
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->set_int_no_check(arg_value->IntAtIndex(value_index, nullptr), result_set_index++);
				}
			}
		}
		
		return result_SP;
	}
	else if (highest_type == EidosValueType::kValueFloat)
	{
		EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(reserve_size);
		EidosValue_Float_vector_SP result_SP = EidosValue_Float_vector_SP(result);
		int result_set_index = 0;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				result->set_float_no_check(arg_value->FloatAtIndex(0, nullptr), result_set_index++);
			}
			else if (arg_value_count)
			{
				if (arg_value->Type() == EidosValueType::kValueFloat)
				{
					// Speed up float arguments, which are probably common since our result is float
					const double *arg_data = arg_value->FloatVector()->data();
					
					// Annoyingly, memcpy() is actually *slower* here on OS X 10.12.6, Xcode 8.3; a test of the
					// memcpy() version runs in ~53.3 seconds versus ~48.4 seconds for the loop.  So the Clang
					// optimizer is smarter than the built-in memcpy() implementation, I guess.
					
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->set_float_no_check(arg_data[value_index], result_set_index++);
					//memcpy(result->data() + result_set_index, arg_data, arg_value_count * sizeof(double));
					//result_set_index += arg_value_count;
				}
				else
				{
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->set_float_no_check(arg_value->FloatAtIndex(value_index, nullptr), result_set_index++);
				}
			}
		}
		
		return result_SP;
	}
	else if (highest_type == EidosValueType::kValueString)
	{
		EidosValue_String_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(reserve_size);
		EidosValue_String_vector_SP result_SP = EidosValue_String_vector_SP(result);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
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
		EidosValue_Object_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(element_class))->resize_no_initialize_RR(reserve_size);
		EidosValue_Object_vector_SP result_SP = EidosValue_Object_vector_SP(result);
		int result_set_index = 0;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_value_count = arg_value->Count();
			
			if (arg_value_count == 1)
			{
				if (result->UsesRetainRelease())
					result->set_object_element_no_check_no_previous_RR(arg_value->ObjectElementAtIndex(0, nullptr), result_set_index++);
				else
					result->set_object_element_no_check_NORR(arg_value->ObjectElementAtIndex(0, nullptr), result_set_index++);
			}
			else if (arg_value_count)
			{
				const EidosValue_Object_vector *object_arg_value = arg_value->ObjectElementVector();
				EidosObject * const *arg_data = object_arg_value->data();
				
				// Given the lack of win for memcpy() for integer and float above, I'm not even going to bother checking it for
				// EidosObject*, since there would also be the complexity of retain/release and DeclareClass() to deal with...
				
				// When retain/release of EidosObject is enabled, we go through the accessors so the copied pointers get retained
				if (object_arg_value->UsesRetainRelease())
				{
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->set_object_element_no_check_no_previous_RR(arg_data[value_index], result_set_index++);
				}
				else
				{
					for (int value_index = 0; value_index < arg_value_count; ++value_index)
						result->set_object_element_no_check_NORR(arg_data[value_index], result_set_index++);
				}
			}
		}
		
		return result_SP;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (ConcatenateEidosValues): type '" << highest_type << "' is not supported by ConcatenateEidosValues()." << EidosTerminate(nullptr);
	}
	
	return EidosValue_SP(nullptr);
}

EidosValue_SP UniqueEidosValue(const EidosValue *p_x_value, bool p_force_new_vector, bool p_preserve_order)
{
	EidosValue_SP result_SP(nullptr);
	
	const EidosValue *x_value = p_x_value;
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		result_SP = x_value->NewMatchingType();
	}
	else if (x_count == 1)
	{
		if (p_force_new_vector)
			result_SP = x_value->VectorBasedCopy();
		else
			result_SP = x_value->CopyValues();
	}
	else if (x_type == EidosValueType::kValueLogical)
	{
		const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
		bool containsF = false, containsT = false;
		
		if (logical_data[0])
		{
			// We have a true, look for a false
			containsT = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (!logical_data[value_index])
				{
					containsF = true;
					break;
				}
		}
		else
		{
			// We have a false, look for a true
			containsF = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (logical_data[value_index])
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
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(logical_data[0], 0);
			logical_result->set_logical_no_check(!logical_data[0], 1);
		}
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
		const int64_t *int_data = x_value->IntVector()->data();
		EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
		result_SP = EidosValue_SP(int_result);
		
		if (p_preserve_order)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t value = int_data[value_index];
				int scan_index;
				
				for (scan_index = 0; scan_index < value_index; ++scan_index)
				{
					if (value == int_data[scan_index])
						break;
				}
				
				if (scan_index == value_index)
					int_result->push_int(value);
			}
		}
		else
		{
			std::vector<int64_t> dup_vec(int_data, int_data + x_count);
			
			std::sort(dup_vec.begin(), dup_vec.end());
			
			auto unique_iter = std::unique(dup_vec.begin(), dup_vec.end());
			size_t unique_count = unique_iter - dup_vec.begin();
			int64_t *dup_ptr = dup_vec.data();
			
			int_result->resize_no_initialize(unique_count);
			
			for (size_t unique_index = 0; unique_index < unique_count; ++unique_index)
				int_result->set_int_no_check(dup_ptr[unique_index], unique_index);
		}
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
		result_SP = EidosValue_SP(float_result);
		
		if (p_preserve_order)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				double value = float_data[value_index];
				int scan_index;
				
				for (scan_index = 0; scan_index < value_index; ++scan_index)
				{
					double comp = float_data[scan_index];
					
					// We need NAN values to compare equal; we unique multiple NANs down to one
					if ((std::isnan(value) && std::isnan(comp)) || (value == comp))
						break;
				}
				
				if (scan_index == value_index)
					float_result->push_float(value);
			}
		}
		else
		{
			std::vector<double> dup_vec(float_data, float_data + x_count);
			
			// sort NANs to the end
			std::sort(dup_vec.begin(), dup_vec.end(), [](const double& a, const double& b) { return std::isnan(b) || (a < b); });
			
			// Remove duplicates, including duplicate NANs
			auto unique_iter = std::unique(dup_vec.begin(), dup_vec.end(), [](const double& a, const double& b) { return (std::isnan(a) && std::isnan(b)) || (a == b); });
			size_t unique_count = unique_iter - dup_vec.begin();
			double *dup_ptr = dup_vec.data();
			
			float_result->resize_no_initialize(unique_count);
			
			for (size_t unique_index = 0; unique_index < unique_count; ++unique_index)
				float_result->set_float_no_check(dup_ptr[unique_index], unique_index);
		}
	}
	else if (x_type == EidosValueType::kValueString)
	{
		// We have x_count != 1, so the type of x_value must be EidosValue_String_vector; we can use the fast API
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		if (p_preserve_order)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				std::string value = string_vec[value_index];
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
		else
		{
			std::vector<std::string> dup_vec = string_vec;
			
			std::sort(dup_vec.begin(), dup_vec.end());
			
			auto unique_iter = std::unique(dup_vec.begin(), dup_vec.end());
			
			for (auto iter = dup_vec.begin(); iter != unique_iter; ++iter)
				string_result->PushString(*iter);
		}
	}
	else if (x_type == EidosValueType::kValueObject)
	{
		// We have x_count != 1, so the type of x_value must be EidosValue_Object_vector; we can use the fast API
		EidosObject * const *object_data = x_value->ObjectElementVector()->data();
		EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
		result_SP = EidosValue_SP(object_result);
		
		if (p_preserve_order)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				EidosObject *value = object_data[value_index];
				int scan_index;
				
				for (scan_index = 0; scan_index < value_index; ++scan_index)
				{
					if (value == object_data[scan_index])
						break;
				}
				
				if (scan_index == value_index)
					object_result->push_object_element_CRR(value);
			}
		}
		else
		{
			std::vector<EidosObject*> dup_vec(object_data, object_data + x_count);
			
			std::sort(dup_vec.begin(), dup_vec.end());
			
			auto unique_iter = std::unique(dup_vec.begin(), dup_vec.end());
			size_t unique_count = unique_iter - dup_vec.begin();
			EidosObject * const *dup_ptr = dup_vec.data();
			
			object_result->resize_no_initialize_RR(unique_count);
			
			if (object_result->UsesRetainRelease())
			{
				for (size_t unique_index = 0; unique_index < unique_count; ++unique_index)
					object_result->set_object_element_no_check_no_previous_RR(dup_ptr[unique_index], unique_index);
			}
			else
			{
				for (size_t unique_index = 0; unique_index < unique_count; ++unique_index)
					object_result->set_object_element_no_check_NORR(dup_ptr[unique_index], unique_index);
			}
		}
	}
	
	return result_SP;
}

EidosValue_SP SubsetEidosValue(const EidosValue *p_original_value, const EidosValue *p_indices, EidosToken *p_error_token, bool p_raise_range_errors)
{
	// We have a simple vector-style subset that is not NULL; handle it as we did in Eidos 1.5 and earlier
	// If p_raise_range_errors is false, out-of-range indices will be ignored; if it is true, they will cause an error
	EidosValueType original_value_type = p_original_value->Type();
	EidosValueType indices_type = p_indices->Type();
	
	int original_value_count = p_original_value->Count();
	int indices_count = p_indices->Count();
	
	EidosValue_SP result_SP;
	
	if (indices_type == EidosValueType::kValueLogical)
	{
		// Subsetting with a logical vector means the vectors must match in length, if p_raise_range_errors is true; indices with a T value will be taken
		// If p_raise_range_errors is false, we here clip indices_count to original_value_count so we can loop over it safely
		if (original_value_count != indices_count)
		{
			if (p_raise_range_errors)
				EIDOS_TERMINATION << "ERROR (SubsetEidosValue): the '[]' operator requires that the size() of a logical index operand must match the size() of the indexed operand." << EidosTerminate(p_error_token);
			else
				indices_count = std::min(indices_count, original_value_count);
		}
		
		// Subsetting with a logical vector does not attempt to allocate singleton values, for now; seems unlikely to be a frequently hit case
		const eidos_logical_t *logical_index_data = p_indices->LogicalVector()->data();
		
		if (original_value_count == 1)
		{
			// This is the simple logic using NewMatchingType() / PushValueFromIndexOfEidosValue()
			result_SP = p_original_value->NewMatchingType();
			
			EidosValue *result = result_SP.get();
			
			for (int value_idx = 0; value_idx < indices_count; value_idx++)
				if (logical_index_data[value_idx])
					result->PushValueFromIndexOfEidosValue(value_idx, *p_original_value, p_error_token);
		}
		else
		{
			// Here we can special-case each type for speed since we know we're not dealing with singletons
			if (original_value_type == EidosValueType::kValueLogical)
			{
				const eidos_logical_t *first_child_data = p_original_value->LogicalVector()->data();
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->reserve(indices_count);
				
				for (int value_idx = 0; value_idx < indices_count; value_idx++)
					if (logical_index_data[value_idx])
						logical_result->push_logical_no_check(first_child_data[value_idx]);
				
				result_SP = logical_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueInt)
			{
				const int64_t *first_child_data = p_original_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->reserve(indices_count);
				
				for (int value_idx = 0; value_idx < indices_count; value_idx++)
					if (logical_index_data[value_idx])
						int_result->push_int_no_check(first_child_data[value_idx]);		// cannot use set_int_no_check() because of the if()
				
				result_SP = int_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueFloat)
			{
				const double *first_child_data = p_original_value->FloatVector()->data();
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->reserve(indices_count);
				
				for (int value_idx = 0; value_idx < indices_count; value_idx++)
					if (logical_index_data[value_idx])
						float_result->push_float_no_check(first_child_data[value_idx]);	// cannot use set_int_no_check() because of the if()
				
				result_SP = float_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueString)
			{
				const std::vector<std::string> &first_child_vec = *p_original_value->StringVector();
				EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
				EidosValue_String_vector *string_result = string_result_SP->Reserve(indices_count);
				
				for (int value_idx = 0; value_idx < indices_count; value_idx++)
					if (logical_index_data[value_idx])
						string_result->PushString(first_child_vec[value_idx]);
				
				result_SP = string_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueObject)
			{
				EidosObject * const *first_child_vec = p_original_value->ObjectElementVector()->data();
				EidosValue_Object_vector_SP obj_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)p_original_value)->Class()));
				EidosValue_Object_vector *obj_result = obj_result_SP->reserve(indices_count);
				
				for (int value_idx = 0; value_idx < indices_count; value_idx++)
					if (logical_index_data[value_idx])
						obj_result->push_object_element_no_check_CRR(first_child_vec[value_idx]);
				
				result_SP = obj_result_SP;
			}
		}
	}
	else
	{
		if (indices_count == 1)
		{
			// Subsetting with a singleton int/float vector is common and should return a singleton value for speed
			// This is guaranteed to return a singleton value (when available)
			int index_value = (int)p_indices->IntAtIndex(0, p_error_token);
			
			if ((index_value < 0) || (index_value >= original_value_count))
			{
				if (p_raise_range_errors)
					EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
				else
					result_SP = p_original_value->NewMatchingType();
			}
			else
				result_SP = p_original_value->GetValueAtIndex(index_value, p_error_token);
		}
		else if (original_value_count == 1)
		{
			// We can't use direct access on p_original_value if it is a singleton, so this needs to be special-cased
			// Note this is identical to the general-case code below that is never hit
			result_SP = p_original_value->NewMatchingType();
			
			EidosValue *result = result_SP.get();
			
			for (int value_idx = 0; value_idx < indices_count; value_idx++)
			{
				int64_t index_value = p_indices->IntAtIndex(value_idx, p_error_token);
				
				if ((index_value < 0) || (index_value >= original_value_count))
				{
					if (p_raise_range_errors)
						EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
				}
				else
					result->PushValueFromIndexOfEidosValue((int)index_value, *p_original_value, p_error_token);
			}
		}
		else
		{
			// Subsetting with a int/float vector can use a vector of any length; the specific indices referenced will be taken
			if (original_value_type == EidosValueType::kValueFloat)
			{
				// result type is float; optimize for that
				const double *first_child_data = p_original_value->FloatVector()->data();
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->reserve(indices_count);
				
				if (indices_type == EidosValueType::kValueInt)
				{
					// integer indices; we can use fast access since we know indices_count != 1
					const int64_t *int_index_data = p_indices->IntVector()->data();
					
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = int_index_data[value_idx];
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							float_result->push_float_no_check(first_child_data[index_value]);
					}
				}
				else
				{
					// float indices; we use IntAtIndex() since it has complex behavior
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = p_indices->IntAtIndex(value_idx, p_error_token);
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							float_result->push_float_no_check(first_child_data[index_value]);
					}
				}
				
				result_SP = float_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueInt)
			{
				// result type is integer; optimize for that
				const int64_t *first_child_data = p_original_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->reserve(indices_count);
				
				if (indices_type == EidosValueType::kValueInt)
				{
					// integer indices; we can use fast access since we know indices_count != 1
					const int64_t *int_index_data = p_indices->IntVector()->data();
					
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = int_index_data[value_idx];
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							int_result->push_int_no_check(first_child_data[index_value]);
					}
				}
				else
				{
					// float indices; we use IntAtIndex() since it has complex behavior
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = p_indices->IntAtIndex(value_idx, p_error_token);
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							int_result->push_int_no_check(first_child_data[index_value]);
					}
				}
				
				result_SP = int_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueObject)
			{
				// result type is object; optimize for that
				EidosObject * const *first_child_vec = p_original_value->ObjectElementVector()->data();
				EidosValue_Object_vector_SP obj_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)p_original_value)->Class()));
				EidosValue_Object_vector *obj_result = obj_result_SP->reserve(indices_count);
				
				if (indices_type == EidosValueType::kValueInt)
				{
					// integer indices; we can use fast access since we know indices_count != 1
					const int64_t *int_index_data = p_indices->IntVector()->data();
					
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = int_index_data[value_idx];
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							obj_result->push_object_element_no_check_CRR(first_child_vec[index_value]);
					}
				}
				else
				{
					// float indices; we use IntAtIndex() since it has complex behavior
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = p_indices->IntAtIndex(value_idx, p_error_token);
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							obj_result->push_object_element_no_check_CRR(first_child_vec[index_value]);
					}
				}
				
				result_SP = obj_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueLogical)
			{
				// result type is logical; optimize for that
				const eidos_logical_t *first_child_data = p_original_value->LogicalVector()->data();
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->reserve(indices_count);
				
				if (indices_type == EidosValueType::kValueInt)
				{
					// integer indices; we can use fast access since we know indices_count != 1
					const int64_t *int_index_data = p_indices->IntVector()->data();
					
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = int_index_data[value_idx];
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							logical_result->push_logical_no_check(first_child_data[index_value]);
					}
				}
				else
				{
					// float indices; we use IntAtIndex() since it has complex behavior
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = p_indices->IntAtIndex(value_idx, p_error_token);
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							logical_result->push_logical_no_check(first_child_data[index_value]);
					}
				}
				
				result_SP = logical_result_SP;
			}
			else if (original_value_type == EidosValueType::kValueString)
			{
				// result type is string; optimize for that
				const std::vector<std::string> &first_child_vec = *p_original_value->StringVector();
				EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
				EidosValue_String_vector *string_result = string_result_SP->Reserve(indices_count);
				
				if (indices_type == EidosValueType::kValueInt)
				{
					// integer indices; we can use fast access since we know indices_count != 1
					const int64_t *int_index_data = p_indices->IntVector()->data();
					
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = int_index_data[value_idx];
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							string_result->PushString(first_child_vec[index_value]);
					}
				}
				else
				{
					// float indices; we use IntAtIndex() since it has complex behavior
					for (int value_idx = 0; value_idx < indices_count; value_idx++)
					{
						int64_t index_value = p_indices->IntAtIndex(value_idx, p_error_token);
						
						if ((index_value < 0) || (index_value >= original_value_count))
						{
							if (p_raise_range_errors)
								EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
						}
						else
							string_result->PushString(first_child_vec[index_value]);
					}
				}
				
				result_SP = string_result_SP;
			}
			else
			{
				// This is the general case; it should never be hit
				// CODE COVERAGE: This is dead code
				result_SP = p_original_value->NewMatchingType();
				
				EidosValue *result = result_SP.get();
				
				for (int value_idx = 0; value_idx < indices_count; value_idx++)
				{
					int64_t index_value = p_indices->IntAtIndex(value_idx, p_error_token);
					
					if ((index_value < 0) || (index_value >= original_value_count))
					{
						if (p_raise_range_errors)
							EIDOS_TERMINATION << "ERROR (SubsetEidosValue): out-of-range index " << index_value << " used with the '[]' operator." << EidosTerminate(p_error_token);
					}
					else
						result->PushValueFromIndexOfEidosValue((int)index_value, *p_original_value, p_error_token);
				}
			}
		}
	}
	
	return result_SP;
}







































