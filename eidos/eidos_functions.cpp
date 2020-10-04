//
//  eidos_functions.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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
#include <chrono>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <utility>
#include <numeric>
#include <cmath>
#include <functional>
#include <limits>
#include <sys/stat.h>
#include <sys/param.h>

#include "string.h"

#include "gsl_linalg.h"
#include "gsl_errno.h"
#include "gsl_cdf.h"

#include "../eidos_zlib/zlib.h"


// BCH 20 October 2016: continuing to try to fix problems with gcc 5.4.0 on Linux without breaking other
// builds.  We will switch to including <cmath> and using the std:: namespace math functions, since on
// that platform <cmath> is included as a side effect even if we don't include it ourselves, and on
// that platform that actually (incorrectly) undefines the global functions defined by math.h.  On other
// platforms, we get the global math.h functions defined as well, so we can't use using to select the
// <cmath> functions, we have to specify them explicitly.
// BCH 21 May 2017: since this continues to come up as an issue, it's worth adding a bit more commentary.
// New code introduced on OS X may not correctly use the std:: namespace qualifier for math functions,
// because it is not required in Xcode, and then the build breaks on Linux.  This problem is made worse
// by the fact that gsl_math.h includes math.h, so that brings in the C functions in the global namespace.
// We can't change that, because the GSL is C code and needs to use the C math library; it has not been
// validated against the C++ math library as far as I know, so changing it to use cmath would be
// dangerous.  So I think we need to just tolerate this build issue and fix it when it arises.
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


//
//	Construct our built-in function map
//

// We allocate all of our function signatures once and keep them forever, for faster EidosInterpreter startup
const std::vector<EidosFunctionSignature_CSP> &EidosInterpreter::BuiltInFunctions(void)
{
	static std::vector<EidosFunctionSignature_CSP> *signatures = nullptr;
	
	if (!signatures)
	{
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
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("range",				Eidos_ExecuteFunction_range,		kEidosValueMaskNumeric))->AddNumeric("x")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sd",				Eidos_ExecuteFunction_sd,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ttest",				Eidos_ExecuteFunction_ttest,		kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddFloat("x")->AddFloat_ON("y", gStaticEidosValueNULL)->AddFloat_OSN("mu", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("var",				Eidos_ExecuteFunction_var,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		
		
		// ************************************************************************************
		//
		//	distribution draw / density functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dmvnorm",			Eidos_ExecuteFunction_dmvnorm,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric("mu")->AddNumeric("sigma"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dbeta",				Eidos_ExecuteFunction_dbeta,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric("alpha")->AddNumeric("beta"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dexp",				Eidos_ExecuteFunction_dexp,			kEidosValueMaskFloat))->AddFloat("x")->AddNumeric_O("mu", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dgamma",			Eidos_ExecuteFunction_dgamma,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric("mean")->AddNumeric("shape"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dnorm",				Eidos_ExecuteFunction_dnorm,		kEidosValueMaskFloat))->AddFloat("x")->AddNumeric_O("mean", gStaticEidosValue_Float0)->AddNumeric_O("sd", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pnorm",				Eidos_ExecuteFunction_pnorm,		kEidosValueMaskFloat))->AddFloat("q")->AddNumeric_O("mean", gStaticEidosValue_Float0)->AddNumeric_O("sd", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("qnorm",				Eidos_ExecuteFunction_qnorm,		kEidosValueMaskFloat))->AddFloat("p")->AddNumeric_O("mean", gStaticEidosValue_Float0)->AddNumeric_O("sd", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbeta",				Eidos_ExecuteFunction_rbeta,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("alpha")->AddNumeric("beta"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbinom",			Eidos_ExecuteFunction_rbinom,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddInt("size")->AddFloat("prob"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rcauchy",			Eidos_ExecuteFunction_rcauchy,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("location", gStaticEidosValue_Float0)->AddNumeric_O("scale", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rdunif",			Eidos_ExecuteFunction_rdunif,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddInt_O("min", gStaticEidosValue_Integer0)->AddInt_O("max", gStaticEidosValue_Integer1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rexp",				Eidos_ExecuteFunction_rexp,			kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("mu", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgamma",			Eidos_ExecuteFunction_rgamma,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("mean")->AddNumeric("shape"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgeom",				Eidos_ExecuteFunction_rgeom,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddFloat("p"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rlnorm",			Eidos_ExecuteFunction_rlnorm,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("meanlog", gStaticEidosValue_Float0)->AddNumeric_O("sdlog", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rmvnorm",			Eidos_ExecuteFunction_rmvnorm,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("mu")->AddNumeric("sigma"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rnorm",				Eidos_ExecuteFunction_rnorm,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("mean", gStaticEidosValue_Float0)->AddNumeric_O("sd", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rpois",				Eidos_ExecuteFunction_rpois,		kEidosValueMaskInt))->AddInt_S(gEidosStr_n)->AddNumeric("lambda"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("runif",				Eidos_ExecuteFunction_runif,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric_O("min", gStaticEidosValue_Float0)->AddNumeric_O("max", gStaticEidosValue_Float1));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rweibull",			Eidos_ExecuteFunction_rweibull,		kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddNumeric("lambda")->AddNumeric("k"));
		
		
		// ************************************************************************************
		//
		//	vector construction functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_c,			Eidos_ExecuteFunction_c,			kEidosValueMaskAny))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_float,		Eidos_ExecuteFunction_float,		kEidosValueMaskFloat))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_integer,	Eidos_ExecuteFunction_integer,		kEidosValueMaskInt))->AddInt_S("length")->AddInt_OS("fill1", gStaticEidosValue_Integer0)->AddInt_OS("fill2", gStaticEidosValue_Integer1)->AddInt_ON("fill2Indices", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_logical,	Eidos_ExecuteFunction_logical,		kEidosValueMaskLogical))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_object,	Eidos_ExecuteFunction_object,		kEidosValueMaskObject, gEidos_UndefinedClassObject)));
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
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cat",				Eidos_ExecuteFunction_cat,			kEidosValueMaskVOID))->AddAny("x")->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("catn",				Eidos_ExecuteFunction_catn,			kEidosValueMaskVOID))->AddAny_O("x", gStaticEidosValue_StringEmpty)->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("format",			Eidos_ExecuteFunction_format,		kEidosValueMaskString))->AddString_S("format")->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("identical",			Eidos_ExecuteFunction_identical,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ifelse",			Eidos_ExecuteFunction_ifelse,		kEidosValueMaskAny))->AddLogical("test")->AddAny("trueValues")->AddAny("falseValues"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("match",				Eidos_ExecuteFunction_match,		kEidosValueMaskInt))->AddAny("x")->AddAny("table"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nchar",				Eidos_ExecuteFunction_nchar,		kEidosValueMaskInt))->AddString("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("order",				Eidos_ExecuteFunction_order,		kEidosValueMaskInt))->AddAnyBase("x")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("paste",				Eidos_ExecuteFunction_paste,		kEidosValueMaskString | kEidosValueMaskSingleton))->AddEllipsis()->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("paste0",			Eidos_ExecuteFunction_paste0,		kEidosValueMaskString | kEidosValueMaskSingleton))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("print",				Eidos_ExecuteFunction_print,		kEidosValueMaskVOID))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rev",				Eidos_ExecuteFunction_rev,			kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_size,		Eidos_ExecuteFunction_size_length,	kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_length,	Eidos_ExecuteFunction_size_length,	kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sort",				Eidos_ExecuteFunction_sort,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sortBy",			Eidos_ExecuteFunction_sortBy,		kEidosValueMaskObject))->AddObject("x", nullptr)->AddString_S("property")->AddLogical_OS("ascending", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_str,		Eidos_ExecuteFunction_str,			kEidosValueMaskVOID))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strsplit",			Eidos_ExecuteFunction_strsplit,		kEidosValueMaskString))->AddString_S("x")->AddString_OS("sep", gStaticEidosValue_StringSpace));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("substr",			Eidos_ExecuteFunction_substr,		kEidosValueMaskString))->AddString("x")->AddInt("first")->AddInt_ON("last", gStaticEidosValueNULL));
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
		
		
		// ************************************************************************************
		//
		//	color manipulation functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cmColors",			Eidos_ExecuteFunction_cmColors,		kEidosValueMaskString))->AddInt_S(gEidosStr_n));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("colors",			Eidos_ExecuteFunction_colors,		kEidosValueMaskString))->AddNumeric(gEidosStr_x)->AddString_S("name"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("heatColors",		Eidos_ExecuteFunction_heatColors,		kEidosValueMaskString))->AddInt_S(gEidosStr_n));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rainbow",			Eidos_ExecuteFunction_rainbow,		kEidosValueMaskString))->AddInt_S(gEidosStr_n)->AddFloat_OS(gEidosStr_s, gStaticEidosValue_Float1)->AddFloat_OS("v", gStaticEidosValue_Float1)->AddFloat_OS(gEidosStr_start, gStaticEidosValue_Float0)->AddFloat_OSN(gEidosStr_end, gStaticEidosValueNULL)->AddLogical_OS("ccw", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("terrainColors",		Eidos_ExecuteFunction_terrainColors,		kEidosValueMaskString))->AddInt_S(gEidosStr_n));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("hsv2rgb",			Eidos_ExecuteFunction_hsv2rgb,		kEidosValueMaskFloat))->AddFloat("hsv"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgb2hsv",			Eidos_ExecuteFunction_rgb2hsv,		kEidosValueMaskFloat))->AddFloat("rgb"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgb2color",			Eidos_ExecuteFunction_rgb2color,	kEidosValueMaskString))->AddFloat("rgb"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("color2rgb",			Eidos_ExecuteFunction_color2rgb,	kEidosValueMaskFloat))->AddString(gEidosStr_color));
		
		
		// ************************************************************************************
		//
		//	miscellaneous functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_apply,		Eidos_ExecuteFunction_apply,		kEidosValueMaskAny))->AddAny("x")->AddInt("margin")->AddString_S("lambdaSource"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_sapply,	Eidos_ExecuteFunction_sapply,		kEidosValueMaskAny))->AddAny("x")->AddString_S("lambdaSource")->AddString_OS("simplify", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("vector"))));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("beep",				Eidos_ExecuteFunction_beep,			kEidosValueMaskVOID))->AddString_OSN("soundName", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("citation",			Eidos_ExecuteFunction_citation,		kEidosValueMaskVOID)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("clock",				Eidos_ExecuteFunction_clock,		kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddString_OS("type", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("cpu"))));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("date",				Eidos_ExecuteFunction_date,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("defineConstant",	Eidos_ExecuteFunction_defineConstant,	kEidosValueMaskVOID))->AddString_S("symbol")->AddAny("value"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_doCall,	Eidos_ExecuteFunction_doCall,		kEidosValueMaskAny | kEidosValueMaskVOID))->AddString_S("functionName")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_executeLambda,	Eidos_ExecuteFunction_executeLambda,	kEidosValueMaskAny | kEidosValueMaskVOID))->AddString_S("lambdaSource")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton, "timed", nullptr, gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr__executeLambda_OUTER,	Eidos_ExecuteFunction__executeLambda_OUTER,	kEidosValueMaskAny | kEidosValueMaskVOID))->AddString_S("lambdaSource")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton, "timed", nullptr, gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("exists",			Eidos_ExecuteFunction_exists,		kEidosValueMaskLogical))->AddString("symbol"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("functionSignature",	Eidos_ExecuteFunction_functionSignature,	kEidosValueMaskVOID))->AddString_OSN("functionName", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_ls,		Eidos_ExecuteFunction_ls,			kEidosValueMaskVOID)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("license",			Eidos_ExecuteFunction_license,		kEidosValueMaskVOID)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_rm,		Eidos_ExecuteFunction_rm,			kEidosValueMaskVOID))->AddString_ON("variableNames", gStaticEidosValueNULL)->AddLogical_OS("removeConstants", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setSeed",			Eidos_ExecuteFunction_setSeed,		kEidosValueMaskVOID))->AddInt_S("seed"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("getSeed",			Eidos_ExecuteFunction_getSeed,		kEidosValueMaskInt | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("stop",				Eidos_ExecuteFunction_stop,			kEidosValueMaskVOID))->AddString_OSN("message", gStaticEidosValueNULL));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("suppressWarnings",	Eidos_ExecuteFunction_suppressWarnings,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical_S("suppress"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("system",			Eidos_ExecuteFunction_system,		kEidosValueMaskString))->AddString_S("command")->AddString_O("args", gStaticEidosValue_StringEmpty)->AddString_O("input", gStaticEidosValue_StringEmpty)->AddLogical_OS("stderr", gStaticEidosValue_LogicalF)->AddLogical_OS("wait", gStaticEidosValue_LogicalT));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("time",				Eidos_ExecuteFunction_time,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("usage",				Eidos_ExecuteFunction_usage,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddLogical_OS("peak", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("version",			Eidos_ExecuteFunction_version,		kEidosValueMaskFloat))->AddLogical_OS("print", gStaticEidosValue_LogicalT));
		
		
		// ************************************************************************************
		//
		//	filesystem access functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("createDirectory",	Eidos_ExecuteFunction_createDirectory,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("path"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("filesAtPath",		Eidos_ExecuteFunction_filesAtPath,	kEidosValueMaskString))->AddString_S("path")->AddLogical_OS("fullPaths", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("getwd",				Eidos_ExecuteFunction_getwd,		kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("deleteFile",		Eidos_ExecuteFunction_deleteFile,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("filePath"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("fileExists",		Eidos_ExecuteFunction_fileExists,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("filePath"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("readFile",			Eidos_ExecuteFunction_readFile,		kEidosValueMaskString))->AddString_S("filePath"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setwd",				Eidos_ExecuteFunction_setwd,		kEidosValueMaskString | kEidosValueMaskSingleton))->AddString_S("path"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("writeFile",			Eidos_ExecuteFunction_writeFile,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("filePath")->AddString("contents")->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("compress", gStaticEidosValue_LogicalF));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("writeTempFile",		Eidos_ExecuteFunction_writeTempFile,	kEidosValueMaskString | kEidosValueMaskSingleton))->AddString_S("prefix")->AddString_S("suffix")->AddString("contents")->AddLogical_OS("compress", gStaticEidosValue_LogicalF));

		
		// ************************************************************************************
		//
		//	built-in user-defined functions
		//
		{
			EidosFunctionSignature *source_signature = (EidosFunctionSignature *)(new EidosFunctionSignature("source",	nullptr,	kEidosValueMaskVOID))->AddString_S("filePath");
			EidosScript *source_script = new EidosScript("{ _executeLambda_OUTER(paste(readFile(filePath), '\\n')); return; }");
			
			source_script->Tokenize();
			source_script->ParseInterpreterBlockToAST(false);
			
			source_signature->body_script_ = source_script;
			
			signatures->emplace_back(source_signature);
		}
		
		
		// ************************************************************************************
		//
		//	object instantiation
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_Test",				Eidos_ExecuteFunction__Test,		kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElement_Class))->AddInt_S("yolk"));
		
		
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
			if (x_value->StringAtIndex(0, nullptr) != y_value->StringAtIndex(0, nullptr))
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
			EidosObjectElement * const *objelement_vec0 = x_value->ObjectElementVector()->data();
			EidosObjectElement * const *objelement_vec1 = y_value->ObjectElementVector()->data();
			
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
	const EidosObjectClass *element_class = gEidos_UndefinedClassObject;
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
				EidosObjectElement * const *arg_data = object_arg_value->data();
				
				// Given the lack of win for memcpy() for integer and float above, I'm not even going to bother checking it for
				// EidosObjectElement*, since there would also be the complexity of retain/release and DeclareClass() to deal with...
				
				// When retain/release of EidosObjectElement is enabled, we go through the accessors so the copied pointers get retained
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
		EidosObjectElement * const *object_data = x_value->ObjectElementVector()->data();
		EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
		result_SP = EidosValue_SP(object_result);
		
		if (p_preserve_order)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				EidosObjectElement *value = object_data[value_index];
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
			std::vector<EidosObjectElement*> dup_vec(object_data, object_data + x_count);
			
			std::sort(dup_vec.begin(), dup_vec.end());
			
			auto unique_iter = std::unique(dup_vec.begin(), dup_vec.end());
			size_t unique_count = unique_iter - dup_vec.begin();
			EidosObjectElement * const *dup_ptr = dup_vec.data();
			
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



// ************************************************************************************
//
//	math functions
//
#pragma mark -
#pragma mark Math functions
#pragma mark -


