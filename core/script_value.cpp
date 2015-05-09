//
//  script_value.cpp
//  SLiM
//
//  Created by Ben Haller on 4/7/15.
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

#include "script_value.h"
#include "script_functions.h"
#include "slim_global.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


string StringForScriptValueType(const ScriptValueType p_type)
{
	switch (p_type)
	{
		case ScriptValueType::kValueNULL:		return "NULL";
		case ScriptValueType::kValueLogical:	return "logical";
		case ScriptValueType::kValueString:		return "string";
		case ScriptValueType::kValueInt:		return "integer";
		case ScriptValueType::kValueFloat:		return "float";
		case ScriptValueType::kValueObject:		return "object";
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const ScriptValueType p_type)
{
	p_outstream << StringForScriptValueType(p_type);
	
	return p_outstream;
}

std::string StringForScriptValueMask(const ScriptValueMask p_mask)
{
	std::string out_string;
	bool is_optional = !!(p_mask & kScriptValueMaskOptional);
	bool requires_singleton = !!(p_mask & kScriptValueMaskSingleton);
	ScriptValueMask type_mask = p_mask & kScriptValueMaskFlagStrip;
	
	if (is_optional)
		out_string += "[";
	
	if (type_mask == kScriptValueMaskNone)			out_string += "?";
	else if (type_mask == kScriptValueMaskAny)		out_string += "*";
	else if (type_mask == kScriptValueMaskAnyBase)	out_string += "+";
	else if (type_mask == kScriptValueMaskNULL)		out_string += "void";
	else if (type_mask == kScriptValueMaskLogical)	out_string += "logical";
	else if (type_mask == kScriptValueMaskString)	out_string += "string";
	else if (type_mask == kScriptValueMaskInt)		out_string += "integer";
	else if (type_mask == kScriptValueMaskFloat)	out_string += "float";
	else if (type_mask == kScriptValueMaskObject)	out_string += "object";
	else if (type_mask == kScriptValueMaskNumeric)	out_string += "numeric";
	else
	{
		if (type_mask & kScriptValueMaskNULL)		out_string += "N";
		if (type_mask & kScriptValueMaskLogical)	out_string += "l";
		if (type_mask & kScriptValueMaskInt)		out_string += "i";
		if (type_mask & kScriptValueMaskFloat)		out_string += "f";
		if (type_mask & kScriptValueMaskString)		out_string += "s";
		if (type_mask & kScriptValueMaskObject)		out_string += "o";
	}
	
	if (requires_singleton)
		out_string += "$";
	
	if (is_optional)
		out_string += "]";
	
	return out_string;
}

// returns -1 if value1[index1] < value2[index2], 0 if ==, 1 if >, with full type promotion
int CompareScriptValues(const ScriptValue *p_value1, int p_index1, const ScriptValue *p_value2, int p_index2)
{
	ScriptValueType type1 = p_value1->Type();
	ScriptValueType type2 = p_value2->Type();
	
	if ((type1 == ScriptValueType::kValueNULL) || (type2 == ScriptValueType::kValueNULL))
		SLIM_TERMINATION << "ERROR (CompareScriptValues): comparison with NULL is illegal." << endl << slim_terminate();
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((type1 == ScriptValueType::kValueString) || (type2 == ScriptValueType::kValueString))
	{
		string string1 = p_value1->StringAtIndex(p_index1);
		string string2 = p_value2->StringAtIndex(p_index2);
		
		return string1.compare(string2);
	}
	
	// float is the next highest type, so we promote to float if either operand is a float
	if ((type1 == ScriptValueType::kValueFloat) || (type2 == ScriptValueType::kValueFloat))
	{
		double float1 = p_value1->FloatAtIndex(p_index1);
		double float2 = p_value2->FloatAtIndex(p_index2);
		
		return (float1 < float2) ? -1 : ((float1 > float2) ? 1 : 0);
	}
	
	// int is the next highest type, so we promote to int if either operand is a int
	if ((type1 == ScriptValueType::kValueInt) || (type2 == ScriptValueType::kValueInt))
	{
		int64_t int1 = p_value1->IntAtIndex(p_index1);
		int64_t int2 = p_value2->IntAtIndex(p_index2);
		
		return (int1 < int2) ? -1 : ((int1 > int2) ? 1 : 0);
	}
	
	// logical is the next highest type, so we promote to logical if either operand is a logical
	if ((type1 == ScriptValueType::kValueLogical) || (type2 == ScriptValueType::kValueLogical))
	{
		bool logical1 = p_value1->LogicalAtIndex(p_index1);
		bool logical2 = p_value2->LogicalAtIndex(p_index2);
		
		return (logical1 < logical2) ? -1 : ((logical1 > logical2) ? 1 : 0);
	}
	
	// that's the end of the road; we should never reach this point
	SLIM_TERMINATION << "ERROR (CompareScriptValues): comparison involving type " << type1 << " and type " << type2 << " is undefined." << endl << slim_terminate();
	return 0;
}


//
//	ScriptValue
//
#pragma mark ScriptValue

ScriptValue::ScriptValue(const ScriptValue &p_original) : in_symbol_table_(false), invisible_(false)	// doesn't use original for these flags
{
#pragma unused(p_original)
}

ScriptValue::ScriptValue(void)
{
}

ScriptValue::~ScriptValue(void)
{
}

bool ScriptValue::Invisible(void) const
{
	return invisible_;
}

bool ScriptValue::InSymbolTable(void) const
{
	return in_symbol_table_;
}

ScriptValue *ScriptValue::SetInSymbolTable(bool p_in_symbol_table)
{
	in_symbol_table_ = p_in_symbol_table;
	return this;
}

bool ScriptValue::LogicalAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type logical." << endl << slim_terminate();
	return false;
}

