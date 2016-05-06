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

#include "math.h"

#include <ctime>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <fstream>
#include <stdexcept>
#include <algorithm>
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
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("abs",				EidosFunctionIdentifier::absFunction,			kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("acos",				EidosFunctionIdentifier::acosFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asin",				EidosFunctionIdentifier::asinFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan",				EidosFunctionIdentifier::atanFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("atan2",			EidosFunctionIdentifier::atan2Function,			kEidosValueMaskFloat))->AddNumeric("x")->AddNumeric("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ceil",				EidosFunctionIdentifier::ceilFunction,			kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cos",				EidosFunctionIdentifier::cosFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cumProduct",		EidosFunctionIdentifier::cumProductFunction,	kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cumSum",			EidosFunctionIdentifier::cumSumFunction,		kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("exp",				EidosFunctionIdentifier::expFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("floor",			EidosFunctionIdentifier::floorFunction,			kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("integerDiv",		EidosFunctionIdentifier::integerDivFunction,	kEidosValueMaskInt))->AddInt("x")->AddInt("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("integerMod",		EidosFunctionIdentifier::integerModFunction,	kEidosValueMaskInt))->AddInt("x")->AddInt("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFinite",			EidosFunctionIdentifier::isFiniteFunction,		kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInfinite",		EidosFunctionIdentifier::isInfiniteFunction,	kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNAN",			EidosFunctionIdentifier::isNaNFunction,			kEidosValueMaskLogical))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log",				EidosFunctionIdentifier::logFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log10",			EidosFunctionIdentifier::log10Function,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("log2",				EidosFunctionIdentifier::log2Function,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("product",			EidosFunctionIdentifier::productFunction,		kEidosValueMaskNumeric | kEidosValueMaskSingleton))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("round",			EidosFunctionIdentifier::roundFunction,			kEidosValueMaskFloat))->AddFloat("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sin",				EidosFunctionIdentifier::sinFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sqrt",				EidosFunctionIdentifier::sqrtFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sum",				EidosFunctionIdentifier::sumFunction,			kEidosValueMaskNumeric | kEidosValueMaskSingleton))->AddLogicalEquiv("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("tan",				EidosFunctionIdentifier::tanFunction,			kEidosValueMaskFloat))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("trunc",			EidosFunctionIdentifier::truncFunction,			kEidosValueMaskFloat))->AddFloat("x"));
		
		
		// ************************************************************************************
		//
		//	summary statistics functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("max",				EidosFunctionIdentifier::maxFunction,			kEidosValueMaskAnyBase | kEidosValueMaskSingleton))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mean",				EidosFunctionIdentifier::meanFunction,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("min",				EidosFunctionIdentifier::minFunction,			kEidosValueMaskAnyBase | kEidosValueMaskSingleton))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pmax",				EidosFunctionIdentifier::pmaxFunction,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddAnyBase("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("pmin",				EidosFunctionIdentifier::pminFunction,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddAnyBase("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("range",			EidosFunctionIdentifier::rangeFunction,			kEidosValueMaskNumeric))->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sd",				EidosFunctionIdentifier::sdFunction,			kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddNumeric("x"));
		
		
		// ************************************************************************************
		//
		//	distribution draw / density functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("dnorm",			EidosFunctionIdentifier::dnormFunction,			kEidosValueMaskFloat))->AddFloat("x")->AddNumeric_O("mean")->AddNumeric_O("sd"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rbinom",			EidosFunctionIdentifier::rbinomFunction,		kEidosValueMaskInt))->AddInt_S("n")->AddInt("size")->AddFloat("prob"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rexp",				EidosFunctionIdentifier::rexpFunction,			kEidosValueMaskFloat))->AddInt_S("n")->AddNumeric_O("mu"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rgamma",			EidosFunctionIdentifier::rgammaFunction,		kEidosValueMaskFloat))->AddInt_S("n")->AddNumeric("mean")->AddNumeric("shape"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rlnorm",			EidosFunctionIdentifier::rlnormFunction,		kEidosValueMaskFloat))->AddInt_S("n")->AddNumeric_O("meanlog")->AddNumeric_O("sdlog"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rnorm",			EidosFunctionIdentifier::rnormFunction,			kEidosValueMaskFloat))->AddInt_S("n")->AddNumeric_O("mean")->AddNumeric_O("sd"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rpois",			EidosFunctionIdentifier::rpoisFunction,			kEidosValueMaskInt))->AddInt_S("n")->AddNumeric("lambda"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("runif",			EidosFunctionIdentifier::runifFunction,			kEidosValueMaskFloat))->AddInt_S("n")->AddNumeric_O("min")->AddNumeric_O("max"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rweibull",			EidosFunctionIdentifier::rweibullFunction,		kEidosValueMaskFloat))->AddInt_S("n")->AddNumeric("lambda")->AddNumeric("k"));
		
		
		// ************************************************************************************
		//
		//	vector construction functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("c",				EidosFunctionIdentifier::cFunction,				kEidosValueMaskAny))->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_float,	EidosFunctionIdentifier::floatFunction,			kEidosValueMaskFloat))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_integer,	EidosFunctionIdentifier::integerFunction,		kEidosValueMaskInt))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_logical,	EidosFunctionIdentifier::logicalFunction,		kEidosValueMaskLogical))->AddInt_S("length"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_object,	EidosFunctionIdentifier::objectFunction,		kEidosValueMaskObject, gEidos_UndefinedClassObject)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rep",				EidosFunctionIdentifier::repFunction,			kEidosValueMaskAny))->AddAny("x")->AddInt_S("count"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("repEach",			EidosFunctionIdentifier::repEachFunction,		kEidosValueMaskAny))->AddAny("x")->AddInt("count"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sample",			EidosFunctionIdentifier::sampleFunction,		kEidosValueMaskAny))->AddAny("x")->AddInt_S("size")->AddLogical_OS("replace")->AddNumeric_O("weights"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("seq",				EidosFunctionIdentifier::seqFunction,			kEidosValueMaskNumeric))->AddNumeric_S("from")->AddNumeric_S("to")->AddNumeric_OS("by"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("seqAlong",			EidosFunctionIdentifier::seqAlongFunction,		kEidosValueMaskInt))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_string,	EidosFunctionIdentifier::stringFunction,		kEidosValueMaskString))->AddInt_S("length"));
		
		
		// ************************************************************************************
		//
		//	value inspection/manipulation functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("all",				EidosFunctionIdentifier::allFunction,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("any",				EidosFunctionIdentifier::anyFunction,			kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddLogical("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("cat",				EidosFunctionIdentifier::catFunction,			kEidosValueMaskNULL))->AddAny("x")->AddString_OS("sep"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("format",			EidosFunctionIdentifier::formatFunction,		kEidosValueMaskString))->AddString_S("format")->AddNumeric("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("identical",		EidosFunctionIdentifier::identicalFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x")->AddAny("y"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("ifelse",			EidosFunctionIdentifier::ifelseFunction,		kEidosValueMaskAny))->AddLogical("test")->AddAny("trueValues")->AddAny("falseValues"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("match",			EidosFunctionIdentifier::matchFunction,			kEidosValueMaskInt))->AddAny("x")->AddAny("table"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nchar",			EidosFunctionIdentifier::ncharFunction,			kEidosValueMaskInt))->AddString("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("paste",			EidosFunctionIdentifier::pasteFunction,			kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x")->AddString_OS("sep"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("print",			EidosFunctionIdentifier::printFunction,			kEidosValueMaskNULL))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("rev",				EidosFunctionIdentifier::revFunction,			kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_size,		EidosFunctionIdentifier::sizeFunction,			kEidosValueMaskInt | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sort",				EidosFunctionIdentifier::sortFunction,			kEidosValueMaskAnyBase))->AddAnyBase("x")->AddLogical_OS("ascending"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("sortBy",			EidosFunctionIdentifier::sortByFunction,		kEidosValueMaskObject))->AddObject("x", nullptr)->AddString_S("property")->AddLogical_OS("ascending"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_str,		EidosFunctionIdentifier::strFunction,			kEidosValueMaskNULL))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("strsplit",			EidosFunctionIdentifier::strsplitFunction,		kEidosValueMaskString))->AddString_S("x")->AddString_OS("sep"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("substr",			EidosFunctionIdentifier::substrFunction,		kEidosValueMaskString))->AddString("x")->AddInt("first")->AddInt_O("last"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("unique",			EidosFunctionIdentifier::uniqueFunction,		kEidosValueMaskAny))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("which",			EidosFunctionIdentifier::whichFunction,			kEidosValueMaskInt))->AddLogical("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMax",			EidosFunctionIdentifier::whichMaxFunction,		kEidosValueMaskInt))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("whichMin",			EidosFunctionIdentifier::whichMinFunction,		kEidosValueMaskInt))->AddAnyBase("x"));
		
		
		// ************************************************************************************
		//
		//	value type testing/coercion functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asFloat",			EidosFunctionIdentifier::asFloatFunction,		kEidosValueMaskFloat))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asInteger",		EidosFunctionIdentifier::asIntegerFunction,		kEidosValueMaskInt))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asLogical",		EidosFunctionIdentifier::asLogicalFunction,		kEidosValueMaskLogical))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("asString",			EidosFunctionIdentifier::asStringFunction,		kEidosValueMaskString))->AddAnyBase("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("elementType",		EidosFunctionIdentifier::elementTypeFunction,	kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isFloat",			EidosFunctionIdentifier::isFloatFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isInteger",		EidosFunctionIdentifier::isIntegerFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isLogical",		EidosFunctionIdentifier::isLogicalFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isNULL",			EidosFunctionIdentifier::isNULLFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isObject",			EidosFunctionIdentifier::isObjectFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("isString",			EidosFunctionIdentifier::isStringFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddAny("x"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("type",				EidosFunctionIdentifier::typeFunction,			kEidosValueMaskString | kEidosValueMaskSingleton))->AddAny("x"));
		
		
		// ************************************************************************************
		//
		//	miscellaneous functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_apply,	EidosFunctionIdentifier::applyFunction,			kEidosValueMaskAny))->AddAny("x")->AddString_S("lambdaSource"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("beep",				EidosFunctionIdentifier::beepFunction,			kEidosValueMaskNULL))->AddString_OS("soundName"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("date",				EidosFunctionIdentifier::dateFunction,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("defineConstant",	EidosFunctionIdentifier::defineConstantFunction,	kEidosValueMaskNULL))->AddString_S("symbol")->AddAnyBase("value"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_doCall,	EidosFunctionIdentifier::doCallFunction,		kEidosValueMaskAny))->AddString_S("function")->AddEllipsis());
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_executeLambda,	EidosFunctionIdentifier::executeLambdaFunction,	kEidosValueMaskAny))->AddString_S("lambdaSource")->AddLogical_OS("timed"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("exists",			EidosFunctionIdentifier::existsFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("symbol"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("function",			EidosFunctionIdentifier::functionFunction,		kEidosValueMaskNULL))->AddString_OS("functionName"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_ls,		EidosFunctionIdentifier::lsFunction,			kEidosValueMaskNULL)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("license",			EidosFunctionIdentifier::licenseFunction,		kEidosValueMaskNULL)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_rm,		EidosFunctionIdentifier::rmFunction,			kEidosValueMaskNULL))->AddString_O("variableNames")->AddLogical_OS("removeConstants"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("setSeed",			EidosFunctionIdentifier::setSeedFunction,		kEidosValueMaskNULL))->AddInt_S("seed"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("getSeed",			EidosFunctionIdentifier::getSeedFunction,		kEidosValueMaskInt | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("stop",				EidosFunctionIdentifier::stopFunction,			kEidosValueMaskNULL))->AddString_OS("message"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("time",				EidosFunctionIdentifier::timeFunction,			kEidosValueMaskString | kEidosValueMaskSingleton)));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("version",			EidosFunctionIdentifier::versionFunction,		kEidosValueMaskNULL)));
		
		
		// ************************************************************************************
		//
		//	filesystem access functions
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("filesAtPath",		EidosFunctionIdentifier::filesAtPathFunction,	kEidosValueMaskString))->AddString_S("path")->AddLogical_OS("fullPaths"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("readFile",			EidosFunctionIdentifier::readFileFunction,		kEidosValueMaskString))->AddString_S("filePath"));
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("writeFile",		EidosFunctionIdentifier::writeFileFunction,		kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddString_S("filePath")->AddString("contents")->AddLogical_OS("append"));

		
		// ************************************************************************************
		//
		//	object instantiation
		//
		
		signatures->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_Test",			EidosFunctionIdentifier::_TestFunction,			kEidosValueMaskObject | kEidosValueMaskSingleton, gEidos_TestElementClass))->AddInt_S("yolk"));
		
		
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
			built_in_function_map_->insert(EidosFunctionMapPair(sig->function_name_, sig));
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