//	(numeric)abs(numeric x)
EidosValue_SP Eidos_ExecuteFunction_abs(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			// This is an overflow-safe version of:
			//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(llabs(x_value->IntAtIndex(0, nullptr)));
			
			int64_t operand = x_value->IntAtIndex(0, nullptr);
			
			// the absolute value of INT64_MIN cannot be represented in int64_t
			if (operand == INT64_MIN)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_abs): function abs() cannot take the absolute value of the most negative integer." << EidosTerminate(nullptr);
			
			int64_t abs_result = llabs(operand);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(abs_result));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// This is an overflow-safe version of:
				//int_result->set_int_no_check(llabs(int_vec[value_index]), value_index);
				
				int64_t operand = int_data[value_index];
				
				// the absolute value of INT64_MIN cannot be represented in int64_t
				if (operand == INT64_MIN)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_abs): function abs() cannot take the absolute value of the most negative integer." << EidosTerminate(nullptr);
				
				int64_t abs_result = llabs(operand);
				
				int_result->set_int_no_check(abs_result, value_index);
			}
		}
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fabs(x_value->FloatAtIndex(0, nullptr))));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				float_result->set_float_no_check(fabs(float_data[value_index]), value_index);
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)acos(numeric x)
EidosValue_SP Eidos_ExecuteFunction_acos(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(acos(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(acos(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)asin(numeric x)
EidosValue_SP Eidos_ExecuteFunction_asin(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(asin(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(asin(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)atan(numeric x)
EidosValue_SP Eidos_ExecuteFunction_atan(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(atan(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(atan(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)atan2(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_atan2(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	int y_count = y_value->Count();
	
	if (x_count != y_count)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_atan2): function atan2() requires arguments of equal length." << EidosTerminate(nullptr);
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int x_dimcount = x_value->DimensionCount();
	int y_dimcount = y_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(x_value, y_value));
	
	if ((x_dimcount > 1) && (y_dimcount > 1) && !EidosValue::MatchingDimensions(x_value, y_value))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_atan2): non-conformable array operands in atan2()." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(atan2(x_value->FloatAtIndex(0, nullptr), y_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(atan2(x_value->FloatAtIndex(value_index, nullptr), y_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	return result_SP;
}

//	(float)ceil(float x)
EidosValue_SP Eidos_ExecuteFunction_ceil(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(ceil(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(ceil(float_data[value_index]), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)cos(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cos(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cos(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(cos(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric)cumProduct(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cumProduct(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t product = 1;
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t operand = int_data[value_index];
				
				bool overflow = Eidos_mul_overflow(product, operand, &product);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cumProduct): integer multiplication overflow in function cumProduct()." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(product, value_index);
			}
		}
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double product = 1.0;
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				product *= float_data[value_index];
				float_result->set_float_no_check(product, value_index);
			}
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric)cumSum(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cumSum(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t sum = 0;
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t operand = int_data[value_index];
				
				bool overflow = Eidos_add_overflow(sum, operand, &sum);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cumSum): integer addition overflow in function cumSum()." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(sum, value_index);
			}
		}
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double sum = 0.0;
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				sum += float_data[value_index];
				float_result->set_float_no_check(sum, value_index);
			}
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)exp(numeric x)
EidosValue_SP Eidos_ExecuteFunction_exp(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(exp(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(exp(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)floor(float x)
EidosValue_SP Eidos_ExecuteFunction_floor(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(floor(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(floor(float_data[value_index]), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(integer)integerDiv(integer x, integer y)
EidosValue_SP Eidos_ExecuteFunction_integerDiv(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	int y_count = y_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int x_dimcount = x_value->DimensionCount();
	int y_dimcount = y_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(x_value, y_value));
	
	if ((x_dimcount > 1) && (y_dimcount > 1) && !EidosValue::MatchingDimensions(x_value, y_value))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): non-conformable array arguments to integerDiv()." << EidosTerminate(nullptr);
	
	if ((x_count == 1) && (y_count == 1))
	{
		int64_t int1 = x_value->IntAtIndex(0, nullptr);
		int64_t int2 = y_value->IntAtIndex(0, nullptr);
		
		if (int2 == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int1 / int2));
	}
	else
	{
		if (x_count == y_count)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int1 = int1_data[value_index];
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 / int2, value_index);
			}
		}
		else if (x_count == 1)
		{
			int64_t int1 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(y_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < y_count; ++value_index)
			{
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 / int2, value_index);
			}
		}
		else if (y_count == 1)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			int64_t int2 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			if (int2 == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
			
			// Special-case division by 2, since it is common
			// BCH 13 April 2017: Removing this optimization; it produces inconsistent behavior for negative numerators.
			// This optimization was originally committed on 2 March 2017; it was never in any release version of SLiM.
//			if (int2 == 2)
//			{
//				for (int value_index = 0; value_index < x_count; ++value_index)
//					int_result->set_int_no_check(int1_vec[value_index] >> 1, value_index);
//			}
//			else
//			{
				for (int value_index = 0; value_index < x_count; ++value_index)
					int_result->set_int_no_check(int1_data[value_index] / int2, value_index);
//			}
		}
		else	// if ((x_count != y_count) && (x_count != 1) && (y_count != 1))
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(nullptr);
		}
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	return result_SP;
}

//	(integer)integerMod(integer x, integer y)
EidosValue_SP Eidos_ExecuteFunction_integerMod(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	int y_count = y_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int x_dimcount = x_value->DimensionCount();
	int y_dimcount = y_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(x_value, y_value));
	
	if ((x_dimcount > 1) && (y_dimcount > 1) && !EidosValue::MatchingDimensions(x_value, y_value))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): non-conformable array arguments to integerMod()." << EidosTerminate(nullptr);
	
	if ((x_count == 1) && (y_count == 1))
	{
		int64_t int1 = x_value->IntAtIndex(0, nullptr);
		int64_t int2 = y_value->IntAtIndex(0, nullptr);
		
		if (int2 == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int1 % int2));
	}
	else
	{
		if (x_count == y_count)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int1 = int1_data[value_index];
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 % int2, value_index);
			}
		}
		else if (x_count == 1)
		{
			int64_t int1 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(y_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < y_count; ++value_index)
			{
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 % int2, value_index);
			}
		}
		else if (y_count == 1)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			int64_t int2 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			if (int2 == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
			
			// Special-case modulo by 2, since it is common
			// BCH 13 April 2017: Removing this optimization; it produces inconsistent behavior for negative numerators.
			// This optimization was originally committed on 2 March 2017; it was never in any release version of SLiM.
//			if (int2 == 2)
//			{
//				for (int value_index = 0; value_index < x_count; ++value_index)
//					int_result->set_int_no_check(int1_vec[value_index] & (int64_t)0x01, value_index);
//			}
//			else
//			{
				for (int value_index = 0; value_index < x_count; ++value_index)
					int_result->set_int_no_check(int1_data[value_index] % int2, value_index);
//			}
		}
		else	// if ((x_count != y_count) && (x_count != 1) && (y_count != 1))
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(nullptr);
		}
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	return result_SP;
}

//	(logical)isFinite(float x)
EidosValue_SP Eidos_ExecuteFunction_isFinite(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		if (x_value ->DimensionCount() == 1)
			result_SP = (std::isfinite(x_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{std::isfinite(x_value->FloatAtIndex(0, nullptr))});
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(std::isfinite(float_data[value_index]), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)isInfinite(float x)
EidosValue_SP Eidos_ExecuteFunction_isInfinite(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		if (x_value ->DimensionCount() == 1)
			result_SP = (std::isinf(x_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{std::isinf(x_value->FloatAtIndex(0, nullptr))});
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(std::isinf(float_data[value_index]), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)isNAN(float x)
EidosValue_SP Eidos_ExecuteFunction_isNAN(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		if (x_value ->DimensionCount() == 1)
			result_SP = (std::isnan(x_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{std::isnan(x_value->FloatAtIndex(0, nullptr))});
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(std::isnan(float_data[value_index]), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)log(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(log(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)log10(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log10(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log10(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(log10(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)log2(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log2(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log2(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(log2(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric$)product(numeric x)
EidosValue_SP Eidos_ExecuteFunction_product(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t product = 1;
			double product_d = 1.0;
			bool fits_in_integer = true;
			
			// We do a tricky thing here.  We want to try to compute in integer, but switch to float if we overflow.
			// If we do overflow, we want to minimize numerical error by accumulating in integer for as long as we
			// can, and then throwing the integer accumulator over into the float accumulator only when it is about
			// to overflow.  We perform both computations in parallel, and use integer for the result if we can.
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t old_product = product;
				int64_t temp = int_data[value_index];
				
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
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double product = 1;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				product *= float_data[value_index];
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(product));
		}
	}
	
	return result_SP;
}

//	(float)round(float x)
EidosValue_SP Eidos_ExecuteFunction_round(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(round(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(round(float_data[value_index]), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(*)setDifference(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setDifference(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setDifference): function setDifference() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setDifference): function setDifference() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if (x_count == 0)
	{
		// If x is empty, the difference is the empty set
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if (y_count == 0)
	{
		// If y is empty, the difference is x, uniqued
		result_SP = UniqueEidosValue(x_value, false, true);
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_data1 = y_value->LogicalVector()->data();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_data0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (!logical_data0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (logical_data0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_data1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (!logical_data1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (logical_data1[value_index])
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
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
		else if (containsT0 && !containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else if (containsF0 && !containsF1)
			result_SP = gStaticEidosValue_LogicalF;
		else
			result_SP = gStaticEidosValue_Logical_ZeroVec;
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = gStaticEidosValue_Integer_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = gStaticEidosValue_Float_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = x_value->StringAtIndex(0, nullptr), string1 = y_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = gStaticEidosValue_String_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = x_value->NewMatchingType();
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if (x_count == 1)
	{
		// If any element in y matches the element in x, the result is an empty vector
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int_data = y_value->IntVector()->data();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
				if (int0 == int_data[value_index])
					return gStaticEidosValue_Integer_ZeroVec;
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr);
			const double *float_data = y_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
			{
				double float1 = float_data[value_index];
				
				if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
					return gStaticEidosValue_Float_ZeroVec;
			}
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = x_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *y_value->StringVector();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
				if (string0 == string_vec[value_index])
					return gStaticEidosValue_String_ZeroVec;
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = x_value->ObjectElementAtIndex(0, nullptr);
			EidosObjectElement * const *object_vec = y_value->ObjectElementVector()->data();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
				if (obj0 == object_vec[value_index])
					return x_value->NewMatchingType();
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if (y_count == 1)
	{
		// The result is x uniqued, minus the element in y if it matches
		result_SP = UniqueEidosValue(x_value, true, true);
		
		int result_count = result_SP->Count();
		
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int1 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_vec = result_SP->IntVector_Mutable();
			const int64_t *int_data = int_vec->data();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				if (int1 == int_data[value_index])
				{
					int_vec->erase_index(value_index);
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float1 = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_vec = result_SP->FloatVector_Mutable();
			double *float_data = float_vec->data();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
			{
				double float0 = float_data[value_index];
				
				if ((std::isnan(float1) && std::isnan(float0)) || (float1 == float0))
				{
					float_vec->erase_index(value_index);
					break;
				}
			}
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string1 = y_value->StringAtIndex(0, nullptr);
			std::vector<std::string> &string_vec = *result_SP->StringVector_Mutable();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				if (string1 == string_vec[value_index])
				{
					string_vec.erase(string_vec.begin() + value_index);
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			EidosValue_Object_vector *object_element_vec = result_SP->ObjectElementVector_Mutable();
			EidosObjectElement * const *object_element_data = object_element_vec->data();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				if (obj1 == object_element_data[value_index])
				{
					object_element_vec->erase_index(value_index);
					break;
				}
		}
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				int64_t value = int_data0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == int_data1[value_index1])
						break;
				
				if (value_index1 < y_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == int_data0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				int_result->push_int(value);
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data0 = x_value->FloatVector()->data();
			const double *float_data1 = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				double value0 = float_data0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
				{
					double value1 = float_data1[value_index1];
					
					if ((std::isnan(value0) && std::isnan(value1)) || (value0 == value1))
						break;
				}
				
				if (value_index1 < y_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
				{
					double value_scan = float_data0[scan_index];
					
					if ((std::isnan(value0) && std::isnan(value_scan)) || (value0 == value_scan))
						break;
				}
				
				if (scan_index < value_index0)
					continue;
				
				float_result->push_float(value0);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *y_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == string_vec1[value_index1])
						break;
				
				if (value_index1 < y_count)
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
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObjectElement * const *object_vec0 = x_value->ObjectElementVector()->data();
			EidosObjectElement * const *object_vec1 = y_value->ObjectElementVector()->data();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				EidosObjectElement *value = object_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == object_vec1[value_index1])
						break;
				
				if (value_index1 < y_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == object_vec0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				object_result->push_object_element_CRR(value);
			}
		}
	}
	
	return result_SP;
}

//	(*)setIntersection(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setIntersection(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setIntersection): function setIntersection() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setIntersection): function setIntersection() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if ((x_count == 0) || (y_count == 0))
	{
		// If either argument is empty, the intersection is the empty set
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_data1 = y_value->LogicalVector()->data();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_data0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (!logical_data0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (logical_data0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_data1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (!logical_data1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (logical_data1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		if (containsF0 && containsT0 && containsF1 && containsT1)
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
		else if (containsF0 && containsF1)
			result_SP = gStaticEidosValue_LogicalF;
		else if (containsT0 && containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else
			result_SP = gStaticEidosValue_Logical_ZeroVec;
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
			else
				result_SP = gStaticEidosValue_Integer_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
			else
				result_SP = gStaticEidosValue_Float_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = x_value->StringAtIndex(0, nullptr), string1 = y_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
			else
				result_SP = gStaticEidosValue_String_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
			else
				result_SP = x_value->NewMatchingType();
		}
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// If either argument is singleton (but not both), handle that case with a fast check
		if (x_count == 1)
		{
			std::swap(x_count, y_count);
			std::swap(x_value, y_value);
		}
		
		// now x_count > 1, y_count == 1
		bool found_match = false;
		
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t value = y_value->IntAtIndex(0, nullptr);
			const int64_t *int_data = x_value->IntVector()->data();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
				if (value == int_data[scan_index])
				{
					found_match = true;
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double value0 = y_value->FloatAtIndex(0, nullptr);
			const double *float_data = x_value->FloatVector()->data();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
			{
				double value1 = float_data[scan_index];
				
				if ((std::isnan(value0) && std::isnan(value1)) || (value0 == value1))
				{
					found_match = true;
					break;
				}
			}
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string value = y_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *x_value->StringVector();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
				if (value == string_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value = y_value->ObjectElementAtIndex(0, nullptr);
			EidosObjectElement * const *object_vec = x_value->ObjectElementVector()->data();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
				if (value == object_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		
		if (found_match)
			result_SP = y_value->CopyValues();
		else
			result_SP = x_value->NewMatchingType();
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				int64_t value = int_data0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == int_data1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == int_data0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							int_result->push_int(value);
						break;
					}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data0 = x_value->FloatVector()->data();
			const double *float_data1 = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				double value0 = float_data0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
				{
					double value1 = float_data1[value_index1];
					
					if ((std::isnan(value0) && std::isnan(value1)) || (value0 == value1))
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							double value_scan = float_data0[scan_index];
							
							if ((std::isnan(value0) && std::isnan(value_scan)) || (value0 == value_scan))
								break;
						}
						
						if (scan_index == value_index0)
							float_result->push_float(value0);
						break;
					}
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *y_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
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
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObjectElement * const *object_vec0 = x_value->ObjectElementVector()->data();
			EidosObjectElement * const *object_vec1 = y_value->ObjectElementVector()->data();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				EidosObjectElement *value = object_vec0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
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
							object_result->push_object_element_CRR(value);
						break;
					}
			}
		}
	}
	
	return result_SP;
}

//	(*)setSymmetricDifference(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setSymmetricDifference(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setSymmetricDifference): function setSymmetricDifference() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setSymmetricDifference): function setSymmetricDifference() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if (x_count + y_count == 0)
	{
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if ((x_count == 1) && (y_count == 0))
	{
		result_SP = x_value->CopyValues();
	}
	else if ((x_count == 0) && (y_count == 1))
	{
		result_SP = y_value->CopyValues();
	}
	else if (x_count == 0)
	{
		result_SP = UniqueEidosValue(y_value, false, true);
	}
	else if (y_count == 0)
	{
		result_SP = UniqueEidosValue(x_value, false, true);
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_data1 = y_value->LogicalVector()->data();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_data0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (!logical_data0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (logical_data0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_data1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (!logical_data1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (logical_data1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		if ((containsF0 != containsF1) && (containsT0 != containsT1))
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
		else if ((containsF0 == containsF1) && (containsT0 == containsT1))
			result_SP = gStaticEidosValue_Logical_ZeroVec;
		else if (containsT0 != containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else // (containsF0 != containsF1)
			result_SP = gStaticEidosValue_LogicalF;
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = gStaticEidosValue_Integer_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{int0, int1});
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = gStaticEidosValue_Float_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{float0, float1});
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = x_value->StringAtIndex(0, nullptr), string1 = y_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = gStaticEidosValue_String_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{string0, string1});
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = x_value->NewMatchingType();
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector({obj0, obj1}, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// We have one value that is a singleton, one that is a vector.  We'd like this case to be fast,
		// so that addition of a single element to a set is a fast operation.
		if (x_count == 1)
		{
			std::swap(x_count, y_count);
			std::swap(x_value, y_value);
		}
		
		// now x_count > 1, y_count == 1
		result_SP = UniqueEidosValue(x_value, true, true);
		
		int result_count = result_SP->Count();
		
		// result_SP is modifiable and is guaranteed to be a vector, so now the result is x uniqued,
		// minus the element in y if it matches, but plus the element in y if it does not match
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int1 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_vec = result_SP->IntVector_Mutable();
			const int64_t *int_data = int_vec->data();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (int1 == int_data[value_index])
					break;
			
			if (value_index == result_count)
				int_vec->push_int(int1);
			else
				int_vec->erase_index(value_index);
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float1 = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_vec = result_SP->FloatVector_Mutable();
			double *float_data = float_vec->data();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
			{
				double float0 = float_data[value_index];
				
				if ((std::isnan(float0) && std::isnan(float1)) || (float1 == float0))
					break;
			}
			
			if (value_index == result_count)
				float_vec->push_float(float1);
			else
				float_vec->erase_index(value_index);
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string1 = y_value->StringAtIndex(0, nullptr);
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
			EidosObjectElement *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			EidosValue_Object_vector *object_element_vec = result_SP->ObjectElementVector_Mutable();
			EidosObjectElement * const *object_element_data = object_element_vec->data();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (obj1 == object_element_data[value_index])
					break;
			
			if (value_index == result_count)
				object_element_vec->push_object_element_CRR(obj1);
			else
				object_element_vec->erase_index(value_index);
		}
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both.  Loop through x adding
		// unique values not in y, then loop through y adding unique values not in x.
		int value_index0, value_index1, scan_index;
		
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				int64_t value = int_data0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == int_data1[value_index1])
						break;
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == int_data0[scan_index])
							break;
					
					if (scan_index == value_index0)
						int_result->push_int(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				int64_t value = int_data1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
					if (value == int_data0[value_index0])
						break;
				
				if (value_index0 == x_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == int_data1[scan_index])
							break;
					
					if (scan_index == value_index1)
						int_result->push_int(value);
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_vec0 = x_value->FloatVector()->data();
			const double *float_vec1 = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				double value0 = float_vec0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
				{
					double float1 = float_vec1[value_index1];
					
					if ((std::isnan(value0) && std::isnan(float1)) || (value0 == float1))
						break;
				}
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
					{
						double value_scan = float_vec0[scan_index];
						
						if ((std::isnan(value0) && std::isnan(value_scan)) || (value0 == value_scan))
							break;
					}
					
					if (scan_index == value_index0)
						float_result->push_float(value0);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				double value1 = float_vec1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
				{
					double value0 = float_vec0[value_index0];
					
					if ((std::isnan(value1) && std::isnan(value0)) || (value1 == value0))
						break;
				}
				
				if (value_index0 == x_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
					{
						double value_scan = float_vec1[scan_index];
						
						if ((std::isnan(value1) && std::isnan(value_scan)) || (value1 == value_scan))
							break;
					}
					
					if (scan_index == value_index1)
						float_result->push_float(value1);
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *y_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == string_vec1[value_index1])
						break;
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == string_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						string_result->PushString(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				std::string value = string_vec1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
					if (value == string_vec0[value_index0])
						break;
				
				if (value_index0 == x_count)
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
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObjectElement * const *object_vec0 = x_value->ObjectElementVector()->data();
			EidosObjectElement * const *object_vec1 = y_value->ObjectElementVector()->data();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				EidosObjectElement *value = object_vec0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == object_vec1[value_index1])
						break;
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == object_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						object_result->push_object_element_CRR(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				EidosObjectElement *value = object_vec1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
					if (value == object_vec0[value_index0])
						break;
				
				if (value_index0 == x_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == object_vec1[scan_index])
							break;
					
					if (scan_index == value_index1)
						object_result->push_object_element_CRR(value);
				}
			}
		}
	}
	
	return result_SP;
}

//	(*)setUnion(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setUnion(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setUnion): function setUnion() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosObjectClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidos_UndefinedClassObject) && (class1 != gEidos_UndefinedClassObject))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setUnion): function setUnion() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if (x_count + y_count == 0)
	{
		if (class1 && (class1 != gEidos_UndefinedClassObject))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if ((x_count == 1) && (y_count == 0))
	{
		result_SP = x_value->CopyValues();
	}
	else if ((x_count == 0) && (y_count == 1))
	{
		result_SP = y_value->CopyValues();
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_vec0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_vec1 = y_value->LogicalVector()->data();
		bool containsF = false, containsT = false;
		
		if (((x_count > 0) && logical_vec0[0]) || ((y_count > 0) && logical_vec1[0]))
		{
			// We have a true value; look for a false value
			containsT = true;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (!logical_vec0[value_index])
				{
					containsF = true;
					break;
				}
			
			if (!containsF)
				for (int value_index = 0; value_index < y_count; ++value_index)
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
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (logical_vec0[value_index])
				{
					containsT = true;
					break;
				}
			
			if (!containsT)
				for (int value_index = 0; value_index < y_count; ++value_index)
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
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
	}
	else if (x_count == 0)
	{
		// x is zero-length, y is >1, so we just need to unique y
		result_SP = UniqueEidosValue(y_value, false, true);
	}
	else if (y_count == 0)
	{
		// y is zero-length, x is >1, so we just need to unique x
		result_SP = UniqueEidosValue(x_value, false, true);
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// Make a bit of an effort to produce a singleton result, while handling the singleton/singleton case
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{int0, int1});
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{float0, float1});
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string string0 = x_value->StringAtIndex(0, nullptr), string1 = y_value->StringAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{string0, string1});
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector({obj0, obj1}, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// We have one value that is a singleton, one that is a vector.  We'd like this case to be fast,
		// so that addition of a single element to a set is a fast operation.
		if (x_count == 1)
		{
			std::swap(x_count, y_count);
			std::swap(x_value, y_value);
		}
		
		// now x_count > 1, y_count == 1
		result_SP = UniqueEidosValue(x_value, true, true);
		
		int result_count = result_SP->Count();
		
		// result_SP is modifiable and is guaranteed to be a vector, so now add y if necessary using the fast APIs
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t value = y_value->IntAtIndex(0, nullptr);
			const int64_t *int_data = result_SP->IntVector()->data();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == int_data[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				result_SP->IntVector_Mutable()->push_int(value);
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double value1 = y_value->FloatAtIndex(0, nullptr);
			const double *float_data = result_SP->FloatVector()->data();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				double value0 = float_data[scan_index];
				
				if ((std::isnan(value1) && std::isnan(value0)) || (value1 == value0))
					break;
			}
			
			if (scan_index == result_count)
				result_SP->FloatVector_Mutable()->push_float(value1);
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			std::string value = y_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *result_SP->StringVector();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == string_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				result_SP->StringVector_Mutable()->emplace_back(value);
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value = y_value->ObjectElementAtIndex(0, nullptr);
			EidosObjectElement * const *object_vec = result_SP->ObjectElementVector()->data();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == object_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				result_SP->ObjectElementVector_Mutable()->push_object_element_CRR(value);
		}
	}
	else
	{
		// We have two arguments which are both vectors of >1 value, so this is the base case.  We construct
		// a new EidosValue containing all elements from both arguments, and then call UniqueEidosValue() to unique it.
		// This code might look slow, but really the uniquing is O(N^2) and everything else is O(N), so since
		// we are in the vector/vector case here, it really isn't worth worrying about optimizing the O(N) part.
		result_SP = ConcatenateEidosValues(p_arguments, false, false);	// no NULL, no VOID
		result_SP = UniqueEidosValue(result_SP.get(), false, true);
	}
	
	return result_SP;
}

//	(float)sin(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sin(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sin(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(sin(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)sqrt(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sqrt(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sqrt(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(sqrt(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric$)sum(lif x)
EidosValue_SP Eidos_ExecuteFunction_sum(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t sum = 0;
			double sum_d = 0;
			bool fits_in_integer = true;
			
			// We do a tricky thing here.  We want to try to compute in integer, but switch to float if we overflow.
			// If we do overflow, we want to minimize numerical error by accumulating in integer for as long as we
			// can, and then throwing the integer accumulator over into the float accumulator only when it is about
			// to overflow.  We perform both computations in parallel, and use integer for the result if we can.
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t old_sum = sum;
				int64_t temp = int_data[value_index];
				
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
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double sum = 0;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				sum += float_data[value_index];
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum));
		}
	}
	else if (x_type == EidosValueType::kValueLogical)
	{
		// EidosValue_Logical does not have a singleton subclass, so we can always use the fast API
		const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
		int64_t sum = 0;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			sum += logical_data[value_index];
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(sum));
	}
	
	return result_SP;
}

//	(float$)sumExact(float x)
EidosValue_SP Eidos_ExecuteFunction_sumExact(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
	}
	else
	{
		// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		double sum = Eidos_ExactSum(float_data, x_count);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum));
	}
	
	return result_SP;
}

//	(float)tan(numeric x)
EidosValue_SP Eidos_ExecuteFunction_tan(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(tan(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(tan(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)trunc(float x)
EidosValue_SP Eidos_ExecuteFunction_trunc(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(trunc(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(trunc(float_data[value_index]), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}


// ************************************************************************************
//
//	statistics functions
//
#pragma mark -
#pragma mark Statistics functions
#pragma mark -


//	(float$)cor(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_cor(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	int count = x_value->Count();
	
	if (x_value->IsArray() || y_value->IsArray())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): function cor() does not currently support matrix/array arguments." << EidosTerminate(nullptr);
	if (count != y_value->Count())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): function cor() requires that x and y be the same size." << EidosTerminate(nullptr);
	
	if (count > 1)
	{
		// calculate means
		double mean_x = 0, mean_y = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			mean_x += x_value->FloatAtIndex(value_index, nullptr);
			mean_y += y_value->FloatAtIndex(value_index, nullptr);
		}
		
		mean_x /= count;
		mean_y /= count;
		
		// calculate sums of squares and products of differences
		double ss_x = 0, ss_y = 0, diff_prod = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			double dx = x_value->FloatAtIndex(value_index, nullptr) - mean_x;
			double dy = y_value->FloatAtIndex(value_index, nullptr) - mean_y;
			
			ss_x += dx * dx;
			ss_y += dy * dy;
			diff_prod += dx * dy;
		}
		
		// calculate correlation
		double cor = diff_prod / (sqrt(ss_x) * sqrt(ss_y));
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cor));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}

//	(float$)cov(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_cov(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	int count = x_value->Count();
	
	if (x_value->IsArray() || y_value->IsArray())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): function cov() does not currently support matrix/array arguments." << EidosTerminate(nullptr);
	if (count != y_value->Count())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): function cov() requires that x and y be the same size." << EidosTerminate(nullptr);
	
	if (count > 1)
	{
		// calculate means
		double mean_x = 0, mean_y = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			mean_x += x_value->FloatAtIndex(value_index, nullptr);
			mean_y += y_value->FloatAtIndex(value_index, nullptr);
		}
		
		mean_x /= count;
		mean_y /= count;
		
		// calculate covariance
		double cov = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			double temp_x = (x_value->FloatAtIndex(value_index, nullptr) - mean_x);
			double temp_y = (y_value->FloatAtIndex(value_index, nullptr) - mean_y);
			cov += temp_x * temp_y;
		}
		
		cov = cov / (count - 1);
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cov));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}

//	(+$)max(+ x, ...)
EidosValue_SP Eidos_ExecuteFunction_max(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValueType x_type = p_arguments[0].get()->Type();
	
	// check the types of ellipsis arguments and find the first nonempty argument
	int first_nonempty_argument = -1;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		EidosValueType arg_type = arg_value->Type();
		
		if (arg_type != x_type)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_max): function max() requires all arguments to be the same type." << EidosTerminate(nullptr);
		
		if (first_nonempty_argument == -1)
		{
			int arg_count = arg_value->Count();
			
			if (arg_count > 0)
				first_nonempty_argument = arg_index;
		}
	}
	
	if (first_nonempty_argument == -1)
	{
		// R uses -Inf or +Inf for max/min of a numeric vector, but we want to be consistent between integer and float, and we
		// want to return an integer value for integer arguments, and there is no integer -Inf/+Inf, so we return NULL.  Note
		// this means that, unlike R, min() and max() in Eidos are not transitive; min(a, min(b)) != min(a, b) in general.  We
		// could fix that by returning NULL whenever any of the arguments are zero-length, but that does not seem desirable.
		result_SP = gStaticEidosValueNULL;
	}
	else if (x_type == EidosValueType::kValueLogical)
	{
		// For logical, we can just scan for a T, in which the result is T, otherwise it is F
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				if (arg_value->LogicalAtIndex(0, nullptr) == true)
					return gStaticEidosValue_LogicalT;
			}
			else
			{
				const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
					if (logical_data[value_index] == true)
						return gStaticEidosValue_LogicalT;
			}
		}
		
		result_SP = gStaticEidosValue_LogicalF;
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		int64_t max = p_arguments[first_nonempty_argument]->IntAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntAtIndex(0, nullptr);
				if (max < temp)
					max = temp;
			}
			else
			{
				const int64_t *int_data = arg_value->IntVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (max < temp)
						max = temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(max));
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		double max = p_arguments[first_nonempty_argument]->FloatAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatAtIndex(0, nullptr);
				
				// if there is a NAN the result is always NAN, so we don't need to scan further
				if (std::isnan(temp))
					return gStaticEidosValue_FloatNAN;
				
				if (max < temp)
					max = temp;
			}
			else
			{
				const double *float_data = arg_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					double temp = float_data[value_index];
					
					// if there is a NAN the result is always NAN, so we don't need to scan further
					if (std::isnan(temp))
						return gStaticEidosValue_FloatNAN;
					
					if (max < temp)
						max = temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(max));
	}
	else if (x_type == EidosValueType::kValueString)
	{
		std::string max = p_arguments[first_nonempty_argument]->StringAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				const std::string &temp = arg_value->StringAtIndex(0, nullptr);
				if (max < temp)
					max = temp;
			}
			else
			{
				const std::vector<std::string> &string_vec = *arg_value->StringVector();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
					if (max < temp)
						max = temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(max));
	}
	
	return result_SP;
}

//	(float$)mean(lif x)
EidosValue_SP Eidos_ExecuteFunction_mean(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
	}
	else
	{
		EidosValueType x_type = x_value->Type();
		double sum = 0;
		
#if EIDOS_HAS_OVERFLOW_BUILTINS
		if (x_type == EidosValueType::kValueInt)
		{
			// Accelerated integer case
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t sum_i = 0;
			
			// We do a tricky thing here.  We want to try to compute in integer, but switch to float if we overflow.
			// If we do overflow, we want to minimize numerical error by accumulating in integer for as long as we
			// can, and then throwing the integer accumulator over into the float accumulator only when it is about
			// to overflow.  We perform both computations in parallel, and use integer for the result if we can.
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t old_sum = sum_i;
				int64_t temp = int_data[value_index];
				
				bool overflow = Eidos_add_overflow(old_sum, temp, &sum_i);
				
				// switch to float computation on overflow, and accumulate in the float sum just before overflow
				if (overflow)
				{
					sum += old_sum;
					sum_i = temp;		// start integer accumulation again from 0 until it overflows again
				}
			}
			
			sum += sum_i;			// add in whatever integer accumulation has not overflowed
		}
#else
		if (x_type == EidosValueType::kValueInt)
		{
			// Accelerated integer case
			const int64_t *int_data = x_value->IntVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				sum += int_data[value_index];
		}
#endif
		else if (x_type == EidosValueType::kValueFloat)
		{
			// Accelerated float case
			const double *float_data = x_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				sum += float_data[value_index];
		}
		else if (x_type == EidosValueType::kValueLogical)
		{
			// Accelerated logical case
			const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
			int64_t logical_sum = 0;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_sum += logical_data[value_index];
			
			sum = logical_sum;	// avoid floating-point roundoff issues
		}
		else
		{
			// General case, never hit
			for (int value_index = 0; value_index < x_count; ++value_index)
				sum += x_value->FloatAtIndex(value_index, nullptr);
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum / x_count));
	}
	
	return result_SP;
}

//	(+$)min(+ x, ...)
EidosValue_SP Eidos_ExecuteFunction_min(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValueType x_type = p_arguments[0].get()->Type();
	
	// check the types of ellipsis arguments and find the first nonempty argument
	int first_nonempty_argument = -1;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		EidosValueType arg_type = arg_value->Type();
		
		if (arg_type != x_type)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_min): function min() requires all arguments to be the same type." << EidosTerminate(nullptr);
		
		if (first_nonempty_argument == -1)
		{
			int arg_count = arg_value->Count();
			
			if (arg_count > 0)
				first_nonempty_argument = arg_index;
		}
	}
	
	if (first_nonempty_argument == -1)
	{
		// R uses -Inf or +Inf for max/min of a numeric vector, but we want to be consistent between integer and float, and we
		// want to return an integer value for integer arguments, and there is no integer -Inf/+Inf, so we return NULL.  Note
		// this means that, unlike R, min() and max() in Eidos are not transitive; min(a, min(b)) != min(a, b) in general.  We
		// could fix that by returning NULL whenever any of the arguments are zero-length, but that does not seem desirable.
		result_SP = gStaticEidosValueNULL;
	}
	else if (x_type == EidosValueType::kValueLogical)
	{
		// For logical, we can just scan for a F, in which the result is F, otherwise it is T
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				if (arg_value->LogicalAtIndex(0, nullptr) == false)
					return gStaticEidosValue_LogicalF;
			}
			else
			{
				const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
					if (logical_data[value_index] == false)
						return gStaticEidosValue_LogicalF;
			}
		}
		
		result_SP = gStaticEidosValue_LogicalT;
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		int64_t min = p_arguments[first_nonempty_argument]->IntAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntAtIndex(0, nullptr);
				if (min > temp)
					min = temp;
			}
			else
			{
				const int64_t *int_data = arg_value->IntVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (min > temp)
						min = temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(min));
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		double min = p_arguments[first_nonempty_argument]->FloatAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatAtIndex(0, nullptr);
				
				// if there is a NAN the result is always NAN, so we don't need to scan further
				if (std::isnan(temp))
					return gStaticEidosValue_FloatNAN;
				
				if (min > temp)
					min = temp;
			}
			else
			{
				const double *float_data = arg_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					double temp = float_data[value_index];
					
					// if there is a NAN the result is always NAN, so we don't need to scan further
					if (std::isnan(temp))
						return gStaticEidosValue_FloatNAN;
					
					if (min > temp)
						min = temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(min));
	}
	else if (x_type == EidosValueType::kValueString)
	{
		std::string min = p_arguments[first_nonempty_argument]->StringAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				const std::string &temp = arg_value->StringAtIndex(0, nullptr);
				if (min > temp)
					min = temp;
			}
			else
			{
				const std::vector<std::string> &string_vec = *arg_value->StringVector();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
					if (min > temp)
						min = temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(min));
	}
	
	return result_SP;
}

//	(+)pmax(+ x, + y)
EidosValue_SP Eidos_ExecuteFunction_pmax(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmax): function pmax() requires arguments x and y to be the same type." << EidosTerminate(nullptr);
	if ((x_count != y_count) && (x_count != 1) && (y_count != 1))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmax): function pmax() requires arguments x and y to be of equal length, or either x or y must be a singleton." << EidosTerminate(nullptr);
	
	// Since we want this operation to be *parallel*, we are stricter about dimensionality than most binary operations; we require the same
	// dimensionality unless we have a vector singleton vs. (any) non-singleton pairing, in which case the non-singleton's dimensions are used
	if (((x_count != 1) && (y_count != 1)) ||							// dims must match if both are non-singleton
		 ((x_count == 1) && (y_count == 1)))							// dims must match if both are singleton
	{
		if (!EidosValue::MatchingDimensions(x_value, y_value))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmax): function pmax() requires arguments x and y to be of the same vector/matrix/array dimensions, unless either x or y (but not both) is a singleton ." << EidosTerminate(nullptr);
	}
	else if (((x_count == 1) && (x_value->DimensionCount() != 1)) ||	// if just one is singleton, it must be a vector
			 ((y_count == 1) && (y_value->DimensionCount() != 1)))		// if just one is singleton, it must be a vector
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmax): function pmax() requires that if arguments x and y involve a singleton-to-non-singleton comparison, the singleton is a vector (not a matrix or array)." << EidosTerminate(nullptr);
	}
	
	if (x_type == EidosValueType::kValueNULL)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// Handle the singleton case separately so we can handle the vector case quickly
		
		// if there is a NAN the result is always NAN
		if (x_type == EidosValueType::kValueFloat)
			if (std::isnan(x_value->FloatAtIndex(0, nullptr)) || std::isnan(y_value->FloatAtIndex(0, nullptr)))
				return gStaticEidosValue_FloatNAN;
		
		if (CompareEidosValues(*x_value, 0, *y_value, 0, EidosComparisonOperator::kLess, nullptr))
			result_SP = y_value->CopyValues();
		else
			result_SP = x_value->CopyValues();
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// One argument, but not both, is singleton; get the singleton value and use fast access on the vector
		
		// First, swap as needed to make y be the singleton
		if (x_count == 1)
		{
			std::swap(x_value, y_value);
			std::swap(x_count, y_count);
		}
		
		// Then split up by type
		if (x_type == EidosValueType::kValueLogical)
		{
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			eidos_logical_t y_singleton_value = y_value->LogicalAtIndex(0, nullptr);
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] || y_singleton_value, value_index); // || is logical max
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int0_data = x_value->IntVector()->data();
			int64_t y_singleton_value = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(std::max(int0_data[value_index], y_singleton_value), value_index);
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float0_data = x_value->FloatVector()->data();
			double y_singleton_value = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				if (std::isnan(float0_data[value_index]) || std::isnan(y_singleton_value))
					float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), value_index);
				else
					float_result->set_float_no_check(std::max(float0_data[value_index], y_singleton_value), value_index);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::string &y_singleton_value = y_value->StringAtIndex(0, nullptr);
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(std::max(string0_vec[value_index], y_singleton_value));
		}
	}
	else
	{
		// We know the type is not NULL or object, that x_count == y_count, and that they are not singletons; we split up by type and handle fast
		if (x_type == EidosValueType::kValueLogical)
		{
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			const eidos_logical_t *logical1_data = y_value->LogicalVector()->data();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] || logical1_data[value_index], value_index); // || is logical max
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int0_data = x_value->IntVector()->data();
			const int64_t *int1_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(std::max(int0_data[value_index], int1_data[value_index]), value_index);
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float0_data = x_value->FloatVector()->data();
			const double *float1_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				if (std::isnan(float0_data[value_index]) || std::isnan(float1_data[value_index]))
					float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), value_index);
				else
					float_result->set_float_no_check(std::max(float0_data[value_index], float1_data[value_index]), value_index);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::vector<std::string> &string1_vec = *y_value->StringVector();
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(std::max(string0_vec[value_index], string1_vec[value_index]));
		}
	}
	
	// Either x and y have the same dimensionality (so it doesn't matter which we copy from), or x is the non-singleton
	// and y is the singleton (due to the swap call above).  So this is the correct choice for all of the cases above.
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(+)pmin(+ x, + y)
EidosValue_SP Eidos_ExecuteFunction_pmin(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmin): function pmin() requires arguments x and y to be the same type." << EidosTerminate(nullptr);
	if ((x_count != y_count) && (x_count != 1) && (y_count != 1))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmin): function pmin() requires arguments x and y to be of equal length, or either x or y must be a singleton." << EidosTerminate(nullptr);
	
	// Since we want this operation to be *parallel*, we are stricter about dimensionality than most binary operations; we require the same
	// dimensionality unless we have a vector singleton vs. (any) non-singleton pairing, in which the non-singleton's dimensions are used
	if (((x_count != 1) && (y_count != 1)) ||							// dims must match if both are non-singleton
		 ((x_count == 1) && (y_count == 1)))							// dims must match if both are singleton
	{
		if (!EidosValue::MatchingDimensions(x_value, y_value))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmin): function pmin() requires arguments x and y to be of the same vector/matrix/array dimensions, unless either x or y (but not both) is a singleton ." << EidosTerminate(nullptr);
	}
	else if (((x_count == 1) && (x_value->DimensionCount() != 1)) ||	// if just one is singleton, it must be a vector
			 ((y_count == 1) && (y_value->DimensionCount() != 1)))		// if just one is singleton, it must be a vector
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pmin): function pmin() requires that if arguments x and y involve a singleton-to-non-singleton comparison, the singleton is a vector (not a matrix or array)." << EidosTerminate(nullptr);
	}
	
	if (x_type == EidosValueType::kValueNULL)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// Handle the singleton case separately so we can handle the vector case quickly
		
		// if there is a NAN the result is always NAN
		if (x_type == EidosValueType::kValueFloat)
			if (std::isnan(x_value->FloatAtIndex(0, nullptr)) || std::isnan(y_value->FloatAtIndex(0, nullptr)))
				return gStaticEidosValue_FloatNAN;
		
		if (CompareEidosValues(*x_value, 0, *y_value, 0, EidosComparisonOperator::kGreater, nullptr))
			result_SP = y_value->CopyValues();
		else
			result_SP = x_value->CopyValues();
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// One argument, but not both, is singleton; get the singleton value and use fast access on the vector
		
		// First, swap as needed to make y be the singleton
		if (x_count == 1)
		{
			std::swap(x_value, y_value);
			std::swap(x_count, y_count);
		}
		
		// Then split up by type
		if (x_type == EidosValueType::kValueLogical)
		{
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			eidos_logical_t y_singleton_value = y_value->LogicalAtIndex(0, nullptr);
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] && y_singleton_value, value_index); // && is logical min
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int0_data = x_value->IntVector()->data();
			int64_t y_singleton_value = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(std::min(int0_data[value_index], y_singleton_value), value_index);
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float0_data = x_value->FloatVector()->data();
			double y_singleton_value = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				if (std::isnan(float0_data[value_index]) || std::isnan(y_singleton_value))
					float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), value_index);
				else
					float_result->set_float_no_check(std::min(float0_data[value_index], y_singleton_value), value_index);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::string &y_singleton_value = y_value->StringAtIndex(0, nullptr);
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(std::min(string0_vec[value_index], y_singleton_value));
		}
	}
	else
	{
		// We know the type is not NULL or object, that x_count == y_count, and that they are not singletons; we split up by type and handle fast
		if (x_type == EidosValueType::kValueLogical)
		{
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			const eidos_logical_t *logical1_data = y_value->LogicalVector()->data();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] && logical1_data[value_index], value_index); // && is logical min
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int0_data = x_value->IntVector()->data();
			const int64_t *int1_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(std::min(int0_data[value_index], int1_data[value_index]), value_index);
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float0_data = x_value->FloatVector()->data();
			const double *float1_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				if (std::isnan(float0_data[value_index]) || std::isnan(float1_data[value_index]))
					float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), value_index);
				else
					float_result->set_float_no_check(std::min(float0_data[value_index], float1_data[value_index]), value_index);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::vector<std::string> &string1_vec = *y_value->StringVector();
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(std::min(string0_vec[value_index], string1_vec[value_index]));
		}
	}
	
	// Either x and y have the same dimensionality (so it doesn't matter which we copy from), or x is the non-singleton
	// and y is the singleton (due to the swap call above).  So this is the correct choice for all of the cases above.
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)quantile(numeric x, [Nf probs = NULL])
EidosValue_SP Eidos_ExecuteFunction_quantile(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	EidosValue *probs_value = p_arguments[1].get();
	int probs_count = probs_value->Count();
	
	if (x_count == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): function quantile() requires x to have length greater than 0." << EidosTerminate(nullptr);
	
	// get the probabilities; this is mostly so we don't have to special-case NULL below, but we also pre-check the probabilities here
	std::vector<double> probs;
	
	if (probs_value->Type() == EidosValueType::kValueNULL)
	{
		probs.emplace_back(0.0);
		probs.emplace_back(0.25);
		probs.emplace_back(0.50);
		probs.emplace_back(0.75);
		probs.emplace_back(1.0);
		probs_count = 5;
	}
	else
	{
		for (int probs_index = 0; probs_index < probs_count; ++probs_index)
		{
			double prob = probs_value->FloatAtIndex(probs_index, nullptr);
			
			if ((prob < 0.0) || (prob > 1.0))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): function quantile() requires probabilities to be in [0, 1]." << EidosTerminate(nullptr);
			
			probs.emplace_back(prob);
		}
	}
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(probs_count);
	result_SP = EidosValue_SP(float_result);
	
	if (x_count == 1)
	{
		// All quantiles of a singleton are the value of the singleton; the probabilities don't matter as long as they're in range (checked above)
		double x_singleton = x_value->FloatAtIndex(0, nullptr);
		
		if (std::isnan(x_singleton))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): quantiles of NAN are undefined." << EidosTerminate(nullptr);
		
		for (int probs_index = 0; probs_index < probs_count; ++probs_index)
			float_result->set_float_no_check(x_singleton, probs_index);
	}
	else
	{
		// Here we handle the non-singleton case, which can be done with direct access
		// First, if x is float, we check for NANs, which are not allowed
		EidosValueType x_type = x_value->Type();
		
		if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data = x_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (std::isnan(float_data[value_index]))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): quantiles of NAN are undefined." << EidosTerminate(nullptr);
		}
		
		// Next we get an order vector for x, which can be integer or float
		std::vector<int64_t> order;
		
		if (x_type == EidosValueType::kValueInt)
			order = EidosSortIndexes(x_value->IntVector()->data(), x_count, true);
		else if (x_type == EidosValueType::kValueFloat)
			order = EidosSortIndexes(x_value->FloatVector()->data(), x_count, true);
		
		// Now loop over the requested probabilities and calculate them
		for (int probs_index = 0; probs_index < probs_count; ++probs_index)
		{
			double prob = probs[probs_index];
			double index = (x_count - 1) * prob;
			long lo = (long)std::floor(index);
			long hi = (long)std::ceil(index);
			
			double quantile = x_value->FloatAtIndex((int)order[lo], nullptr);
			if (lo != hi) {
				double h = index - lo;
				quantile *= (1.0 - h);
				quantile += h * x_value->FloatAtIndex((int)order[hi], nullptr);
			}
			
			float_result->set_float_no_check(quantile, probs_index);
		}
	}
	
	return result_SP;
}