std::string ScriptValue::StringAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type string." << endl << slim_terminate();
	return std::string();
}

int64_t ScriptValue::IntAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type integer." << endl << slim_terminate();
	return 0;
}

double ScriptValue::FloatAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type float." << endl << slim_terminate();
	return 0.0;
}

ScriptObjectElement *ScriptValue::ElementAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type object." << endl << slim_terminate();
	return nullptr;
}

std::ostream &operator<<(std::ostream &p_outstream, const ScriptValue &p_value)
{
	p_value.Print(p_outstream);		// get dynamic dispatch
	
	return p_outstream;
}


//
//	ScriptValue_NULL
//
#pragma mark ScriptValue_NULL

ScriptValue_NULL::ScriptValue_NULL(void)
{
}

ScriptValue_NULL::~ScriptValue_NULL(void)
{
}

/* static */ ScriptValue_NULL *ScriptValue_NULL::ScriptValue_NULL_Invisible(void)
{
	ScriptValue_NULL *new_null = new ScriptValue_NULL();
	
	new_null->invisible_ = true;
	
	return new_null;
}

ScriptValueType ScriptValue_NULL::Type(void) const
{
	return ScriptValueType::kValueNULL;
}

int ScriptValue_NULL::Count(void) const
{
	return 0;
}

void ScriptValue_NULL::Print(std::ostream &p_ostream) const
{
	p_ostream << "NULL";
}

ScriptValue *ScriptValue_NULL::GetValueAtIndex(const int p_idx) const
{
#pragma unused(p_idx)
	return new ScriptValue_NULL;
}

void ScriptValue_NULL::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " does not support setting values with the subscript operator ('[]')." << endl << slim_terminate();
}

ScriptValue *ScriptValue_NULL::CopyValues(void) const
{
	return new ScriptValue_NULL(*this);
}

ScriptValue *ScriptValue_NULL::NewMatchingType(void) const
{
	return new ScriptValue_NULL;
}

void ScriptValue_NULL::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
#pragma unused(p_idx)
	if (p_source_script_value->Type() == ScriptValueType::kValueNULL)
		;	// NULL doesn't have values or indices, so this is a no-op
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_NULL::PushValueFromIndexOfScriptValue): type mismatch." << endl << slim_terminate();
}