EidosValue_SP EidosInterpreter::ExecuteFunctionCall(string const &p_function_name, const EidosFunctionSignature *p_function_signature, const EidosValue_SP *const p_arguments, int p_argument_count)
{
	EidosValue_SP result_SP(nullptr);
	
	// If the function call is a built-in Eidos function, we might already have a pointer to its signature cached; if not, we'll have to look it up
	if (!p_function_signature)
	{
		// Get the function signature and check our arguments against it
		auto signature_iter = function_map_.find(p_function_name);
		
		if (signature_iter == function_map_.end())
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): unrecognized function name " << p_function_name << "." << eidos_terminate(nullptr);
		
		p_function_signature = signature_iter->second;
	}
	
	// Check the functions arguments against the signature
	p_function_signature->CheckArguments(p_arguments, p_argument_count);
	
	// Now we look up the function again and actually execute it
	switch (p_function_signature->function_id_)
	{
		case EidosFunctionIdentifier::kNoFunction:
		{
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): internal logic error." << eidos_terminate(nullptr);
			break;
		}
			
		case EidosFunctionIdentifier::kDelegatedFunction:
		{
			result_SP = p_function_signature->delegate_function_(p_function_signature->delegate_object_, p_function_name, p_arguments, p_argument_count, *this);
			break;
		}
			
			
		// ************************************************************************************
		//
		//	math functions
		//
#pragma mark -
#pragma mark Math functions
#pragma mark -
			
			
			//	(numeric)abs(numeric x)
			#pragma mark abs
			
		case EidosFunctionIdentifier::absFunction:
		{
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function abs() cannot take the absolute value of the most negative integer." << eidos_terminate(nullptr);
					
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function abs() cannot take the absolute value of the most negative integer." << eidos_terminate(nullptr);
						
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
			break;
		}
			
			
			//	(float)acos(numeric x)
			#pragma mark acos
			
		case EidosFunctionIdentifier::acosFunction:
		{
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
			break;
		}
			
			
			//	(float)asin(numeric x)
			#pragma mark asin
			
		case EidosFunctionIdentifier::asinFunction:
		{
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
			break;
		}
			
			
			//	(float)atan(numeric x)
			#pragma mark atan
			
		case EidosFunctionIdentifier::atanFunction:
		{
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
			break;
		}
			
			
			//	(float)atan2(numeric x, numeric y)
			#pragma mark atan2
			
		case EidosFunctionIdentifier::atan2Function:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			int arg1_count = arg1_value->Count();
			
			if (arg0_count != arg1_count)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function atan2() requires arguments of equal length." << eidos_terminate(nullptr);
			
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
			break;
		}
			
			
			//	(float)ceil(float x)
			#pragma mark ceil
			
		case EidosFunctionIdentifier::ceilFunction:
		{
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
			break;
		}
			
			
			//	(float)cos(numeric x)
			#pragma mark cos
			
		case EidosFunctionIdentifier::cosFunction:
		{
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
			break;
		}
			
			
			//	(numeric)cumProduct(numeric x)
			#pragma mark cumProduct
			
		case EidosFunctionIdentifier::cumProductFunction:
		{
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): integer multiplication overflow in function cumProduct()." << eidos_terminate(nullptr);
						
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
			break;
		}
			
			
			//	(numeric)cumSum(numeric x)
			#pragma mark cumSum
			
		case EidosFunctionIdentifier::cumSumFunction:
		{
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): integer addition overflow in function cumSum()." << eidos_terminate(nullptr);
						
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
			break;
		}
			
			
			//	(float)exp(numeric x)
			#pragma mark exp
			
		case EidosFunctionIdentifier::expFunction:
		{
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
			break;
		}
			
			
			//	(float)floor(float x)
			#pragma mark floor
			
		case EidosFunctionIdentifier::floorFunction:
		{
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
			break;
		}
			
			
			//	(integer)integerDiv(integer x, integer y)
#pragma mark integerDiv
			
		case EidosFunctionIdentifier::integerDivFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			int arg1_count = arg1_value->Count();
			
			if ((arg0_count == 1) && (arg1_count == 1))
			{
				int64_t int1 = arg0_value->IntAtIndex(0, nullptr);
				int64_t int2 = arg1_value->IntAtIndex(0, nullptr);
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
				
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
						
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
						
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerDiv() cannot perform division by 0." << eidos_terminate(nullptr);
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						int_result->PushInt(int1_vec[value_index] / int2);
				}
				else	// if ((arg0_count != arg1_count) && (arg0_count != 1) && (arg1_count != 1))
				{
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerDiv() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(nullptr);
				}
			}
			
			break;
		}
			
			
			//	(integer)integerMod(integer x, integer y)