//	(numeric)range(numeric x, ...)
EidosValue_SP Eidos_ExecuteFunction_range(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValueType x_type = p_arguments[0].get()->Type();
	
	// check the types of ellipsis arguments and find the first nonempty argument
	int first_nonempty_argument = -1;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		EidosValueType arg_type = arg_value->Type();
		
		if (arg_type != x_type)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_range): function range() requires all arguments to be the same type." << EidosTerminate(nullptr);
		
		if (first_nonempty_argument == -1)
		{
			int arg_count = arg_value->Count();
			
			if (arg_count > 0)
				first_nonempty_argument = arg_index;
		}
	}
	
	if (first_nonempty_argument == -1)
	{
		// R uses -Inf or +Inf for max/min of a numeric vector, but we want to be consistent between integer and float, and we
		// want to return an integer value for integer arguments, and there is no integer -Inf/+Inf, so we return NULL.  Note
		// this means that, unlike R, min() and max() in Eidos are not transitive; min(a, min(b)) != min(a, b) in general.  We
		// could fix that by returning NULL whenever any of the arguments are zero-length, but that does not seem desirable.
		result_SP = gStaticEidosValueNULL;
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(2);
		result_SP = EidosValue_SP(int_result);
		
		int64_t max = p_arguments[first_nonempty_argument]->IntAtIndex(0, nullptr);
		int64_t min = max;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntAtIndex(0, nullptr);
				if (max < temp)
					max = temp;
				else if (min > temp)
					min = temp;
			}
			else
			{
				const int64_t *int_data = arg_value->IntVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (max < temp)
						max = temp;
					else if (min > temp)
						min = temp;
				}
			}
		}
		
		int_result->set_int_no_check(min, 0);
		int_result->set_int_no_check(max, 1);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(2);
		result_SP = EidosValue_SP(float_result);
		
		double max = p_arguments[first_nonempty_argument]->FloatAtIndex(0, nullptr);
		double min = max;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatAtIndex(0, nullptr);
				
				// if there is a NAN, the range is always (NAN,NAN); short-circuit
				if (std::isnan(temp))
				{
					float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), 0);
					float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), 1);
					return result_SP;
				}
				
				if (max < temp)
					max = temp;
				else if (min > temp)
					min = temp;
			}
			else
			{
				const double *float_data = arg_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					double temp = float_data[value_index];
					
					// if there is a NAN, the range is always (NAN,NAN); short-circuit
					if (std::isnan(temp))
					{
						float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), 0);
						float_result->set_float_no_check(std::numeric_limits<double>::quiet_NaN(), 1);
						return result_SP;
					}
					
					if (max < temp)
						max = temp;
					else if (min > temp)
						min = temp;
				}
			}
		}
		
		float_result->set_float_no_check(min, 0);
		float_result->set_float_no_check(max, 1);
	}
	
	return result_SP;
}

//	(float$)sd(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sd(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count > 1)
	{
		double mean = 0;
		double sd = 0;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			mean += x_value->FloatAtIndex(value_index, nullptr);
		
		mean /= x_count;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			double temp = (x_value->FloatAtIndex(value_index, nullptr) - mean);
			sd += temp * temp;
		}
		
		sd = sqrt(sd / (x_count - 1));
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sd));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}

//	(float$)ttest(float x, [Nf y = NULL], [Nf$ mu = NULL])
EidosValue_SP Eidos_ExecuteFunction_ttest(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	EidosValue *mu_value = p_arguments[2].get();
	EidosValueType mu_type = mu_value->Type();
	
	if ((y_type == EidosValueType::kValueNULL) && (mu_type == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ttest): function ttest() requires either y or mu to be non-NULL." << EidosTerminate(nullptr);
	if ((y_type != EidosValueType::kValueNULL) && (mu_type != EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ttest): function ttest() requires either y or mu to be NULL." << EidosTerminate(nullptr);
	if (x_count <= 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ttest): function ttest() requires enough elements in x to compute variance." << EidosTerminate(nullptr);
	
	const double *vec1 = x_value->FloatVector()->data();
	double pvalue = 0.0;
	
	if (y_type != EidosValueType::kValueNULL)
	{
		// This is the x & y case, which is a two-sample Welch's t-test
		if (y_count <= 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ttest): function ttest() requires enough elements in y to compute variance." << EidosTerminate(nullptr);
		
		const double *vec2 = y_value->FloatVector()->data();
		
		// Right now this function only provides a two-sample t-test; we could add an optional mu argument and make y optional in order to allow a one-sample test as well
		// If we got into that, we'd probably want to provide one-sided t-tests as well, yada yada...
		pvalue = Eidos_TTest_TwoSampleWelch(vec1, x_count, vec2, y_count, nullptr, nullptr);
	}
	else if (mu_type != EidosValueType::kValueNULL)
	{
		// This is the x & mu case, which is a one-sample t-test
		double mu = mu_value->FloatAtIndex(0, nullptr);
		
		pvalue = Eidos_TTest_OneSample(vec1, x_count, mu, nullptr);
	}
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(pvalue));
	
	return result_SP;
}

//	(float$)var(numeric x)
EidosValue_SP Eidos_ExecuteFunction_var(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_value->IsArray())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_var): function var() does not currently support a matrix/array argument." << EidosTerminate(nullptr);
	
	if (x_count > 1)
	{
		// calculate mean
		double mean = 0;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			mean += x_value->FloatAtIndex(value_index, nullptr);
		
		mean /= x_count;
		
		// calculate variance
		double var = 0;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			double temp = (x_value->FloatAtIndex(value_index, nullptr) - mean);
			var += temp * temp;
		}
		
		var = var / (x_count - 1);
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(var));
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


//	(float)dmvnorm(float x, numeric mu, numeric sigma)
EidosValue_SP Eidos_ExecuteFunction_dmvnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_x = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	
	if (arg_x->Count() == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// matrix with n rows (one row per quantile vector) and k columns (one column per dimension)
	int dimension_count = arg_x->DimensionCount();
	int64_t num_quantiles;
	int d;
	
	if (dimension_count == 1)
	{
		num_quantiles = 1;
		d = arg_x->Count();
	}
	else if (dimension_count == 2)
	{
		const int64_t *dimensions = arg_x->Dimensions();
		num_quantiles = dimensions[0];
		d = (int)dimensions[1];
	}
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires x to be a vector containing a single quantile, or a matrix of quantiles." << EidosTerminate(nullptr);
	
	if (d <= 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires a Gaussian function dimensionality of >= 2 (use dnorm() for dimensionality of 1)." << EidosTerminate(nullptr);
	
	int mu_count = arg_mu->Count();
	int mu_dimcount = arg_mu->DimensionCount();
	int sigma_dimcount = arg_sigma->DimensionCount();
	const int64_t *sigma_dims = arg_sigma->Dimensions();
	
	if ((mu_dimcount != 1) || (mu_count != d))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires mu to be a plain vector of length k, where k is the number of dimensions for the multivariate Gaussian function (>= 2), matching the dimensionality of the quantile vectors in x." << EidosTerminate(nullptr);
	if (sigma_dimcount != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires sigma to be a matrix." << EidosTerminate(nullptr);
	if ((sigma_dims[0] != d) || (sigma_dims[1] != d))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires sigma to be a k x k matrix, where k is the number of dimensions for the multivariate Gaussian function (>= 2), matching the dimensionality of the quantile vectors in x." << EidosTerminate(nullptr);
	
	// Set up the GSL vectors
	gsl_vector *gsl_mu = gsl_vector_calloc(d);
	gsl_matrix *gsl_Sigma = gsl_matrix_calloc(d, d);
	gsl_matrix *gsl_L = gsl_matrix_calloc(d, d);
	gsl_vector *gsl_x = gsl_vector_calloc(d);
	gsl_vector *gsl_work = gsl_vector_calloc(d);
	
	for (int dim_index = 0; dim_index < d; ++dim_index)
		gsl_vector_set(gsl_mu, dim_index, arg_mu->FloatAtIndex(dim_index, nullptr));
	
	for (int row_index = 0; row_index < d; ++row_index)
	{
		for (int col_index = 0; col_index < d; ++col_index)
		{
			double value = arg_sigma->FloatAtIndex(row_index + col_index * d, nullptr);
			
			if (std::isnan(value))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() does not allow sigma to contain NANs." << EidosTerminate(nullptr);	// oddly, GSL does not raise an error on this!
			
			gsl_matrix_set(gsl_Sigma, row_index, col_index, value);
		}
	}
	
	gsl_matrix_memcpy(gsl_L, gsl_Sigma);
	
	// Disable the GSL's default error handler, which calls abort().  Normally we run with that handler,
	// which is perhaps a bit risky, but we want to check for all error cases and avert them before we
	// ever call into the GSL; having the GSL raise when it encounters an error condition is kind of OK
	// because it should never ever happen.  But the GSL calls we will make in this function could hit
	// errors unpredictably, if for example it turns out that Sigma is not positive-definite.  So for
	// this stretch of code we disable the default handler and check for errors returned from the GSL.
	gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
	int gsl_err;
	
	// Do the draws, which involves a preliminary step of doing a Cholesky decomposition
	gsl_err = gsl_linalg_cholesky_decomp1(gsl_L);
	
	if (gsl_err)
	{
		gsl_set_error_handler(old_handler);
		
		// Clean up GSL stuff
		gsl_vector_free(gsl_mu);
		gsl_matrix_free(gsl_Sigma);
		gsl_matrix_free(gsl_L);
		gsl_vector_free(gsl_x);
		gsl_vector_free(gsl_work);
		
		if (gsl_err == GSL_EDOM)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires that sigma, the variance-covariance matrix, be positive-definite." << EidosTerminate(nullptr);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_linalg_cholesky_decomp1() function." << EidosTerminate(nullptr);
	}
	
	const double *float_data = arg_x->FloatVector()->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
	result_SP = EidosValue_SP(float_result);
	
	for (int64_t value_index = 0; value_index < num_quantiles; ++value_index)
	{
		double gsl_result;
		
		for (int dim_index = 0; dim_index < d; ++dim_index)
			gsl_vector_set(gsl_x, dim_index, *(float_data + value_index + dim_index * num_quantiles));
		
		gsl_err = gsl_ran_multivariate_gaussian_pdf (gsl_x, gsl_mu, gsl_L, &gsl_result, gsl_work);
		
		if (gsl_err)
		{
			gsl_set_error_handler(old_handler);
			
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_ran_multivariate_gaussian_pdf() function." << EidosTerminate(nullptr);
		}
		
		float_result->set_float_no_check(gsl_result, value_index);
	}
	
	// Clean up GSL stuff
	gsl_vector_free(gsl_mu);
	gsl_matrix_free(gsl_Sigma);
	gsl_matrix_free(gsl_L);
	gsl_vector_free(gsl_x);
	gsl_vector_free(gsl_work);
	
	gsl_set_error_handler(old_handler);
	
	return result_SP;
}

//	(float)dnorm(float x, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_dnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int num_quantiles = arg_quantile->Count();
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires mean to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gaussian_pdf(arg_quantile->FloatAtIndex(0, nullptr) - mu0, sigma0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_gaussian_pdf(float_data[value_index] - mu0, sigma0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(value_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(value_index, nullptr));
			
			if (sigma <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_gaussian_pdf(float_data[value_index] - mu, sigma), value_index);
		}
	}
	
	return result_SP;
}

//	(float)qnorm(float p, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_qnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_prob = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_probs = arg_prob->Count();
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_probs))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires mean to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_probs))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires sd to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_probs == 1)
		{
			const double float_prob = arg_prob->FloatAtIndex(0, nullptr);
			if (float_prob < 0.0 || float_prob > 1.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires 0.0 <= p <= 1.0 (" << EidosStringForFloat(float_prob) << " supplied)." << EidosTerminate(nullptr);

			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_cdf_gaussian_Pinv(float_prob, sigma0) + mu0));
		}
		else
		{
			const double *float_data = arg_prob->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_probs);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t value_index = 0; value_index < num_probs; ++value_index) {
				if (float_data[value_index] < 0.0 || float_data[value_index] > 1.0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires 0.0 <= p <= 1.0 (" << EidosStringForFloat(float_data[value_index]) << " supplied)." << EidosTerminate(nullptr);
				float_result->set_float_no_check(gsl_cdf_gaussian_Pinv(float_data[value_index], sigma0) + mu0, value_index);
        }
		}
	}
	else
	{
		const double *float_data = arg_prob->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_probs);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_probs; ++value_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(value_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(value_index, nullptr));
		  if (float_data[value_index] < 0.0 || float_data[value_index] > 1.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires 0.0 <= p <= 1.0 (" << EidosStringForFloat(float_data[value_index]) << " supplied)." << EidosTerminate(nullptr);
			
			if (sigma <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_cdf_gaussian_Pinv(float_data[value_index], sigma) + mu, value_index);
		}
	}
	
	return result_SP;
}


//	(float)pnorm(float q, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_pnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
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
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires mean to be of length 1 or equal in length to q." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires sd to be of length 1 or equal in length to q." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_cdf_gaussian_P(arg_quantile->FloatAtIndex(0, nullptr) - mu0, sigma0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_cdf_gaussian_P(float_data[value_index] - mu0, sigma0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(value_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(value_index, nullptr));
			
			if (sigma <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_cdf_gaussian_P(float_data[value_index] - mu, sigma), value_index);
		}
	}
	
	return result_SP;
}