//
//	ScriptValue_Logical
//
#pragma mark ScriptValue_Logical

ScriptValue_Logical::ScriptValue_Logical(void)
{
}

ScriptValue_Logical::ScriptValue_Logical(bool p_bool1)
{
	values_.push_back(p_bool1);
}

ScriptValue_Logical::ScriptValue_Logical(bool p_bool1, bool p_bool2)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
}

ScriptValue_Logical::ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
}

ScriptValue_Logical::ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
}

ScriptValue_Logical::ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
	values_.push_back(p_bool5);
}

ScriptValue_Logical::ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5, bool p_bool6)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
	values_.push_back(p_bool5);
	values_.push_back(p_bool6);
}

ScriptValue_Logical::~ScriptValue_Logical(void)
{
}

ScriptValueType ScriptValue_Logical::Type(void) const
{
	return ScriptValueType::kValueLogical;
}

int ScriptValue_Logical::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_Logical::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "logical(0)";
	}
	else
	{
		bool first = true;
		
		for (bool value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << (value ? "T" : "F");
		}
	}
}

bool ScriptValue_Logical::LogicalAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

std::string ScriptValue_Logical::StringAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? "T" : "F");
}

int64_t ScriptValue_Logical::IntAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? 1 : 0);
}

double ScriptValue_Logical::FloatAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? 1.0 : 0.0);
}

void ScriptValue_Logical::PushLogical(bool p_logical)
{
	values_.push_back(p_logical);
}

void ScriptValue_Logical::SetLogicalAtIndex(const int p_idx, bool p_logical)
{
	values_.at(p_idx) = p_logical;
}

ScriptValue *ScriptValue_Logical::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_Logical(values_.at(p_idx));
}

void ScriptValue_Logical::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Logical::SetValueAtIndex): subscript " << p_idx << " out of range." << endl << slim_terminate();
	
	values_.at(p_idx) = p_value->LogicalAtIndex(0);
}

ScriptValue *ScriptValue_Logical::CopyValues(void) const
{
	return new ScriptValue_Logical(*this);
}

ScriptValue *ScriptValue_Logical::NewMatchingType(void) const
{
	return new ScriptValue_Logical;
}

void ScriptValue_Logical::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueLogical)
		values_.push_back(p_source_script_value->LogicalAtIndex(p_idx));
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Logical::PushValueFromIndexOfScriptValue): type mismatch." << endl << slim_terminate();
}


//
//	ScriptValue_String
//
#pragma mark ScriptValue_String

ScriptValue_String::ScriptValue_String(void)
{
}

ScriptValue_String::ScriptValue_String(std::string p_string1)
{
	values_.push_back(p_string1);
}

ScriptValue_String::ScriptValue_String(std::string p_string1, std::string p_string2)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
}

ScriptValue_String::ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
}

ScriptValue_String::ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3, std::string p_string4)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
}

ScriptValue_String::ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3, std::string p_string4, std::string p_string5)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
	values_.push_back(p_string5);
}

ScriptValue_String::ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3, std::string p_string4, std::string p_string5, std::string p_string6)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
	values_.push_back(p_string5);
	values_.push_back(p_string6);
}

ScriptValue_String::~ScriptValue_String(void)
{
}

ScriptValueType ScriptValue_String::Type(void) const
{
	return ScriptValueType::kValueString;
}

int ScriptValue_String::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_String::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "string(0)";
	}
	else
	{
		bool first = true;
		
		for (string value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << '"' << value << '"';
		}
	}
}

bool ScriptValue_String::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx).length() > 0);
}

std::string ScriptValue_String::StringAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

int64_t ScriptValue_String::IntAtIndex(int p_idx) const
{
	return strtoll(values_.at(p_idx).c_str(), nullptr, 10);
}

double ScriptValue_String::FloatAtIndex(int p_idx) const
{
	return strtod(values_.at(p_idx).c_str(), nullptr);
}