#pragma mark integerMod
			
		case EidosFunctionIdentifier::integerModFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			int arg1_count = arg1_value->Count();
			
			if ((arg0_count == 1) && (arg1_count == 1))
			{
				int64_t int1 = arg0_value->IntAtIndex(0, nullptr);
				int64_t int2 = arg1_value->IntAtIndex(0, nullptr);
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
				
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
						
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
						
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerMod() cannot perform modulo by 0." << eidos_terminate(nullptr);
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						int_result->PushInt(int1_vec[value_index] % int2);
				}
				else	// if ((arg0_count != arg1_count) && (arg0_count != 1) && (arg1_count != 1))
				{
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integerMod() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << eidos_terminate(nullptr);
				}
			}
			
			break;
		}
			
			
			//	(logical)isFinite(float x)
			#pragma mark isFinite
			
		case EidosFunctionIdentifier::isFiniteFunction:
		{
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
			break;
		}
			
			
			//	(logical)isInfinite(float x)
			#pragma mark isInfinite
			
		case EidosFunctionIdentifier::isInfiniteFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			
			if (arg0_count == 1)
			{
				result_SP = (isinf(arg0_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
				const std::vector<double> &float_vec = *arg0_value->FloatVector();
				EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
				std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
				result_SP = EidosValue_SP(logical_result);
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					logical_result_vec.emplace_back(isinf(float_vec[value_index]));
			}
			break;
		}
			
			
			//	(logical)isNAN(float x)
			#pragma mark isNAN
			
		case EidosFunctionIdentifier::isNaNFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			
			if (arg0_count == 1)
			{
				result_SP = (isnan(arg0_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				// We have arg0_count != 1 and arg0_value is guaranteed to be an EidosValue_Float, so we can use the fast API
				const std::vector<double> &float_vec = *arg0_value->FloatVector();
				EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
				std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
				result_SP = EidosValue_SP(logical_result);
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					logical_result_vec.emplace_back(isnan(float_vec[value_index]));
			}
			break;
		}
			
			
			//	(float)log(numeric x)
			#pragma mark log
			
		case EidosFunctionIdentifier::logFunction:
		{
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
			break;
		}
			
			
			//	(float)log10(numeric x)
			#pragma mark log10
			
		case EidosFunctionIdentifier::log10Function:
		{
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
			break;
		}
			
			
			//	(float)log2(numeric x)
			#pragma mark log2
			
		case EidosFunctionIdentifier::log2Function:
		{
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
			break;
		}
			
			
			//	(numeric$)product(numeric x)
			#pragma mark product
			
		case EidosFunctionIdentifier::productFunction:
		{
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
			break;
		}
			
			
			//	(numeric$)sum(lif x)
			#pragma mark sum
			
		case EidosFunctionIdentifier::sumFunction:
		{
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
			break;
		}
			
			
			//	(float)round(float x)
			#pragma mark round
			
		case EidosFunctionIdentifier::roundFunction:
		{
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
			break;
		}
			
			
			//	(float)sin(numeric x)
			#pragma mark sin
			
		case EidosFunctionIdentifier::sinFunction:
		{
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
			break;
		}
			
			
			//	(float)sqrt(numeric x)
			#pragma mark sqrt
			
		case EidosFunctionIdentifier::sqrtFunction:
		{
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
			break;
		}
			
			
			//	(float)tan(numeric x)
			#pragma mark tan
			
		case EidosFunctionIdentifier::tanFunction:
		{
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
			break;
		}
			
			
			//	(float)trunc(float x)
			#pragma mark trunc
			
		case EidosFunctionIdentifier::truncFunction:
		{
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
			break;
		}
			
			
		// ************************************************************************************
		//
		//	summary statistics functions
		//
#pragma mark -
#pragma mark Summary statistics functions
#pragma mark -
			
			
			//	(+$)max(+ x)
			#pragma mark max
			
		case EidosFunctionIdentifier::maxFunction:
		{
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
			break;
		}
			
			
			//	(float$)mean(numeric x)
			#pragma mark mean
			
		case EidosFunctionIdentifier::meanFunction:
		{
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
			break;
		}
			
			
			//	(+$)min(+ x)
			#pragma mark min
			
		case EidosFunctionIdentifier::minFunction:
		{
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
			break;
		}
			
			
			//	(+)pmax(+ x, + y)
			#pragma mark pmax
			
		case EidosFunctionIdentifier::pmaxFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			EidosValueType arg1_type = arg1_value->Type();
			int arg1_count = arg1_value->Count();
			
			if (arg0_type != arg1_type)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function pmax() requires arguments x and y to be the same type." << eidos_terminate(nullptr);
			if (arg0_count != arg1_count)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function pmax() requires arguments x and y to be of equal length." << eidos_terminate(nullptr);
			
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
			break;
		}
			
			
			//	(+)pmin(+ x, + y)
			#pragma mark pmin
			
		case EidosFunctionIdentifier::pminFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			EidosValueType arg1_type = arg1_value->Type();
			int arg1_count = arg1_value->Count();
			
			if (arg0_type != arg1_type)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function pmin() requires arguments x and y to be the same type." << eidos_terminate(nullptr);
			if (arg0_count != arg1_count)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function pmin() requires arguments x and y to be of equal length." << eidos_terminate(nullptr);
			
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
			break;
		}
			
			
			//	(numeric)range(numeric x)
			#pragma mark range
			
		case EidosFunctionIdentifier::rangeFunction:
		{
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
			break;
		}
			
			
			//	(float$)sd(numeric x)
			#pragma mark sd
			
		case EidosFunctionIdentifier::sdFunction:
		{
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
			break;
		}
			
			
		// ************************************************************************************
		//
		//	distribution draw / density functions
		//