//	(float)dbeta(float x, numeric alpha, numeric beta)
EidosValue_SP Eidos_ExecuteFunction_dbeta(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_alpha = p_arguments[1].get();
	EidosValue *arg_beta = p_arguments[2].get();
	int num_quantiles = arg_quantile->Count();
	int arg_alpha_count = arg_alpha->Count();
	int arg_beta_count = arg_beta->Count();
	bool alpha_singleton = (arg_alpha_count == 1);
	bool beta_singleton = (arg_beta_count == 1);
	
	if (!alpha_singleton && (arg_alpha_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires alpha to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	if (!beta_singleton && (arg_beta_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires beta to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	double alpha0 = (arg_alpha_count ? arg_alpha->FloatAtIndex(0, nullptr) : 0.0);
	double beta0 = (arg_beta_count ? arg_beta->FloatAtIndex(0, nullptr) : 0.0);
	
	if (alpha_singleton && beta_singleton)
	{
		if (!(alpha0 > 0.0))	// true for NaN
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha0) << " supplied)." << EidosTerminate(nullptr);
		if (!(beta0 > 0.0))		// true for NaN
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires beta > 0.0 (" << EidosStringForFloat(beta0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_beta_pdf(arg_quantile->FloatAtIndex(0, nullptr), alpha0, beta0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_beta_pdf(float_data[value_index], alpha0, beta0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double alpha = (alpha_singleton ? alpha0 : arg_alpha->FloatAtIndex(value_index, nullptr));
			double beta = (beta_singleton ? beta0 : arg_beta->FloatAtIndex(value_index, nullptr));
			
			if (!(alpha > 0.0))		// true for NaN
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha) << " supplied)." << EidosTerminate(nullptr);
			if (!(beta > 0.0))		// true for NaN
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires beta > 0.0 (" << EidosStringForFloat(beta) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_beta_pdf(float_data[value_index], alpha, beta), value_index);
		}
	}
	
	return result_SP;
}

//	(float)rbeta(integer$ n, numeric alpha, numeric beta)
EidosValue_SP Eidos_ExecuteFunction_rbeta(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_alpha = p_arguments[1].get();
	EidosValue *arg_beta = p_arguments[2].get();
	int arg_alpha_count = arg_alpha->Count();
	int arg_beta_count = arg_beta->Count();
	bool alpha_singleton = (arg_alpha_count == 1);
	bool beta_singleton = (arg_beta_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!alpha_singleton && (arg_alpha_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires alpha to be of length 1 or n." << EidosTerminate(nullptr);
	if (!beta_singleton && (arg_beta_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires beta to be of length 1 or n." << EidosTerminate(nullptr);
	
	double alpha0 = (arg_alpha_count ? arg_alpha->FloatAtIndex(0, nullptr) : 0.0);
	double beta0 = (arg_beta_count ? arg_beta->FloatAtIndex(0, nullptr) : 0.0);
	
	if (alpha_singleton && beta_singleton)
	{
		if (alpha0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha0) << " supplied)." << EidosTerminate(nullptr);
		if (beta0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires beta > 0.0 (" << EidosStringForFloat(beta0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_beta(EIDOS_GSL_RNG, alpha0, beta0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_beta(EIDOS_GSL_RNG, alpha0, beta0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double alpha = (alpha_singleton ? alpha0 : arg_alpha->FloatAtIndex(draw_index, nullptr));
			double beta = (beta_singleton ? beta0 : arg_beta->FloatAtIndex(draw_index, nullptr));
			
			if (alpha <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha) << " supplied)." << EidosTerminate(nullptr);
			if (beta <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires beta > 0.0 (" << EidosStringForFloat(beta) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_beta(EIDOS_GSL_RNG, alpha, beta), draw_index);
		}
	}
	
	return result_SP;
}

//	(integer)rbinom(integer$ n, integer size, float prob)
EidosValue_SP Eidos_ExecuteFunction_rbinom(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_size = p_arguments[1].get();
	EidosValue *arg_prob = p_arguments[2].get();
	int arg_size_count = arg_size->Count();
	int arg_prob_count = arg_prob->Count();
	bool size_singleton = (arg_size_count == 1);
	bool prob_singleton = (arg_prob_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!size_singleton && (arg_size_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size to be of length 1 or n." << EidosTerminate(nullptr);
	if (!prob_singleton && (arg_prob_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires prob to be of length 1 or n." << EidosTerminate(nullptr);
	
	int size0 = (int)arg_size->IntAtIndex(0, nullptr);
	double probability0 = arg_prob->FloatAtIndex(0, nullptr);
	
	if (size_singleton && prob_singleton)
	{
		if (size0 < 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size >= 0 (" << size0 << " supplied)." << EidosTerminate(nullptr);
		if ((probability0 < 0.0) || (probability0 > 1.0) || std::isnan(probability0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires probability in [0.0, 1.0] (" << EidosStringForFloat(probability0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			if ((probability0 == 0.5) && (size0 == 1))
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(Eidos_RandomBool() ? 1 : 0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_binomial(EIDOS_GSL_RNG, probability0, size0)));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			if ((probability0 == 0.5) && (size0 == 1))
			{
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(Eidos_RandomBool() ? 1 : 0, draw_index);
			}
			else
			{
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(gsl_ran_binomial(EIDOS_GSL_RNG, probability0, size0), draw_index);
			}
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			int size = (size_singleton ? size0 : (int)arg_size->IntAtIndex(draw_index, nullptr));
			double probability = (prob_singleton ? probability0 : arg_prob->FloatAtIndex(draw_index, nullptr));
			
			if (size < 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size >= 0 (" << size << " supplied)." << EidosTerminate(nullptr);
			if ((probability < 0.0) || (probability > 1.0) || std::isnan(probability))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires probability in [0.0, 1.0] (" << EidosStringForFloat(probability) << " supplied)." << EidosTerminate(nullptr);
			
			int_result->set_int_no_check(gsl_ran_binomial(EIDOS_GSL_RNG, probability, size), draw_index);
		}
	}
	
	return result_SP;
}

//	(float)rcauchy(integer$ n, [numeric location = 0], [numeric scale = 1])
EidosValue_SP Eidos_ExecuteFunction_rcauchy(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_location = p_arguments[1].get();
	EidosValue *arg_scale = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_location_count = arg_location->Count();
	int arg_scale_count = arg_scale->Count();
	bool location_singleton = (arg_location_count == 1);
	bool scale_singleton = (arg_scale_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!location_singleton && (arg_location_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires location to be of length 1 or n." << EidosTerminate(nullptr);
	if (!scale_singleton && (arg_scale_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires scale to be of length 1 or n." << EidosTerminate(nullptr);
	
	double location0 = (arg_location_count ? arg_location->FloatAtIndex(0, nullptr) : 0.0);
	double scale0 = (arg_scale_count ? arg_scale->FloatAtIndex(0, nullptr) : 1.0);
	
	if (location_singleton && scale_singleton)
	{
		if (scale0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires scale > 0.0 (" << EidosStringForFloat(scale0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_cauchy(EIDOS_GSL_RNG, scale0) + location0));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_cauchy(EIDOS_GSL_RNG, scale0) + location0, draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double location = (location_singleton ? location0 : arg_location->FloatAtIndex(draw_index, nullptr));
			double scale = (scale_singleton ? scale0 : arg_scale->FloatAtIndex(draw_index, nullptr));
			
			if (scale <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires scale > 0.0 (" << EidosStringForFloat(scale) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_cauchy(EIDOS_GSL_RNG, scale) + location, draw_index);
		}
	}
	
	return result_SP;
}

//	(integer)rdunif(integer$ n, [integer min = 0], [integer max = 1])
EidosValue_SP Eidos_ExecuteFunction_rdunif(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_min = p_arguments[1].get();
	EidosValue *arg_max = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_min_count = arg_min->Count();
	int arg_max_count = arg_max->Count();
	bool min_singleton = (arg_min_count == 1);
	bool max_singleton = (arg_max_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!min_singleton && (arg_min_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires min to be of length 1 or n." << EidosTerminate(nullptr);
	if (!max_singleton && (arg_max_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires max to be of length 1 or n." << EidosTerminate(nullptr);
	
	int64_t min_value0 = (arg_min_count ? arg_min->IntAtIndex(0, nullptr) : 0);
	int64_t max_value0 = (arg_max_count ? arg_max->IntAtIndex(0, nullptr) : 1);
	
	if (min_singleton && max_singleton)
	{
		uint64_t count0 = (max_value0 - min_value0) + 1;
		
		if (max_value0 < min_value0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires min <= max." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			if (count0 == 2)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(Eidos_RandomBool() + min_value0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(Eidos_rng_uniform_int_MT64(count0) + min_value0));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			if (count0 == 2)
			{
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(Eidos_RandomBool() + min_value0, draw_index);
			}
			else
			{
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(Eidos_rng_uniform_int_MT64(count0) + min_value0, draw_index);
			}
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			int64_t min_value = (min_singleton ? min_value0 : arg_min->IntAtIndex(draw_index, nullptr));
			int64_t max_value = (max_singleton ? max_value0 : arg_max->IntAtIndex(draw_index, nullptr));
			int64_t count = (max_value - min_value) + 1;
			
			if (max_value < min_value)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires min <= max." << EidosTerminate(nullptr);
			
			int_result->set_int_no_check(Eidos_rng_uniform_int_MT64(count) + min_value, draw_index);
		}
	}
	
	return result_SP;
}

//	(float)dexp(float x, [numeric mu = 1])
EidosValue_SP Eidos_ExecuteFunction_dexp(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	int num_quantiles = arg_quantile->Count();
	int arg_mu_count = arg_mu->Count();
	bool mu_singleton = (arg_mu_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dexp): function dexp() requires mu to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	if (mu_singleton)
	{
		double mu0 = arg_mu->FloatAtIndex(0, nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_exponential_pdf(arg_quantile->FloatAtIndex(0, nullptr), mu0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_exponential_pdf(float_data[value_index], mu0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mu = arg_mu->FloatAtIndex(value_index, nullptr);
			
			float_result->set_float_no_check(gsl_ran_exponential_pdf(float_data[value_index], mu), value_index);
		}
	}
	
	return result_SP;
}

//	(float)rexp(integer$ n, [numeric mu = 1])
EidosValue_SP Eidos_ExecuteFunction_rexp(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_mu_count = arg_mu->Count();
	bool mu_singleton = (arg_mu_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rexp): function rexp() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!mu_singleton && (arg_mu_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rexp): function rexp() requires mu to be of length 1 or n." << EidosTerminate(nullptr);
	
	if (mu_singleton)
	{
		double mu0 = arg_mu->FloatAtIndex(0, nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_exponential(EIDOS_GSL_RNG, mu0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_exponential(EIDOS_GSL_RNG, mu0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double mu = arg_mu->FloatAtIndex(draw_index, nullptr);
			
			float_result->set_float_no_check(gsl_ran_exponential(EIDOS_GSL_RNG, mu), draw_index);
		}
	}
	
	return result_SP;
}

//	(float)dgamma(float x, numeric mean, numeric shape)
EidosValue_SP Eidos_ExecuteFunction_dgamma(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mean = p_arguments[1].get();
	EidosValue *arg_shape = p_arguments[2].get();
	int num_quantiles = arg_quantile->Count();
	int arg_mean_count = arg_mean->Count();
	int arg_shape_count = arg_shape->Count();
	bool mean_singleton = (arg_mean_count == 1);
	bool shape_singleton = (arg_shape_count == 1);
	
	if (!mean_singleton && (arg_mean_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires mean to be of length 1 or n." << EidosTerminate(nullptr);
	if (!shape_singleton && (arg_shape_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires shape to be of length 1 or n." << EidosTerminate(nullptr);
	
	double mean0 = (arg_mean_count ? arg_mean->FloatAtIndex(0, nullptr) : 1.0);
	double shape0 = (arg_shape_count ? arg_shape->FloatAtIndex(0, nullptr) : 0.0);
	
	if (mean_singleton && shape_singleton)
	{
		if (!(shape0 > 0.0))	// true for NaN
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires shape > 0.0 (" << EidosStringForFloat(shape0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gamma_pdf(arg_quantile->FloatAtIndex(0, nullptr), shape0, mean0/shape0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			double scale = mean0 / shape0;
			
			for (int64_t value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_gamma_pdf(float_data[value_index], shape0, scale), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mean = (mean_singleton ? mean0 : arg_mean->FloatAtIndex(value_index, nullptr));
			double shape = (shape_singleton ? shape0 : arg_shape->FloatAtIndex(value_index, nullptr));
			
			if (!(shape > 0.0))		// true for NaN
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires shape > 0.0 (" << EidosStringForFloat(shape) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_gamma_pdf(float_data[value_index], shape, mean / shape), value_index);
		}
	}
	
	return result_SP;
}

//	(float)rgamma(integer$ n, numeric mean, numeric shape)
EidosValue_SP Eidos_ExecuteFunction_rgamma(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_mean = p_arguments[1].get();
	EidosValue *arg_shape = p_arguments[2].get();
	int arg_mean_count = arg_mean->Count();
	int arg_shape_count = arg_shape->Count();
	bool mean_singleton = (arg_mean_count == 1);
	bool shape_singleton = (arg_shape_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!mean_singleton && (arg_mean_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires mean to be of length 1 or n." << EidosTerminate(nullptr);
	if (!shape_singleton && (arg_shape_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape to be of length 1 or n." << EidosTerminate(nullptr);
	
	double mean0 = (arg_mean_count ? arg_mean->FloatAtIndex(0, nullptr) : 1.0);
	double shape0 = (arg_shape_count ? arg_shape->FloatAtIndex(0, nullptr) : 0.0);
	
	if (mean_singleton && shape_singleton)
	{
		if (shape0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape > 0.0 (" << EidosStringForFloat(shape0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gamma(EIDOS_GSL_RNG, shape0, mean0/shape0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			double scale = mean0 / shape0;
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_gamma(EIDOS_GSL_RNG, shape0, scale), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double mean = (mean_singleton ? mean0 : arg_mean->FloatAtIndex(draw_index, nullptr));
			double shape = (shape_singleton ? shape0 : arg_shape->FloatAtIndex(draw_index, nullptr));
			
			if (shape <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape > 0.0 (" << EidosStringForFloat(shape) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_gamma(EIDOS_GSL_RNG, shape, mean / shape), draw_index);
		}
	}
	
	return result_SP;
}

//	(integer)rgeom(integer$ n, float p)
EidosValue_SP Eidos_ExecuteFunction_rgeom(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_p = p_arguments[1].get();
	int arg_p_count = arg_p->Count();
	bool p_singleton = (arg_p_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!p_singleton && (arg_p_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires p to be of length 1 or n." << EidosTerminate(nullptr);
	
	// Note that there are two different definitions of the geometric distribution (see https://en.wikipedia.org/wiki/Geometric_distribution).
	// We follow R in using the definition that is supported on the set {0, 1, 2, 3, ...}.  Unfortunately, gsl_ran_geometric() uses the other
	// definition, which is supported on the set {1, 2, 3, ...}.  The GSL's version does not allow p==1.0, so we have to special-case that.
	// Otherwise, the result of our definition is equal to the result from the GSL's definition minus 1; the GSL uses the "shifted geometric".
	
	if (p_singleton)
	{
		double p0 = arg_p->FloatAtIndex(0, nullptr);
		
		if ((p0 <= 0.0) || (p0 > 1.0) || std::isnan(p0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires 0.0 < p <= 1.0 (" << EidosStringForFloat(p0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			if (p0 == 1.0)	// special-case p==1.0
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_geometric(EIDOS_GSL_RNG, p0) - 1));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			if (p0 == 1.0)	// special-case p==1.0
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(0, draw_index);
			else
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(gsl_ran_geometric(EIDOS_GSL_RNG, p0) - 1, draw_index);
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double p = arg_p->FloatAtIndex(draw_index, nullptr);
			
			if ((p <= 0.0) || (p >= 1.0) || std::isnan(p))
			{
				if (p == 1.0)	// special-case p==1.0; inside here to avoid an extra comparison per loop in the standard case
				{
					int_result->set_int_no_check(0, draw_index);
					continue;
				}
				
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires 0.0 < p <= 1.0 (" << EidosStringForFloat(p) << " supplied)." << EidosTerminate(nullptr);
			}
			
			int_result->set_int_no_check(gsl_ran_geometric(EIDOS_GSL_RNG, p) - 1, draw_index);
		}
	}
	
	return result_SP;
}

//	(float)rlnorm(integer$ n, [numeric meanlog = 0], [numeric sdlog = 1])
EidosValue_SP Eidos_ExecuteFunction_rlnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_meanlog = p_arguments[1].get();
	EidosValue *arg_sdlog = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_meanlog_count = arg_meanlog->Count();
	int arg_sdlog_count = arg_sdlog->Count();
	bool meanlog_singleton = (arg_meanlog_count == 1);
	bool sdlog_singleton = (arg_sdlog_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!meanlog_singleton && (arg_meanlog_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires meanlog to be of length 1 or n." << EidosTerminate(nullptr);
	if (!sdlog_singleton && (arg_sdlog_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires sdlog to be of length 1 or n." << EidosTerminate(nullptr);
	
	double meanlog0 = (arg_meanlog_count ? arg_meanlog->FloatAtIndex(0, nullptr) : 0.0);
	double sdlog0 = (arg_sdlog_count ? arg_sdlog->FloatAtIndex(0, nullptr) : 1.0);
	
	if (meanlog_singleton && sdlog_singleton)
	{
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_lognormal(EIDOS_GSL_RNG, meanlog0, sdlog0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_lognormal(EIDOS_GSL_RNG, meanlog0, sdlog0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double meanlog = (meanlog_singleton ? meanlog0 : arg_meanlog->FloatAtIndex(draw_index, nullptr));
			double sdlog = (sdlog_singleton ? sdlog0 : arg_sdlog->FloatAtIndex(draw_index, nullptr));
			
			float_result->set_float_no_check(gsl_ran_lognormal(EIDOS_GSL_RNG, meanlog, sdlog), draw_index);
		}
	}
	
	return result_SP;
}

// (float)rmvnorm(integer$ n, numeric mu, numeric sigma)
EidosValue_SP Eidos_ExecuteFunction_rmvnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_n = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_draws = arg_n->IntAtIndex(0, nullptr);
	int mu_count = arg_mu->Count();
	int mu_dimcount = arg_mu->DimensionCount();
	int sigma_dimcount = arg_sigma->DimensionCount();
	const int64_t *sigma_dims = arg_sigma->Dimensions();
	int d = mu_count;
	
	if (num_draws < 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires n to be greater than or equal to 1 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	
	if ((mu_dimcount != 1) || (mu_count < 2))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires mu to be a plain vector of length k, where k is the number of dimensions for the multivariate Gaussian function (k must be >= 2)." << EidosTerminate(nullptr);
	if (sigma_dimcount != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires sigma to be a matrix." << EidosTerminate(nullptr);
	if ((sigma_dims[0] != d) || (sigma_dims[1] != d))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires sigma to be a k x k matrix, where k is the number of dimensions for the multivariate Gaussian function (k must be >= 2)." << EidosTerminate(nullptr);
	
	for (int row_index = 0; row_index < d; ++row_index)
		for (int col_index = 0; col_index < d; ++col_index)
			if (std::isnan(arg_sigma->FloatAtIndex(row_index + col_index * d, nullptr)))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() does not allow sigma to contain NANs." << EidosTerminate(nullptr);	// oddly, GSL does not raise an error on this!
	
	// Set up the GSL vectors
	gsl_vector *gsl_mu = gsl_vector_calloc(d);
	gsl_matrix *gsl_Sigma = gsl_matrix_calloc(d, d);
	gsl_matrix *gsl_L = gsl_matrix_calloc(d, d);
	gsl_vector *gsl_result = gsl_vector_calloc(d);
	
	for (int dim_index = 0; dim_index < d; ++dim_index)
		gsl_vector_set(gsl_mu, dim_index, arg_mu->FloatAtIndex(dim_index, nullptr));
	
	for (int row_index = 0; row_index < d; ++row_index)
		for (int col_index = 0; col_index < d; ++col_index)
			gsl_matrix_set(gsl_Sigma, row_index, col_index, arg_sigma->FloatAtIndex(row_index + col_index * d, nullptr));
	
	gsl_matrix_memcpy(gsl_L, gsl_Sigma);
	
	// Disable the GSL's default error handler, which calls abort().  Normally we run with that handler,
	// which is perhaps a bit risky, but we want to check for all error cases and avert them before we
	// ever call into the GSL; having the GSL raise when it encounters an error condition is kind of OK
	// because it should never ever happen.  But the GSL calls we will make in this function could hit
	// errors unpredictably, if for example it turns out that Sigma is not positive-definite.  So for
	// this stretch of code we disable the default handler and check for errors returned from the GSL.
	gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
	int gsl_err;
	
	// Do the draws, which involves a preliminary step of doing a Cholesky decomposition
	gsl_err = gsl_linalg_cholesky_decomp1(gsl_L);
	
	if (gsl_err)
	{
		gsl_set_error_handler(old_handler);
		
		// Clean up GSL stuff
		gsl_vector_free(gsl_mu);
		gsl_matrix_free(gsl_Sigma);
		gsl_matrix_free(gsl_L);
		gsl_vector_free(gsl_result);
		
		if (gsl_err == GSL_EDOM)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires that sigma, the variance-covariance matrix, be positive-definite." << EidosTerminate(nullptr);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_linalg_cholesky_decomp1() function." << EidosTerminate(nullptr);
	}
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws * d);
	result_SP = EidosValue_SP(float_result);
	
	for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
	{
		gsl_err = gsl_ran_multivariate_gaussian(EIDOS_GSL_RNG, gsl_mu, gsl_L, gsl_result);
		
		if (gsl_err)
		{
			gsl_set_error_handler(old_handler);
			
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_ran_multivariate_gaussian() function." << EidosTerminate(nullptr);
		}
		
		for (int dim_index = 0; dim_index < d; ++dim_index)
			float_result->set_float_no_check(gsl_vector_get(gsl_result, dim_index), draw_index + dim_index * num_draws);
	}
	
	// Clean up GSL stuff
	gsl_vector_free(gsl_mu);
	gsl_matrix_free(gsl_Sigma);
	gsl_matrix_free(gsl_L);
	gsl_vector_free(gsl_result);
	
	gsl_set_error_handler(old_handler);
	
	// Set the dimensions of the result; we want one row per draw
	int64_t dim[2] = {num_draws, d};
	
	float_result->SetDimensions(2, dim);
	
	return result_SP;
}

//	(float)rnorm(integer$ n, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_rnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!mu_singleton && (arg_mu_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires mean to be of length 1 or n." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd to be of length 1 or n." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 < 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd >= 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gaussian(EIDOS_GSL_RNG, sigma0) + mu0));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_gaussian(EIDOS_GSL_RNG, sigma0) + mu0, draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(draw_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(draw_index, nullptr));
			
			if (sigma < 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd >= 0.0 (" << EidosStringForFloat(sigma) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_gaussian(EIDOS_GSL_RNG, sigma) + mu, draw_index);
		}
	}
	
	return result_SP;
}

//	(integer)rpois(integer$ n, numeric lambda)
EidosValue_SP Eidos_ExecuteFunction_rpois(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_lambda = p_arguments[1].get();
	int arg_lambda_count = arg_lambda->Count();
	bool lambda_singleton = (arg_lambda_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!lambda_singleton && (arg_lambda_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda to be of length 1 or n." << EidosTerminate(nullptr);
	
	// Here we ignore USE_GSL_POISSON and always use the GSL.  This is because we don't know whether lambda (otherwise known as mu) is
	// small or large, and because we don't know what level of accuracy is demanded by whatever the user is doing with the deviates,
	// and so forth; it makes sense to just rely on the GSL for maximal accuracy and reliability.
	
	if (lambda_singleton)
	{
		double lambda0 = arg_lambda->FloatAtIndex(0, nullptr);
		
		if ((lambda0 <= 0.0) || std::isnan(lambda0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda > 0.0 (" << EidosStringForFloat(lambda0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_poisson(EIDOS_GSL_RNG, lambda0)));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				int_result->set_int_no_check(gsl_ran_poisson(EIDOS_GSL_RNG, lambda0), draw_index);
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double lambda = arg_lambda->FloatAtIndex(draw_index, nullptr);
			
			if ((lambda <= 0.0) || std::isnan(lambda))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda > 0.0 (" << EidosStringForFloat(lambda) << " supplied)." << EidosTerminate(nullptr);
			
			int_result->set_int_no_check(gsl_ran_poisson(EIDOS_GSL_RNG, lambda), draw_index);
		}
	}
	
	return result_SP;
}

//	(float)runif(integer$ n, [numeric min = 0], [numeric max = 1])
EidosValue_SP Eidos_ExecuteFunction_runif(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_min = p_arguments[1].get();
	EidosValue *arg_max = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_min_count = arg_min->Count();
	int arg_max_count = arg_max->Count();
	bool min_singleton = (arg_min_count == 1);
	bool max_singleton = (arg_max_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!min_singleton && (arg_min_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min to be of length 1 or n." << EidosTerminate(nullptr);
	if (!max_singleton && (arg_max_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires max to be of length 1 or n." << EidosTerminate(nullptr);
	
	double min_value0 = (arg_min_count ? arg_min->FloatAtIndex(0, nullptr) : 0.0);
	double max_value0 = (arg_max_count ? arg_max->FloatAtIndex(0, nullptr) : 1.0);
	
	if (min_singleton && max_singleton && (min_value0 == 0.0) && (max_value0 == 1.0))
	{
		// With the default min and max, we can streamline quite a bit
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(Eidos_rng_uniform(EIDOS_GSL_RNG)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG), draw_index);
		}
	}
	else
	{
		double range0 = max_value0 - min_value0;
		
		if (min_singleton && max_singleton)
		{
			if (range0 < 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min < max." << EidosTerminate(nullptr);
			
			if (num_draws == 1)
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(Eidos_rng_uniform(EIDOS_GSL_RNG) * range0 + min_value0));
			}
			else
			{
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
				result_SP = EidosValue_SP(float_result);
				
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * range0 + min_value0, draw_index);
			}
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				double min_value = (min_singleton ? min_value0 : arg_min->FloatAtIndex(draw_index, nullptr));
				double max_value = (max_singleton ? max_value0 : arg_max->FloatAtIndex(draw_index, nullptr));
				double range = max_value - min_value;
				
				if (range < 0.0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min < max." << EidosTerminate(nullptr);
				
				float_result->set_float_no_check(Eidos_rng_uniform(EIDOS_GSL_RNG) * range + min_value, draw_index);
			}
		}
	}
	
	return result_SP;
}

//	(float)rweibull(integer$ n, numeric lambda, numeric k)
EidosValue_SP Eidos_ExecuteFunction_rweibull(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_lambda = p_arguments[1].get();
	EidosValue *arg_k = p_arguments[2].get();
	int arg_lambda_count = arg_lambda->Count();
	int arg_k_count = arg_k->Count();
	bool lambda_singleton = (arg_lambda_count == 1);
	bool k_singleton = (arg_k_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!lambda_singleton && (arg_lambda_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda to be of length 1 or n." << EidosTerminate(nullptr);
	if (!k_singleton && (arg_k_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k to be of length 1 or n." << EidosTerminate(nullptr);
	
	double lambda0 = (arg_lambda_count ? arg_lambda->FloatAtIndex(0, nullptr) : 0.0);
	double k0 = (arg_k_count ? arg_k->FloatAtIndex(0, nullptr) : 0.0);
	
	if (lambda_singleton && k_singleton)
	{
		if (lambda0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda > 0.0 (" << EidosStringForFloat(lambda0) << " supplied)." << EidosTerminate(nullptr);
		if (k0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k > 0.0 (" << EidosStringForFloat(k0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_weibull(EIDOS_GSL_RNG, lambda0, k0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_weibull(EIDOS_GSL_RNG, lambda0, k0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double lambda = (lambda_singleton ? lambda0 : arg_lambda->FloatAtIndex(draw_index, nullptr));
			double k = (k_singleton ? k0 : arg_k->FloatAtIndex(draw_index, nullptr));
			
			if (lambda <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda > 0.0 (" << EidosStringForFloat(lambda) << " supplied)." << EidosTerminate(nullptr);
			if (k <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k > 0.0 (" << EidosStringForFloat(k) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_weibull(EIDOS_GSL_RNG, lambda, k), draw_index);
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
EidosValue_SP Eidos_ExecuteFunction_c(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	if (p_arguments.size() == 0)
		result_SP = gStaticEidosValueNULL;	// c() returns NULL, by definition
	else
		result_SP = ConcatenateEidosValues(p_arguments, true, false);	// allow NULL but not VOID
	
	return result_SP;
}

//	(float)float(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_float(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_float): function float() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(element_count);
	result_SP = EidosValue_SP(float_result);
	
	for (int64_t value_index = 0; value_index < element_count; ++value_index)
		float_result->set_float_no_check(0.0, value_index);
	
	return result_SP;
}

//	(integer)integer(integer$ length, [integer$ fill1 = 0], [integer$ fill2 = 1], [Ni fill2Indices = NULL])
EidosValue_SP Eidos_ExecuteFunction_integer(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	EidosValue *fill1_value = p_arguments[1].get();
	EidosValue *fill2_value = p_arguments[2].get();
	EidosValue *fill2Indices_value = p_arguments[3].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	int64_t fill1 = fill1_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integer): function integer() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(element_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int64_t value_index = 0; value_index < element_count; ++value_index)
		int_result->set_int_no_check(fill1, value_index);
	
	if (fill2Indices_value->Type() == EidosValueType::kValueInt)
	{
		int64_t fill2 = fill2_value->IntAtIndex(0, nullptr);
		int64_t *result_data = int_result->data();
		int positions_count = fill2Indices_value->Count();
		
		if (positions_count == 1)
		{
			int64_t position = fill2Indices_value->IntAtIndex(0, nullptr);
			
			if ((position < 0) || (position >= element_count))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integer): function integer() requires positions in fill2Indices to be between 0 and length - 1 (" << position << " supplied)." << EidosTerminate(nullptr);
			
			result_data[position] = fill2;
		}
		else
		{
			const int64_t *positions_data = fill2Indices_value->IntVector()->data();
			
			for (int positions_index = 0; positions_index < positions_count; ++positions_index)
			{
				int64_t position = positions_data[positions_index];
				
				if ((position < 0) || (position >= element_count))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integer): function integer() requires positions in fill2Indices to be between 0 and length - 1 (" << position << " supplied)." << EidosTerminate(nullptr);
				
				result_data[position] = fill2;
			}
		}
	}
	
	return result_SP;
}

//	(logical)logical(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_logical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_logical): function logical() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_Logical_ZeroVec;
	
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(element_count);
	result_SP = EidosValue_SP(logical_result);
	
	for (int64_t value_index = 0; value_index < element_count; ++value_index)
		logical_result->set_logical_no_check(false, value_index);
	
	return result_SP;
}

//	(object<undefined>)object(void)
EidosValue_SP Eidos_ExecuteFunction_object(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	result_SP = gStaticEidosValue_Object_ZeroVec;
	
	return result_SP;
}

//	(*)rep(* x, integer$ count)
EidosValue_SP Eidos_ExecuteFunction_rep(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *count_value = p_arguments[1].get();
	
	int64_t rep_count = count_value->IntAtIndex(0, nullptr);
	
	if (rep_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rep): function rep() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << EidosTerminate(nullptr);
	
	// the return type depends on the type of the first argument, which will get replicated
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
		for (int value_idx = 0; value_idx < x_count; value_idx++)
			result->PushValueFromIndexOfEidosValue(value_idx, *x_value, nullptr);
	
	return result_SP;
}

//	(*)repEach(* x, integer count)
EidosValue_SP Eidos_ExecuteFunction_repEach(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *count_value = p_arguments[1].get();
	int count_count = count_value->Count();
	
	// the return type depends on the type of the first argument, which will get replicated
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	if (count_count == 1)
	{
		int64_t rep_count = count_value->IntAtIndex(0, nullptr);
		
		if (rep_count < 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << EidosTerminate(nullptr);
		
		for (int value_idx = 0; value_idx < x_count; value_idx++)
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfEidosValue(value_idx, *x_value, nullptr);
	}
	else if (count_count == x_count)
	{
		for (int value_idx = 0; value_idx < x_count; value_idx++)
		{
			int64_t rep_count = count_value->IntAtIndex(value_idx, nullptr);
			
			if (rep_count < 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires all elements of count to be greater than or equal to 0 (" << rep_count << " supplied)." << EidosTerminate(nullptr);
			
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfEidosValue(value_idx, *x_value, nullptr);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires that parameter count's size() either (1) be equal to 1, or (2) be equal to the size() of its first argument." << EidosTerminate(nullptr);
	}
	
	return result_SP;
}

//	(*)sample(* x, integer$ size, [logical$ replace = F], [Nif weights = NULL])
EidosValue_SP Eidos_ExecuteFunction_sample(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int64_t sample_size = p_arguments[1]->IntAtIndex(0, nullptr);
	bool replace = p_arguments[2]->LogicalAtIndex(0, nullptr);
	EidosValue *weights_value = p_arguments[3].get();
	int x_count = x_value->Count();
	
	if (sample_size < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires a sample size >= 0 (" << sample_size << " supplied)." << EidosTerminate(nullptr);
	if (sample_size == 0)
	{
		result_SP = x_value->NewMatchingType();
		return result_SP;
	}
	
	if (x_count == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() provided with insufficient elements (0 supplied)." << EidosTerminate(nullptr);
	
	if (!replace && (x_count < sample_size))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() provided with insufficient elements (" << x_count << " supplied, " << sample_size << " needed)." << EidosTerminate(nullptr);
	
	// decide whether to use weights, if weights were supplied
	EidosValueType weights_type = weights_value->Type();
	int weights_count = weights_value->Count();
	
	if (weights_type == EidosValueType::kValueNULL)
	{
		weights_value = nullptr;
	}
	else
	{
		if (weights_count != x_count)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires x and weights to be the same length." << EidosTerminate(nullptr);
		
		if (weights_count == 1)
		{
			double weight = weights_value->FloatAtIndex(0, nullptr);
			
			if ((weight < 0.0) || std::isnan(weight))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << EidosStringForFloat(weight) << " supplied)." << EidosTerminate(nullptr);
			if (weight == 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
			
			// one weight, greater than zero; no need to use it, and this guarantees below that weights_value is non-singleton
			weights_value = nullptr;
		}
	}
	
	// if replace==F but we're only sampling one item, we might as well set replace=T, which chooses a simpler case below
	// at present this doesn't matter since sample_size == 1 is handled separately anyway, but it is a good inference to draw
	if (!replace && (sample_size == 1))
		replace = true;
	
	// the algorithm used depends on whether weights were supplied
	if (weights_value)
	{
		// handle the weights vector with separate cases for float and integer, so we can access it directly for speed
		if (weights_type == EidosValueType::kValueFloat)
		{
			const double *weights_float = weights_value->FloatVector()->data();
			double weights_sum = 0.0;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				double weight = weights_float[value_index];
				
				if ((weight < 0.0) || std::isnan(weight))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << EidosStringForFloat(weight) << " supplied)." << EidosTerminate(nullptr);
				
				weights_sum += weight;
			}
			
			if (weights_sum <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
			
			if (sample_size == 1)
			{
				// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
				double rose = Eidos_rng_uniform(EIDOS_GSL_RNG) * weights_sum;
				double rose_sum = 0.0;
				int rose_index;
				
				for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
				{
					rose_sum += weights_float[rose_index];
					
					if (rose <= rose_sum)
						break;
				}
				
				return x_value->GetValueAtIndex(rose_index, nullptr);
			}
			else if (replace)
			{
				// with replacement, we can just do a series of independent draws
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					double rose = Eidos_rng_uniform(EIDOS_GSL_RNG) * weights_sum;
					double rose_sum = 0.0;
					int rose_index;
					
					for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_float[rose_index];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(rose_index, *x_value, nullptr);
				}
			}
			else
			{
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				// get indices of x; we sample from this vector and then look up the corresponding weight and EidosValue element
				std::vector<int> index_vector;
				
				for (int value_index = 0; value_index < x_count; ++value_index)
					index_vector.emplace_back(value_index);
				
				// do the sampling
				int64_t contender_count = x_count;
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					if (weights_sum <= 0.0)
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
					
					double rose = Eidos_rng_uniform(EIDOS_GSL_RNG) * weights_sum;
					double rose_sum = 0.0;
					int rose_index;
					
					for (rose_index = 0; rose_index < contender_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_float[index_vector[rose_index]];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(index_vector[rose_index], *x_value, nullptr);
					
					// remove the sampled index since replace==F
					weights_sum -= weights_float[index_vector[rose_index]];	// possible source of numerical error
					index_vector.erase(index_vector.begin() + rose_index);
					--contender_count;
				}
			}
		}
		else if (weights_type == EidosValueType::kValueInt)
		{
			const int64_t *weights_int = weights_value->IntVector()->data();
			int64_t weights_sum = 0;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t weight = weights_int[value_index];
				
				if (weight < 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << weight << " supplied)." << EidosTerminate(nullptr);
				
				weights_sum += weight;
				
				if (weights_sum < 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): overflow of integer sum of weights in function sample(); the weights used are too large." << EidosTerminate(nullptr);
			}
			
			if (weights_sum <= 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
			
			if (sample_size == 1)
			{
				// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
				int64_t rose = (int64_t)ceil(Eidos_rng_uniform(EIDOS_GSL_RNG) * weights_sum);
				int64_t rose_sum = 0;
				int rose_index;
				
				for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
				{
					rose_sum += weights_int[rose_index];
					
					if (rose <= rose_sum)
						break;
				}
				
				return x_value->GetValueAtIndex(rose_index, nullptr);
			}
			else if (replace)
			{
				// with replacement, we can just do a series of independent draws
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					int64_t rose = (int64_t)ceil(Eidos_rng_uniform(EIDOS_GSL_RNG) * weights_sum);
					int64_t rose_sum = 0;
					int rose_index;
					
					for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_int[rose_index];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(rose_index, *x_value, nullptr);
				}
			}
			else
			{
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				// get indices of x; we sample from this vector and then look up the corresponding weight and EidosValue element
				std::vector<int> index_vector;
				
				for (int value_index = 0; value_index < x_count; ++value_index)
					index_vector.emplace_back(value_index);
				
				// do the sampling
				int64_t contender_count = x_count;
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					if (weights_sum <= 0)
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
					
					int64_t rose = (int64_t)ceil(Eidos_rng_uniform(EIDOS_GSL_RNG) * weights_sum);
					int64_t rose_sum = 0;
					int rose_index;
					
					for (rose_index = 0; rose_index < contender_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_int[index_vector[rose_index]];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(index_vector[rose_index], *x_value, nullptr);
					
					// remove the sampled index since replace==F
					weights_sum -= weights_int[index_vector[rose_index]];
					index_vector.erase(index_vector.begin() + rose_index);
					--contender_count;
				}
			}
		}
		else
		{
			// CODE COVERAGE: This is dead code
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): (internal error) weights vector must be type float or integer." << EidosTerminate(nullptr);
		}
	}
	else
	{
		// weights not supplied; use equal weights
		if (sample_size == 1)
		{
			// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
			return x_value->GetValueAtIndex((int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, x_count), nullptr);
		}
		else if (replace)
		{
			// with replacement, we can just do a series of independent draws
			result_SP = x_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				result->PushValueFromIndexOfEidosValue((int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, x_count), *x_value, nullptr);
		}
		else if ((sample_size == x_count) && (x_value->Type() != EidosValueType::kValueString))
		{
			// full shuffle; optimized case for everything but std::string, which is difficult as usual
			result_SP = x_value->CopyValues();
			EidosValue *result = result_SP.get();
			
			switch (x_value->Type())
			{
				case EidosValueType::kValueVOID: break;
				case EidosValueType::kValueNULL: break;
				case EidosValueType::kValueLogical:
					gsl_ran_shuffle(EIDOS_GSL_RNG, result->LogicalVector_Mutable()->data(), x_count, sizeof(eidos_logical_t));
					break;
				case EidosValueType::kValueInt:
					gsl_ran_shuffle(EIDOS_GSL_RNG, result->IntVector_Mutable()->data(), x_count, sizeof(int64_t));
					break;
				case EidosValueType::kValueFloat:
					gsl_ran_shuffle(EIDOS_GSL_RNG, result->FloatVector_Mutable()->data(), x_count, sizeof(double));
					break;
				case EidosValueType::kValueString:
					// handled below, because gsl_ran_shuffle() can't move std::string safely
					break;
				case EidosValueType::kValueObject:
					gsl_ran_shuffle(EIDOS_GSL_RNG, result->ObjectElementVector_Mutable()->data(), x_count, sizeof(EidosObjectElement *));
					break;
			}
		}
		else
		{
			// get indices of x; we sample from this vector and then look up the corresponding EidosValue element
			result_SP = x_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			std::vector<int> index_vector;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				index_vector.emplace_back(value_index);
			
			// do the sampling
			int64_t contender_count = x_count;
			
			for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
			{
				int rose_index = (int)Eidos_rng_uniform_int(EIDOS_GSL_RNG, (uint32_t)contender_count);
				
				result->PushValueFromIndexOfEidosValue(index_vector[rose_index], *x_value, nullptr);
				
				index_vector[rose_index] = index_vector.back();
				index_vector.resize(--contender_count);
			}
		}
	}
	
	return result_SP;
}

//	(numeric)seq(numeric$ from, numeric$ to, [Nif$ by = NULL], [Ni$ length = NULL])
EidosValue_SP Eidos_ExecuteFunction_seq(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *from_value = p_arguments[0].get();
	EidosValueType from_type = from_value->Type();
	EidosValue *to_value = p_arguments[1].get();
	EidosValueType to_type = to_value->Type();
	EidosValue *by_value = p_arguments[2].get();
	EidosValueType by_type = by_value->Type();
	EidosValue *length_value = p_arguments[3].get();
	EidosValueType length_type = length_value->Type();
	
	if ((from_type == EidosValueType::kValueFloat) && !std::isfinite(from_value->FloatAtIndex(0, nullptr)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires a finite value for the 'from' parameter." << EidosTerminate(nullptr);
	if ((to_type == EidosValueType::kValueFloat) && !std::isfinite(to_value->FloatAtIndex(0, nullptr)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires a finite value for the 'to' parameter." << EidosTerminate(nullptr);
	if ((by_type != EidosValueType::kValueNULL) && (length_type != EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() may be supplied with either 'by' or 'length', but not both." << EidosTerminate(nullptr);
	
	if (length_type != EidosValueType::kValueNULL)
	{
		// A length value has been supplied, so we guarantee a vector of that length even if from==to
		int64_t length = length_value->IntAtIndex(0, nullptr);
		
		if (length <= 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires that length, if supplied, must be > 0." << EidosTerminate(nullptr);
		if (length > 10000000)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() cannot construct a sequence with more than 10000000 entries." << EidosTerminate(nullptr);
		
		if ((from_type == EidosValueType::kValueFloat) || (to_type == EidosValueType::kValueFloat))
		{
			// a float value was given, so we will generate a float sequence in all cases
			double first_value = from_value->FloatAtIndex(0, nullptr);
			double second_value = to_value->FloatAtIndex(0, nullptr);
			
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(length);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t seq_index = 0; seq_index < length; ++seq_index)
			{
				if (seq_index == 0)
					float_result->set_float_no_check(first_value, seq_index);
				else if (seq_index == length - 1)
					float_result->set_float_no_check(second_value, seq_index);
				else
					float_result->set_float_no_check(first_value + (second_value - first_value) * (seq_index / (double)(length - 1)), seq_index);
			}
		}
		else
		{
			// int values were given, so whether we generate a float sequence or an int sequence depends on whether length divides evenly
			int64_t first_value = from_value->IntAtIndex(0, nullptr);
			int64_t second_value = to_value->IntAtIndex(0, nullptr);
			
			if (length == 1)
			{
				// If a sequence of length 1 is requested, generate a single integer at the start
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_value));
			}
			else if ((second_value - first_value) % (length - 1) == 0)
			{
				// length divides evenly, so generate an integer sequence
				int64_t by = (second_value - first_value) / (length - 1);
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(length);
				result_SP = EidosValue_SP(int_result);
				
				for (int64_t seq_index = 0; seq_index < length; ++seq_index)
					int_result->set_int_no_check(first_value + by * seq_index, seq_index);
			}
			else
			{
				// length does not divide evenly, so generate a float sequence
				double by = (second_value - first_value) / (double)(length - 1);
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(length);
				result_SP = EidosValue_SP(float_result);
				
				for (int64_t seq_index = 0; seq_index < length; ++seq_index)
				{
					if (seq_index == 0)
						float_result->set_float_no_check(first_value, seq_index);
					else if (seq_index == length - 1)
						float_result->set_float_no_check(second_value, seq_index);
					else
						float_result->set_float_no_check(first_value + by * seq_index, seq_index);
				}
			}
		}
	}
	else
	{
		// Either a by value has been supplied, or we're using our default step
		if ((from_type == EidosValueType::kValueFloat) || (to_type == EidosValueType::kValueFloat) || (by_type == EidosValueType::kValueFloat))
		{
			// float return case
			double first_value = from_value->FloatAtIndex(0, nullptr);
			double second_value = to_value->FloatAtIndex(0, nullptr);
			double default_by = ((first_value < second_value) ? 1 : -1);
			double by = ((by_type != EidosValueType::kValueNULL) ? by_value->FloatAtIndex(0, nullptr) : default_by);
			
			if (by == 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires by != 0." << EidosTerminate(nullptr);
			if (!std::isfinite(by))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires a finite value for the 'by' parameter." << EidosTerminate(nullptr);
			if (((first_value < second_value) && (by < 0)) || ((first_value > second_value) && (by > 0)))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() by has incorrect sign." << EidosTerminate(nullptr);
			
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(int(1 + ceil((second_value - first_value) / by)));	// take a stab at a reserve size; might not be quite right, but no harm
			result_SP = EidosValue_SP(float_result);
			
			if (by > 0)
				for (double seq_value = first_value; seq_value <= second_value; seq_value += by)
					float_result->push_float(seq_value);
			else
				for (double seq_value = first_value; seq_value >= second_value; seq_value += by)
					float_result->push_float(seq_value);
		}
		else
		{
			// int return case
			int64_t first_value = from_value->IntAtIndex(0, nullptr);
			int64_t second_value = to_value->IntAtIndex(0, nullptr);
			int64_t default_by = ((first_value < second_value) ? 1 : -1);
			int64_t by = ((by_type != EidosValueType::kValueNULL) ? by_value->IntAtIndex(0, nullptr) : default_by);
			
			if (by == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires by != 0." << EidosTerminate(nullptr);
			if (!std::isfinite(by))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires a finite value for the 'by' parameter." << EidosTerminate(nullptr);
			if (((first_value < second_value) && (by < 0)) || ((first_value > second_value) && (by > 0)))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() by has incorrect sign." << EidosTerminate(nullptr);
			
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve((int)(1 + (second_value - first_value) / by));		// take a stab at a reserve size; might not be quite right, but no harm
			result_SP = EidosValue_SP(int_result);
			
			if (by > 0)
				for (int64_t seq_value = first_value; seq_value <= second_value; seq_value += by)
					int_result->push_int(seq_value);
			else
				for (int64_t seq_value = first_value; seq_value >= second_value; seq_value += by)
					int_result->push_int(seq_value);
		}
	}
	
	return result_SP;
}

//	(integer)seqAlong(* x)
EidosValue_SP Eidos_ExecuteFunction_seqAlong(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	// That might seem like an odd policy, since the sequence doesn't match the reality of the value,
	// but it follows R's behavior, and it gives one sequence-element per value-element.
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	int x_count = x_value->Count();
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < x_count; ++value_index)
		int_result->set_int_no_check(value_index, value_index);
	
	return result_SP;
}

//	(integer)seqLen(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_seqLen(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t length = length_value->IntAtIndex(0, nullptr);
	
	if (length < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seqLen): function seqLen() requires length to be greater than or equal to 0 (" << length << " supplied)." << EidosTerminate(nullptr);
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(length);
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < length; ++value_index)
		int_result->set_int_no_check(value_index, value_index);
	
	return result_SP;
}

//	(string)string(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_string(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_string): function string() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_String_ZeroVec;
	
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


//	(logical$)all(logical x, ...)
EidosValue_SP Eidos_ExecuteFunction_all(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	result_SP = gStaticEidosValue_LogicalT;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		
		if (arg_value->Type() != EidosValueType::kValueLogical)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_all): function all() requires that all arguments be of type logical." << EidosTerminate(nullptr);
		
		int arg_count = arg_value->Count();
		const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
		
		for (int value_index = 0; value_index < arg_count; ++value_index)
			if (!logical_data[value_index])
			{
				result_SP = gStaticEidosValue_LogicalF;
				break;
			}
	}
	
	return result_SP;
}

//	(logical$)any(logical x, ...)
EidosValue_SP Eidos_ExecuteFunction_any(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	result_SP = gStaticEidosValue_LogicalF;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		
		if (arg_value->Type() != EidosValueType::kValueLogical)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_any): function any() requires that all arguments be of type logical." << EidosTerminate(nullptr);
		
		int arg_count = arg_value->Count();
		const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
		
		for (int value_index = 0; value_index < arg_count; ++value_index)
			if (logical_data[value_index])
			{
				result_SP = gStaticEidosValue_LogicalT;
				break;
			}
	}
	
	return result_SP;
}

//	(void)cat(* x, [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_cat(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH catn() BELOW!
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValueType x_type = x_value->Type();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	std::string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	
	for (int value_index = 0; value_index < x_count; ++value_index)
	{
		if (value_index > 0)
			output_stream << separator;
		
		if (x_type == EidosValueType::kValueObject)
			output_stream << *x_value->ObjectElementAtIndex(value_index, nullptr);
		else
			output_stream << x_value->StringAtIndex(value_index, nullptr);
	}
	
	return gStaticEidosValueVOID;
}

//	(void)catn([* x = ""], [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_catn(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH cat() ABOVE!
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValueType x_type = x_value->Type();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	std::string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	
	for (int value_index = 0; value_index < x_count; ++value_index)
	{
		if (value_index > 0)
			output_stream << separator;
		
		if (x_type == EidosValueType::kValueObject)
			output_stream << *x_value->ObjectElementAtIndex(value_index, nullptr);
		else
			output_stream << x_value->StringAtIndex(value_index, nullptr);
	}
	
	output_stream << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(string)format(string$ format, numeric x)
EidosValue_SP Eidos_ExecuteFunction_format(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *format_value = p_arguments[0].get();
	std::string format = format_value->StringAtIndex(0, nullptr);
	EidosValue *x_value = p_arguments[1].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
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
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); only one % escape is allowed." << EidosTerminate(nullptr);
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
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '+' specified more than once." << EidosTerminate(nullptr);
						
						flag_plus = true;
						++pos;	// skip the '+'
					}
					else if (flag == '-')
					{
						if (flag_minus)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '-' specified more than once." << EidosTerminate(nullptr);
						
						flag_minus = true;
						++pos;	// skip the '-'
					}
					else if (flag == ' ')
					{
						if (flag_space)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag ' ' specified more than once." << EidosTerminate(nullptr);
						
						flag_space = true;
						++pos;	// skip the ' '
					}
					else if (flag == '#')
					{
						if (flag_pound)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '#' specified more than once." << EidosTerminate(nullptr);
						
						flag_pound = true;
						++pos;	// skip the '#'
					}
					else if (flag == '0')
					{
						if (flag_zero)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '0' specified more than once." << EidosTerminate(nullptr);
						
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
						if (x_type != EidosValueType::kValueInt)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type integer." << EidosTerminate(nullptr);
					}
					else if ((conv_ch == 'f') || (conv_ch == 'F') || (conv_ch == 'e') || (conv_ch == 'E') || (conv_ch == 'g') || (conv_ch == 'G'))
					{
						if (x_type != EidosValueType::kValueFloat)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type float." << EidosTerminate(nullptr);
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' not supported." << EidosTerminate(nullptr);
					}
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); missing conversion specifier after '%'." << EidosTerminate(nullptr);
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
	if (x_type == EidosValueType::kValueInt)
	{
		std::string new_conv_string;
		
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
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): (internal error) bad format string in function format(); conversion specifier '" << conv_ch << "' not recognized." << EidosTerminate(nullptr);		// CODE COVERAGE: This is dead code
		
		format.replace(conversion_specifier_pos, 1, new_conv_string);
	}
	
	// Check for possibilities that produce undefined behavior according to the C++11 standard
	if (flag_pound && ((conv_ch == 'd') || (conv_ch == 'i')))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); the flag '#' may not be used with the conversion specifier '" << conv_ch << "'." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		// singleton case
		std::string result_string;
		
		if (x_type == EidosValueType::kValueInt)
			result_string = EidosStringFormat(format, x_value->IntAtIndex(0, nullptr));
		else if (x_type == EidosValueType::kValueFloat)
			result_string = EidosStringFormat(format, x_value->FloatAtIndex(0, nullptr));
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
	}
	else
	{
		// non-singleton x vector, with a singleton format vector
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
		result_SP = EidosValue_SP(string_result);
		
		if (x_type == EidosValueType::kValueInt)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(EidosStringFormat(format, x_value->IntAtIndex(value_index, nullptr)));
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(EidosStringFormat(format, x_value->FloatAtIndex(value_index, nullptr)));
		}
	}
	
	return result_SP;
}

//	(logical$)identical(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_identical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	
	return IdenticalEidosValues(x_value, y_value) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
}

//	(*)ifelse(logical test, * trueValues, * falseValues)
EidosValue_SP Eidos_ExecuteFunction_ifelse(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *test_value = p_arguments[0].get();
	int test_count = test_value->Count();
	const eidos_logical_t *logical_vec = (*test_value->LogicalVector()).data();
	
	EidosValue *trueValues_value = p_arguments[1].get();
	EidosValueType trueValues_type = trueValues_value->Type();
	int trueValues_count = trueValues_value->Count();
	
	EidosValue *falseValues_value = p_arguments[2].get();
	EidosValueType falseValues_type = falseValues_value->Type();
	int falseValues_count = falseValues_value->Count();
	
	if (trueValues_type != falseValues_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): function ifelse() requires arguments 2 and 3 to be the same type (" << trueValues_type << " and " << falseValues_type << " supplied)." << EidosTerminate(nullptr);
	
	if ((trueValues_count == test_count) && (falseValues_count == test_count))
	{
		// All three are equal counts, so we can do the whole thing in parallel
		if (test_count > 1)
		{
			// Use direct access to make this fast
			if (trueValues_type == EidosValueType::kValueLogical)
			{
				const eidos_logical_t *true_vec = trueValues_value->LogicalVector()->data();
				const eidos_logical_t *false_vec = falseValues_value->LogicalVector()->data();
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					logical_result->set_logical_no_check(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index], value_index);
				
				result_SP = logical_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueInt)
			{
				const int64_t *true_data = trueValues_value->IntVector()->data();
				const int64_t *false_data = falseValues_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					int_result->set_int_no_check(logical_vec[value_index] ? true_data[value_index] : false_data[value_index], value_index);
				
				result_SP = int_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueFloat)
			{
				const double *true_data = trueValues_value->FloatVector()->data();
				const double *false_data = falseValues_value->FloatVector()->data();
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					float_result->set_float_no_check(logical_vec[value_index] ? true_data[value_index] : false_data[value_index], value_index);
				
				result_SP = float_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueString)
			{
				const std::vector<std::string> &true_vec = (*trueValues_value->StringVector());
				const std::vector<std::string> &false_vec = (*falseValues_value->StringVector());
				EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
				EidosValue_String_vector *string_result = string_result_SP->Reserve(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					string_result->PushString(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index]);
				
				result_SP = string_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueObject)
			{
				const EidosObjectClass *trueValues_class = ((EidosValue_Object *)trueValues_value)->Class();
				const EidosObjectClass *falseValues_class = ((EidosValue_Object *)falseValues_value)->Class();
				
				if (trueValues_class != falseValues_class)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): objects of different types cannot be mixed in function ifelse()." << EidosTerminate(nullptr);
				
				EidosObjectElement * const *true_vec = trueValues_value->ObjectElementVector()->data();
				EidosObjectElement * const *false_vec = falseValues_value->ObjectElementVector()->data();
				EidosValue_Object_vector_SP object_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(trueValues_class));
				EidosValue_Object_vector *object_result = object_result_SP->resize_no_initialize_RR(test_count);
				
				if (object_result->UsesRetainRelease())
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_no_previous_RR(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index], value_index);
				}
				else
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_NORR(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index], value_index);
				}
				
				result_SP = object_result_SP;
			}
		}
		
		if (!result_SP)
		{
			// General case
			result_SP = trueValues_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int value_index = 0; value_index < test_count; ++value_index)
			{
				if (logical_vec[value_index])
					result->PushValueFromIndexOfEidosValue(value_index, *trueValues_value, nullptr);
				else
					result->PushValueFromIndexOfEidosValue(value_index, *falseValues_value, nullptr);
			}
		}
	}
	else if ((trueValues_count == 1) && (falseValues_count == 1))
	{
		// trueValues and falseValues are both singletons, so we can prefetch both values
		if (test_count > 1)
		{
			// Use direct access to make this fast
			if (trueValues_type == EidosValueType::kValueLogical)
			{
				eidos_logical_t true_value = trueValues_value->LogicalAtIndex(0, nullptr);
				eidos_logical_t false_value = falseValues_value->LogicalAtIndex(0, nullptr);
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					logical_result->set_logical_no_check(logical_vec[value_index] ? true_value : false_value, value_index);
				
				result_SP = logical_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueInt)
			{
				int64_t true_value = trueValues_value->IntAtIndex(0, nullptr);
				int64_t false_value = falseValues_value->IntAtIndex(0, nullptr);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					int_result->set_int_no_check(logical_vec[value_index] ? true_value : false_value, value_index);
				
				result_SP = int_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueFloat)
			{
				double true_value = trueValues_value->FloatAtIndex(0, nullptr);
				double false_value = falseValues_value->FloatAtIndex(0, nullptr);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					float_result->set_float_no_check(logical_vec[value_index] ? true_value : false_value, value_index);
				
				result_SP = float_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueString)
			{
				std::string true_value = trueValues_value->StringAtIndex(0, nullptr);
				std::string false_value = falseValues_value->StringAtIndex(0, nullptr);
				EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
				EidosValue_String_vector *string_result = string_result_SP->Reserve(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					string_result->PushString(logical_vec[value_index] ? true_value : false_value);
				
				result_SP = string_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueObject)
			{
				const EidosObjectClass *trueValues_class = ((EidosValue_Object *)trueValues_value)->Class();
				const EidosObjectClass *falseValues_class = ((EidosValue_Object *)falseValues_value)->Class();
				
				if (trueValues_class != falseValues_class)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): objects of different types cannot be mixed in function ifelse()." << EidosTerminate(nullptr);
				
				EidosObjectElement *true_value = trueValues_value->ObjectElementAtIndex(0, nullptr);
				EidosObjectElement *false_value = falseValues_value->ObjectElementAtIndex(0, nullptr);
				EidosValue_Object_vector_SP object_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(trueValues_class));
				EidosValue_Object_vector *object_result = object_result_SP->resize_no_initialize_RR(test_count);
				
				if (object_result->UsesRetainRelease())
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_no_previous_RR(logical_vec[value_index] ? true_value : false_value, value_index);
				}
				else
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_NORR(logical_vec[value_index] ? true_value : false_value, value_index);
				}
				
				result_SP = object_result_SP;
			}
		}
		
		if (!result_SP)
		{
			// General case; this is hit when (trueValues_count == falseValues_count == 1) && (test_count == 0), since the
			// test_count > 1 case is handled directly above and the test_count == 1 case is further above.
			result_SP = trueValues_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int value_index = 0; value_index < test_count; ++value_index)
			{
				// CODE COVERAGE: The interior of the loop here is actually dead code; see above.
				if (logical_vec[value_index])
					result->PushValueFromIndexOfEidosValue(0, *trueValues_value, nullptr);
				else
					result->PushValueFromIndexOfEidosValue(0, *falseValues_value, nullptr);
			}
		}
	}
	else if ((trueValues_count == test_count) && (falseValues_count == 1))
	{
		// vector trueValues, singleton falseValues; I suspect this case is less common so I'm deferring optimization
		result_SP = trueValues_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < test_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(value_index, *trueValues_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(0, *falseValues_value, nullptr);
		}
	}
	else if ((trueValues_count == 1) && (falseValues_count == test_count))
	{
		// singleton trueValues, vector falseValues; I suspect this case is less common so I'm deferring optimization
		result_SP = trueValues_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < test_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(0, *trueValues_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(value_index, *falseValues_value, nullptr);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): function ifelse() requires that trueValues and falseValues each be either of length 1, or equal in length to test." << EidosTerminate(nullptr);
	}
	
	// Dimensionality of the result always matches that of the test parameter; this is R's policy and it makes sense
	result_SP->CopyDimensionsFromValue(test_value);
	
	return result_SP;
}

//	(integer)match(* x, * table)
EidosValue_SP Eidos_ExecuteFunction_match(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	EidosValue *table_value = p_arguments[1].get();
	EidosValueType table_type = table_value->Type();
	int table_count = table_value->Count();
	
	if (x_type != table_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_match): function match() requires arguments x and table to be the same type." << EidosTerminate(nullptr);
	
	if (x_type == EidosValueType::kValueNULL)
	{
		result_SP = gStaticEidosValue_Integer_ZeroVec;
		return result_SP;
	}
	
	if ((x_count == 1) && (table_count == 1))
	{
		// Handle singleton matching separately, to allow the use of the fast vector API below
		if (x_type == EidosValueType::kValueLogical)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->LogicalAtIndex(0, nullptr) == table_value->LogicalAtIndex(0, nullptr) ? 0 : -1));
		else if (x_type == EidosValueType::kValueInt)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr) == table_value->IntAtIndex(0, nullptr) ? 0 : -1));
		else if (x_type == EidosValueType::kValueFloat)
		{
			double f0 = x_value->FloatAtIndex(0, nullptr), f1 = table_value->FloatAtIndex(0, nullptr);
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(((std::isnan(f0) && std::isnan(f1)) || (f0 == f1)) ? 0 : -1));
		}
		else if (x_type == EidosValueType::kValueString)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->StringAtIndex(0, nullptr) == table_value->StringAtIndex(0, nullptr) ? 0 : -1));
		else if (x_type == EidosValueType::kValueObject)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->ObjectElementAtIndex(0, nullptr) == table_value->ObjectElementAtIndex(0, nullptr) ? 0 : -1));
	}
	else if (x_count == 1)	// && (table_count != 1)
	{
		int table_index;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t value0 = x_value->LogicalAtIndex(0, nullptr);
			const eidos_logical_t *logical_data1 = table_value->LogicalVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == logical_data1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t value0 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int_data1 = table_value->IntVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == int_data1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double value0 = x_value->FloatAtIndex(0, nullptr);
			const double *float_data1 = table_value->FloatVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
			{
				double f1 = float_data1[table_index];
				
				if ((std::isnan(value0) && std::isnan(f1)) || (value0 == f1))
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			std::string value0 = x_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec1 = *table_value->StringVector();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == string_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else // if (x_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value0 = x_value->ObjectElementAtIndex(0, nullptr);
			EidosObjectElement * const *objelement_vec1 = table_value->ObjectElementVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == objelement_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		
		if (table_index == table_count)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1));
	}
	else if (table_count == 1)	// && (x_count != 1)
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t value1 = table_value->LogicalAtIndex(0, nullptr);
			const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(logical_data0[value_index] == value1 ? 0 : -1, value_index);
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t value1 = table_value->IntAtIndex(0, nullptr);
			const int64_t *int_data0 = x_value->IntVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(int_data0[value_index] == value1 ? 0 : -1, value_index);
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double value1 = table_value->FloatAtIndex(0, nullptr);
			const double *float_data0 = x_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				double f0 = float_data0[value_index];
				
				int_result->set_int_no_check(((std::isnan(f0) && std::isnan(value1)) || (f0 == value1)) ? 0 : -1, value_index);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			std::string value1 = table_value->StringAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(string_vec0[value_index] == value1 ? 0 : -1, value_index);
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObjectElement *value1 = table_value->ObjectElementAtIndex(0, nullptr);
			EidosObjectElement * const *objelement_vec0 = x_value->ObjectElementVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(objelement_vec0[value_index] == value1 ? 0 : -1, value_index);
		}
	}
	else						// ((x_count != 1) && (table_count != 1))
	{
		// We can use the fast vector API; we want match() to be very fast since it is a common bottleneck
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		int table_index;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
			const eidos_logical_t *logical_data1 = table_value->LogicalVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				for (table_index = 0; table_index < table_count; ++table_index)
					if (logical_data0[value_index] == logical_data1[table_index])
						break;
				
				int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
			}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = table_value->IntVector()->data();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table (i.e. std::unordered_map) to speed up lookups from O(N) to O(1)
				std::unordered_map<int64_t, int64_t> fromValueToIndex;
				
				for (table_index = 0; table_index < table_count; ++table_index)
					fromValueToIndex.insert(std::pair<int64_t, int64_t>(int_data1[table_index], table_index));	// does nothing if the key is already in the map
				
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(int_data0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result->set_int_no_check(find_index, value_index);
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
						if (int_data0[value_index] == int_data1[table_index])
							break;
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data0 = x_value->FloatVector()->data();
			const double *float_data1 = table_value->FloatVector()->data();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table (i.e. std::unordered_map) to speed up lookups from O(N) to O(1)
				// we have to use a custom comparator so that NAN==NAN is true, so that NAN gets matched correctly
				auto equal = [](const double& l, const double& r) { if (std::isnan(l) && std::isnan(r)) return true; return l == r; };
				std::unordered_map<double, int64_t, std::hash<double>, decltype(equal)> fromValueToIndex(0, std::hash<double>{}, equal);
				
				for (table_index = 0; table_index < table_count; ++table_index)
					fromValueToIndex.insert(std::pair<double, int64_t>(float_data1[table_index], table_index));	// does nothing if the key is already in the map
				
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(float_data0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result->set_int_no_check(find_index, value_index);
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
					{
						double f0 = float_data0[value_index], f1 = float_data1[table_index];
						
						if ((std::isnan(f0) && std::isnan(f1)) || (f0 == f1))	// need to make NAN match NAN
							break;
					}
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *table_value->StringVector();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table (i.e. std::unordered_map) to speed up lookups from O(N) to O(1)
				std::unordered_map<std::string, int64_t> fromValueToIndex;
				
				for (table_index = 0; table_index < table_count; ++table_index)
					fromValueToIndex.insert(std::pair<std::string, int64_t>(string_vec1[table_index], table_index));	// does nothing if the key is already in the map
				
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(string_vec0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result->set_int_no_check(find_index, value_index);
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
						if (string_vec0[value_index] == string_vec1[table_index])
							break;
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObjectElement * const *objelement_vec0 = x_value->ObjectElementVector()->data();
			EidosObjectElement * const *objelement_vec1 = table_value->ObjectElementVector()->data();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table (i.e. std::unordered_map) to speed up lookups from O(N) to O(1)
				std::unordered_map<EidosObjectElement *, int64_t> fromValueToIndex;
				
				for (table_index = 0; table_index < table_count; ++table_index)
					fromValueToIndex.insert(std::pair<EidosObjectElement *, int64_t>(objelement_vec1[table_index], table_index));	// does nothing if the key is already in the map
				
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(objelement_vec0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result->set_int_no_check(find_index, value_index);
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
						if (objelement_vec0[value_index] == objelement_vec1[table_index])
							break;
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
	}
	
	return result_SP;
}

//	(integer)nchar(string x)
EidosValue_SP Eidos_ExecuteFunction_nchar(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->StringAtIndex(0, nullptr).size()));
	}
	else
	{
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			int_result->set_int_no_check(string_vec[value_index].size(), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(integer)order(+ x, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_order(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		// This handles all the zero-length cases by returning integer(0)
		result_SP = gStaticEidosValue_Integer_ZeroVec;
	}
	else if (x_count == 1)
	{
		// This handles all the singleton cases by returning 0
		result_SP = gStaticEidosValue_Integer0;
	}
	else
	{
		// Here we handle the vector cases, which can be done with direct access
		EidosValueType x_type = x_value->Type();
		bool ascending = p_arguments[1]->LogicalAtIndex(0, nullptr);
		std::vector<int64_t> order;
		
		if (x_type == EidosValueType::kValueLogical)
			order = EidosSortIndexes(x_value->LogicalVector()->data(), x_count, ascending);
		else if (x_type == EidosValueType::kValueInt)
			order = EidosSortIndexes(x_value->IntVector()->data(), x_count, ascending);
		else if (x_type == EidosValueType::kValueFloat)
			order = EidosSortIndexes(x_value->FloatVector()->data(), x_count, ascending);
		else if (x_type == EidosValueType::kValueString)
			order = EidosSortIndexes(*x_value->StringVector(), ascending);
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(order));
		result_SP = EidosValue_SP(int_result);
	}
	
	return result_SP;
}

//	(string$)paste(..., [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_paste(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH paste0() BELOW!
	size_t argument_count = p_arguments.size();
	std::string separator = p_arguments[argument_count - 1]->StringAtIndex(0, nullptr);
	std::string result_string;
	
	// SLiM 3.5 breaks backward compatibility for paste() because the second argument, which would have been interpreted as "sep="
	// before, now gets eaten by the ellipsis unless it is explicitly named.  Here we try to issue a useful warning about this,
	// for the strings that seem like the most likely to be used as separators.
	if ((argument_count == 3) && (separator == " ") &&
		((p_arguments[1]->Type() == EidosValueType::kValueString) && (p_arguments[1]->Count() == 1)))
	{
		std::string pseudosep = p_arguments[1]->StringAtIndex(0, nullptr);	// perhaps intended as sep, and now sep=" " has been used as a default?
		
		if ((pseudosep == "") || (pseudosep == " ") || (pseudosep == "\t") || (pseudosep == "\n") || (pseudosep == ",") || (pseudosep == ", ") || (pseudosep == " , ") || (pseudosep == ";") || (pseudosep == "; ") || (pseudosep == " ; "))
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_paste): function paste() changed its semantics in Eidos 2.5 (SLiM 3.5).  The second argument here is no longer interpreted to be a separator string; if you want those semantics, use 'sep=' to name the second argument, as in 'paste(1:5, sep=\",\");'.  That is the way to regain backward compatibility.  If, on the other hand, you do not intend the second argument here to be a separator string, you can get rid of this warning by appending the second argument using the + operator instead.  For example, you would transform 'x = paste(1:5, \",\");' into 'x = paste(1:5) + \" ,\";'.  You can also use suppressWarnings() to avoid this warning message." << std::endl;
		}
	}
	
	for (size_t argument_index = 0; argument_index < argument_count - 1; ++argument_index)
	{
		EidosValue *x_value = p_arguments[argument_index].get();
		int x_count = x_value->Count();
		EidosValueType x_type = x_value->Type();
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			if (!((value_index == 0) && (argument_index == 0)))
				result_string.append(separator);
			
			if (x_type == EidosValueType::kValueObject)
			{
				std::ostringstream oss;
				
				oss << *x_value->ObjectElementAtIndex(value_index, nullptr);
				
				result_string.append(oss.str());
			}
			else
				result_string.append(x_value->StringAtIndex(value_index, nullptr));
		}
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
}

//	(string$)paste0(...)
EidosValue_SP Eidos_ExecuteFunction_paste0(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH paste() ABOVE!
	size_t argument_count = p_arguments.size();
	std::string result_string;
	
	for (size_t argument_index = 0; argument_index < argument_count; ++argument_index)
	{
		EidosValue *x_value = p_arguments[argument_index].get();
		int x_count = x_value->Count();
		EidosValueType x_type = x_value->Type();
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			if (x_type == EidosValueType::kValueObject)
			{
				std::ostringstream oss;
				
				oss << *x_value->ObjectElementAtIndex(value_index, nullptr);
				
				result_string.append(oss.str());
			}
			else
				result_string.append(x_value->StringAtIndex(value_index, nullptr));
		}
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
}

//	(void)print(* x)
EidosValue_SP Eidos_ExecuteFunction_print(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	
	p_interpreter.ExecutionOutputStream() << *x_value << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(*)rev(* x)
EidosValue_SP Eidos_ExecuteFunction_rev(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int value_index = x_count - 1; value_index >= 0; --value_index)
		result->PushValueFromIndexOfEidosValue(value_index, *x_value, nullptr);
	
	return result_SP;
}

//	(integer$)size(* x)
//	(integer$)length(* x)
EidosValue_SP Eidos_ExecuteFunction_size_length(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->Count()));
	
	return result_SP;
}

//	(+)sort(+ x, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_sort(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int value_index = 0; value_index < x_count; ++value_index)
		result->PushValueFromIndexOfEidosValue(value_index, *x_value, nullptr);
	
	result->Sort(p_arguments[1]->LogicalAtIndex(0, nullptr));
	
	return result_SP;
}

//	(object)sortBy(object x, string$ property, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_sortBy(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class()))->resize_no_initialize_RR(x_count);
	result_SP = EidosValue_SP(object_result);
	
	if (object_result->UsesRetainRelease())
	{
		for (int value_index = 0; value_index < x_count; ++value_index)
			object_result->set_object_element_no_check_no_previous_RR(x_value->ObjectElementAtIndex(value_index, nullptr), value_index);
	}
	else
	{
		for (int value_index = 0; value_index < x_count; ++value_index)
			object_result->set_object_element_no_check_NORR(x_value->ObjectElementAtIndex(value_index, nullptr), value_index);
	}
	
	object_result->SortBy(p_arguments[1]->StringAtIndex(0, nullptr), p_arguments[2]->LogicalAtIndex(0, nullptr));
	
	return result_SP;
}

//	(void)str(* x)
EidosValue_SP Eidos_ExecuteFunction_str(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	int x_dimcount = x_value->DimensionCount();
	const int64_t *x_dims = x_value->Dimensions();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (x_count == 0)
	{
		// zero-length vectors get printed according to the standard code in EidosValue
		x_value->Print(output_stream);
	}
	else
	{
		// start with the type, and then the class for object-type values
		output_stream << x_type;
		
		if (x_type == EidosValueType::kValueObject)
			output_stream << "<" << x_value->ElementType() << ">";
		
		// then print the ranges for each dimension
		output_stream << " [";
		
		if (x_dimcount == 1)
			output_stream << "0:" << (x_count - 1) << "] ";
		else
		{
			for (int dim_index = 0; dim_index < x_dimcount; ++dim_index)
			{
				if (dim_index > 0)
					output_stream << ", ";
				output_stream << "0:" << (x_dims[dim_index] - 1);
			}
			
			output_stream << "] ";
		}
		
		// finally, print up to two values, if available, followed by an ellipsis if not all values were printed
		int output_count = std::min(2, x_count);
		
		for (int output_index = 0; output_index < output_count; ++output_index)
		{
			EidosValue_SP value = x_value->GetValueAtIndex(output_index, nullptr);
			
			if (output_index > 0)
				output_stream << gEidosStr_space_string;
			
			output_stream << *value;
		}
		
		if (x_count > output_count)
			output_stream << " ...";
	}
	
	output_stream << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(string)strsplit(string$ x, [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_strsplit(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
	result_SP = EidosValue_SP(string_result);
	
	std::string joined_string = x_value->StringAtIndex(0, nullptr);
	std::string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	std::string::size_type start_idx = 0, sep_idx;
	
	if (separator.length() == 0)
	{
		// special-case a zero-length separator
		for (const char &ch : joined_string)
			string_result->PushString(std::string(&ch, 1));
	}
	else
	{
		// non-zero-length separator
		while (true)
		{
			sep_idx = joined_string.find(separator, start_idx);
			
			if (sep_idx == std::string::npos)
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

//	(string)substr(string x, integer first, [Ni last = NULL])
EidosValue_SP Eidos_ExecuteFunction_substr(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *arg_last = p_arguments[2].get();
	EidosValueType arg_last_type = arg_last->Type();
	
	if (x_count == 1)
	{
		const std::string &string_value = x_value->StringAtIndex(0, nullptr);
		int64_t len = (int64_t)string_value.size();
		EidosValue *arg_first = p_arguments[1].get();
		int arg_first_count = arg_first->Count();
		
		if (arg_first_count != x_count)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of first to be 1, or equal to the size of x." << EidosTerminate(nullptr);
		
		int64_t first0 = arg_first->IntAtIndex(0, nullptr);
		
		if (arg_last_type != EidosValueType::kValueNULL)
		{
			// last supplied
			int arg_last_count = arg_last->Count();
			
			if (arg_last_count != x_count)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of last to be 1, or equal to the size of x." << EidosTerminate(nullptr);
			
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
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		EidosValue *arg_first = p_arguments[1].get();
		int arg_first_count = arg_first->Count();
		bool first_singleton = (arg_first_count == 1);
		
		if (!first_singleton && (arg_first_count != x_count))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of first to be 1, or equal to the size of x." << EidosTerminate(nullptr);
		
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
		result_SP = EidosValue_SP(string_result);
		
		int64_t first0 = arg_first->IntAtIndex(0, nullptr);
		
		if (arg_last_type != EidosValueType::kValueNULL)
		{
			// last supplied
			int arg_last_count = arg_last->Count();
			bool last_singleton = (arg_last_count == 1);
			
			if (!last_singleton && (arg_last_count != x_count))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of last to be 1, or equal to the size of x." << EidosTerminate(nullptr);
			
			int64_t last0 = arg_last->IntAtIndex(0, nullptr);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
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
			for (int value_index = 0; value_index < x_count; ++value_index)
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

//	(integer)tabulate(integer bin, [Ni$ maxbin = NULL])
EidosValue_SP Eidos_ExecuteFunction_tabulate(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *bin_value = p_arguments[0].get();
	int bin_count = bin_value->Count();
	
	EidosValue *maxbin_value = p_arguments[1].get();
	EidosValueType maxbin_type = maxbin_value->Type();
	
	// set up to work with either a singleton or a non-singleton vector
	int64_t singleton_value = (bin_count == 1) ? bin_value->IntAtIndex(0, nullptr) : 0;
	const int64_t *int_data = (bin_count == 1) ? &singleton_value : bin_value->IntVector()->data();
	
	// determine maxbin
	int64_t maxbin;
	
	if (maxbin_type == EidosValueType::kValueNULL)
	{
		maxbin = 0;
		for (int value_index = 0; value_index < bin_count; ++value_index)
		{
			int64_t value = int_data[value_index];
			if (value > maxbin)
				maxbin = value;
		}
	}
	else
	{
		maxbin = maxbin_value->IntAtIndex(0, nullptr);
	}
	
	if (maxbin < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_tabulate): function tabulate() requires maxbin to be greater than or equal to 0." << EidosTerminate(nullptr);
	
	// set up the result vector and zero it out
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(maxbin + 1);
	result_SP = EidosValue_SP(int_result);
	
	for (int result_index = 0; result_index <= maxbin; ++result_index)
		int_result->set_int_no_check(0, result_index);
	
	// do the tabulation
	int64_t *result_data = int_result->data();
	
	for (int value_index = 0; value_index < bin_count; ++value_index)
	{
		int64_t value = int_data[value_index];
		
		if ((value >= 0) && (value <= maxbin))
			result_data[value]++;
	}
	
	return result_SP;
}

//	(*)unique(* x, [logical$ preserveOrder = T])
EidosValue_SP Eidos_ExecuteFunction_unique(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	return UniqueEidosValue(p_arguments[0].get(), false, p_arguments[1]->LogicalAtIndex(0, nullptr));
}

//	(integer)which(logical x)
EidosValue_SP Eidos_ExecuteFunction_which(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
	EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < x_count; ++value_index)
		if (logical_data[value_index])
			int_result->push_int(value_index);
	
	return result_SP;
}

//	(integer$)whichMax(+ x)
EidosValue_SP Eidos_ExecuteFunction_whichMax(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		int first_index = 0;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t max = x_value->LogicalAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					eidos_logical_t temp = logical_data[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t max = x_value->IntAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const int64_t *int_data = x_value->IntVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double max = x_value->FloatAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
				const double *float_data = x_value->FloatVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					double temp = float_data[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			std::string max = x_value->StringAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_String_vector; we can use the fast API
				const std::vector<std::string> &string_vec = *x_value->StringVector();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_index));
	}
	
	return result_SP;
}

//	(integer$)whichMin(+ x)
EidosValue_SP Eidos_ExecuteFunction_whichMin(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		int first_index = 0;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t min = x_value->LogicalAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					eidos_logical_t temp = logical_data[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t min = x_value->IntAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const int64_t *int_data = x_value->IntVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double min = x_value->FloatAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
				const double *float_data = x_value->FloatVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					double temp = float_data[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			std::string min = x_value->StringAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_String_vector; we can use the fast API
				const std::vector<std::string> &string_vec = *x_value->StringVector();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
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
EidosValue_SP Eidos_ExecuteFunction_asFloat(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(x_value->FloatAtIndex(value_index, nullptr), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(integer)asInteger(+ x)
EidosValue_SP Eidos_ExecuteFunction_asInteger(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			int_result->set_int_no_check(x_value->IntAtIndex(value_index, nullptr), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)asLogical(+ x)
EidosValue_SP Eidos_ExecuteFunction_asLogical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if ((x_count == 1) && (x_value->DimensionCount() == 1))
	{
		// Use the global constants, but only if we do not have to impose a dimensionality upon the value below
		result_SP = (x_value->LogicalAtIndex(0, nullptr) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(x_value->LogicalAtIndex(value_index, nullptr), value_index);
		
		result_SP->CopyDimensionsFromValue(x_value);
	}
	
	return result_SP;
}

//	(string)asString(+ x)
EidosValue_SP Eidos_ExecuteFunction_asString(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if ((x_count == 0) && (x_value->Type() == EidosValueType::kValueNULL))
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_NULL));
	}
	else if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(x_value->StringAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			string_result->PushString(x_value->StringAtIndex(value_index, nullptr));
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(string$)elementType(* x)
EidosValue_SP Eidos_ExecuteFunction_elementType(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(x_value->ElementType()));
	
	return result_SP;
}

//	(logical$)isFloat(* x)
EidosValue_SP Eidos_ExecuteFunction_isFloat(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueFloat) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isInteger(* x)
EidosValue_SP Eidos_ExecuteFunction_isInteger(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueInt) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isLogical(* x)
EidosValue_SP Eidos_ExecuteFunction_isLogical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueLogical) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isNULL(* x)
EidosValue_SP Eidos_ExecuteFunction_isNULL(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueNULL) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isObject(* x)
EidosValue_SP Eidos_ExecuteFunction_isObject(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueObject) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isString(* x)
EidosValue_SP Eidos_ExecuteFunction_isString(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueString) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(string$)type(* x)
EidosValue_SP Eidos_ExecuteFunction_type(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(StringForEidosValueType(x_value->Type())));
	
	return result_SP;
}


// ************************************************************************************
//
//	matrix and array functions
//

#pragma mark -
#pragma mark Matrix and array functions
#pragma mark -


//	(*)apply(* x, integer margin, string$ lambdaSource)
EidosValue_SP Eidos_ExecuteFunction_apply(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_dimcount = x_value->DimensionCount();
	const int64_t *x_dim = x_value->Dimensions();
	
	if (x_dimcount < 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): function apply() requires parameter x to be a matrix or array." << std::endl << "NOTE: The apply() function was renamed sapply() in Eidos 1.6, and a new function named apply() has been added; you may need to change this call to be a call to sapply() instead." << EidosTerminate(nullptr);
	
	// Determine the margins requested and check their validity
	EidosValue *margin_value = p_arguments[1].get();
	int margin_count = margin_value->Count();
	std::vector<int> margins;
	std::vector<int64_t> margin_sizes;
	
	if (margin_count <= 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): function apply() requires that margins be specified." << EidosTerminate(nullptr);
	
	for (int margin_index = 0; margin_index < margin_count; ++margin_index)
	{
		int64_t margin = margin_value->IntAtIndex(margin_index, nullptr);
		
		if ((margin < 0) || (margin >= x_dimcount))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): specified margin " << margin << " is out of range in function apply(); margin indices are zero-based, and thus must be from 0 to size(dim(x)) - 1." << EidosTerminate(nullptr);
		
		for (int margin_index_2 = 0; margin_index_2 < margin_index; ++margin_index_2)
		{
			int64_t margin_2 = margin_value->IntAtIndex(margin_index_2, nullptr);
			
			if (margin_2 == margin)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): specified margin " << margin << " was already specified to function apply(); a given margin may be specified only once." << EidosTerminate(nullptr);
		}
		
		margins.push_back((int)margin);
		margin_sizes.push_back(x_dim[margin]);
	}
	
	// Get the lambda string and cache its script
	EidosValue *lambda_value = p_arguments[2].get();
	EidosValue_String_singleton *lambda_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[2].get());
	EidosScript *script = (lambda_value_singleton ? lambda_value_singleton->CachedScript() : nullptr);
	
	// Errors in lambdas should be reported for the lambda script, not for the calling script,
	// if possible.  In the GUI this does not work well, however; there, errors should be
	// reported as occurring in the call to sapply().  Here we save off the current
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
		script = new EidosScript(lambda_value->StringAtIndex(0, nullptr));
		
		gEidosCharacterStartOfError = -1;
		gEidosCharacterEndOfError = -1;
		gEidosCharacterStartOfErrorUTF16 = -1;
		gEidosCharacterEndOfErrorUTF16 = -1;
		gEidosCurrentScript = script;
		gEidosExecutingRuntimeScript = true;
		
		try
		{
			script->Tokenize();
			script->ParseInterpreterBlockToAST(false);
		}
		catch (...)
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
		
		if (lambda_value_singleton)
			lambda_value_singleton->SetCachedScript(script);
	}
	
	std::vector<EidosValue_SP> results;
	
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
		bool consistent_return_length = true;	// consistent across all values, including NULLs?
		int return_length = -1;					// what the consistent length is
		
		// Set up inclusion_indices and inclusion_counts vectors as a skeleton for each marginal subset below
		std::vector<std::vector<int64_t>> inclusion_indices;	// the chosen indices for each dimension
		std::vector<int> inclusion_counts;						// the number of chosen indices for each dimension
		
		for (int subset_index = 0; subset_index < x_dimcount; ++subset_index)
		{
			int dim_size = (int)x_dim[subset_index];
			std::vector<int64_t> indices;
			
			for (int dim_index = 0; dim_index < dim_size; ++dim_index)
				indices.push_back(dim_index);
			
			inclusion_counts.push_back((int)indices.size());
			inclusion_indices.emplace_back(indices);
		}
		
		for (int margin_index = 0; margin_index < margin_count; ++margin_index)
			inclusion_counts[margins[margin_index]] = 1;
		
		// Iterate through each index for the marginal dimensions, in order
		std::vector<int64_t> margin_counter(margin_count, 0);
		
		do
		{
			// margin_counter has values for each margin; generate a slice through x with them
			for (int margin_index = 0; margin_index < margin_count; ++margin_index)
			{
				int margin_dim = margins[margin_index];
				
				inclusion_indices[margin_dim].clear();
				inclusion_indices[margin_dim].push_back(margin_counter[margin_index]);
			}
			
			EidosValue_SP apply_value = x_value->Subset(inclusion_indices, true, nullptr);
			
			// Set the iterator variable "applyValue" to the value
			symbols.SetValueForSymbolNoCopy(gEidosID_applyValue, std::move(apply_value));
			
			// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
			// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
			// bugs that would otherwise not manifest.
			EidosValue_SP &&return_value_SP = interpreter.EvaluateInterpreterBlock(false, true);		// do not print output, return the last statement value
			
			if (return_value_SP->Type() == EidosValueType::kValueVOID)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): each iteration within apply() must return a non-void value." << EidosTerminate(nullptr);
			
			if (consistent_return_length)
			{
				int length = return_value_SP->Count();
				
				if (return_length == -1)
					return_length = length;
				else if (length != return_length)
					consistent_return_length = false;
			}
			
			results.emplace_back(return_value_SP);
			
			// increment margin_counter in the base system of margin_sizes
			int margin_counter_index = 0;
			
			do
			{
				if (++margin_counter[margin_counter_index] == margin_sizes[margin_counter_index])
				{
					margin_counter[margin_counter_index] = 0;
					margin_counter_index++;		// carry
				}
				else
					break;
			}
			while (margin_counter_index < margin_count);
			
			// if we carried out off the top, we are done iterating across all margins
			if (margin_counter_index == margin_count)
				break;
		}
		while (true);
		
		// We do not want a leftover applyValue symbol in the symbol table, so we remove it now
		symbols.RemoveValueForSymbol(gEidosID_applyValue);
		
		// Assemble all the individual results together, just as c() does
		interpreter.FlushExecutionOutputToStream(p_interpreter.ExecutionOutputStream());
		
		result_SP = ConcatenateEidosValues(results, true, false);	// allow NULL but not VOID
		
		// Set the dimensions of the result.  If the returns from the lambda were not consistent in their
		// length and dimensions, we just return the plain vector without dimensions; we can't do anything
		// with it.  With returns of a consistent length n, (1) if n == 1, the return value will be a vector
		// if only one margin was used, or a matrix/array of dimension dim(x)[margin] if more than one
		// margin was used, (2) if n > 1, the return value will be an array of dimension c(n, dim(x)[margin]),
		// or (3) if n == 0, the return value will be a length zero vector of the appropriate type.  I think
		// we follow R's policy more or less exactly; that is my intent.
		if (consistent_return_length && (return_length > 0))
		{
			if (return_length == 1)
			{
				if (margin_count == 1)
				{
					// do nothing and return a plain vector
				}
				else
				{
					// dimensions of dim(x)[margin]; in other words, the sizes of the marginal dimensions of x, in the specified order of the margins
					result_SP->SetDimensions(margin_count, margin_sizes.data());
				}
			}
			else // return_length > 1)
			{
				// dimensions of c(n, dim(x)[margin]); in other words, n rows, and the marginal dim sizes after that
				int64_t *dims = (int64_t *)malloc((margin_count + 1) * sizeof(int64_t));
				
				dims[0] = return_length;
				
				for (int dim_index = 0; dim_index < margin_count; ++dim_index)
					dims[dim_index + 1] = margin_sizes[dim_index];
				
				result_SP->SetDimensions(margin_count + 1, dims);
				
				free(dims);
			}
		}
	}
	catch (...)
	{
		// If exceptions throw, then we want to set up the error information to highlight the
		// sapply() that failed, since we can't highlight the actual error.  (If exceptions
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
		
		if (!lambda_value_singleton)
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
	
	if (!lambda_value_singleton)
		delete script;
	
	return result_SP;
}

// (*)array(* data, integer dim)
EidosValue_SP Eidos_ExecuteFunction_array(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *data_value = p_arguments[0].get();
	EidosValue *dim_value = p_arguments[1].get();
	
	int data_count = data_value->Count();
	int dim_count = dim_value->Count();
	
	if (dim_count < 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_array): function array() requires at least two dimensions (i.e., at least a matrix)" << EidosTerminate(nullptr);
	
	int64_t dim_product = 1;
	
	for (int dim_index = 0; dim_index < dim_count; ++dim_index)
	{
		int64_t dim = dim_value->IntAtIndex(dim_index, nullptr);
		
		if (dim < 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_array): function array() requires that all dimensions be >= 1." << EidosTerminate(nullptr);
		
		dim_product *= dim;
	}
	
	if (data_count != dim_product)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_array): function array() requires a data vector with a length equal to the product of the proposed dimensions." << EidosTerminate(nullptr);
	
	// construct the array from the data and dimensions
	result_SP = data_value->CopyValues();
	
	result_SP->SetDimensions(dim_count, dim_value->IntVector()->data());
	
	return result_SP;
}

// (*)cbind(...)
EidosValue_SP Eidos_ExecuteFunction_cbind(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// First check the type and class of the result; NULL may be mixed in, but otherwise all arguments must be the same type and class
	EidosValueType result_type = EidosValueType::kValueNULL;
	const EidosObjectClass *result_class = gEidos_UndefinedClassObject;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		EidosValueType arg_type = arg->Type();
		
		if (arg_type == EidosValueType::kValueNULL)
			continue;
		else if (result_type == EidosValueType::kValueNULL)
			result_type = arg_type;
		else if (arg_type != result_type)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() requires that all arguments be the same type (or NULL)." << EidosTerminate(nullptr);
		
		if (arg_type == EidosValueType::kValueObject)
		{
			EidosValue_Object *arg_object = (EidosValue_Object *)arg;
			const EidosObjectClass *arg_class = arg_object->Class();
			
			if (arg_class == gEidos_UndefinedClassObject)
				continue;
			else if (result_class == gEidos_UndefinedClassObject)
				result_class = arg_class;
			else if (arg_class != result_class)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() requires that all object arguments be of the same class." << EidosTerminate(nullptr);
		}
	}
	
	if (result_type == EidosValueType::kValueNULL)
		return gStaticEidosValueNULL;
	
	// Next determine the dimensions of the result; every argument must be zero-size or a match for the dimensions we already have
	int64_t result_rows = 0;
	int64_t result_cols = 0;
	int64_t result_length = 0;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		int64_t arg_length = arg->Count();
		
		// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
		if (arg_length == 0)
			continue;
		
		int arg_dimcount = arg->DimensionCount();
		
		if ((arg_dimcount != 1) && (arg_dimcount != 2))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() requires that all arguments be vectors or matrices." << EidosTerminate(nullptr);
		
		const int64_t *arg_dims = arg->Dimensions();
		int64_t arg_nrow = (arg_dimcount == 1) ? arg_length : arg_dims[0];
		int64_t arg_ncol = (arg_dimcount == 1) ? 1 : arg_dims[1];
		
		if (result_rows == 0)
			result_rows = arg_nrow;
		else if (result_rows != arg_nrow)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() mismatch among arguments in their number of rows." << EidosTerminate(nullptr);
		
		result_cols += arg_ncol;
		result_length += arg_length;
	}
	
	// Construct our result; this is simpler than rbind(), here we basically just concatenate the arguments directly...
	EidosValue_SP result_SP(nullptr);
	
	switch (result_type)
	{
		case EidosValueType::kValueVOID:	break;		// never hit
		case EidosValueType::kValueNULL:	break;		// never hit
		case EidosValueType::kValueLogical:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->reserve(result_length)); break;
		case EidosValueType::kValueInt:		result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve(result_length)); break;
		case EidosValueType::kValueFloat:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(result_length)); break;
		case EidosValueType::kValueString:	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector()); break;
		case EidosValueType::kValueObject:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(result_class))->reserve(result_length)); break;
	}
	
	EidosValue *result = result_SP.get();
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		int64_t arg_length = arg->Count();
		
		// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
		if (arg_length == 0)
			continue;
		
		for (int element_index = 0; element_index < arg_length; ++element_index)
			result->PushValueFromIndexOfEidosValue(element_index, *arg, nullptr);
	}
	
	const int64_t dim_buf[2] = {result_rows, result_cols};
	
	result->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (integer)dim(* x)
EidosValue_SP Eidos_ExecuteFunction_dim(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *data_value = p_arguments[0].get();
	int dim_count = data_value->DimensionCount();
	
	if (dim_count <= 1)
		return gStaticEidosValueNULL;
	
	const int64_t *dim_values = data_value->Dimensions();
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(dim_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int dim_index = 0; dim_index < dim_count; ++dim_index)
		int_result->set_int_no_check(dim_values[dim_index], dim_index);
	
	return result_SP;
}

// (*)drop(* x)
EidosValue_SP Eidos_ExecuteFunction_drop(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	EidosValue *x_value = p_arguments[0].get();
	int source_dimcount = x_value->DimensionCount();
	const int64_t *source_dim = x_value->Dimensions();
	
	if (source_dimcount <= 1)
	{
		// x is already a vector, so just return it
		result_SP = EidosValue_SP(x_value);
	}
	else
	{
		// Count the number of dimensions that have a size != 1
		int needed_dim_count = 0;
		
		for (int dim_index = 0; dim_index < source_dimcount; dim_index++)
			if (source_dim[dim_index] > 1)
				needed_dim_count++;
		
		if (needed_dim_count == source_dimcount)
		{
			// No dimensions can be dropped, so do nothing
			result_SP = EidosValue_SP(x_value);
		}
		else if (needed_dim_count <= 1)
		{
			// A vector is all that is needed
			result_SP = x_value->CopyValues();
			
			result_SP->SetDimensions(1, nullptr);
		}
		else
		{
			// We need to drop some dimensions but still end up with a matrix or array
			result_SP = x_value->CopyValues();
			
			int64_t *dim_buf = (int64_t *)malloc(needed_dim_count * sizeof(int64_t));
			int dim_buf_index = 0;
			
			for (int dim_index = 0; dim_index < source_dimcount; dim_index++)
				if (source_dim[dim_index] > 1)
					dim_buf[dim_buf_index++] = source_dim[dim_index];
			
			result_SP->SetDimensions(needed_dim_count, dim_buf);
			
			free(dim_buf);
		}
	}
	
	return result_SP;
}

// (*)matrix(* data, [integer$ nrow = 1], [integer$ ncol = 1], [logical$ byrow = F])
EidosValue_SP Eidos_ExecuteFunction_matrix(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *data_value = p_arguments[0].get();
	EidosValue *nrow_value = p_arguments[1].get();
	EidosValue *ncol_value = p_arguments[2].get();
	EidosValue *byrow_value = p_arguments[3].get();
	
	int data_count = data_value->Count();
	bool nrow_null = (nrow_value->Type() == EidosValueType::kValueNULL);
	bool ncol_null = (ncol_value->Type() == EidosValueType::kValueNULL);
	
	int64_t nrow, ncol;
	
	if (nrow_null && ncol_null)
	{
		// return a one-column matrix, following R
		ncol = 1;
		nrow = data_count;
	}
	else if (nrow_null)
	{
		// try to infer a row count
		ncol = ncol_value->IntAtIndex(0, nullptr);
		
		if (data_count % ncol == 0)
			nrow = data_count / ncol;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): function matrix() data size is not a multiple of the supplied column count." << EidosTerminate(nullptr);
	}
	else if (ncol_null)
	{
		// try to infer a column count
		nrow = nrow_value->IntAtIndex(0, nullptr);
		
		if (data_count % nrow == 0)
			ncol = data_count / nrow;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): function matrix() data size is not a multiple of the supplied row count." << EidosTerminate(nullptr);
	}
	else
	{
		nrow = nrow_value->IntAtIndex(0, nullptr);
		ncol = ncol_value->IntAtIndex(0, nullptr);
		
		if (data_count != nrow * ncol)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): function matrix() requires a data vector with a length equal to the product of the proposed number of rows and columns." << EidosTerminate(nullptr);
	}
	
	eidos_logical_t byrow = byrow_value->LogicalAtIndex(0, nullptr);
	
	if (byrow)
	{
		// The non-default case: use the values in data to fill rows one by one, requiring transposition here; singletons are easy, though
		if (data_count == 1)
			result_SP = data_value->CopyValues();
		else
		{
			// non-singleton byrow case, so we need to actually transpose
			result_SP = data_value->NewMatchingType();
			
			EidosValue *result = result_SP.get();
			
			for (int64_t value_index = 0; value_index < data_count; ++value_index)
			{
				int64_t dest_col = (value_index / nrow);
				int64_t dest_row = (value_index % nrow);
				int64_t src_index = dest_col + dest_row * ncol;
				
				result->PushValueFromIndexOfEidosValue((int)src_index, *data_value, nullptr);
			}
		}
	}
	else
	{
		// The default case: use the values in data to fill columns one by one, mirroring the internal layout used by Eidos
		result_SP = data_value->CopyValues();
	}
	
	const int64_t dim_buf[2] = {nrow, ncol};
	
	result_SP->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (numeric)matrixMult(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_matrixMult(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	
	if (x_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): function matrixMult() x is not a matrix." << EidosTerminate(nullptr);
	if (y_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): function matrixMult() y is not a matrix." << EidosTerminate(nullptr);
	
	EidosValueType x_type = x_value->Type();
	EidosValueType y_type = y_value->Type();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): function matrixMult() requires that x and y are the same type." << EidosTerminate(nullptr);
	
	const int64_t *x_dim = x_value->Dimensions();
	int64_t x_rows = x_dim[0];
	int64_t x_cols = x_dim[1];
	int64_t x_length = x_rows * x_cols;
	const int64_t *y_dim = y_value->Dimensions();
	int64_t y_rows = y_dim[0];
	int64_t y_cols = y_dim[1];
	int64_t y_length = y_rows * y_cols;
	
	if (x_cols != y_rows)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): in function matrixMult(), x and y are not conformable." << EidosTerminate(nullptr);
	
	EidosValue_SP result_SP(nullptr);
	int64_t result_rows = x_rows;
	int64_t result_cols = y_cols;
	int64_t result_length = result_rows * result_cols;
	
	// Do the multiplication.  We split out singleton cases here so that the big general case can run fast.
	// We also split by integer integer versus float, because we have to in order to multiply.  Finally,
	// in the integer cases we have to check for overflows; see EidosInterpreter::Evaluate_Mult().
	
	if ((x_length == 1) && (y_length == 1))
	{
		// a 1x1 vector multiplied by a 1x1 vector
		if (x_type == EidosValueType::kValueInt)
		{
			int64_t x_singleton = x_value->IntAtIndex(0, nullptr);
			int64_t y_singleton = y_value->IntAtIndex(0, nullptr);
			int64_t multiply_result;
			bool overflow = Eidos_mul_overflow(x_singleton, y_singleton, &multiply_result);
			
			if (overflow)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(multiply_result));
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			double x_singleton = x_value->FloatAtIndex(0, nullptr);
			double y_singleton = y_value->FloatAtIndex(0, nullptr);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_singleton * y_singleton));
		}
	}
	else if (x_length == 1)
	{
		// a 1x1 vector multiplied by a row vector
		if (x_type == EidosValueType::kValueInt)
		{
			int64_t x_singleton = x_value->IntAtIndex(0, nullptr);
			const int64_t *y_data = y_value->IntVector()->data();
			EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t y_index = 0; y_index < y_length; ++y_index)
			{
				int64_t y_operand = y_data[y_index];
				int64_t multiply_result;
				bool overflow = Eidos_mul_overflow(x_singleton, y_operand, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
				
				result->set_int_no_check(multiply_result, y_index);
			}
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			double x_singleton = x_value->FloatAtIndex(0, nullptr);
			const double *y_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t y_index = 0; y_index < y_length; ++y_index)
				result->set_float_no_check(x_singleton * y_data[y_index], y_index);
		}
	}
	else if (y_length == 1)
	{
		// a column vector multiplied by a 1x1 vector
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *x_data = x_value->IntVector()->data();
			int64_t y_singleton = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t x_index = 0; x_index < x_length; ++x_index)
			{
				int64_t x_operand = x_data[x_index];
				int64_t multiply_result;
				bool overflow = Eidos_mul_overflow(x_operand, y_singleton, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
				
				result->set_int_no_check(multiply_result, x_index);
			}
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			const double *x_data = x_value->FloatVector()->data();
			double y_singleton = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t x_index = 0; x_index < x_length; ++x_index)
				result->set_float_no_check(x_data[x_index] * y_singleton, x_index);
		}
	}
	else
	{
		// this is the general case; we have non-singleton matrices for both x and y, so we can divide by integer/float and use direct access
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *x_data = x_value->IntVector()->data();
			const int64_t *y_data = y_value->IntVector()->data();
			EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t result_col_index = 0; result_col_index < result_cols; ++result_col_index)
			{
				for (int64_t result_row_index = 0; result_row_index < result_rows; ++result_row_index)
				{
					int64_t result_index = result_col_index * result_rows + result_row_index;
					int64_t summation_result = 0;
					
					for (int64_t product_index = 0; product_index < x_cols; ++product_index)	// x_cols == y_rows; this is the dimension that matches, n x m * m x p
					{
						int64_t x_row = result_row_index;
						int64_t x_col = product_index;
						int64_t x_index = x_col * x_rows + x_row;
						int64_t y_row = product_index;
						int64_t y_col = result_col_index;
						int64_t y_index = y_col * y_rows + y_row;
						int64_t x_operand = x_data[x_index];
						int64_t y_operand = y_data[y_index];
						int64_t multiply_result;
						bool overflow = Eidos_mul_overflow(x_operand, y_operand, &multiply_result);
						
						if (overflow)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
						
						int64_t add_result;
						bool overflow2 = Eidos_add_overflow(summation_result, multiply_result, &add_result);
						
						if (overflow2)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer addition overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
						
						summation_result = add_result;
					}
					
					result->set_int_no_check(summation_result, result_index);
				}
			}
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			const double *x_data = x_value->FloatVector()->data();
			const double *y_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t result_col_index = 0; result_col_index < result_cols; ++result_col_index)
			{
				for (int64_t result_row_index = 0; result_row_index < result_rows; ++result_row_index)
				{
					int64_t result_index = result_col_index * result_rows + result_row_index;
					double summation_result = 0;
					
					for (int64_t product_index = 0; product_index < x_cols; ++product_index)	// x_cols == y_rows; this is the dimension that matches, n x m * m x p
					{
						int64_t x_row = result_row_index;
						int64_t x_col = product_index;
						int64_t x_index = x_col * x_rows + x_row;
						int64_t y_row = product_index;
						int64_t y_col = result_col_index;
						int64_t y_index = y_col * y_rows + y_row;
						double x_operand = x_data[x_index];
						double y_operand = y_data[y_index];
						
						summation_result += x_operand * y_operand;
					}
					
					result->set_float_no_check(summation_result, result_index);
				}
			}
		}
	}
	
	const int64_t dim_buf[2] = {result_rows, result_cols};
	
	result_SP->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (integer$)ncol(* x)
EidosValue_SP Eidos_ExecuteFunction_ncol(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *data_value = p_arguments[0].get();
	int dim_count = data_value->DimensionCount();
	
	if (dim_count < 2)
		return gStaticEidosValueNULL;
	
	const int64_t *dim_values = data_value->Dimensions();
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(dim_values[1]));
}

// (integer$)nrow(* x)
EidosValue_SP Eidos_ExecuteFunction_nrow(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *data_value = p_arguments[0].get();
	int dim_count = data_value->DimensionCount();
	
	if (dim_count < 2)
		return gStaticEidosValueNULL;
	
	const int64_t *dim_values = data_value->Dimensions();
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(dim_values[0]));
}

// (*)rbind(...)
EidosValue_SP Eidos_ExecuteFunction_rbind(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// First check the type and class of the result; NULL may be mixed in, but otherwise all arguments must be the same type and class
	EidosValueType result_type = EidosValueType::kValueNULL;
	const EidosObjectClass *result_class = gEidos_UndefinedClassObject;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		EidosValueType arg_type = arg->Type();
		
		if (arg_type == EidosValueType::kValueNULL)
			continue;
		else if (result_type == EidosValueType::kValueNULL)
			result_type = arg_type;
		else if (arg_type != result_type)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() requires that all arguments be the same type (or NULL)." << EidosTerminate(nullptr);
		
		if (arg_type == EidosValueType::kValueObject)
		{
			EidosValue_Object *arg_object = (EidosValue_Object *)arg;
			const EidosObjectClass *arg_class = arg_object->Class();
			
			if (arg_class == gEidos_UndefinedClassObject)
				continue;
			else if (result_class == gEidos_UndefinedClassObject)
				result_class = arg_class;
			else if (arg_class != result_class)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() requires that all object arguments be of the same class." << EidosTerminate(nullptr);
		}
	}
	
	if (result_type == EidosValueType::kValueNULL)
		return gStaticEidosValueNULL;
	
	// Next determine the dimensions of the result; every argument must be zero-size or a match for the dimensions we already have
	int64_t result_rows = 0;
	int64_t result_cols = 0;
	int64_t result_length = 0;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		int64_t arg_length = arg->Count();
		
		// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
		if (arg_length == 0)
			continue;
		
		int arg_dimcount = arg->DimensionCount();
		
		if ((arg_dimcount != 1) && (arg_dimcount != 2))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() requires that all arguments be vectors or matrices." << EidosTerminate(nullptr);
		
		const int64_t *arg_dims = arg->Dimensions();
		int64_t arg_nrow = (arg_dimcount == 1) ? 1 : arg_dims[0];
		int64_t arg_ncol = (arg_dimcount == 1) ? arg_length : arg_dims[1];
		
		if (result_cols == 0)
			result_cols = arg_ncol;
		else if (result_cols != arg_ncol)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() mismatch among arguments in their number of columns." << EidosTerminate(nullptr);
		
		result_rows += arg_nrow;
		result_length += arg_length;
	}
	
	// Construct our result; for each column, we scan through our arguments and append rows from that column; not very efficient, but general...
	EidosValue_SP result_SP(nullptr);
	
	switch (result_type)
	{
		case EidosValueType::kValueVOID:	break;		// never hit
		case EidosValueType::kValueNULL:	break;		// never hit
		case EidosValueType::kValueLogical:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->reserve(result_length)); break;
		case EidosValueType::kValueInt:		result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve(result_length)); break;
		case EidosValueType::kValueFloat:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(result_length)); break;
		case EidosValueType::kValueString:	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector()); break;
		case EidosValueType::kValueObject:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(result_class))->reserve(result_length)); break;
	}
	
	EidosValue *result = result_SP.get();
	
	for (int col_index = 0; col_index < result_cols; ++col_index)
	{
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg = p_arguments[arg_index].get();
			int64_t arg_length = arg->Count();
			
			// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
			if (arg_length == 0)
				continue;
			
			int arg_dimcount = arg->DimensionCount();
			
			if (arg_dimcount == 1)
			{
				// vector; take the nth value to fill the nth column in the result
				result->PushValueFromIndexOfEidosValue(col_index, *arg, nullptr);
			}
			else
			{
				// matrix; take the whole nth column of the matrix to fill the nth column in the result
				const int64_t *arg_dims = arg->Dimensions();
				int64_t arg_nrow = arg_dims[0];
				
				for (int64_t row_index = 0; row_index < arg_nrow; ++row_index)
					result->PushValueFromIndexOfEidosValue((int)(col_index * arg_nrow + row_index), *arg, nullptr);
			}
		}
	}
	
	const int64_t dim_buf[2] = {result_rows, result_cols};
	
	result->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (*)t(* x)