void ScriptValue_String::PushString(std::string p_string)
{
	values_.push_back(p_string);
}

ScriptValue *ScriptValue_String::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_String(values_.at(p_idx));
}

void ScriptValue_String::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_String::SetValueAtIndex): subscript " << p_idx << " out of range." << endl << slim_terminate();
	
	values_.at(p_idx) = p_value->StringAtIndex(0);
}

ScriptValue *ScriptValue_String::CopyValues(void) const
{
	return new ScriptValue_String(*this);
}

ScriptValue *ScriptValue_String::NewMatchingType(void) const
{
	return new ScriptValue_String;
}

void ScriptValue_String::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueString)
		values_.push_back(p_source_script_value->StringAtIndex(p_idx));
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_String::PushValueFromIndexOfScriptValue): type mismatch." << endl << slim_terminate();
}


//
//	ScriptValue_Int
//
#pragma mark ScriptValue_Int

ScriptValue_Int::ScriptValue_Int(void)
{
}

ScriptValue_Int::ScriptValue_Int(int64_t p_int1)
{
	values_.push_back(p_int1);
}

ScriptValue_Int::ScriptValue_Int(int64_t p_int1, int64_t p_int2)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
}

ScriptValue_Int::ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
}

ScriptValue_Int::ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
}

ScriptValue_Int::ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
}

ScriptValue_Int::ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5, int64_t p_int6)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
	values_.push_back(p_int6);
}

ScriptValue_Int::~ScriptValue_Int(void)
{
}

ScriptValueType ScriptValue_Int::Type(void) const
{
	return ScriptValueType::kValueInt;
}

int ScriptValue_Int::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_Int::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "integer(0)";
	}
	else
	{
		bool first = true;
		
		for (int64_t value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << value;
		}
	}
}

bool ScriptValue_Int::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string ScriptValue_Int::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t ScriptValue_Int::IntAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

double ScriptValue_Int::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void ScriptValue_Int::PushInt(int64_t p_int)
{
	values_.push_back(p_int);
}

ScriptValue *ScriptValue_Int::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_Int(values_.at(p_idx));
}

void ScriptValue_Int::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Int::SetValueAtIndex): subscript " << p_idx << " out of range." << endl << slim_terminate();
	
	values_.at(p_idx) = p_value->IntAtIndex(0);
}

ScriptValue *ScriptValue_Int::CopyValues(void) const
{
	return new ScriptValue_Int(*this);
}

ScriptValue *ScriptValue_Int::NewMatchingType(void) const
{
	return new ScriptValue_Int;
}

void ScriptValue_Int::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueInt)
		values_.push_back(p_source_script_value->IntAtIndex(p_idx));
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Int::PushValueFromIndexOfScriptValue): type mismatch." << endl << slim_terminate();
}


//
//	ScriptValue_Float
//
#pragma mark ScriptValue_Float

ScriptValue_Float::ScriptValue_Float(void)
{
}

ScriptValue_Float::ScriptValue_Float(double p_float1)
{
	values_.push_back(p_float1);
}

ScriptValue_Float::ScriptValue_Float(double p_float1, double p_float2)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
}

ScriptValue_Float::ScriptValue_Float(double p_float1, double p_float2, double p_float3)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
}

ScriptValue_Float::ScriptValue_Float(double p_float1, double p_float2, double p_float3, double p_float4)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
}

ScriptValue_Float::ScriptValue_Float(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
}

ScriptValue_Float::ScriptValue_Float(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5, double p_float6)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
	values_.push_back(p_float6);
}

ScriptValue_Float::~ScriptValue_Float(void)
{
}

ScriptValueType ScriptValue_Float::Type(void) const
{
	return ScriptValueType::kValueFloat;
}

int ScriptValue_Float::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_Float::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "float(0)";
	}
	else
	{
		bool first = true;
		
		for (double value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << value;
		}
	}
}

bool ScriptValue_Float::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string ScriptValue_Float::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t ScriptValue_Float::IntAtIndex(int p_idx) const
{
	return static_cast<int64_t>(values_.at(p_idx));
}