#pragma mark -
#pragma mark Distribution draw/density functions
#pragma mark -
			
			
			//	(float)dnorm(float x, [numeric mean], [numeric sd])
			#pragma mark dnorm
			
		case EidosFunctionIdentifier::dnormFunction:
		{
			EidosValue *arg_quantile = p_arguments[0].get();
			int64_t num_quantiles = arg_quantile->Count();
			EidosValue *arg_mu = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
			EidosValue *arg_sigma = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
			int arg_mu_count = (arg_mu ? arg_mu->Count() : 1);
			int arg_sigma_count = (arg_sigma ? arg_sigma->Count() : 1);
			bool mu_singleton = (arg_mu_count == 1);
			bool sigma_singleton = (arg_sigma_count == 1);
			
			if (!mu_singleton && (arg_mu_count != num_quantiles))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function dnorm() requires mean to be of length 1 or equal in length to x." << eidos_terminate(nullptr);
			if (!sigma_singleton && (arg_sigma_count != num_quantiles))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function dnorm() requires sd to be of length 1 or equal in length to x." << eidos_terminate(nullptr);
			
			double mu0 = ((arg_mu && arg_mu_count) ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
			double sigma0 = ((arg_sigma && arg_sigma_count) ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
			
			if (mu_singleton && sigma_singleton)
			{
				if (sigma0 <= 0.0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function dnorm() requires sd > 0.0 (" << sigma0 << " supplied)." << eidos_terminate(nullptr);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function dnorm() requires sd > 0.0 (" << sigma << " supplied)." << eidos_terminate(nullptr);
					
					float_result->PushFloat(gsl_ran_gaussian_pdf(float_vec[value_index] - mu, sigma));
				}
			}
			
			break;
		}
			
			
			//	(integer)rbinom(integer$ n, integer size, float prob)
			#pragma mark rbinom
			
		case EidosFunctionIdentifier::rbinomFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			EidosValue *arg_size = p_arguments[1].get();
			EidosValue *arg_prob = p_arguments[2].get();
			int arg_size_count = arg_size->Count();
			int arg_prob_count = arg_prob->Count();
			bool size_singleton = (arg_size_count == 1);
			bool prob_singleton = (arg_prob_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rbinom() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
			if (!size_singleton && (arg_size_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rbinom() requires size to be of length 1 or n." << eidos_terminate(nullptr);
			if (!prob_singleton && (arg_prob_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rbinom() requires prob to be of length 1 or n." << eidos_terminate(nullptr);
			
			int size0 = (int)arg_size->IntAtIndex(0, nullptr);
			double probability0 = arg_prob->FloatAtIndex(0, nullptr);
			
			if (size_singleton && prob_singleton)
			{
				if (size0 < 0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rbinom() requires size >= 0 (" << size0 << " supplied)." << eidos_terminate(nullptr);
				if ((probability0 < 0.0) || (probability0 > 1.0))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rbinom() requires probability in [0.0, 1.0] (" << probability0 << " supplied)." << eidos_terminate(nullptr);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rbinom() requires size >= 0 (" << size << " supplied)." << eidos_terminate(nullptr);
					if ((probability < 0.0) || (probability > 1.0))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rbinom() requires probability in [0.0, 1.0] (" << probability << " supplied)." << eidos_terminate(nullptr);
					
					int_result->PushInt(gsl_ran_binomial(gEidos_rng, probability, size));
				}
			}
			
			break;
		}
			
			
			//	(float)rexp(integer$ n, [numeric mu])
			#pragma mark rexp
			
		case EidosFunctionIdentifier::rexpFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			EidosValue *arg_mu = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
			int arg_mu_count = (arg_mu ? arg_mu->Count() : 1);
			bool mu_singleton = (arg_mu_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rexp() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
			if (!mu_singleton && (arg_mu_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rexp() requires mu to be of length 1 or n." << eidos_terminate(nullptr);
			
			if (mu_singleton)
			{
				double mu0 = (arg_mu ? arg_mu->FloatAtIndex(0, nullptr) : 1.0);
				
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
			
			break;
		}
			
			
			//	(float)rgamma(integer$ n, numeric mean, numeric shape)
			#pragma mark rgamma
			
		case EidosFunctionIdentifier::rgammaFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			EidosValue *arg_mean = p_arguments[1].get();
			EidosValue *arg_shape = p_arguments[2].get();
			int arg_mean_count = arg_mean->Count();
			int arg_shape_count = arg_shape->Count();
			bool mean_singleton = (arg_mean_count == 1);
			bool shape_singleton = (arg_shape_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rgamma() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
			if (!mean_singleton && (arg_mean_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rgamma() requires mean to be of length 1 or n." << eidos_terminate(nullptr);
			if (!shape_singleton && (arg_shape_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rgamma() requires shape to be of length 1 or n." << eidos_terminate(nullptr);
			
			double mean0 = (arg_mean_count ? arg_mean->FloatAtIndex(0, nullptr) : 1.0);
			double shape0 = (arg_shape_count ? arg_shape->FloatAtIndex(0, nullptr) : 0.0);
			
			if (mean_singleton && shape_singleton)
			{
				if (shape0 <= 0.0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rgamma() requires shape > 0.0 (" << shape0 << " supplied)." << eidos_terminate(nullptr);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rgamma() requires shape > 0.0 (" << shape << " supplied)." << eidos_terminate(nullptr);
					
					float_result->PushFloat(gsl_ran_gamma(gEidos_rng, shape, mean / shape));
				}
			}
			
			break;
		}
			
			
			//	(float)rlnorm(integer$ n, [numeric meanlog], [numeric sdlog])
			#pragma mark rlnorm
			
		case EidosFunctionIdentifier::rlnormFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			EidosValue *arg_meanlog = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
			EidosValue *arg_sdlog = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
			int arg_meanlog_count = (arg_meanlog ? arg_meanlog->Count() : 1);
			int arg_sdlog_count = (arg_sdlog ? arg_sdlog->Count() : 1);
			bool meanlog_singleton = (arg_meanlog_count == 1);
			bool sdlog_singleton = (arg_sdlog_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rlnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
			if (!meanlog_singleton && (arg_meanlog_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rlnorm() requires meanlog to be of length 1 or n." << eidos_terminate(nullptr);
			if (!sdlog_singleton && (arg_sdlog_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rlnorm() requires sdlog to be of length 1 or n." << eidos_terminate(nullptr);
			
			double meanlog0 = ((arg_meanlog && arg_meanlog_count) ? arg_meanlog->FloatAtIndex(0, nullptr) : 0.0);
			double sdlog0 = ((arg_sdlog && arg_sdlog_count) ? arg_sdlog->FloatAtIndex(0, nullptr) : 1.0);
			
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
			
			break;
		}
			
			
			//	(float)rnorm(integer$ n, [numeric mean], [numeric sd])
			#pragma mark rnorm
			
		case EidosFunctionIdentifier::rnormFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			EidosValue *arg_mu = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
			EidosValue *arg_sigma = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
			int arg_mu_count = (arg_mu ? arg_mu->Count() : 1);
			int arg_sigma_count = (arg_sigma ? arg_sigma->Count() : 1);
			bool mu_singleton = (arg_mu_count == 1);
			bool sigma_singleton = (arg_sigma_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
			if (!mu_singleton && (arg_mu_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rnorm() requires mean to be of length 1 or n." << eidos_terminate(nullptr);
			if (!sigma_singleton && (arg_sigma_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rnorm() requires sd to be of length 1 or n." << eidos_terminate(nullptr);
			
			double mu0 = ((arg_mu && arg_mu_count) ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
			double sigma0 = ((arg_sigma && arg_sigma_count) ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
			
			if (mu_singleton && sigma_singleton)
			{
				if (sigma0 < 0.0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rnorm() requires sd >= 0.0 (" << sigma0 << " supplied)." << eidos_terminate(nullptr);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rnorm() requires sd >= 0.0 (" << sigma << " supplied)." << eidos_terminate(nullptr);
					
					float_result->PushFloat(gsl_ran_gaussian(gEidos_rng, sigma) + mu);
				}
			}
			
			break;
		}
			
			
			//	(integer)rpois(integer$ n, numeric lambda)
			#pragma mark rpois
			
		case EidosFunctionIdentifier::rpoisFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			EidosValue *arg_lambda = p_arguments[1].get();
			int arg_lambda_count = arg_lambda->Count();
			bool lambda_singleton = (arg_lambda_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rpois() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
			if (!lambda_singleton && (arg_lambda_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rpois() requires lambda to be of length 1 or n." << eidos_terminate(nullptr);
			
			// Here we ignore USE_GSL_POISSON and always use the GSL.  This is because we don't know whether lambda (otherwise known as mu) is
			// small or large, and because we don't know what level of accuracy is demanded by whatever the user is doing with the deviates,
			// and so forth; it makes sense to just rely on the GSL for maximal accuracy and reliability.
			
			if (lambda_singleton)
			{
				double lambda0 = arg_lambda->FloatAtIndex(0, nullptr);
				
				if (lambda0 <= 0.0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rpois() requires lambda > 0.0 (" << lambda0 << " supplied)." << eidos_terminate(nullptr);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rpois() requires lambda > 0.0 (" << lambda << " supplied)." << eidos_terminate(nullptr);
					
					int_result->PushInt(gsl_ran_poisson(gEidos_rng, lambda));
				}
			}
			
			break;
		}
			
			
			//	(float)runif(integer$ n, [numeric min], [numeric max])
			#pragma mark runif
			
		case EidosFunctionIdentifier::runifFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			
			if (p_argument_count == 1)
			{
				// With no min or max, we can streamline quite a bit
				if (num_draws == 1)
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_rng_uniform(gEidos_rng)));
				}
				else
				{
					if (num_draws < 0)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function runif() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
					
					EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)num_draws);
					result_SP = EidosValue_SP(float_result);
					
					for (int draw_index = 0; draw_index < num_draws; ++draw_index)
						float_result->PushFloat(gsl_rng_uniform(gEidos_rng));
				}
			}
			else
			{
				EidosValue *arg_min = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
				EidosValue *arg_max = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
				int arg_min_count = (arg_min ? arg_min->Count() : 1);
				int arg_max_count = (arg_max ? arg_max->Count() : 1);
				bool min_singleton = (arg_min_count == 1);
				bool max_singleton = (arg_max_count == 1);
				
				if (num_draws < 0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function runif() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
				if (!min_singleton && (arg_min_count != num_draws))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function runif() requires min to be of length 1 or n." << eidos_terminate(nullptr);
				if (!max_singleton && (arg_max_count != num_draws))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function runif() requires max to be of length 1 or n." << eidos_terminate(nullptr);
				
				double min_value0 = ((arg_min && arg_min_count) ? arg_min->FloatAtIndex(0, nullptr) : 0.0);
				double max_value0 = ((arg_max && arg_max_count) ? arg_max->FloatAtIndex(0, nullptr) : 1.0);
				double range0 = max_value0 - min_value0;
				
				if (min_singleton && max_singleton)
				{
					if (range0 < 0.0)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function runif() requires min < max." << eidos_terminate(nullptr);
					
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function runif() requires min < max." << eidos_terminate(nullptr);
						
						float_result->PushFloat(gsl_rng_uniform(gEidos_rng) * range + min_value);
					}
				}
			}
			
			break;
		}
			
			
			//	(float)rweibull(integer$ n, numeric lambda, numeric k)
			#pragma mark rweibull
			
		case EidosFunctionIdentifier::rweibullFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t num_draws = arg0_value->IntAtIndex(0, nullptr);
			EidosValue *arg_lambda = p_arguments[1].get();
			EidosValue *arg_k = p_arguments[2].get();
			int arg_lambda_count = arg_lambda->Count();
			int arg_k_count = arg_k->Count();
			bool lambda_singleton = (arg_lambda_count == 1);
			bool k_singleton = (arg_k_count == 1);
			
			if (num_draws < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rweibull() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << eidos_terminate(nullptr);
			if (!lambda_singleton && (arg_lambda_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rweibull() requires lambda to be of length 1 or n." << eidos_terminate(nullptr);
			if (!k_singleton && (arg_k_count != num_draws))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rweibull() requires k to be of length 1 or n." << eidos_terminate(nullptr);
			
			double lambda0 = (arg_lambda_count ? arg_lambda->FloatAtIndex(0, nullptr) : 0.0);
			double k0 = (arg_k_count ? arg_k->FloatAtIndex(0, nullptr) : 0.0);
			
			if (lambda_singleton && k_singleton)
			{
				if (lambda0 <= 0.0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rweibull() requires lambda > 0.0 (" << lambda0 << " supplied)." << eidos_terminate(nullptr);
				if (k0 <= 0.0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rweibull() requires k > 0.0 (" << k0 << " supplied)." << eidos_terminate(nullptr);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rweibull() requires lambda > 0.0 (" << lambda << " supplied)." << eidos_terminate(nullptr);
					if (k <= 0.0)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rweibull() requires k > 0.0 (" << k << " supplied)." << eidos_terminate(nullptr);
					
					float_result->PushFloat(gsl_ran_weibull(gEidos_rng, lambda, k));
				}
			}
			
			break;
		}
			
			
		// ************************************************************************************
		//
		//	vector construction functions
		//
#pragma mark -
#pragma mark Vector conversion functions
#pragma mark -
			
			
			//	(*)c(...)
			#pragma mark c
			
		case EidosFunctionIdentifier::cFunction:
		{
			result_SP = ConcatenateEidosValues(p_arguments, p_argument_count, true);
			break;
		}
			
			
			//	(float)float(integer$ length)
			#pragma mark float
			
		case EidosFunctionIdentifier::floatFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
			
			if (element_count < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function float() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
			
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)element_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t value_index = element_count; value_index > 0; --value_index)
				float_result->PushFloat(0.0);
			break;
		}
			
			
			//	(integer)integer(integer$ length)
			#pragma mark integer
			
		case EidosFunctionIdentifier::integerFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
			
			if (element_count < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function integer() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
			
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)element_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int64_t value_index = element_count; value_index > 0; --value_index)
				int_result->PushInt(0);
			break;
		}
			
			
			//	(logical)logical(integer$ length)
			#pragma mark logical
			
		case EidosFunctionIdentifier::logicalFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
			
			if (element_count < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function logical() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
			
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve((int)element_count);
			std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
			result_SP = EidosValue_SP(logical_result);
			
			for (int64_t value_index = element_count; value_index > 0; --value_index)
				logical_result_vec.emplace_back(false);
			break;
		}
			
			
			//	(object<undefined>)object(void)
			#pragma mark object
			
		case EidosFunctionIdentifier::objectFunction:
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidos_UndefinedClassObject));
			break;
		}
			
			
			//	(*)rep(* x, integer$ count)
			#pragma mark rep
			
		case EidosFunctionIdentifier::repFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			
			int64_t rep_count = arg1_value->IntAtIndex(0, nullptr);
			
			if (rep_count < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function rep() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << eidos_terminate(nullptr);
			
			// the return type depends on the type of the first argument, which will get replicated
			result_SP = arg0_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				for (int value_idx = 0; value_idx < arg0_count; value_idx++)
					result->PushValueFromIndexOfEidosValue(value_idx, *arg0_value, nullptr);
			
			break;
		}
			
			
			//	(*)repEach(* x, integer count)
			#pragma mark repEach
			
		case EidosFunctionIdentifier::repEachFunction:
		{
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
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function repEach() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << eidos_terminate(nullptr);
				
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function repEach() requires all elements of count to be greater than or equal to 0 (" << rep_count << " supplied)." << eidos_terminate(nullptr);
					
					for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
						result->PushValueFromIndexOfEidosValue(value_idx, *arg0_value, nullptr);
				}
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function repEach() requires that parameter count's size() either (1) be equal to 1, or (2) be equal to the size() of its first argument." << eidos_terminate(nullptr);
			}
			
			break;
		}
			
			
			//	(*)sample(* x, integer$ size, [logical$ replace], [numeric weights])
			#pragma mark sample
			
		case EidosFunctionIdentifier::sampleFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			int64_t sample_size = p_arguments[1]->IntAtIndex(0, nullptr);
			bool replace = ((p_argument_count >= 3) ? p_arguments[2]->LogicalAtIndex(0, nullptr) : false);
			
			if (sample_size < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function sample() requires a sample size >= 0 (" << sample_size << " supplied)." << eidos_terminate(nullptr);
			if (sample_size == 0)
			{
				result_SP = arg0_value->NewMatchingType();
				break;
			}
			
			if (arg0_count == 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function sample() provided with insufficient elements (0 supplied)." << eidos_terminate(nullptr);
			
			if (!replace && (arg0_count < sample_size))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function sample() provided with insufficient elements (" << arg0_count << " supplied, " << sample_size << " needed)." << eidos_terminate(nullptr);
			
			result_SP = arg0_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			// the algorithm used depends on whether weights were supplied
			if (p_argument_count >= 4)
			{
				// weights supplied
				vector<double> weights_vector;
				double weights_sum = 0.0;
				EidosValue *arg3_value = p_arguments[3].get();
				int arg3_count = arg3_value->Count();
				
				if (arg3_count != arg0_count)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function sample() requires x and weights to be the same length." << eidos_terminate(nullptr);
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					double weight = arg3_value->FloatAtIndex(value_index, nullptr);
					
					if (weight < 0.0)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function sample() requires all weights to be non-negative (" << weight << " supplied)." << eidos_terminate(nullptr);
					
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function sample() ran out of eligible elements from which to sample." << eidos_terminate(nullptr);
					if (weights_sum <= 0.0)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function sample() encountered weights summing to <= 0." << eidos_terminate(nullptr);
					
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
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): (internal error) function sample() ran out of eligible elements from which to sample." << eidos_terminate(nullptr);
						
						int rose_index = (int)gsl_rng_uniform_int(gEidos_rng, contender_count);
						
						result->PushValueFromIndexOfEidosValue(index_vector[rose_index], *arg0_value, nullptr);
						
						index_vector.erase(index_vector.begin() + rose_index);
						--contender_count;
					}
				}
			}
			
			break;
		}
			
			
			//	(numeric)seq(numeric$ from, numeric$ to, [numeric$ by])
			#pragma mark seq
			
		case EidosFunctionIdentifier::seqFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			EidosValue *arg1_value = p_arguments[1].get();
			EidosValueType arg1_type = arg1_value->Type();
			EidosValue *arg2_value = ((p_argument_count == 3) ? p_arguments[2].get() : nullptr);
			EidosValueType arg2_type = (arg2_value ? arg2_value->Type() : EidosValueType::kValueInt);
			
			if ((arg0_type == EidosValueType::kValueFloat) || (arg1_type == EidosValueType::kValueFloat) || (arg2_type == EidosValueType::kValueFloat))
			{
				// float return case
				double first_value = arg0_value->FloatAtIndex(0, nullptr);
				double second_value = arg1_value->FloatAtIndex(0, nullptr);
				double default_by = ((first_value < second_value) ? 1 : -1);
				double by_value = (arg2_value ? arg2_value->FloatAtIndex(0, nullptr) : default_by);
				
				if (by_value == 0.0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function seq() requires by != 0." << eidos_terminate(nullptr);
				if (((first_value < second_value) && (by_value < 0)) || ((first_value > second_value) && (by_value > 0)))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function seq() by has incorrect sign." << eidos_terminate(nullptr);
				
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
				int64_t by_value = (arg2_value ? arg2_value->IntAtIndex(0, nullptr) : default_by);
				
				if (by_value == 0)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function seq() requires by != 0." << eidos_terminate(nullptr);
				if (((first_value < second_value) && (by_value < 0)) || ((first_value > second_value) && (by_value > 0)))
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function seq() by has incorrect sign." << eidos_terminate(nullptr);
				
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)(1 + (second_value - first_value) / by_value));		// take a stab at a reserve size; might not be quite right, but no harm
				result_SP = EidosValue_SP(int_result);
				
				if (by_value > 0)
					for (int64_t seq_value = first_value; seq_value <= second_value; seq_value += by_value)
						int_result->PushInt(seq_value);
				else
					for (int64_t seq_value = first_value; seq_value >= second_value; seq_value += by_value)
						int_result->PushInt(seq_value);
			}
			
			break;
		}
			
			
			//	(integer)seqAlong(* x)
			#pragma mark seqAlong
			
		case EidosFunctionIdentifier::seqAlongFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(arg0_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				int_result->PushInt(value_index);
			break;
		}
			
			
			//	(string)string(integer$ length)
			#pragma mark string
			
		case EidosFunctionIdentifier::stringFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int64_t element_count = arg0_value->IntAtIndex(0, nullptr);
			
			if (element_count < 0)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function string() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << eidos_terminate(nullptr);
			
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)element_count);
			result_SP = EidosValue_SP(string_result);
			
			for (int64_t value_index = element_count; value_index > 0; --value_index)
				string_result->PushString(gEidosStr_empty_string);
			break;
		}
			

		// ************************************************************************************
		//
		//	value inspection/manipulation functions
		//
#pragma mark -
#pragma mark Value inspection/manipulation functions
#pragma mark -
			
			
			//	(logical$)all(logical x)
			#pragma mark all
			
		case EidosFunctionIdentifier::allFunction:
		{
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
			
			break;
		}
			
			
			//	(logical$)any(logical x)
			#pragma mark any
			
		case EidosFunctionIdentifier::anyFunction:
		{
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
			
			break;
		}
			
			
			//	(void)cat(* x, [string$ sep])
			#pragma mark cat
			
		case EidosFunctionIdentifier::catFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValueType arg0_type = arg0_value->Type();
			std::ostringstream &output_stream = ExecutionOutputStream();
			string separator = ((p_argument_count >= 2) ? p_arguments[1]->StringAtIndex(0, nullptr) : gEidosStr_space_string);
			
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
			break;
		}
			
			
			//	(string)format(string$ format, numeric x)
			#pragma mark format
			
		case EidosFunctionIdentifier::formatFunction:
		{
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
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); only one % escape is allowed." << eidos_terminate(nullptr);
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
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); flag '+' specified more than once." << eidos_terminate(nullptr);
								
								flag_plus = true;
								++pos;	// skip the '+'
							}
							else if (flag == '-')
							{
								if (flag_minus)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); flag '-' specified more than once." << eidos_terminate(nullptr);
								
								flag_minus = true;
								++pos;	// skip the '-'
							}
							else if (flag == ' ')
							{
								if (flag_space)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); flag ' ' specified more than once." << eidos_terminate(nullptr);
								
								flag_space = true;
								++pos;	// skip the ' '
							}
							else if (flag == '#')
							{
								if (flag_pound)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); flag '#' specified more than once." << eidos_terminate(nullptr);
								
								flag_pound = true;
								++pos;	// skip the '#'
							}
							else if (flag == '0')
							{
								if (flag_zero)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); flag '0' specified more than once." << eidos_terminate(nullptr);
								
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
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type integer." << eidos_terminate(nullptr);
							}
							else if ((conv_ch == 'f') || (conv_ch == 'F') || (conv_ch == 'e') || (conv_ch == 'E') || (conv_ch == 'g') || (conv_ch == 'G'))
							{
								if (arg1_type != EidosValueType::kValueFloat)
									EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type float." << eidos_terminate(nullptr);
							}
							else
							{
								EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); conversion specifier '" << conv_ch << "' not supported." << eidos_terminate(nullptr);
							}
						}
						else
						{
							EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); missing conversion specifier after '%'." << eidos_terminate(nullptr);
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
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): (internal error) bad format string in function format(); conversion specifier '" << conv_ch << "' not recognized in format string fix code." << eidos_terminate(nullptr);
				
				format.replace(conversion_specifier_pos, 1, new_conv_string);
			}
			
			// Check for possibilities that produce undefined behavior according to the C++11 standard
			if (flag_pound && ((conv_ch == 'd') || (conv_ch == 'i')))
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): bad format string in function format(); the flag '#' may not be used with the conversion specifier '" << conv_ch << "'." << eidos_terminate(nullptr);
			
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
			
			break;
		}
			
			
			//	(logical$)identical(* x, * y)
			#pragma mark identical
			
		case EidosFunctionIdentifier::identicalFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			EidosValueType arg1_type = arg1_value->Type();
			int arg1_count = arg1_value->Count();
			
			if ((arg0_type != arg1_type) || (arg0_count != arg1_count))
			{
				result_SP = gStaticEidosValue_LogicalF;
				break;
			}
			
			result_SP = gStaticEidosValue_LogicalT;
			
			if (arg0_type == EidosValueType::kValueNULL)
				break;
			
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
			break;
		}
			
			
			//	(*)ifelse(logical test, * trueValues, * falseValues)
			#pragma mark ifelse
			
		case EidosFunctionIdentifier::ifelseFunction:
		{
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
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function ifelse() requires arguments 2 and 3 to be the same type (" << arg1_type << " and " << arg2_type << " supplied)." << eidos_terminate(nullptr);
			
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
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function ifelse() requires arguments of equal length, or trueValues and falseValues most both be of length 1." << eidos_terminate(nullptr);
			}
			break;
		}
			
			
			//	(integer)match(* x, * table)
			#pragma mark match
			
		case EidosFunctionIdentifier::matchFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			int arg0_count = arg0_value->Count();
			EidosValue *arg1_value = p_arguments[1].get();
			EidosValueType arg1_type = arg1_value->Type();
			int arg1_count = arg1_value->Count();
			
			if (arg0_type != arg1_type)
				EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function match() requires arguments x and table to be the same type." << eidos_terminate(nullptr);
			
			if (arg0_type == EidosValueType::kValueNULL)
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				break;
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
			break;
		}
			
			
			//	(integer)nchar(string x)
			#pragma mark nchar
			
		case EidosFunctionIdentifier::ncharFunction:
		{
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
			break;
		}
			
			
			//	(string$)paste(* x, [string$ sep])
			#pragma mark paste
			
		case EidosFunctionIdentifier::pasteFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValueType arg0_type = arg0_value->Type();
			string separator = ((p_argument_count >= 2) ? p_arguments[1]->StringAtIndex(0, nullptr) : gEidosStr_space_string);
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
			break;
		}
			
			
			//	(void)print(* x)
			#pragma mark print
			
		case EidosFunctionIdentifier::printFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			
			ExecutionOutputStream() << *arg0_value << endl;
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(*)rev(* x)
			#pragma mark rev
			
		case EidosFunctionIdentifier::revFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			
			result_SP = arg0_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int value_index = arg0_count - 1; value_index >= 0; --value_index)
				result->PushValueFromIndexOfEidosValue(value_index, *arg0_value, nullptr);
			break;
		}
			
			
			//	(integer$)size(* x)
			#pragma mark size
			
		case EidosFunctionIdentifier::sizeFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(arg0_value->Count()));
			break;
		}
			
			
			//	(+)sort(+ x, [logical$ ascending])
			#pragma mark sort
			
		case EidosFunctionIdentifier::sortFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			
			result_SP = arg0_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				result->PushValueFromIndexOfEidosValue(value_index, *arg0_value, nullptr);
			
			result->Sort((p_argument_count == 1) ? true : p_arguments[1]->LogicalAtIndex(0, nullptr));
			break;
		}
			
			
			//	(object)sortBy(object x, string$ property, [logical$ ascending])
			#pragma mark sortBy
			
		case EidosFunctionIdentifier::sortByFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)arg0_value)->Class()))->Reserve(arg0_count);
			result_SP = EidosValue_SP(object_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				object_result->PushObjectElement(arg0_value->ObjectElementAtIndex(value_index, nullptr));
			
			object_result->SortBy(p_arguments[1]->StringAtIndex(0, nullptr), (p_argument_count == 2) ? true : p_arguments[2]->LogicalAtIndex(0, nullptr));
			break;
		}
			
			
			//	(void)str(* x)
			#pragma mark str
			
		case EidosFunctionIdentifier::strFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			int arg0_count = arg0_value->Count();
			std::ostringstream &output_stream = ExecutionOutputStream();
			
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
			break;
		}
			
			
			//	(string)strsplit(string$ x, [string$ sep])
			#pragma mark strsplit
			
		case EidosFunctionIdentifier::strsplitFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			string joined_string = arg0_value->StringAtIndex(0, nullptr);
			string separator = ((p_argument_count >= 2) ? p_arguments[1]->StringAtIndex(0, nullptr) : gEidosStr_space_string);
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
					start_idx = sep_idx + separator.size();
				}
			}
			
			break;
		}
			
			
			//	(string)substr(string x, integer first, [integer last])
			#pragma mark substr
			
		case EidosFunctionIdentifier::substrFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			
			if (arg0_count == 1)
			{
				const std::string &string_value = arg0_value->StringAtIndex(0, nullptr);
				int64_t len = (int64_t)string_value.size();
				EidosValue *arg_first = p_arguments[1].get();
				int arg_first_count = arg_first->Count();
				
				if (arg_first_count != arg0_count)
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function substr() requires the size of first to be 1, or equal to the size of x." << eidos_terminate(nullptr);
				
				int64_t first0 = arg_first->IntAtIndex(0, nullptr);
				
				if (p_argument_count >= 3)
				{
					// last supplied
					EidosValue *arg_last = p_arguments[2].get();
					int arg_last_count = arg_last->Count();
					
					if (arg_last_count != arg0_count)
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function substr() requires the size of last to be 1, or equal to the size of x." << eidos_terminate(nullptr);
					
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
					EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function substr() requires the size of first to be 1, or equal to the size of x." << eidos_terminate(nullptr);
				
				EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(arg0_count);
				result_SP = EidosValue_SP(string_result);
				
				int64_t first0 = arg_first->IntAtIndex(0, nullptr);
				
				if (p_argument_count >= 3)
				{
					// last supplied
					EidosValue *arg_last = p_arguments[2].get();
					int arg_last_count = arg_last->Count();
					bool last_singleton = (arg_last_count == 1);
					
					if (!last_singleton && (arg_last_count != arg0_count))
						EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): function substr() requires the size of last to be 1, or equal to the size of x." << eidos_terminate(nullptr);
					
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
			
			break;
		}
			
			
			//	(*)unique(* x)
			#pragma mark unique
			
		case EidosFunctionIdentifier::uniqueFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			int arg0_count = arg0_value->Count();
			
			if (arg0_count == 0)
			{
				result_SP = arg0_value->NewMatchingType();
			}
			else if (arg0_count == 1)
			{
				result_SP = arg0_value->CopyValues();
			}
			else if (arg0_type == EidosValueType::kValueLogical)
			{
				const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
				bool containsF = false, containsT = false;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					if (logical_vec[value_index])
						containsT = true;
					else
						containsF = true;
				}
				
				if (containsF && !containsT)
					result_SP = gStaticEidosValue_LogicalF;
				else if (containsT && !containsF)
					result_SP = gStaticEidosValue_LogicalT;
				else if (!containsT && !containsF)
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
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
			break;
		}
			
			
			//	(integer)which(logical x)
			#pragma mark which
			
		case EidosFunctionIdentifier::whichFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			int arg0_count = arg0_value->Count();
			const std::vector<eidos_logical_t> &logical_vec = *arg0_value->LogicalVector();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
				if (logical_vec[value_index])
					int_result->PushInt(value_index);
			break;
		}
			
			
			//	(integer)whichMax(+ x)
			#pragma mark whichMax
			
		case EidosFunctionIdentifier::whichMaxFunction:
		{
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
			break;
		}
			
			
			//	(integer)whichMin(+ x)
			#pragma mark whichMin
			
		case EidosFunctionIdentifier::whichMinFunction:
		{
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
			break;
		}
			
			
		// ************************************************************************************
		//
		//	value type testing/coercion functions
		//