EidosValue_SP Eidos_ExecuteFunction_t(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	EidosValue *x_value = p_arguments[0].get();
	
	if (x_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_t): in function t() x is not a matrix." << EidosTerminate(nullptr);
	
	const int64_t *source_dim = x_value->Dimensions();
	int64_t source_rows = source_dim[0];
	int64_t source_cols = source_dim[1];
	int64_t dest_rows = source_cols;
	int64_t dest_cols = source_rows;
	
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int64_t col_index = 0; col_index < dest_cols; ++col_index)
	{
		for (int64_t row_index = 0; row_index < dest_rows; ++row_index)
		{
			//int64_t dest_index = col_index * dest_rows + row_index;
			int64_t source_index = row_index * source_rows + col_index;
			
			result->PushValueFromIndexOfEidosValue((int)source_index, *x_value, nullptr);
		}
	}
	
	const int64_t dim_buf[2] = {dest_rows, dest_cols};
	
	result->SetDimensions(2, dim_buf);
	
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
EidosValue_SP Eidos_ExecuteFunction_createDirectory(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *path_value = p_arguments[0].get();
	std::string base_path = path_value->StringAtIndex(0, nullptr);
	std::string error_string;
	bool success = Eidos_CreateDirectory(base_path, &error_string);
	
	// Emit a warning if there was one
	if (error_string.length())
		if (!gEidosSuppressWarnings)
			p_interpreter.ExecutionOutputStream() << error_string << std::endl;
	
	return (success ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

//	(logical$)deleteFile(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_deleteFile(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	result_SP = ((remove(file_path.c_str()) == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	return result_SP;
}

//	(logical$)fileExists(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_fileExists(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	struct stat file_info;
	bool path_exists = (stat(file_path.c_str(), &file_info) == 0);
	
	result_SP = (path_exists ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	return result_SP;
}

//	(string)filesAtPath(string$ path, [logical$ fullPaths = F])
EidosValue_SP Eidos_ExecuteFunction_filesAtPath(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *path_value = p_arguments[0].get();
	std::string base_path = path_value->StringAtIndex(0, nullptr);
	int base_path_length = (int)base_path.length();
	bool base_path_ends_in_slash = (base_path_length > 0) && (base_path[base_path_length-1] == '/');
	std::string path = Eidos_ResolvedPath(base_path);
	bool fullPaths = p_arguments[1]->LogicalAtIndex(0, nullptr);
	
	// this code modified from GNU: http://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html#Simple-Directory-Lister
	// I'm not sure if it works on Windows... sigh...
	DIR *dp;
	
	dp = opendir(path.c_str());
	
	if (dp != NULL)
	{
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		while (true)
		{
			// BCH 5 April 2018: switching to readdir() instead of readdir_r().  It seemed like readdir_r() would be better in principle, since
			// it's thread-safe, but apparently it has been deprecated in POSIX because of several issues, and readdir() is now recommended
			// after all (and is thread-safe in most situations, supposedly, but if we ever go multithreaded that will need to be checked.)
			errno = 0;
			
			struct dirent *ep = readdir(dp);
			int error = errno;
			
			if (!ep && error)
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_filesAtPath): function filesAtPath() encountered error code " << error << " while iterating through path " << path << "." << std::endl;
				result_SP = gStaticEidosValueNULL;
				break;
			}
			
			if (!ep)
				break;
			
			std::string filename = ep->d_name;
			
			if (fullPaths)
				filename = base_path + (base_path_ends_in_slash ? "" : "/") + filename;
			
			string_result->PushString(filename);
		}
		
		(void)closedir(dp);
	}
	else
	{
		if (!gEidosSuppressWarnings)
			p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_filesAtPath): function filesAtPath() could not open path " << path << "." << std::endl;
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}