double ScriptValue_Float::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void ScriptValue_Float::PushFloat(double p_float)
{
	values_.push_back(p_float);
}

ScriptValue *ScriptValue_Float::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_Float(values_.at(p_idx));
}

void ScriptValue_Float::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Float::SetValueAtIndex): subscript " << p_idx << " out of range." << endl << slim_terminate();
	
	values_.at(p_idx) = p_value->FloatAtIndex(0);
}

ScriptValue *ScriptValue_Float::CopyValues(void) const
{
	return new ScriptValue_Float(*this);
}

ScriptValue *ScriptValue_Float::NewMatchingType(void) const
{
	return new ScriptValue_Float;
}

void ScriptValue_Float::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueFloat)
		values_.push_back(p_source_script_value->FloatAtIndex(p_idx));
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Float::PushValueFromIndexOfScriptValue): type mismatch." << endl << slim_terminate();
}


//
//	ScriptValue_Object
//
#pragma mark ScriptValue_Object

ScriptValue_Object::ScriptValue_Object(const ScriptValue_Object &p_original)
{
	for (auto value : p_original.values_)
		values_.push_back(value->Retain());
}

ScriptValue_Object::ScriptValue_Object(void)
{
}

ScriptValue_Object::ScriptValue_Object(ScriptObjectElement *p_element1)
{
	values_.push_back(p_element1->Retain());
}

ScriptValue_Object::~ScriptValue_Object(void)
{
	if (values_.size() != 0)
	{
		for (auto value : values_)
			value->Release();
	}
}

ScriptValueType ScriptValue_Object::Type(void) const
{
	return ScriptValueType::kValueObject;
}

std::string ScriptValue_Object::ElementType(void) const
{
	if (values_.size() == 0)
		return "undefined";
	else
		return values_[0]->ElementType();
}

int ScriptValue_Object::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_Object::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "object(0)";
	}
	else
	{
		bool first = true;
		
		for (ScriptObjectElement *value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << *value;
		}
	}
}

ScriptObjectElement *ScriptValue_Object::ElementAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void ScriptValue_Object::PushElement(ScriptObjectElement *p_element)
{
	if ((values_.size() > 0) && (ElementType().compare(p_element->ElementType()) != 0))
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::PushElement): the type of an object cannot be changed." << endl << slim_terminate();
	else
		values_.push_back(p_element->Retain());
}

ScriptValue *ScriptValue_Object::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_Object(values_.at(p_idx));
}

void ScriptValue_Object::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::SetValueAtIndex): subscript " << p_idx << " out of range." << endl << slim_terminate();
	
	// can't change the type of element object we collect
	if ((values_.size() > 0) && (ElementType().compare(p_value->ElementAtIndex(0)->ElementType()) != 0))
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::SetValueAtIndex): the type of an object cannot be changed." << endl << slim_terminate();
	
	values_.at(p_idx)->Release();
	values_.at(p_idx) = p_value->ElementAtIndex(0)->Retain();
}

std::vector<std::string> ScriptValue_Object::ReadOnlyMembers(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->ReadOnlyMembers();
}

std::vector<std::string> ScriptValue_Object::ReadWriteMembers(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->ReadWriteMembers();
}

ScriptValue *ScriptValue_Object::GetValueForMember(const std::string &p_member_name) const
{
	if (values_.size() == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::GetValueForMember): unrecognized member name " << p_member_name << "." << endl << slim_terminate();
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	else
	{
		// get the value from all members and collect the results
		std::vector<std::string> constant_members = values_[0]->ReadOnlyMembers();
		bool is_constant_member = (std::find(constant_members.begin(), constant_members.end(), p_member_name) != constant_members.end());
		vector<ScriptValue*> results;
		
		for (auto value : values_)
		{
			ScriptValue *temp_result = value->GetValueForMember(p_member_name);
			
			if (!is_constant_member && (temp_result->Count() != 1))
				SLIM_TERMINATION << "ERROR (ScriptValue_Object::GetValueForMember): internal error: non-const member " << p_member_name << " produced " << temp_result->Count() << " values for a single element." << endl << slim_terminate();
			
			results.push_back(temp_result);
		}
		
		// concatenate the results using Execute_c(); we pass our own name as p_function_name, which just makes errors be in our name
		ScriptValue *result = ConcatenateScriptValues("ScriptValue_Object::GetValueForMember", results);
		
		// Now we just need to dispose of our temporary ScriptValues
		for (ScriptValue *temp_value : results)
			delete temp_value;
		
		return result;
	}
}