#pragma mark -
#pragma mark Value type testing/coercion functions
#pragma mark -
			
			
			//	(float)asFloat(+ x)
			#pragma mark asFloat
			
		case EidosFunctionIdentifier::asFloatFunction:
		{
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
            break;
		}
			
			
			//	(integer)asInteger(+ x)
			#pragma mark asInteger
			
		case EidosFunctionIdentifier::asIntegerFunction:
		{
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
            break;
		}
			
			
			//	(logical)asLogical(+ x)
			#pragma mark asLogical
			
		case EidosFunctionIdentifier::asLogicalFunction:
		{
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
			break;
		}
			
			
			//	(string)asString(+ x)
			#pragma mark asString
			
		case EidosFunctionIdentifier::asStringFunction:
		{
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
            break;
		}
			
			
			//	(string$)elementType(* x)
			#pragma mark elementType
			
		case EidosFunctionIdentifier::elementTypeFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(arg0_value->ElementType()));
			break;
		}
			
			
			//	(logical$)isFloat(* x)
			#pragma mark isFloat
			
		case EidosFunctionIdentifier::isFloatFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			
			result_SP = (arg0_type == EidosValueType::kValueFloat) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
			
			//	(logical$)isInteger(* x)
			#pragma mark isInteger
			
		case EidosFunctionIdentifier::isIntegerFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			
			result_SP = (arg0_type == EidosValueType::kValueInt) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
			
			//	(logical$)isLogical(* x)
			#pragma mark isLogical
			
		case EidosFunctionIdentifier::isLogicalFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			
			result_SP = (arg0_type == EidosValueType::kValueLogical) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
			
			//	(logical$)isNULL(* x)
			#pragma mark isNULL
			
		case EidosFunctionIdentifier::isNULLFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			
			result_SP = (arg0_type == EidosValueType::kValueNULL) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
			
			//	(logical$)isObject(* x)
			#pragma mark isObject
			
		case EidosFunctionIdentifier::isObjectFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			
			result_SP = (arg0_type == EidosValueType::kValueObject) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
			
			//	(logical$)isString(* x)
			#pragma mark isString
			
		case EidosFunctionIdentifier::isStringFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			EidosValueType arg0_type = arg0_value->Type();
			
			result_SP = (arg0_type == EidosValueType::kValueString) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			break;
		}
			
			
			//	(string$)type(* x)
			#pragma mark type
			
		case EidosFunctionIdentifier::typeFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(StringForEidosValueType(arg0_value->Type())));
			break;
		}
			
			
		// ************************************************************************************
		//
		//	filesystem access functions
		//