//	(string$)getwd(void)
EidosValue_SP Eidos_ExecuteFunction_getwd(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	std::string cwd = Eidos_CurrentDirectory();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(cwd));
	
	return result_SP;
}

//	(string)readFile(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_readFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	// read the contents in
	std::ifstream file_stream(file_path.c_str());
	
	if (!file_stream.is_open())
	{
		if (!gEidosSuppressWarnings)
			p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_readFile): function readFile() could not read file at path " << file_path << "." << std::endl;
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		std::string line;
		
		while (getline(file_stream, line))
			string_result->PushString(line);
		
		if (file_stream.bad())
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_readFile): function readFile() encountered stream errors while reading file at path " << file_path << "." << std::endl;
			result_SP = gStaticEidosValueNULL;
		}
	}
	
	return result_SP;
}

//	(string$)setwd(string$ path)
EidosValue_SP Eidos_ExecuteFunction_setwd(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	// Get the path; this code is identical to getwd() above, except it makes the value invisible
	std::string cwd = Eidos_CurrentDirectory();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(cwd));
	result_SP->SetInvisible(true);
	
	// Now set the path
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string final_path = Eidos_ResolvedPath(base_path);
	
	errno = 0;
	int retval = chdir(final_path.c_str());
	
	if (retval == -1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setwd): the working directory could not be set (error " << errno << ")" << EidosTerminate(nullptr);
	
	return result_SP;
}