void ScriptValue_Object::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	if (values_.size() == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::SetValueForMember): unrecognized member name " << p_member_name << "." << endl << slim_terminate();
	}
	else
	{
		int p_value_count = p_value->Count();
		
		if (p_value_count == 1)
		{
			// we have a multiplex assignment of one value to (maybe) more than one element: x.foo = 10
			for (auto value : values_)
				value->SetValueForMember(p_member_name, p_value);
		}
		else if (p_value_count == Count())
		{
			// we have a one-to-one assignment of values to elements: x.foo = 1:5 (where x has 5 elements)
			for (int value_idx = 0; value_idx < p_value_count; value_idx++)
			{
				ScriptValue *temp_rvalue = p_value->GetValueAtIndex(value_idx);
				
				values_[value_idx]->SetValueForMember(p_member_name, temp_rvalue);
				
				delete temp_rvalue;
			}
		}
		else
			SLIM_TERMINATION << "ERROR (ScriptValue_Object::SetValueForMember): assignment to a member requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << endl << slim_terminate();
	}
}

ScriptValue *ScriptValue_Object::CopyValues(void) const
{
	return new ScriptValue_Object(*this);
}

ScriptValue *ScriptValue_Object::NewMatchingType(void) const
{
	return new ScriptValue_Object;
}

void ScriptValue_Object::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueObject)
	{
		if ((values_.size() > 0) && (ElementType().compare(p_source_script_value->ElementAtIndex(p_idx)->ElementType()) != 0))
			SLIM_TERMINATION << "ERROR (ScriptValue_Object::PushValueFromIndexOfScriptValue): the type of an object cannot be changed." << endl << slim_terminate();
		else
			values_.push_back(p_source_script_value->ElementAtIndex(p_idx)->Retain());
	}
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::PushValueFromIndexOfScriptValue): type mismatch." << endl << slim_terminate();
}

std::vector<std::string> ScriptValue_Object::Methods(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->Methods();
}

const FunctionSignature *ScriptValue_Object::SignatureForMethod(std::string const &p_method_name) const
{
	if (values_.size() == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::SignatureForMethod): unrecognized method name " << p_method_name << "." << endl << slim_terminate();
		
		return new FunctionSignature("", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL);
	}
	else
		return values_[0]->SignatureForMethod(p_method_name);
}

ScriptValue *ScriptValue_Object::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	if (values_.size() == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::ExecuteMethod): unrecognized method name " << p_method_name << "." << endl << slim_terminate();
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	else
	{
		// call the method on all members and collect the results
		vector<ScriptValue*> results;
		
		for (auto value : values_)
			results.push_back(value->ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter));
		
		// concatenate the results using Execute_c(); we pass our own name as p_function_name, which just makes errors be in our name
		ScriptValue *result = ConcatenateScriptValues("ScriptValue_Object::ExecuteMethod", results);
		
		// Now we just need to dispose of our temporary ScriptValues
		for (ScriptValue *temp_value : results)
			delete temp_value;
		
		return result;
	}
}


//
//	ScriptObjectElement
//
#pragma mark ScriptObjectElement

ScriptObjectElement::ScriptObjectElement(void)
{
}

ScriptObjectElement::~ScriptObjectElement(void)
{
}

ScriptObjectElement *ScriptObjectElement::Retain(void)
{
	// no-op; our lifetime is controlled externally
	return this;
}