#pragma mark -
#pragma mark Filesystem access functions
#pragma mark -
			
			
			//	(string)filesAtPath(string$ path, [logical$ fullPaths])
			#pragma mark filesAtPath
			
		case EidosFunctionIdentifier::filesAtPathFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			string base_path = arg0_value->StringAtIndex(0, nullptr);
			int base_path_length = (int)base_path.length();
			bool base_path_ends_in_slash = (base_path_length > 0) && (base_path[base_path_length-1] == '/');
			string path = EidosResolvedPath(base_path);
			bool fullPaths = (p_argument_count >= 2) ? p_arguments[1]->LogicalAtIndex(0, nullptr) : false;
			
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
				ExecutionOutputStream() << "#WARNING (ExecuteFunctionCall): function filesAtPath() could not open path " << path << "." << endl;
				result_SP = gStaticEidosValueNULL;
			}
			break;
		}
			
			
			//	(string)readFile(string$ filePath)
			#pragma mark readFile
			
		case EidosFunctionIdentifier::readFileFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			string base_path = arg0_value->StringAtIndex(0, nullptr);
			string file_path = EidosResolvedPath(base_path);
			
			// read the contents in
			std::ifstream file_stream(file_path.c_str());
			
			if (!file_stream.is_open())
			{
				// not a fatal error, just a warning log
				ExecutionOutputStream() << "#WARNING (ExecuteFunctionCall): function readFile() could not read file at path " << file_path << "." << endl;
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
					ExecutionOutputStream() << "#WARNING (ExecuteFunctionCall): function readFile() encountered stream errors while reading file at path " << file_path << "." << endl;
					
					result_SP = gStaticEidosValueNULL;
				}
			}
			break;
		}
			
			
			//	(logical$)writeFile(string$ filePath, string contents, [logical$ append])
			#pragma mark writeFile
			
		case EidosFunctionIdentifier::writeFileFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			string base_path = arg0_value->StringAtIndex(0, nullptr);
			string file_path = EidosResolvedPath(base_path);
			
			// the second argument is the file contents to write
			EidosValue *arg1_value = p_arguments[1].get();
			int arg1_count = arg1_value->Count();
			
			// the third argument is an optional append flag, F by default
			bool append = (p_argument_count >= 3) ? p_arguments[2]->LogicalAtIndex(0, nullptr) : false;
			
			// write the contents out
			std::ofstream file_stream(file_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
			
			if (!file_stream.is_open())
			{
				// Not a fatal error, just a warning log
				ExecutionOutputStream() << "#WARNING (ExecuteFunctionCall): function writeFile() could not write to file at path " << file_path << "." << endl;
				result_SP = gStaticEidosValue_LogicalF;
			}
			else
			{
				if (arg1_count == 1)
				{
					file_stream << arg1_value->StringAtIndex(0, nullptr);
				}
				else
				{
					const std::vector<std::string> &string_vec = *arg1_value->StringVector();
					
					for (int value_index = 0; value_index < arg1_count; ++value_index)
					{
						file_stream << string_vec[value_index];
						
						// Add newlines after all lines but the last
						if (value_index + 1 < arg1_count)
							file_stream << endl;
					}
				}
				
				if (file_stream.bad())
				{
					// Not a fatal error, just a warning log
					ExecutionOutputStream() << "#WARNING (ExecuteFunctionCall): function writeFile() encountered stream errors while writing to file at path " << file_path << "." << endl;
					result_SP = gStaticEidosValue_LogicalF;
				}
				else
				{
					result_SP = gStaticEidosValue_LogicalT;
				}
			}
			break;
		}
			
			
		// ************************************************************************************
		//
		//	miscellaneous functions
		//
