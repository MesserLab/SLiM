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
#include "slim_global.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>


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
		case ScriptValueType::kValueProxy:		return "object";
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const ScriptValueType p_type)
{
	switch (p_type)
	{
		case ScriptValueType::kValueNULL:		p_outstream << "NULL";		break;
		case ScriptValueType::kValueLogical:	p_outstream << "logical";	break;
		case ScriptValueType::kValueString:		p_outstream << "string";	break;
		case ScriptValueType::kValueInt:		p_outstream << "integer";	break;
		case ScriptValueType::kValueFloat:		p_outstream << "float";		break;
		case ScriptValueType::kValueProxy:		p_outstream << "object";	break;
	}
	
	return p_outstream;
}

// returns -1 if value1[index1] < value2[index2], 0 if ==, 1 if >, with full type promotion
int CompareScriptValues(const ScriptValue *p_value1, int p_index1, const ScriptValue *p_value2, int p_index2)
{
	ScriptValueType type1 = p_value1->Type();
	ScriptValueType type2 = p_value2->Type();
	
	if ((type1 == ScriptValueType::kValueNULL) || (type2 == ScriptValueType::kValueNULL))
		SLIM_TERMINATION << "ERROR (CompareScriptValues): called with a NULL argument." << endl << slim_terminate();
	
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

std::vector<std::string> ScriptValue::ReadOnlyMembers(void) const
{
	return std::vector<std::string>();
}

std::vector<std::string> ScriptValue::ReadWriteMembers(void) const
{
	return std::vector<std::string>();
}

ScriptValue *ScriptValue::GetValueForMember(const std::string &p_member_name) const
{
#pragma unused(p_member_name)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " does not support the member operator ('.')." << endl << slim_terminate();
	return nullptr;
}

void ScriptValue::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
#pragma unused(p_member_name, p_value)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " does not support the member operator ('.')." << endl << slim_terminate();
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
//	ScriptValue_Proxy
//
#pragma mark ScriptValue_Proxy

ScriptValueType ScriptValue_Proxy::Type(void) const
{
	return ScriptValueType::kValueProxy;
}

int ScriptValue_Proxy::Count(void) const
{
	return 1;
}

void ScriptValue_Proxy::Print(std::ostream &p_ostream) const
{
	p_ostream << "<object: " << ProxyType() << ">";
}

ScriptValue *ScriptValue_Proxy::GetValueAtIndex(const int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: ScriptValue_Proxy does not support the subscript operator ('[]')." << endl << slim_terminate();
	return nullptr;
}

void ScriptValue_Proxy::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR: ScriptValue_Proxy does not support the subscript operator ('[]')." << endl << slim_terminate();
}

void ScriptValue_Proxy::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	SLIM_TERMINATION << "ERROR: ScriptValue_Proxy does not support the subscript operator ('[]')." << endl << slim_terminate();
}


//
//	ScriptValue_PathProxy
//
#pragma mark ScriptValue_PathProxy

ScriptValue_PathProxy::ScriptValue_PathProxy(void) : base_path_("~")
{
}

ScriptValue_PathProxy::ScriptValue_PathProxy(std::string p_base_path) : base_path_(p_base_path)
{
}

std::string ScriptValue_PathProxy::ProxyType(void) const
{
	return "Path";
}

ScriptValue *ScriptValue_PathProxy::CopyValues(void) const
{
	return new ScriptValue_PathProxy(base_path_);
}

ScriptValue *ScriptValue_PathProxy::NewMatchingType(void) const
{
	return new ScriptValue_PathProxy(base_path_);
}

std::vector<std::string> ScriptValue_PathProxy::ReadOnlyMembers(void) const
{
	static std::vector<std::string> members;
	static bool been_here = false;
	
	// put hard-coded constants at the top of the list
	if (!been_here)
	{
		members.push_back("files");
		
		been_here = true;
	}
	
	return members;
}

std::vector<std::string> ScriptValue_PathProxy::ReadWriteMembers(void) const
{
	static std::vector<std::string> members;
	static bool been_here = false;
	
	// put hard-coded constants at the top of the list
	if (!been_here)
	{
		members.push_back("path");
		
		been_here = true;
	}
	
	return members;
}

ScriptValue *ScriptValue_PathProxy::GetValueForMember(const std::string &p_member_name) const
{
	if (p_member_name.compare("path") == 0)
		return new ScriptValue_String(base_path_);
	
	if (p_member_name.compare("files") == 0)
	{
		string path = base_path_;
		
		// if there is a leading '~', replace it with the user's home directory; not sure if this works on Windows...
		if ((path.length() > 0) && (path.at(0) == '~'))
		{
			const char *homedir;
			
			if ((homedir = getenv("HOME")) == NULL)
				homedir = getpwuid(getuid())->pw_dir;
			
			if (strlen(homedir))
				path.replace(0, 1, homedir);
		}
		
		// this code modified from GNU: http://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html#Simple-Directory-Lister
		// I'm not sure if it works on Windows... sigh...
		DIR *dp;
		struct dirent *ep;
		
		dp = opendir(path.c_str());
		
		if (dp != NULL)
		{
			ScriptValue_String *return_string = new ScriptValue_String();
			
			while ((ep = readdir(dp)))
				return_string->PushString(ep->d_name);

			(void)closedir(dp);
			
			return return_string;
		}
		else
		{
			// would be nice to emit an error message, but at present we don't have access to the stream...
			//p_output_stream << "Path " << path << " could not be opened." << endl;
			return ScriptValue_NULL::ScriptValue_NULL_Invisible();
		}
	}
	
	// FIXME could return superclass call, and the superclass could implement ls

	SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::GetValueForMember): no member '" << p_member_name << "'." << endl << slim_terminate();
	
	return nullptr;
}

void ScriptValue_PathProxy::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	ScriptValueType value_type = p_value->Type();
	int value_count = p_value->Count();
	
	if (p_member_name.compare("path") == 0)
	{
		if (value_type != ScriptValueType::kValueString)
			SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): type mismatch in assignment to member 'directory'." << endl << slim_terminate();
		if (value_count != 1)
			SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): value of size() == 1 expected in assignment to member 'directory'." << endl << slim_terminate();
		
		base_path_ = p_value->StringAtIndex(0);
	}
	else if (p_member_name.compare("files") == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): member '" << p_member_name << "' is read-only." << endl << slim_terminate();
	}
	else
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): no member '" << p_member_name << "'." << endl << slim_terminate();
	}
}










































