ScriptObjectElement *ScriptObjectElement::Release(void)
{
	// no-op; our lifetime is controlled externally
	return this;
}

std::vector<std::string> ScriptObjectElement::ReadOnlyMembers(void) const
{
	return std::vector<std::string>();	// no read-only members
}

std::vector<std::string> ScriptObjectElement::ReadWriteMembers(void) const
{
	return std::vector<std::string>();	// no read-write members
}

ScriptValue *ScriptObjectElement::GetValueForMember(const std::string &p_member_name)
{
	SLIM_TERMINATION << "ERROR (ScriptValue_Object::GetValueForMember): unrecognized member name " << p_member_name << "." << endl << slim_terminate();
	return nullptr;
}

void ScriptObjectElement::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
#pragma unused(p_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Object::SetValueForMember): unrecognized member name " << p_member_name << "." << endl << slim_terminate();
}

std::vector<std::string> ScriptObjectElement::Methods(void) const
{
	std::vector<std::string> methods;
	
	methods.push_back("ls");
	methods.push_back("method");
	
	return methods;
}

const FunctionSignature *ScriptObjectElement::SignatureForMethod(std::string const &p_method_name) const
{
	// Signatures are all preallocated, for speed
	static FunctionSignature *lsSig = nullptr;
	static FunctionSignature *methodsSig = nullptr;
	
	if (!lsSig)
	{
		lsSig = (new FunctionSignature("ls", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL));
		methodsSig = (new FunctionSignature("method", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->AddString_OS();
	}
	
	if (p_method_name.compare("ls") == 0)
		return lsSig;
	else if (p_method_name.compare("method") == 0)
		return methodsSig;
	
	SLIM_TERMINATION << "ERROR (ScriptValue_Object::SignatureForMethod): unrecognized method name " << p_method_name << "." << endl << slim_terminate();
	return new FunctionSignature("", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL);
}

ScriptValue *ScriptObjectElement::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_interpreter)
	if (p_method_name.compare("ls") == 0)
	{
		std::vector<std::string> read_only_member_names = ReadOnlyMembers();
		std::vector<std::string> read_write_member_names = ReadWriteMembers();
		
		for (auto member_name_iter = read_only_member_names.begin(); member_name_iter != read_only_member_names.end(); ++member_name_iter)
		{
			const std::string member_name = *member_name_iter;
			ScriptValue *member_value = GetValueForMember(member_name);
			int member_count = member_value->Count();
			
			if (member_count <= 1)
				p_output_stream << member_name << " => (" << member_value->Type() << ") " << *member_value << endl;
			else
			{
				ScriptValue *first_value = member_value->GetValueAtIndex(0);
				
				p_output_stream << member_name << " => (" << member_value->Type() << ") " << *first_value << " ... (" << member_count << " values)" << endl;
				if (!first_value->InSymbolTable()) delete first_value;
			}
			
			if (!member_value->InSymbolTable()) delete member_value;
		}
		for (auto member_name_iter = read_write_member_names.begin(); member_name_iter != read_write_member_names.end(); ++member_name_iter)
		{
			const std::string member_name = *member_name_iter;
			ScriptValue *member_value = GetValueForMember(member_name);
			int member_count = member_value->Count();
			
			if (member_count <= 1)
				p_output_stream << member_name << " -> (" << member_value->Type() << ") " << *member_value << endl;
			else
			{
				ScriptValue *first_value = member_value->GetValueAtIndex(0);
				
				p_output_stream << member_name << " -> (" << member_value->Type() << ") " << *first_value << " ... (" << member_count << " values)" << endl;
				if (!first_value->InSymbolTable()) delete first_value;
			}
			
			if (!member_value->InSymbolTable()) delete member_value;
		}
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	else if (p_method_name.compare("method") == 0)
	{
		bool has_match_string = (p_arguments.size() == 1);
		string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0) : "");
		std::vector<std::string> method_names = Methods();
		bool signature_found = false;
		
		for (auto method_name_iter = method_names.begin(); method_name_iter != method_names.end(); ++method_name_iter)
		{
			const std::string method_name = *method_name_iter;
			
			if (has_match_string && (method_name.compare(match_string) != 0))
				continue;
			
			const FunctionSignature *method_signature = SignatureForMethod(method_name);
			
			p_output_stream << *method_signature << endl;
			signature_found = true;
		}
		
		if (has_match_string && !signature_found)
			p_output_stream << "No method signature found for \"" << match_string << "\"." << endl;
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	else
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object::ExecuteMethod): unrecognized method name " << p_method_name << "." << endl << slim_terminate();
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
}