//	(logical$)writeFile(string$ filePath, string contents, [logical$ append = F], [logical$ compress = F])
EidosValue_SP Eidos_ExecuteFunction_writeFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	// the second argument is the file contents to write
	EidosValue *contents_value = p_arguments[1].get();
	int contents_count = contents_value->Count();
	
	// the third argument is an optional append flag, F by default
	bool append = p_arguments[2]->LogicalAtIndex(0, nullptr);
	
	// and then there is a flag for optional gzip compression
	bool do_compress = p_arguments[3]->LogicalAtIndex(0, nullptr);
	
	if (do_compress && !Eidos_string_hasSuffix(file_path, ".gz"))
		file_path.append(".gz");
	
	// write the contents out
	if (do_compress)
	{
		// compression using zlib; very different from the no-compression case, unfortunately, because here we use C-based APIs
		#if EIDOS_BUFFER_ZIP_APPENDS
		if (append)
		{
			// the append case gets handled by _Eidos_FlushZipBuffer() if EIDOS_BUFFER_ZIP_APPENDS is true
			auto buffer_iter = gEidosBufferedZipAppendData.find(file_path);
			
			if (buffer_iter == gEidosBufferedZipAppendData.end())
				buffer_iter = gEidosBufferedZipAppendData.emplace(std::pair<std::string, std::string>(file_path, "")).first;
			
			std::string &buffer = buffer_iter->second;
			
			// append lines to the buffer; this copies bytes, which is a bit inefficient but shouldn't matter in the big picture
			if (contents_count == 1)
			{
				buffer.append(contents_value->StringAtIndex(0, nullptr));
				buffer.append(1, '\n');
			}
			else
			{
				const std::vector<std::string> &string_vec = *contents_value->StringVector();
				
				for (int value_index = 0; value_index < contents_count; ++value_index)
				{
					buffer.append(string_vec[value_index]);
					buffer.append(1, '\n');
				}
			}
			
			// if the buffer data exceeds a (somewhat arbitrary) 128K buffer maximum, write it out and remove the buffer entry
			bool result = true;
			
			if (buffer.length() > (1024L * 128L))
			{
				result = _Eidos_FlushZipBuffer(file_path, buffer);
				gEidosBufferedZipAppendData.erase(buffer_iter);
				
				if (!result && !gEidosSuppressWarnings)
					p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeFile): function writeFile() could not flush zip buffer to file at path " << file_path << "." << std::endl;
				
			}
			
			result_SP = result ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
		}
		else
		#endif
		{
			// this code can handle both the append and the non-append case, but the append case may generate very low-quality
			// compression (potentially even worse than the uncompressed data) due to having an excess of gzip headers
			gzFile gzf = z_gzopen(file_path.c_str(), append ? "ab" : "wb");
			
			if (!gzf)
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeFile): function writeFile() could not write to file at path " << file_path << "." << std::endl;
				result_SP = gStaticEidosValue_LogicalF;
			}
			else
			{
				std::ostringstream outstream;
				
				if (contents_count == 1)
				{
					outstream << contents_value->StringAtIndex(0, nullptr) << std::endl;
				}
				else
				{
					const std::vector<std::string> &string_vec = *contents_value->StringVector();
					
					for (int value_index = 0; value_index < contents_count; ++value_index)
						outstream << string_vec[value_index] << std::endl;
				}
				
				std::string outstring = outstream.str();
				const char *outcstr = outstring.c_str();
				size_t outcstr_length = strlen(outcstr);
				
				// do the writing with zlib
				bool failed = true;
				int retval = gzbuffer(gzf, 128*1024L);	// bigger buffer for greater speed
				
				if (retval != -1)
				{
					retval = gzwrite(gzf, outcstr, (unsigned)outcstr_length);
					
					if (retval != 0)
					{
						retval = gzclose_w(gzf);
						
						if (retval == Z_OK)
							failed = false;
					}
				}
				
				if (failed)
					if (!gEidosSuppressWarnings)
						p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeFile): function writeFile() encountered zlib errors while writing to file at path " << file_path << "." << std::endl;
				
				result_SP = failed ? gStaticEidosValue_LogicalF : gStaticEidosValue_LogicalT;
			}
		}
	}
	else
	{
		// no compression
		std::ofstream file_stream(file_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (!file_stream.is_open())
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeFile): function writeFile() could not write to file at path " << file_path << "." << std::endl;
			result_SP = gStaticEidosValue_LogicalF;
		}
		else
		{
			if (contents_count == 1)
			{
				// BCH 27 January 2017: changed to add a newline after the last line, too, so appending new content to a file produces correct line breaks
				// Note that system() and writeTempFile() do not append this newline, to allow the user to exactly specify file contents,
				// but with writeFile() appending seems more likely to me; we'll see if anybody squawks
				file_stream << contents_value->StringAtIndex(0, nullptr) << std::endl;
			}
			else
			{
				const std::vector<std::string> &string_vec = *contents_value->StringVector();
				
				for (int value_index = 0; value_index < contents_count; ++value_index)
				{
					file_stream << string_vec[value_index];
					
					// Add newlines after all lines but the last
					// BCH 27 January 2017: changed to add a newline after the last line, too, so appending new content to a file produces correct line breaks
					//if (value_index + 1 < contents_count)
					file_stream << std::endl;
				}
			}
			
			if (file_stream.bad())
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeFile): function writeFile() encountered stream errors while writing to file at path " << file_path << "." << std::endl;
				result_SP = gStaticEidosValue_LogicalF;
			}
			else
			{
				result_SP = gStaticEidosValue_LogicalT;
			}
		}
	}
	
	return result_SP;
}

//	(string$)writeTempFile(string$ prefix, string$ suffix, string contents, [logical$ compress = F])
EidosValue_SP Eidos_ExecuteFunction_writeTempFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	if (!Eidos_SlashTmpExists())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeTempFile): in function writeTempFile(), the /tmp directory appears not to exist or is not writeable." << EidosTerminate(nullptr);
	
	EidosValue *prefix_value = p_arguments[0].get();
	std::string prefix = prefix_value->StringAtIndex(0, nullptr);
	EidosValue *suffix_value = p_arguments[1].get();
	std::string suffix = suffix_value->StringAtIndex(0, nullptr);
	
	// the third argument is the file contents to write
	EidosValue *contents_value = p_arguments[2].get();
	int contents_count = contents_value->Count();
	
	// and then there is a flag for optional gzip compression
	bool do_compress = p_arguments[3]->LogicalAtIndex(0, nullptr);
	
	if (do_compress && !Eidos_string_hasSuffix(suffix, ".gz"))
		suffix.append(".gz");
	
	// generate the filename template from the prefix/suffix
	std::string filename = prefix + "XXXXXX" + suffix;
	std::string file_path_template = "/tmp/" + filename;		// the /tmp directory is standard on OS X and Linux; probably on all Un*x systems
	
	if ((filename.find("~") != std::string::npos) || (filename.find("/") != std::string::npos))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeTempFile): in function writeTempFile(), prefix and suffix may not contain '~' or '/'; they may specify only a filename." << EidosTerminate(nullptr);
	
	// write the contents out; thanks to http://stackoverflow.com/questions/499636/how-to-create-a-stdofstream-to-a-temp-file for the temp file creation code
	char *file_path_cstr = strdup(file_path_template.c_str());
	int fd = Eidos_mkstemps(file_path_cstr, (int)suffix.length());
	
	if (fd == -1)
	{
		free(file_path_cstr);
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeTempFile): (internal error) Eidos_mkstemps() failed!" << EidosTerminate(nullptr);
	}
	
	if (do_compress)
	{
		// compression using zlib; very different from the no-compression case, unfortunately, because here we use C-based APIs
		gzFile gzf = z_gzdopen(fd, "wb");
		
		if (!gzf)
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() could not write to file at path " << file_path_cstr << "." << std::endl;
			result_SP = gStaticEidosValue_StringEmpty;
		}
		else
		{
			std::ostringstream outstream;
			
			if (contents_count == 1)
			{
				outstream << contents_value->StringAtIndex(0, nullptr);
			}
			else
			{
				const std::vector<std::string> &string_vec = *contents_value->StringVector();
				
				for (int value_index = 0; value_index < contents_count; ++value_index)
					outstream << string_vec[value_index] << std::endl;
			}
			
			std::string outstring = outstream.str();
			const char *outcstr = outstring.c_str();
			size_t outcstr_length = strlen(outcstr);
			
			// do the writing with zlib
			bool failed = true;
			int retval = gzbuffer(gzf, 128*1024L);	// bigger buffer for greater speed
			
			if (retval != -1)
			{
				retval = gzwrite(gzf, outcstr, (unsigned)outcstr_length);
				
				if (retval != 0)
				{
					retval = gzclose_w(gzf);
					
					if (retval == Z_OK)
						failed = false;
				}
			}
			
			if (failed)
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() encountered zlib errors while writing to file at path " << file_path_cstr << "." << std::endl;
				result_SP = gStaticEidosValue_StringEmpty;
			}
			else
			{
				std::string file_path(file_path_cstr);
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(file_path));
			}
		}
	}
	else
	{
		// no compression
		std::string file_path(file_path_cstr);
		std::ofstream file_stream(file_path.c_str(), std::ios_base::out);
		close(fd);	// opened by Eidos_mkstemps()
		
		if (!file_stream.is_open())
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() could not write to file at path " << file_path << "." << std::endl;
			result_SP = gStaticEidosValue_StringEmpty;
		}
		else
		{
			if (contents_count == 1)
			{
				file_stream << contents_value->StringAtIndex(0, nullptr);	// no final newline in this case, so the user can precisely specify the file contents if desired
			}
			else
			{
				const std::vector<std::string> &string_vec = *contents_value->StringVector();
				
				for (int value_index = 0; value_index < contents_count; ++value_index)
				{
					file_stream << string_vec[value_index];
					
					// Add newlines after all lines but the last
					// BCH 27 January 2017: changed to add a newline after the last line, too, so appending new content to a file produces correct line breaks
					//if (value_index + 1 < contents_count)
					file_stream << std::endl;
				}
			}
			
			if (file_stream.bad())
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ExecutionOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() encountered stream errors while writing to file at path " << file_path << "." << std::endl;
				result_SP = gStaticEidosValue_StringEmpty;
			}
			else
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(file_path));
			}
		}
	}
	
	free(file_path_cstr);
	return result_SP;
}


// ************************************************************************************
//
//	color manipulation functions
//
#pragma mark -
#pragma mark Color manipulation functions
#pragma mark -


//	(string)cmColors(integer$ n)
EidosValue_SP Eidos_ExecuteFunction_cmColors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex(0, nullptr);
	char hex_chars[8];
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cmColors): cmColors() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double red, green, blue;
		
		Eidos_ColorPaletteLookup(fraction, EidosColorPalette::kPalette_cm, red, green, blue);
		Eidos_GetColorString(red, green, blue, hex_chars);
		string_result->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

//	(string)colors(numeric x, string$ name)
EidosValue_SP Eidos_ExecuteFunction_colors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *name_value = p_arguments[1].get();
	std::string name = name_value->StringAtIndex(0, nullptr);
	EidosColorPalette palette;
	char hex_chars[8];
	
	if (name == "cm")				palette = EidosColorPalette::kPalette_cm;
	else if (name == "heat")		palette = EidosColorPalette::kPalette_heat;
	else if (name == "terrain")		palette = EidosColorPalette::kPalette_terrain;
	else if (name == "parula")		palette = EidosColorPalette::kPalette_parula;
	else if (name == "hot")			palette = EidosColorPalette::kPalette_hot;
	else if (name == "jet")			palette = EidosColorPalette::kPalette_jet;
	else if (name == "turbo")		palette = EidosColorPalette::kPalette_turbo;
	else if (name == "gray")		palette = EidosColorPalette::kPalette_gray;
	else if (name == "magma")		palette = EidosColorPalette::kPalette_magma;
	else if (name == "inferno")		palette = EidosColorPalette::kPalette_inferno;
	else if (name == "plasma")		palette = EidosColorPalette::kPalette_plasma;
	else if (name == "viridis")		palette = EidosColorPalette::kPalette_viridis;
	else if (name == "cividis")		palette = EidosColorPalette::kPalette_cividis;
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_colors): unrecognized color palette name in colors()." << EidosTerminate(nullptr);
	
	if (x_value->Type() == EidosValueType::kValueInt)
	{
		if (x_value->Count() != 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_colors): colors() requires an integer x parameter value to be singleton (the number of colors to generate)." << EidosTerminate(nullptr);
		
		int64_t x = x_value->IntAtIndex(0, nullptr);
		if ((x < 0) || (x > 100000))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_colors): colors() requires 0 <= x <= 100000." << EidosTerminate(nullptr);
		
		int color_count = (int)x;
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
			double red, green, blue;
			
			Eidos_ColorPaletteLookup(fraction, palette, red, green, blue);
			
			Eidos_GetColorString(red, green, blue, hex_chars);
			string_result->PushString(std::string(hex_chars));
		}
	}
	else if (x_value->Type() == EidosValueType::kValueFloat)
	{
		int color_count = x_value->Count();
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			double fraction = x_value->FloatAtIndex(value_index, nullptr);
			double red, green, blue;
			
			Eidos_ColorPaletteLookup(fraction, palette, red, green, blue);
			
			Eidos_GetColorString(red, green, blue, hex_chars);
			string_result->PushString(std::string(hex_chars));
		}
	}
	
	return result_SP;
}

//	(float)color2rgb(string color)
EidosValue_SP Eidos_ExecuteFunction_color2rgb(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *color_value = p_arguments[0].get();
	int color_count = color_value->Count();
	float r, g, b;
	
	if (color_count == 1)
	{
		Eidos_GetColorComponents(color_value->StringAtIndex(0, nullptr), &r, &g, &b);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{r, g, b});
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(color_count * 3);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			Eidos_GetColorComponents(color_value->StringAtIndex(value_index, nullptr), &r, &g, &b);
			float_result->set_float_no_check(r, value_index);
			float_result->set_float_no_check(g, value_index + color_count);
			float_result->set_float_no_check(b, value_index + color_count + color_count);
		}
		
		const int64_t dim_buf[2] = {color_count, 3};
		
		result_SP->SetDimensions(2, dim_buf);
	}
	
	return result_SP;
}

//	(string)heatColors(integer$ n)
EidosValue_SP Eidos_ExecuteFunction_heatColors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex(0, nullptr);
	char hex_chars[8];
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_heatColors): heatColors() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double red, green, blue;
		
		Eidos_ColorPaletteLookup(fraction, EidosColorPalette::kPalette_heat, red, green, blue);
		Eidos_GetColorString(red, green, blue, hex_chars);
		string_result->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