#pragma mark -
#pragma mark Miscellaneous functions
#pragma mark -
		
			
			//	(*)apply(* x, string$ lambdaSource)
#pragma mark apply
			
		case EidosFunctionIdentifier::applyFunction:
		{
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
				EidosSymbolTable &symbols = SymbolTable();									// use our own symbol table
				EidosFunctionMap &function_map = FunctionMap();								// use our own function map
				EidosInterpreter interpreter(*script, symbols, function_map, this->eidos_context_);
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					EidosValue_SP apply_value = arg0_value->GetValueAtIndex(value_index, nullptr);
					
					// Set the iterator variable "applyValue" to the value
					symbols.SetValueForSymbolNoCopy(gEidosID_applyValue, std::move(apply_value));
					
					// Get the result
					results.emplace_back(interpreter.EvaluateInterpreterBlock(false));
				}
				
				// We do not want a leftover applyValue symbol in the symbol table, so we remove it now
				symbols.RemoveValueForSymbol(gEidosID_applyValue);
				
				// Assemble all the individual results together, just as c() does
				ExecutionOutputStream() << interpreter.ExecutionOutput();
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
			
			break;
		}
			
			
			//	(void)beep([string$ soundName])
			#pragma mark beep
			
		case EidosFunctionIdentifier::beepFunction:
		{
			EidosValue *arg0_value = (p_argument_count >= 1) ? p_arguments[0].get() : nullptr;
			string name_string = (arg0_value ? arg0_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
			
			string beep_error = EidosBeep(name_string);
			
			if (beep_error.length())
			{
				std::ostringstream &output_stream = ExecutionOutputStream();
				
				output_stream << beep_error << std::endl;
			}
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(string$)date(void)
			#pragma mark date
			
		case EidosFunctionIdentifier::dateFunction:
		{
			time_t rawtime;
			struct tm *timeinfo;
			char buffer[25];	// should never be more than 10, in fact, plus a null
			
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(buffer, 25, "%d-%m-%Y", timeinfo);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string(buffer)));
			break;
		}
			
			
			//	(void)defineConstant(string$ symbol, + x)
			#pragma mark defineConstant
			
		case EidosFunctionIdentifier::defineConstantFunction:
		{
			std::string symbol_name = p_arguments[0]->StringAtIndex(0, nullptr);
			const EidosValue_SP x_value_sp = p_arguments[1];
			EidosGlobalStringID symbol_id = EidosGlobalStringIDForString(symbol_name);
			EidosSymbolTable &symbols = SymbolTable();
			
			symbols.DefineConstantForSymbol(symbol_id, x_value_sp);
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(*)doCall(string$ function, ...)
			#pragma mark doCall
			
		case EidosFunctionIdentifier::doCallFunction:
		{
			std::string function_name = p_arguments[0]->StringAtIndex(0, nullptr);
			const EidosValue_SP *const arguments = p_arguments + 1;
			int argument_count = p_argument_count - 1;
			
			result_SP = ExecuteFunctionCall(function_name, nullptr, arguments, argument_count);
			break;
		}
			
			
			//	(*)executeLambda(string$ lambdaSource, [logical$ timed])
			#pragma mark executeLambda
			
		case EidosFunctionIdentifier::executeLambdaFunction:
		{
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
			bool timed = (p_argument_count >= 2) ? p_arguments[1]->LogicalAtIndex(0, nullptr) : false;
			clock_t begin = 0, end = 0;
			
			gEidosCharacterStartOfError = -1;
			gEidosCharacterEndOfError = -1;
			gEidosCharacterStartOfErrorUTF16 = -1;
			gEidosCharacterEndOfErrorUTF16 = -1;
			gEidosCurrentScript = script;
			gEidosExecutingRuntimeScript = true;
			
			try
			{
				EidosSymbolTable &symbols = SymbolTable();									// use our own symbol table
				EidosFunctionMap &function_map = FunctionMap();								// use our own function map
				EidosInterpreter interpreter(*script, symbols, function_map, this->eidos_context_);
				
				if (timed)
					begin = clock();
				
				result_SP = interpreter.EvaluateInterpreterBlock(false);
				
				if (timed)
					end = clock();
				
				ExecutionOutputStream() << interpreter.ExecutionOutput();
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
				
				ExecutionOutputStream() << "// ********** executeLambda() elapsed time: " << time_spent << std::endl;
			}
			
			if (!arg0_value_singleton)
				delete script;
			
			break;
		}
			
			
			//	(logical$)exists(string$ symbol)
			#pragma mark exists
			
		case EidosFunctionIdentifier::existsFunction:
		{
			std::string symbol_name = p_arguments[0]->StringAtIndex(0, nullptr);
			EidosGlobalStringID symbol_id = EidosGlobalStringIDForString(symbol_name);
			EidosSymbolTable &symbols = SymbolTable();
			
			bool exists = symbols.ContainsSymbol(symbol_id);
			
			result_SP = (exists ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			break;
		}
			
			
			//	(void)function([string$ functionName])
			#pragma mark function
			
		case EidosFunctionIdentifier::functionFunction:
		{
			EidosValue *arg0_value = (p_argument_count >= 1) ? p_arguments[0].get() : nullptr;
			std::ostringstream &output_stream = ExecutionOutputStream();
			string match_string = (arg0_value ? arg0_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
			bool signature_found = false;
			
			// function_map_ is already alphebetized since maps keep sorted order
			for (auto functionPairIter = function_map_.begin(); functionPairIter != function_map_.end(); ++functionPairIter)
			{
				const EidosFunctionSignature *iter_signature = functionPairIter->second;
				
				if (arg0_value && (iter_signature->function_name_.compare(match_string) != 0))
					continue;
				
				if (!arg0_value && (iter_signature->function_name_.substr(0, 1).compare("_") == 0))
					continue;	// skip internal functions that start with an underscore, unless specifically requested
				
				output_stream << *iter_signature << endl;
				signature_found = true;
			}
			
			if (arg0_value && !signature_found)
				output_stream << "No function signature found for \"" << match_string << "\"." << endl;
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(void)ls(void)
			#pragma mark ls
			
		case EidosFunctionIdentifier::lsFunction:
		{
			ExecutionOutputStream() << global_symbols_;
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(void)license(void)
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
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(void)rm([string variableNames], [logical$ removeConstants])
			#pragma mark rm
			
		case EidosFunctionIdentifier::rmFunction:
		{
			bool removeConstants = ((p_argument_count >= 2) ? p_arguments[1]->LogicalAtIndex(0, nullptr) : false);
			vector<string> symbols_to_remove;
			
			if (p_argument_count == 0)
				symbols_to_remove = global_symbols_.ReadWriteSymbols();
			else
			{
				EidosValue *arg0_value = p_arguments[0].get();
				int arg0_count = arg0_value->Count();
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					symbols_to_remove.emplace_back(arg0_value->StringAtIndex(value_index, nullptr));
			}
			
			if (removeConstants)
				for (string &symbol : symbols_to_remove)
					global_symbols_.RemoveConstantForSymbol(EidosGlobalStringIDForString(symbol));
			else
				for (string &symbol : symbols_to_remove)
					global_symbols_.RemoveValueForSymbol(EidosGlobalStringIDForString(symbol));
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(void)setSeed(integer$ seed)
			#pragma mark setSeed
			
		case EidosFunctionIdentifier::setSeedFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			
			EidosInitializeRNGFromSeed(arg0_value->IntAtIndex(0, nullptr));
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
			//	(integer$)getSeed(void)
			#pragma mark getSeed
			
		case EidosFunctionIdentifier::getSeedFunction:
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_rng_last_seed));
			break;
		}
			
			
			//	(void)stop([string$ message])
			#pragma mark stop
			
		case EidosFunctionIdentifier::stopFunction:
		{
			if (p_argument_count >= 1)
				ExecutionOutputStream() << p_arguments[0]->StringAtIndex(0, nullptr) << endl;
			
			EIDOS_TERMINATION << "ERROR (EidosInterpreter::ExecuteFunctionCall): stop() called." << eidos_terminate(nullptr);
			break;
		}
			
			
			//	(string$)time(void)
			#pragma mark time
			
		case EidosFunctionIdentifier::timeFunction:
		{
			time_t rawtime;
			struct tm *timeinfo;
			char buffer[20];		// should never be more than 8, in fact, plus a null
			
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(buffer, 20, "%H:%M:%S", timeinfo);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string(buffer)));
			break;
		}
			
			
			//	(void)version(void)
			#pragma mark version
			
		case EidosFunctionIdentifier::versionFunction:
		{
			std::ostringstream &output_stream = ExecutionOutputStream();
			
			output_stream << "Eidos version 1.0.2" << endl;	// EIDOS VERSION
			
			if (gEidosContextVersion.length())
				output_stream << gEidosContextVersion << endl;
			
			result_SP = gStaticEidosValueNULLInvisible;
			break;
		}
			
			
		// ************************************************************************************
		//
		//	object instantiation
		//
			
			
			//	(object<_TestElement>$)_Test(integer$ yolk)
			#pragma mark _TestFunction
			
		case EidosFunctionIdentifier::_TestFunction:
		{
			EidosValue *arg0_value = p_arguments[0].get();
			Eidos_TestElement *testElement = new Eidos_TestElement(arg0_value->IntAtIndex(0, nullptr));
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(testElement, gEidos_TestElementClass));
			testElement->Release();
			break;
		}
			
	}
	
	// If the code above supplied no return value, invisible NULL is assumed as a default to prevent a crash;
	// but this is an internal error so the check is run only when in debug.  Not in debug, we crash.
#ifdef DEBUG
	if (!result_SP)
	{
		std::cerr << "result_SP not set in function " << p_function_name << std::endl;
		result_SP = gStaticEidosValueNULLInvisible;
	}
#endif
	
	// Check the return value against the signature
	p_function_signature->CheckReturn(*result_SP);
	
	return result_SP;
}



































