void ScriptObjectElement::ConstantSetError(const std::string &p_method_name, const std::string &p_member_name)
{
	SLIM_TERMINATION << "ERROR (" << ElementType() << "::" << p_method_name << "): attempt to set a new value for read-only member " << p_member_name << "." << endl << slim_terminate();
}

void ScriptObjectElement::TypeCheckValue(const std::string &p_method_name, const std::string &p_member_name, ScriptValue *p_value, ScriptValueMask p_type_mask)
{
	uint32_t typemask = p_type_mask;
	bool type_ok = true;
	
	switch (p_value->Type())
	{
		case ScriptValueType::kValueNULL:		type_ok = !!(typemask & kScriptValueMaskNULL);		break;
		case ScriptValueType::kValueLogical:	type_ok = !!(typemask & kScriptValueMaskLogical);	break;
		case ScriptValueType::kValueInt:		type_ok = !!(typemask & kScriptValueMaskInt);		break;
		case ScriptValueType::kValueFloat:		type_ok = !!(typemask & kScriptValueMaskFloat);		break;
		case ScriptValueType::kValueString:		type_ok = !!(typemask & kScriptValueMaskString);	break;
		case ScriptValueType::kValueObject:		type_ok = !!(typemask & kScriptValueMaskObject);	break;
	}
	
	if (!type_ok)
		SLIM_TERMINATION << "ERROR (" << ElementType() << "::" << p_method_name << "): type " << p_value->Type() << " is not legal for member " << p_member_name << "." << endl << slim_terminate();
}

void ScriptObjectElement::RangeCheckValue(const std::string &p_method_name, const std::string &p_member_name, bool p_in_range)
{
	if (!p_in_range)
		SLIM_TERMINATION << "ERROR (" << ElementType() << "::" << p_method_name << "): new value for member " << p_member_name << " is illegal." << endl << slim_terminate();
}

std::ostream &operator<<(std::ostream &p_outstream, const ScriptObjectElement &p_element)
{
	p_outstream << "element(" << p_element.ElementType() << ")";
	
	return p_outstream;
}


//
//	ScriptObjectElementInternal
//
#pragma mark ScriptObjectElementInternal

ScriptObjectElementInternal::ScriptObjectElementInternal(void)
{
//	std::cerr << "ScriptObjectElementInternal::ScriptObjectElementInternal allocated " << this << " with refcount == 1" << endl;
//	print_stacktrace(stderr, 10);
}

ScriptObjectElementInternal::~ScriptObjectElementInternal(void)
{
//	std::cerr << "ScriptObjectElementInternal::~ScriptObjectElementInternal deallocated " << this << endl;
//	print_stacktrace(stderr, 10);
}

ScriptObjectElement *ScriptObjectElementInternal::Retain(void)
{
	refcount_++;
	
//	std::cerr << "ScriptObjectElementInternal::Retain for " << this << ", new refcount == " << refcount_ << endl;
//	print_stacktrace(stderr, 10);
	
	return this;
}

ScriptObjectElement *ScriptObjectElementInternal::Release(void)
{
	refcount_--;
	
//	std::cerr << "ScriptObjectElementInternal::Release for " << this << ", new refcount == " << refcount_ << endl;
//	print_stacktrace(stderr, 10);
	
	if (refcount_ == 0)
	{
		delete this;
		return nullptr;
	}
	
	return this;
}







































