//	(float)hsv2rgb(float hsv)
EidosValue_SP Eidos_ExecuteFunction_hsv2rgb(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *hsv_value = p_arguments[0].get();
	int hsv_count = hsv_value->Count();
	
	if (((hsv_value->DimensionCount() != 1) || (hsv_count != 3)) &&
		((hsv_value->DimensionCount() != 2) || (hsv_value->Dimensions()[1] != 3)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_hsv2rgb): in function hsv2rgb(), hsv must contain exactly three elements, or be a matrix with exactly three columns." << EidosTerminate(nullptr);
	
	int color_count = hsv_count / 3;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(color_count * 3);
	result_SP = EidosValue_SP(float_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double h = hsv_value->FloatAtIndex(value_index, nullptr);
		double s = hsv_value->FloatAtIndex(value_index + color_count, nullptr);
		double v = hsv_value->FloatAtIndex(value_index + color_count + color_count, nullptr);
		double r, g, b;
		
		Eidos_HSV2RGB(h, s, v, &r, &g, &b);
		
		float_result->set_float_no_check(r, value_index);
		float_result->set_float_no_check(g, value_index + color_count);
		float_result->set_float_no_check(b, value_index + color_count + color_count);
	}
	
	float_result->CopyDimensionsFromValue(hsv_value);
	
	return result_SP;
}

//	(string)rainbow(integer$ n, [float$ s = 1], [float$ v = 1], [float$ start = 0], [Nf$ end = NULL], [logical$ ccw = T])
EidosValue_SP Eidos_ExecuteFunction_rainbow(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *s_value = p_arguments[1].get();
	EidosValue *v_value = p_arguments[2].get();
	EidosValue *start_value = p_arguments[3].get();
	EidosValue *end_value = p_arguments[4].get();
	EidosValue *ccw_value = p_arguments[5].get();
	
	int64_t n = n_value->IntAtIndex(0, nullptr);
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	double s = s_value->FloatAtIndex(0, nullptr);
	
	if ((s < 0.0) || (s > 1.0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV saturation s to be in the interval [0.0, 1.0]." << EidosTerminate(nullptr);
	
	double v = v_value->FloatAtIndex(0, nullptr);
	
	if ((v < 0.0) || (v > 1.0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV value v to be in the interval [0.0, 1.0]." << EidosTerminate(nullptr);
	
	double start = start_value->FloatAtIndex(0, nullptr);
	
	if ((start < 0.0) || (start > 1.0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV hue start to be in the interval [0.0, 1.0]." << EidosTerminate(nullptr);
	
	double end = (end_value->Type() == EidosValueType::kValueNULL) ? ((n-1) / (double)n) : end_value->FloatAtIndex(0, nullptr);
	
	if ((n > 0) && ((end < 0.0) || (end > 1.0)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires HSV hue end to be in the interval [0.0, 1.0], or NULL." << EidosTerminate(nullptr);
	
	if ((n > 1) && (start == end))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rainbow): rainbow() requires start != end." << EidosTerminate(nullptr);
	
	eidos_logical_t ccw = ccw_value->LogicalAtIndex(0, nullptr);
	
	if (ccw && (end < start))
		end += 1.0;
	else if (!ccw && (end > start))
		start += 1.0;
	
	char hex_chars[8];
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	double r, g, b;
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double w = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double h = start + (end - start) * w;
		
		if (h >= 1.0)
			h -= 1.0;
		
		Eidos_HSV2RGB(h, s, v, &r, &g, &b);
		Eidos_GetColorString(r, g, b, hex_chars);
		string_result->PushString(std::string(hex_chars));
	}
	
	return result_SP;
}

//	(string)rgb2color(float rgb)
EidosValue_SP Eidos_ExecuteFunction_rgb2color(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *rgb_value = p_arguments[0].get();
	int rgb_count = rgb_value->Count();
	char hex_chars[8];
	
	if (((rgb_value->DimensionCount() != 1) || (rgb_count != 3)) &&
		((rgb_value->DimensionCount() != 2) || (rgb_value->Dimensions()[1] != 3)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2color): in function rgb2color(), rgb must contain exactly three elements, or be a matrix with exactly three columns." << EidosTerminate(nullptr);
	
	if ((rgb_value->DimensionCount() == 1) && (rgb_count == 3))
	{
		double r = rgb_value->FloatAtIndex(0, nullptr);
		double g = rgb_value->FloatAtIndex(1, nullptr);
		double b = rgb_value->FloatAtIndex(2, nullptr);
		
		if (std::isnan(r) || std::isnan(g) || std::isnan(b))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2color): color component with value NAN is not legal." << EidosTerminate();
		
		Eidos_GetColorString(r, g, b, hex_chars);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(std::string(hex_chars)));
	}
	else
	{
		int color_count = rgb_count / 3;
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < color_count; ++value_index)
		{
			double r = rgb_value->FloatAtIndex(value_index, nullptr);
			double g = rgb_value->FloatAtIndex(value_index + color_count, nullptr);
			double b = rgb_value->FloatAtIndex(value_index + color_count + color_count, nullptr);
			
			if (std::isnan(r) || std::isnan(g) || std::isnan(b))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2color): color component with value NAN is not legal." << EidosTerminate();
			
			Eidos_GetColorString(r, g, b, hex_chars);
			
			string_result->PushString(std::string(hex_chars));
		}
	}
	
	return result_SP;
}

//	(float)rgb2hsv(float rgb)
EidosValue_SP Eidos_ExecuteFunction_rgb2hsv(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *rgb_value = p_arguments[0].get();
	int rgb_count = rgb_value->Count();
	
	if (((rgb_value->DimensionCount() != 1) || (rgb_count != 3)) &&
		((rgb_value->DimensionCount() != 2) || (rgb_value->Dimensions()[1] != 3)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgb2hsv): in function rgb2hsv(), rgb must contain exactly three elements, or be a matrix with exactly three columns." << EidosTerminate(nullptr);
	
	int color_count = rgb_count / 3;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(color_count * 3);
	result_SP = EidosValue_SP(float_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double r = rgb_value->FloatAtIndex(value_index, nullptr);
		double g = rgb_value->FloatAtIndex(value_index + color_count, nullptr);
		double b = rgb_value->FloatAtIndex(value_index + color_count + color_count, nullptr);
		double h, s, v;
		
		Eidos_RGB2HSV(r, g, b, &h, &s, &v);
		
		float_result->set_float_no_check(h, value_index);
		float_result->set_float_no_check(s, value_index + color_count);
		float_result->set_float_no_check(v, value_index + color_count + color_count);
	}
	
	float_result->CopyDimensionsFromValue(rgb_value);
	
	return result_SP;
}

//	(string)terrainColors(integer$ n)
EidosValue_SP Eidos_ExecuteFunction_terrainColors(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex(0, nullptr);
	char hex_chars[8];
	
	if ((n < 0) || (n > 100000))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_terrainColors): terrainColors() requires 0 <= n <= 100000." << EidosTerminate(nullptr);
	
	int color_count = (int)n;
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(color_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int value_index = 0; value_index < color_count; ++value_index)
	{
		double fraction = (value_index ? value_index / (double)(color_count - 1) : 0.0);
		double red, green, blue;
		
		Eidos_ColorPaletteLookup(fraction, EidosColorPalette::kPalette_terrain, red, green, blue);
		Eidos_GetColorString(red, green, blue, hex_chars);
		string_result->PushString(std::string(hex_chars));
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


//	(void)beep([Ns$ soundName = NULL])
EidosValue_SP Eidos_ExecuteFunction_beep(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *soundName_value = p_arguments[0].get();
	std::string name_string = ((soundName_value->Type() == EidosValueType::kValueString) ? soundName_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	
	std::string beep_error = Eidos_Beep(name_string);
	
	if (beep_error.length())
	{
		if (!gEidosSuppressWarnings)
		{
			std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
			output_stream << beep_error << std::endl;
		}
	}
	
	return gStaticEidosValueVOID;
}

//	(void)citation(void)
EidosValue_SP Eidos_ExecuteFunction_citation(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "To cite Eidos in publications please use:" << std::endl << std::endl;
	output_stream << "Haller, B.C. (2016). Eidos: A Simple Scripting Language." << std::endl;
	output_stream << "URL: http://benhaller.com/slim/Eidos_Manual.pdf" << std::endl << std::endl;
	
	if (gEidosContextCitation.length())
	{
		output_stream << "---------------------------------------------------------" << std::endl << std::endl;
		output_stream << gEidosContextCitation << std::endl;
	}
	
	return gStaticEidosValueVOID;
}

//	(float$)clock([string$ type = "cpu"])
static std::chrono::steady_clock::time_point timebase = std::chrono::steady_clock::now();	// start at launch

EidosValue_SP Eidos_ExecuteFunction_clock(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	std::string type_name = p_arguments[0]->StringAtIndex(0, nullptr);
	
	if (type_name == "cpu")
	{
		// elapsed CPU time; this is across all cores, so it can be larger than the elapsed wall clock time!
		std::clock_t cpu_time = std::clock();
		double cpu_time_d = static_cast<double>(cpu_time) / CLOCKS_PER_SEC;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cpu_time_d));
	}
	else if (type_name == "mono")
	{
		// monotonic clock time; this is best for measured user-perceived elapsed times
		std::chrono::steady_clock::time_point ts = std::chrono::steady_clock::now();
		std::chrono::steady_clock::duration clock_duration = ts - timebase;
		double seconds = std::chrono::duration<double>(clock_duration).count();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(seconds));
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_clock): unrecognized clock type " << type_name << " in function clock()." << EidosTerminate(nullptr);
	}
}

//	(string$)date(void)
EidosValue_SP Eidos_ExecuteFunction_date(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	time_t rawtime;
	struct tm timeinfo;
	char buffer[25];	// should never be more than 10, in fact, plus a null
	
	time(&rawtime);
	localtime_r(&rawtime, &timeinfo);
	strftime(buffer, 25, "%d-%m-%Y", &timeinfo);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(std::string(buffer)));
	
	return result_SP;
}

//	(void)defineConstant(string$ symbol, * x)
EidosValue_SP Eidos_ExecuteFunction_defineConstant(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	std::string symbol_name = p_arguments[0]->StringAtIndex(0, nullptr);
	const EidosValue_SP x_value_sp = p_arguments[1];
	EidosGlobalStringID symbol_id = Eidos_GlobalStringIDForString(symbol_name);
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	// Object values can only be remembered if their class is under retain/release, so that we have control over the object lifetime
	// See also EidosDictionary::ExecuteMethod_Accelerated_setValue(), which enforces the same rule
	if (x_value_sp->Type() == EidosValueType::kValueObject)
	{
		const EidosObjectClass *x_value_class = ((EidosValue_Object *)x_value_sp.get())->Class();
		
		if (!x_value_class->UsesRetainRelease())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_defineConstant): defineConstant() can only accept object classes that are under retain/release memory management internally; class " << x_value_class->ElementType() << "is not.  This restriction is necessary in order to guarantee that the kept object elements remain valid." << EidosTerminate(nullptr);
	}
	
	symbols.DefineConstantForSymbol(symbol_id, x_value_sp);
	
	return gStaticEidosValueVOID;
}

//	(*)doCall(string$ functionName, ...)
EidosValue_SP Eidos_ExecuteFunction_doCall(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	std::string function_name = p_arguments[0]->StringAtIndex(0, nullptr);
	
	// Copy the argument list; this is a little slow, but not a big deal, and it provides protection against re-entrancy
	std::vector<EidosValue_SP> arguments;
	
	for (int argument_index = 1; argument_index < argument_count; argument_index++)
		arguments.push_back(p_arguments[argument_index]);
	
	// Look up the signature for this function dynamically
	EidosFunctionMap &function_map = p_interpreter.FunctionMap();
	auto signature_iter = function_map.find(function_name);
	
	if (signature_iter == function_map.end())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): unrecognized function name " << function_name << " in function doCall()." << EidosTerminate(nullptr);
	
	const EidosFunctionSignature *function_signature = signature_iter->second.get();
	
	// Check the function's arguments
	function_signature->CheckArguments(arguments);
	
	// BEWARE!  Since the function called here could be a function, like executeLambda() or apply() or sapply(),
	// that causes re-entrancy into the Eidos engine, this call is rather dangerous.  See the comments on those
	// functions for further details.
	if (function_signature->internal_function_)
		result_SP = function_signature->internal_function_(arguments, p_interpreter);
	else if (!function_signature->delegate_name_.empty())
	{
		EidosContext *context = p_interpreter.Context();
		
		if (context)
			result_SP = context->ContextDefinedFunctionDispatch(function_name, arguments, p_interpreter);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): (internal error) function " << function_name << " is defined by the Context, but the Context is not defined." << EidosTerminate(nullptr);
	}
	else if (function_signature->body_script_)
	{
		result_SP = p_interpreter.DispatchUserDefinedFunction(*function_signature, arguments);
	}
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): (internal error) unbound function " << function_name << "." << EidosTerminate(nullptr);
	
	// Check the return value against the signature
	function_signature->CheckReturn(*result_SP);
	
	return result_SP;
}

// This is an internal utility method that implements executeLambda() and _executeLambda_OUTER().
// The purpose of the p_execute_in_outer_scope flag is specifically for the source() function.  Since
// source() is a user-defined function, it executes in a new scope with its own variables table.
// However, we don't want that; we want it to execute the source file in the caller's scope.  So it
// uses a special internal version of executeLambda(), named _executeLambda_OUTER(), that achieves
// that by passing true here for p_execute_in_outer_scope.  Yes, this is a hack; an end-user could
// not implement source() themselves as a user-defined function without using private API.  That's
// OK, though; many Eidos built-in functions could not be implemented in Eidos themselves.
EidosValue_SP Eidos_ExecuteLambdaInternal(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter, bool p_execute_in_outer_scope)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *lambdaSource_value = p_arguments[0].get();
	EidosValue_String_singleton *lambdaSource_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[0].get());
	EidosScript *script = (lambdaSource_value_singleton ? lambdaSource_value_singleton->CachedScript() : nullptr);
	
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
		script = new EidosScript(lambdaSource_value->StringAtIndex(0, nullptr));
		
		gEidosCharacterStartOfError = -1;
		gEidosCharacterEndOfError = -1;
		gEidosCharacterStartOfErrorUTF16 = -1;
		gEidosCharacterEndOfErrorUTF16 = -1;
		gEidosCurrentScript = script;
		gEidosExecutingRuntimeScript = true;
		
		try
		{
			script->Tokenize();
			script->ParseInterpreterBlockToAST(true);
			
			// Note that true is passed to ParseInterpreterBlockToAST(), indicating that the script is to parse the code for the lambda
			// as if it were a top-level interpreter block, allowing functions to be defined.  We don't actually know, here, whether we
			// have been called at the top level or not, but we allow function definitions regardless.  Eidos doesn't disallow function
			// definitions inside blocks for any particularly strong reason; the reason is mostly just that such declarations look like
			// they should be scoped, because in most languages they are.  Since in Eidos they wouldn't be (no scope), we disallow them
			// to prevent confusion.  But when a function is declared inside executeLambda(), the user is presumably advanced and knows
			// what they're doing; there's no need to get in their way.  Sometimes it might be convenient to declare a function in this
			// way, particularly inside a file executed by source(); so let's  let the user do that, rather than bending over backwards
			// to prevent it (which is actually difficult, if you want to allow it when executeLambda() is called at the top level).
		}
		catch (...)
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
		
		if (lambdaSource_value_singleton)
			lambdaSource_value_singleton->SetCachedScript(script);
	}
	
	// Execute inside try/catch so we can handle errors well
	EidosValue *timed_value = p_arguments[1].get();
	EidosValueType timed_value_type = timed_value->Type();
	bool timed = false;
	int timer_type = 0;		// cpu by default, for legacy reasons
	
	if (timed_value_type == EidosValueType::kValueLogical)
	{
		if (timed_value->LogicalAtIndex(0, nullptr))
			timed = true;
	}
	else if (timed_value_type == EidosValueType::kValueString)
	{
		std::string timed_string = timed_value->StringAtIndex(0, nullptr);
		
		if (timed_string == "cpu")
		{
			timed = true;
			timer_type = 0;		// cpu timer
		}
		else if (timed_string == "mono")
		{
			timed = true;
			timer_type = 1;		// monotonic timer
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteLambdaInternal): unrecognized clock type " << timed_string << " in function executeLambda()." << EidosTerminate(nullptr);
		}
	}
	
	std::clock_t begin_clock = 0, end_clock = 0;
	std::chrono::steady_clock::time_point begin_ts, end_ts;
	
	gEidosCharacterStartOfError = -1;
	gEidosCharacterEndOfError = -1;
	gEidosCharacterStartOfErrorUTF16 = -1;
	gEidosCharacterEndOfErrorUTF16 = -1;
	gEidosCurrentScript = script;
	gEidosExecutingRuntimeScript = true;
	
	try
	{
		EidosSymbolTable *symbols = &p_interpreter.SymbolTable();									// use our own symbol table
		
		if (p_execute_in_outer_scope)
			symbols = symbols->ParentSymbolTable();
		
		EidosInterpreter interpreter(*script, *symbols, p_interpreter.FunctionMap(), p_interpreter.Context());
		
		if (timed)
		{
			if (timer_type == 0)
				begin_clock = std::clock();
			else
				begin_ts = std::chrono::steady_clock::now();
		}
		
		// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
		// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
		// bugs that would otherwise not manifest.
		result_SP = interpreter.EvaluateInterpreterBlock(false, true);		// do not print output, return the last statement value
		
		if (timed)
		{
			if (timer_type == 0)
				end_clock = std::clock();
			else
				end_ts = std::chrono::steady_clock::now();
		}
		
		// Assimilate output
		interpreter.FlushExecutionOutputToStream(p_interpreter.ExecutionOutputStream());
	}
	catch (...)
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
		
		if (!lambdaSource_value_singleton)
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
		double time_spent;
		
		if (timer_type == 0)
			time_spent = static_cast<double>(end_clock - begin_clock) / CLOCKS_PER_SEC;
		else
			time_spent = std::chrono::duration<double>(end_ts - begin_ts).count();
		
		p_interpreter.ExecutionOutputStream() << "// ********** executeLambda() elapsed time: " << time_spent << std::endl;
	}
	
	if (!lambdaSource_value_singleton)
		delete script;
	
	return result_SP;
}

//	(*)executeLambda(string$ lambdaSource, [ls$ timed = F])
EidosValue_SP Eidos_ExecuteFunction_executeLambda(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	return Eidos_ExecuteLambdaInternal(p_arguments, p_interpreter, false);
}

//	(*)_executeLambda_OUTER(string$ lambdaSource, [ls$ timed = F])
EidosValue_SP Eidos_ExecuteFunction__executeLambda_OUTER(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	return Eidos_ExecuteLambdaInternal(p_arguments, p_interpreter, true);	// see Eidos_ExecuteLambdaInternal() for comments on the true flag
}

//	(logical)exists(string symbol)
EidosValue_SP Eidos_ExecuteFunction_exists(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	EidosValue *symbol_value = p_arguments[0].get();
	int symbol_count = symbol_value->Count();
	
	if ((symbol_count == 1) && (symbol_value->DimensionCount() == 1))
	{
		// Use the global constants, but only if we do not have to impose a dimensionality upon the value below
		EidosGlobalStringID symbol_id = Eidos_GlobalStringIDForString(symbol_value->StringAtIndex(0, nullptr));
		
		result_SP = (symbols.ContainsSymbol(symbol_id) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(symbol_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < symbol_count; ++value_index)
		{
			EidosGlobalStringID symbol_id = Eidos_GlobalStringIDForString(symbol_value->StringAtIndex(value_index, nullptr));
			
			logical_result->set_logical_no_check(symbols.ContainsSymbol(symbol_id), value_index);
		}
		
		result_SP->CopyDimensionsFromValue(symbol_value);
	}
	
	return result_SP;
}

//	(void)functionSignature([Ns$ functionName = NULL])
EidosValue_SP Eidos_ExecuteFunction_functionSignature(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *functionName_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	bool function_name_specified = (functionName_value->Type() == EidosValueType::kValueString);
	std::string match_string = (function_name_specified ? functionName_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	bool signature_found = false;
	
	// function_map_ is already alphabetized since maps keep sorted order
	EidosFunctionMap &function_map = p_interpreter.FunctionMap();
	
	for (auto functionPairIter : function_map)
	{
		const EidosFunctionSignature *iter_signature = functionPairIter.second.get();
		
		if (function_name_specified && (iter_signature->call_name_.compare(match_string) != 0))
			continue;
		
		if (!function_name_specified && (iter_signature->call_name_.substr(0, 1).compare("_") == 0))
			continue;	// skip internal functions that start with an underscore, unless specifically requested
		
		output_stream << *iter_signature;
		
		if (iter_signature->body_script_ && iter_signature->user_defined_)
		{
			output_stream << " <user-defined>";
		}
		
		output_stream << std::endl;
		
		signature_found = true;
	}
	
	if (function_name_specified && !signature_found)
		output_stream << "No function signature found for \"" << match_string << "\"." << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(integer$)getSeed(void)
EidosValue_SP Eidos_ExecuteFunction_getSeed(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_RNG.rng_last_seed_));
	
	return result_SP;
}

//	(void)license(void)
EidosValue_SP Eidos_ExecuteFunction_license(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "Eidos is free software: you can redistribute it and/or" << std::endl;
	output_stream << "modify it under the terms of the GNU General Public" << std::endl;
	output_stream << "License as published by the Free Software Foundation," << std::endl;
	output_stream << "either version 3 of the License, or (at your option)" << std::endl;
	output_stream << "any later version." << std::endl << std::endl;
	
	output_stream << "Eidos is distributed in the hope that it will be" << std::endl;
	output_stream << "useful, but WITHOUT ANY WARRANTY; without even the" << std::endl;
	output_stream << "implied warranty of MERCHANTABILITY or FITNESS FOR" << std::endl;
	output_stream << "A PARTICULAR PURPOSE.  See the GNU General Public" << std::endl;
	output_stream << "License for more details." << std::endl << std::endl;
	
	output_stream << "You should have received a copy of the GNU General" << std::endl;
	output_stream << "Public License along with Eidos.  If not, see" << std::endl;
	output_stream << "<http://www.gnu.org/licenses/>." << std::endl << std::endl;
	
	if (gEidosContextLicense.length())
	{
		output_stream << "---------------------------------------------------------" << std::endl << std::endl;
		output_stream << gEidosContextLicense << std::endl;
	}
	
	return gStaticEidosValueVOID;
}

//	(void)ls(void)
EidosValue_SP Eidos_ExecuteFunction_ls(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	p_interpreter.ExecutionOutputStream() << p_interpreter.SymbolTable();
	
	return gStaticEidosValueVOID;
}

//	(void)rm([Ns variableNames = NULL], [logical$ removeConstants = F])
EidosValue_SP Eidos_ExecuteFunction_rm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *variableNames_value = p_arguments[0].get();
	bool removeConstants = p_arguments[1]->LogicalAtIndex(0, nullptr);
	std::vector<std::string> symbols_to_remove;
	
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	if (variableNames_value->Type() == EidosValueType::kValueNULL)
		symbols_to_remove = symbols.ReadWriteSymbols();
	else
	{
		int variableNames_count = variableNames_value->Count();
		
		for (int value_index = 0; value_index < variableNames_count; ++value_index)
			symbols_to_remove.emplace_back(variableNames_value->StringAtIndex(value_index, nullptr));
	}
	
	if (removeConstants)
		for (std::string &symbol : symbols_to_remove)
			symbols.RemoveConstantForSymbol(Eidos_GlobalStringIDForString(symbol));
	else
		for (std::string &symbol : symbols_to_remove)
			symbols.RemoveValueForSymbol(Eidos_GlobalStringIDForString(symbol));
	
	return gStaticEidosValueVOID;
}

//	(*)sapply(* x, string$ lambdaSource, [string$ simplify = "vector"])
EidosValue_SP Eidos_ExecuteFunction_sapply(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	// an empty x argument yields invisible NULL; this short-circuit is new but the behavior is the same as it was before,
	// except that we skip all the script tokenizing/parsing and so forth...
	if (x_count == 0)
		return gStaticEidosValueNULLInvisible;
	
	// Determine the simplification mode requested
	EidosValue *simplify_value = p_arguments[2].get();
	const std::string &simplify_string = simplify_value->StringAtIndex(0, nullptr);
	int simplify;
	
	if (simplify_string == "vector")		simplify = 0;
	else if (simplify_string == "matrix")	simplify = 1;
	else if (simplify_string == "match")	simplify = 2;
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): unrecognized simplify option \"" << simplify_string << "\" in function sapply()." << EidosTerminate(nullptr);
	
	// Get the lambda string and cache its script
	EidosValue *lambda_value = p_arguments[1].get();
	EidosValue_String_singleton *lambda_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[1].get());
	EidosScript *script = (lambda_value_singleton ? lambda_value_singleton->CachedScript() : nullptr);
	
	// Errors in lambdas should be reported for the lambda script, not for the calling script,
	// if possible.  In the GUI this does not work well, however; there, errors should be
	// reported as occurring in the call to sapply().  Here we save off the current
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
		script = new EidosScript(lambda_value->StringAtIndex(0, nullptr));
		
		gEidosCharacterStartOfError = -1;
		gEidosCharacterEndOfError = -1;
		gEidosCharacterStartOfErrorUTF16 = -1;
		gEidosCharacterEndOfErrorUTF16 = -1;
		gEidosCurrentScript = script;
		gEidosExecutingRuntimeScript = true;
		
		try
		{
			script->Tokenize();
			script->ParseInterpreterBlockToAST(false);
		}
		catch (...)
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
		
		if (lambda_value_singleton)
			lambda_value_singleton->SetCachedScript(script);
	}
	
	// Execute inside try/catch so we can handle errors well
	std::vector<EidosValue_SP> results;
	
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
		bool null_included = false;				// has a NULL been seen among the return values
		bool consistent_return_length = true;	// consistent except for any NULLs returned
		int return_length = -1;					// what the consistent length is
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			EidosValue_SP apply_value = x_value->GetValueAtIndex(value_index, nullptr);
			
			// Set the iterator variable "applyValue" to the value
			symbols.SetValueForSymbolNoCopy(gEidosID_applyValue, std::move(apply_value));
			
			// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
			// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
			// bugs that would otherwise not manifest.
			EidosValue_SP &&return_value_SP = interpreter.EvaluateInterpreterBlock(false, true);		// do not print output, return the last statement value
			
			if (return_value_SP->Type() == EidosValueType::kValueVOID)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): each iteration within sapply() must return a non-void value." << EidosTerminate(nullptr);
			
			if (return_value_SP->Type() == EidosValueType::kValueNULL)
			{
				null_included = true;
			}
			else if (consistent_return_length)
			{
				int length = return_value_SP->Count();
				
				if (return_length == -1)
					return_length = length;
				else if (length != return_length)
					consistent_return_length = false;
			}
			
			results.emplace_back(return_value_SP);
		}
		
		// We do not want a leftover applyValue symbol in the symbol table, so we remove it now
		symbols.RemoveValueForSymbol(gEidosID_applyValue);
		
		// Assemble all the individual results together, just as c() does
		interpreter.FlushExecutionOutputToStream(p_interpreter.ExecutionOutputStream());
		result_SP = ConcatenateEidosValues(results, true, false);	// allow NULL but not VOID
		
		// Finally, we restructure the results:
		//
		//	simplify == 0 ("vector") returns a plain vector, which is what we have already
		//	simplify == 1 ("matrix") returns a matrix with one column per return value
		//	simplify == 2 ("match") returns a matrix/array exactly matching the dimensions of x
		if (simplify == 1)
		{
			// A zero-length result is allowed and is returned verbatim
			if (result_SP->Count() > 0)
			{
				if (!consistent_return_length)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): simplify = \"matrix\" was requested in function sapply(), but return values from lambdaSource were not of a consistent length." << EidosTerminate(nullptr);
				
				// one matrix column per return value, omitting NULLs; no need to reorder values to achieve this
				int64_t dim[2] = {return_length, result_SP->Count() / return_length};
				
				result_SP->SetDimensions(2, dim);
			}
		}
		else if (simplify == 2)
		{
			if (null_included)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): simplify = \"match\" was requested in function sapply(), but return values included NULL." << EidosTerminate(nullptr);
			if (!consistent_return_length || (return_length != 1))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): simplify = \"match\" was requested in function sapply(), but return values from lambdaSource were not all singletons." << EidosTerminate(nullptr);
			
			// match the dimensionality of x
			result_SP->CopyDimensionsFromValue(x_value);
		}
	}
	catch (...)
	{
		// If exceptions throw, then we want to set up the error information to highlight the
		// sapply() that failed, since we can't highlight the actual error.  (If exceptions
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
		
		if (!lambda_value_singleton)
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
	
	if (!lambda_value_singleton)
		delete script;
	
	return result_SP;
}

//	(void)setSeed(integer$ seed)
EidosValue_SP Eidos_ExecuteFunction_setSeed(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *seed_value = p_arguments[0].get();
	
	Eidos_SetRNGSeed(seed_value->IntAtIndex(0, nullptr));
	
	return gStaticEidosValueVOID;
}

//	(void)stop([Ns$ message = NULL])
EidosValue_SP Eidos_ExecuteFunction_stop(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *message_value = p_arguments[0].get();
	
	if (message_value->Type() != EidosValueType::kValueNULL)
	{
		std::string &&stop_string = p_arguments[0]->StringAtIndex(0, nullptr);
		
		p_interpreter.ExecutionOutputStream() << stop_string << std::endl;
		
		EIDOS_TERMINATION << ("ERROR (Eidos_ExecuteFunction_stop): stop(\"" + stop_string + "\") called.") << EidosTerminate(nullptr);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_stop): stop() called." << EidosTerminate(nullptr);
	}
	
	// CODE COVERAGE: This is dead code
	return result_SP;
}

//	(logical$)suppressWarnings(logical$ suppress)
EidosValue_SP Eidos_ExecuteFunction_suppressWarnings(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *suppress_value = p_arguments[0].get();
	eidos_logical_t new_suppress = suppress_value->LogicalAtIndex(0, nullptr);
	eidos_logical_t old_suppress = gEidosSuppressWarnings;
	
	gEidosSuppressWarnings = new_suppress;
	
	return (old_suppress ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

//	(string)system(string$ command, [string args = ""], [string input = ""], [logical$ stderr = F], [logical$ wait = T])
EidosValue_SP Eidos_ExecuteFunction_system(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	if (!Eidos_SlashTmpExists())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): in function system(), the /tmp directory appears not to exist or is not writeable." << EidosTerminate(nullptr);
	
	EidosValue *command_value = p_arguments[0].get();
	EidosValue *args_value = p_arguments[1].get();
	int arg_count = args_value->Count();
	bool has_args = ((arg_count > 1) || ((arg_count == 1) && (args_value->StringAtIndex(0, nullptr).length() > 0)));
	EidosValue *input_value = p_arguments[2].get();
	int input_count = input_value->Count();
	bool has_input = ((input_count > 1) || ((input_count == 1) && (input_value->StringAtIndex(0, nullptr).length() > 0)));
	bool redirect_stderr = p_arguments[3]->LogicalAtIndex(0, nullptr);
	bool wait = p_arguments[4]->LogicalAtIndex(0, nullptr);
	
	// Construct the command string
	std::string command_string = command_value->StringAtIndex(0, nullptr);
	
	if (command_string.length() == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): a non-empty command string must be supplied to system()." << EidosTerminate(nullptr);
	
	if (has_args)
	{
		for (int value_index = 0; value_index < arg_count; ++value_index)
		{
			command_string.append(" ");
			command_string.append(args_value->StringAtIndex(value_index, nullptr));
		}
	}
	
	// Make the input temporary file and redirect, if requested
	if (has_input)
	{
		// thanks to http://stackoverflow.com/questions/499636/how-to-create-a-stdofstream-to-a-temp-file for the temp file creation code
		
		char *name = strdup("/tmp/eidos_system_XXXXXX");	// the /tmp directory is standard on OS X and Linux; probably on all Un*x systems
		int fd = mkstemp(name);
		
		if (fd == -1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) mkstemp() failed!" << EidosTerminate(nullptr);
		
		std::ofstream file_stream(name, std::ios_base::out);
		close(fd);	// opened by mkstemp()
		
		if (!file_stream.is_open())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) ofstream() failed!" << EidosTerminate(nullptr);
		
		if (input_count == 1)
		{
			file_stream << input_value->StringAtIndex(0, nullptr);	// no final newline in this case, so the user can precisely specify the file contents if desired
		}
		else
		{
			const std::vector<std::string> &string_vec = *input_value->StringVector();
			
			for (int value_index = 0; value_index < input_count; ++value_index)
			{
				file_stream << string_vec[value_index];
				
				// Add newlines after all lines, including the last
				file_stream << std::endl;
			}
		}
		
		if (file_stream.bad())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) stream errors writing temporary file for input" << EidosTerminate(nullptr);
		
		command_string.append(" < ");
		command_string.append(name);
		
		free(name);
	}
	
	// Redirect standard error, if requested
	if (redirect_stderr)
	{
		command_string.append(" 2>&1");
	}
	
	// Run in the background, if requested
	if (!wait)
	{
		command_string.append(" &");
	}
	
	// Determine whether we are running in the background, as indicated by a command line ending in " &"
	if ((command_string.length() > 2) && (command_string.substr(command_string.length() - 2, std::string::npos) == " &"))
	{
		wait = false;
	}
	
	if (wait)
	{
		// Execute the command string; thanks to http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
		//std::cout << "Executing command string: " << command_string << std::endl;
		//std::cout << "Command string length: " << command_string.length() << " bytes" << std::endl;
		
		char buffer[128];
		std::string result = "";
		std::shared_ptr<FILE> command_pipe(popen(command_string.c_str(), "r"), pclose);
		if (!command_pipe)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) popen() failed!" << EidosTerminate(nullptr);
		while (!feof(command_pipe.get())) {
			if (fgets(buffer, 128, command_pipe.get()) != NULL)
				result += buffer;
		}
		
		// Parse the result into lines and make a result vector
		std::istringstream result_stream(result);
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		std::string line;
		
		while (getline(result_stream, line))
			string_result->PushString(line);
		
		return result_SP;
	}
	else
	{
		// Execute the command string without collecting output, hopefully in the background with an immediate return from system()
		int ret = system(command_string.c_str());
		
		if (ret != 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) system() failed with return code " << ret << "." << EidosTerminate(nullptr);
		
		return gStaticEidosValue_String_ZeroVec;
	}
}

//	(string$)time(void)
EidosValue_SP Eidos_ExecuteFunction_time(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	time_t rawtime;
	struct tm timeinfo;
	char buffer[20];		// should never be more than 8, in fact, plus a null
	
	time(&rawtime);
	localtime_r(&rawtime, &timeinfo);
	strftime(buffer, 20, "%H:%M:%S", &timeinfo);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(std::string(buffer)));
	
	return result_SP;
}

//	(float$)usage([logical$ peak = F])
EidosValue_SP Eidos_ExecuteFunction_usage(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	bool peak = p_arguments[0]->LogicalAtIndex(0, nullptr);
	
	size_t usage = (peak ? Eidos_GetPeakRSS() : Eidos_GetCurrentRSS());
	double usage_MB = usage / (1024.0 * 1024.0);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(usage_MB));
	
	return result_SP;
}

//	(void)version([logical$ print = T])
EidosValue_SP Eidos_ExecuteFunction_version(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	bool print = p_arguments[0]->LogicalAtIndex(0, nullptr);
	
	if (print)
	{
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		output_stream << "Eidos version " << EIDOS_VERSION_STRING << std::endl;
		
		if (gEidosContextVersionString.length())
			output_stream << gEidosContextVersionString << std::endl;
	}
	
	// Return the versions as floats
	EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(2);
	result_SP = EidosValue_SP(result);
	
	result->push_float_no_check(EIDOS_VERSION_FLOAT);
	
	if (gEidosContextVersion != 0.0)
		result->push_float_no_check(gEidosContextVersion);
	
	if (print)
		result->SetInvisible(true);
	
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
EidosValue_SP Eidos_ExecuteFunction__Test(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *yolk_value = p_arguments[0].get();
	EidosTestElement *testElement = new EidosTestElement(yolk_value->IntAtIndex(0, nullptr));
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(testElement, gEidosTestElement_Class));
	
	// testElement is now retained by result_SP, so we can release it
	testElement->Release();
	
	return result_SP;
}




































































