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
#include "script_functionsignature.h"
#include "slim_global.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


//
//	Global static ScriptValue objects; these are effectively const, although ScriptValues can't be declared as const.
//	Internally, thse are implemented as subclasses that terminate if they are dealloced or modified.
//

ScriptValue_NULL *gStaticScriptValueNULL = ScriptValue_NULL_const::Static_ScriptValue_NULL();
ScriptValue_NULL *gStaticScriptValueNULLInvisible = ScriptValue_NULL_const::Static_ScriptValue_NULL_Invisible();

ScriptValue_Logical *gStaticScriptValue_LogicalT = ScriptValue_Logical_const::Static_ScriptValue_Logical_T();
ScriptValue_Logical *gStaticScriptValue_LogicalF = ScriptValue_Logical_const::Static_ScriptValue_Logical_F();


string StringForScriptValueType(const ScriptValueType p_type)
{
	switch (p_type)
	{
		case ScriptValueType::kValueNULL:		return gStr_NULL;
		case ScriptValueType::kValueLogical:	return gStr_logical;
		case ScriptValueType::kValueString:		return gStr_string;
		case ScriptValueType::kValueInt:		return gStr_integer;
		case ScriptValueType::kValueFloat:		return gStr_float;
		case ScriptValueType::kValueObject:		return gStr_object;
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
	else if (type_mask == kScriptValueMaskNULL)		out_string += gStr_void;
	else if (type_mask == kScriptValueMaskLogical)	out_string += gStr_logical;
	else if (type_mask == kScriptValueMaskString)	out_string += gStr_string;
	else if (type_mask == kScriptValueMaskInt)		out_string += gStr_integer;
	else if (type_mask == kScriptValueMaskFloat)	out_string += gStr_float;
	else if (type_mask == kScriptValueMaskObject)	out_string += gStr_object;
	else if (type_mask == kScriptValueMaskNumeric)	out_string += gStr_numeric;
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
		SLIM_TERMINATION << "ERROR (CompareScriptValues): comparison with NULL is illegal." << slim_terminate();
	
	// comparing one object to another is legal, but objects cannot be compared to other types
	if ((type1 == ScriptValueType::kValueObject) && (type2 == ScriptValueType::kValueObject))
	{
		ScriptObjectElement *element1 = p_value1->ElementAtIndex(p_index1);
		ScriptObjectElement *element2 = p_value2->ElementAtIndex(p_index2);
		
		return (element1 == element2) ? 0 : -1;		// no relative ordering, just equality comparison; enforced in script_interpreter
	}
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((type1 == ScriptValueType::kValueString) || (type2 == ScriptValueType::kValueString))
	{
		string string1 = p_value1->StringAtIndex(p_index1);
		string string2 = p_value2->StringAtIndex(p_index2);
		int compare_result = string1.compare(string2);		// not guaranteed to be -1 / 0 / +1, just negative / 0 / positive
		
		return (compare_result < 0) ? -1 : ((compare_result > 0) ? 1 : 0);
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
	SLIM_TERMINATION << "ERROR (CompareScriptValues): comparison involving type " << type1 << " and type " << type2 << " is undefined." << slim_terminate();
	return 0;
}


//
//	ScriptValue
//
#pragma mark ScriptValue

ScriptValue::ScriptValue(const ScriptValue &p_original) : in_symbol_table_(false), externally_owned_(false), invisible_(false)	// doesn't use original for these flags
{
#pragma unused(p_original)
}

ScriptValue::ScriptValue(void)
{
}

ScriptValue::~ScriptValue(void)
{
}

bool ScriptValue::LogicalAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type logical." << slim_terminate();
	return false;
}

std::string ScriptValue::StringAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type string." << slim_terminate();
	return std::string();
}

int64_t ScriptValue::IntAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type integer." << slim_terminate();
	return 0;
}

double ScriptValue::FloatAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type float." << slim_terminate();
	return 0.0;
}

ScriptObjectElement *ScriptValue::ElementAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " cannot be converted to type object." << slim_terminate();
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
	p_ostream << gStr_NULL;
}

ScriptValue *ScriptValue_NULL::GetValueAtIndex(const int p_idx) const
{
#pragma unused(p_idx)
	return new ScriptValue_NULL;
}

void ScriptValue_NULL::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR: operand type " << this->Type() << " does not support setting values with the subscript operator ('[]')." << slim_terminate();
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
		SLIM_TERMINATION << "ERROR (ScriptValue_NULL::PushValueFromIndexOfScriptValue): type mismatch." << slim_terminate();
}

void ScriptValue_NULL::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	// nothing to do
}


ScriptValue_NULL_const::~ScriptValue_NULL_const(void)
{
	SLIM_TERMINATION << "ERROR (ScriptValue_NULL_const::~ScriptValue_NULL_const): internal error: global constant deallocated." << slim_terminate();
}

/* static */ ScriptValue_NULL *ScriptValue_NULL_const::Static_ScriptValue_NULL(void)
{
	static ScriptValue_NULL_const *static_null = nullptr;
	
	if (!static_null)
	{
		static_null = new ScriptValue_NULL_const();
		static_null->SetExternallyOwned();
	}
	
	return static_null;
}

/* static */ ScriptValue_NULL *ScriptValue_NULL_const::Static_ScriptValue_NULL_Invisible(void)
{
	static ScriptValue_NULL_const *static_null = nullptr;
	
	if (!static_null)
	{
		static_null = new ScriptValue_NULL_const();
		static_null->invisible_ = true;
		static_null->SetExternallyOwned();
	}
	
	return static_null;
}



//
//	ScriptValue_Logical
//
#pragma mark ScriptValue_Logical

ScriptValue_Logical::ScriptValue_Logical(void)
{
}

ScriptValue_Logical::ScriptValue_Logical(std::vector<bool> &p_boolvec)
{
	values_ = p_boolvec;
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
			
			p_ostream << (value ? gStr_T : gStr_F);
		}
	}
}

const std::vector<bool> &ScriptValue_Logical::LogicalVector(void) const
{
	return values_;
}

bool ScriptValue_Logical::LogicalAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

std::string ScriptValue_Logical::StringAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? gStr_T : gStr_F);
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
	return (values_.at(p_idx) ? gStaticScriptValue_LogicalT : gStaticScriptValue_LogicalF);
}

void ScriptValue_Logical::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Logical::SetValueAtIndex): subscript " << p_idx << " out of range." << slim_terminate();
	
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
		SLIM_TERMINATION << "ERROR (ScriptValue_Logical::PushValueFromIndexOfScriptValue): type mismatch." << slim_terminate();
}

void ScriptValue_Logical::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<bool>());
}


ScriptValue_Logical_const::ScriptValue_Logical_const(bool p_bool1) : ScriptValue_Logical(p_bool1)
{
}

ScriptValue_Logical_const::~ScriptValue_Logical_const(void)
{
	SLIM_TERMINATION << "ERROR (ScriptValue_NULL_const::~ScriptValue_NULL_const): internal error: global constant deallocated." << slim_terminate();
}

/* static */ ScriptValue_Logical *ScriptValue_Logical_const::Static_ScriptValue_Logical_T(void)
{
	static ScriptValue_Logical_const *static_T = nullptr;
	
	if (!static_T)
	{
		static_T = new ScriptValue_Logical_const(true);
		static_T->SetExternallyOwned();
	}
	
	return static_T;
}

/* static */ ScriptValue_Logical *ScriptValue_Logical_const::Static_ScriptValue_Logical_F(void)
{
	static ScriptValue_Logical_const *static_F = nullptr;
	
	if (!static_F)
	{
		static_F = new ScriptValue_Logical_const(false);
		static_F->SetExternallyOwned();
	}
	
	return static_F;
}

void ScriptValue_Logical_const::PushLogical(bool p_logical)
{
#pragma unused(p_logical)
	SLIM_TERMINATION << "ERROR (ScriptValue_Logical_const::PushLogical): internal error: ScriptValue_Logical_const is not modifiable." << slim_terminate();
}

void ScriptValue_Logical_const::SetLogicalAtIndex(const int p_idx, bool p_logical)
{
#pragma unused(p_idx, p_logical)
	SLIM_TERMINATION << "ERROR (ScriptValue_Logical_const::SetLogicalAtIndex): internal error: ScriptValue_Logical_const is not modifiable." << slim_terminate();
}

void ScriptValue_Logical_const::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Logical_const::SetValueAtIndex): internal error: ScriptValue_Logical_const is not modifiable." << slim_terminate();
}

void ScriptValue_Logical_const::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Logical_const::PushValueFromIndexOfScriptValue): internal error: ScriptValue_Logical_const is not modifiable." << slim_terminate();
}

void ScriptValue_Logical_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	SLIM_TERMINATION << "ERROR (ScriptValue_Logical_const::Sort): internal error: ScriptValue_Logical_const is not modifiable." << slim_terminate();
}


//
//	ScriptValue_String
//
#pragma mark ScriptValue_String

ScriptValue_String::ScriptValue_String(void)
{
}

ScriptValue_String::ScriptValue_String(std::vector<std::string> &p_stringvec)
{
	values_ = p_stringvec;
}

ScriptValue_String::ScriptValue_String(const std::string &p_string1)
{
	values_.push_back(p_string1);
}

ScriptValue_String::ScriptValue_String(const std::string &p_string1, const std::string &p_string2)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
}

ScriptValue_String::ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
}

ScriptValue_String::ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
}

ScriptValue_String::ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
	values_.push_back(p_string5);
}

ScriptValue_String::ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5, const std::string &p_string6)
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
		
		for (const string &value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << '"' << value << '"';
		}
	}
}

const std::vector<std::string> &ScriptValue_String::StringVector(void) const
{
	return values_;
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
	const string &index_str = values_.at(p_idx);
	
	return strtoll(index_str.c_str(), nullptr, 10);
}

double ScriptValue_String::FloatAtIndex(int p_idx) const
{
	const string &index_str = values_.at(p_idx);
	
	return strtod(index_str.c_str(), nullptr);
}

void ScriptValue_String::PushString(const std::string &p_string)
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
		SLIM_TERMINATION << "ERROR (ScriptValue_String::SetValueAtIndex): subscript " << p_idx << " out of range." << slim_terminate();
	
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
		SLIM_TERMINATION << "ERROR (ScriptValue_String::PushValueFromIndexOfScriptValue): type mismatch." << slim_terminate();
}

void ScriptValue_String::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<std::string>());
}


//
//	ScriptValue_Int
//
#pragma mark ScriptValue_Int

ScriptValue_Int::~ScriptValue_Int(void)
{
}

ScriptValueType ScriptValue_Int::Type(void) const
{
	return ScriptValueType::kValueInt;
}

ScriptValue *ScriptValue_Int::NewMatchingType(void) const
{
	return new ScriptValue_Int_vector;
}


// ScriptValue_Int_vector

ScriptValue_Int_vector::ScriptValue_Int_vector(void)
{
}

ScriptValue_Int_vector::ScriptValue_Int_vector(std::vector<int> &p_intvec)
{
	for (auto intval : p_intvec)
		values_.push_back(intval);
}

ScriptValue_Int_vector::ScriptValue_Int_vector(std::vector<int64_t> &p_intvec)
{
	values_ = p_intvec;
}

ScriptValue_Int_vector::ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
}

ScriptValue_Int_vector::ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
}

ScriptValue_Int_vector::ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
}

ScriptValue_Int_vector::ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
}

ScriptValue_Int_vector::ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5, int64_t p_int6)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
	values_.push_back(p_int6);
}

ScriptValue_Int_vector::~ScriptValue_Int_vector(void)
{
}

int ScriptValue_Int_vector::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_Int_vector::Print(std::ostream &p_ostream) const
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

const std::vector<int64_t> &ScriptValue_Int_vector::IntVector(void) const
{
	return values_;
}

bool ScriptValue_Int_vector::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string ScriptValue_Int_vector::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t ScriptValue_Int_vector::IntAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

double ScriptValue_Int_vector::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void ScriptValue_Int_vector::PushInt(int64_t p_int)
{
	values_.push_back(p_int);
}

ScriptValue *ScriptValue_Int_vector::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_Int_singleton_const(values_.at(p_idx));
}

void ScriptValue_Int_vector::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Int_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << slim_terminate();
	
	values_.at(p_idx) = p_value->IntAtIndex(0);
}

ScriptValue *ScriptValue_Int_vector::CopyValues(void) const
{
	return new ScriptValue_Int_vector(*this);
}

void ScriptValue_Int_vector::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueInt)
		values_.push_back(p_source_script_value->IntAtIndex(p_idx));
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Int_vector::PushValueFromIndexOfScriptValue): type mismatch." << slim_terminate();
}

void ScriptValue_Int_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<int64_t>());
}



// ScriptValue_Int_singleton_const

ScriptValue_Int_singleton_const::ScriptValue_Int_singleton_const(int64_t p_int1) : value_(p_int1)
{
}

ScriptValue_Int_singleton_const::~ScriptValue_Int_singleton_const(void)
{
}

int ScriptValue_Int_singleton_const::Count(void) const
{
	return 1;
}

void ScriptValue_Int_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << value_;
}

bool ScriptValue_Int_singleton_const::LogicalAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return (value_ == 0 ? false : true);
}

std::string ScriptValue_Int_singleton_const::StringAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t ScriptValue_Int_singleton_const::IntAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return value_;
}

double ScriptValue_Int_singleton_const::FloatAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return value_;
}

ScriptValue *ScriptValue_Int_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return new ScriptValue_Int_singleton_const(value_);
}

ScriptValue *ScriptValue_Int_singleton_const::CopyValues(void) const
{
	return new ScriptValue_Int_singleton_const(value_);
}

void ScriptValue_Int_singleton_const::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Int_singleton_const::SetValueAtIndex): internal error: ScriptValue_Float_singleton_const is not modifiable." << slim_terminate();
}

void ScriptValue_Int_singleton_const::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Int_singleton_const::PushValueFromIndexOfScriptValue): internal error: ScriptValue_Float_singleton_const is not modifiable." << slim_terminate();
}

void ScriptValue_Int_singleton_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	SLIM_TERMINATION << "ERROR (ScriptValue_Int_singleton_const::Sort): internal error: ScriptValue_Float_singleton_const is not modifiable." << slim_terminate();
}


//
//	ScriptValue_Float
//
#pragma mark ScriptValue_Float

ScriptValue_Float::~ScriptValue_Float(void)
{
}

ScriptValueType ScriptValue_Float::Type(void) const
{
	return ScriptValueType::kValueFloat;
}

ScriptValue *ScriptValue_Float::NewMatchingType(void) const
{
	return new ScriptValue_Float_vector;
}


// ScriptValue_Float_vector

ScriptValue_Float_vector::ScriptValue_Float_vector(void)
{
}

ScriptValue_Float_vector::ScriptValue_Float_vector(std::vector<double> &p_doublevec)
{
	values_ = p_doublevec;
}

ScriptValue_Float_vector::ScriptValue_Float_vector(double *p_doublebuf, int p_buffer_length)
{
	for (int index = 0; index < p_buffer_length; index++)
		values_.push_back(p_doublebuf[index]);
}

ScriptValue_Float_vector::ScriptValue_Float_vector(double p_float1, double p_float2)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
}

ScriptValue_Float_vector::ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
}

ScriptValue_Float_vector::ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
}

ScriptValue_Float_vector::ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
}

ScriptValue_Float_vector::ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5, double p_float6)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
	values_.push_back(p_float6);
}

ScriptValue_Float_vector::~ScriptValue_Float_vector(void)
{
}

int ScriptValue_Float_vector::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_Float_vector::Print(std::ostream &p_ostream) const
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

const std::vector<double> &ScriptValue_Float_vector::FloatVector(void) const
{
	return values_;
}

bool ScriptValue_Float_vector::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string ScriptValue_Float_vector::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t ScriptValue_Float_vector::IntAtIndex(int p_idx) const
{
	return static_cast<int64_t>(values_.at(p_idx));
}

double ScriptValue_Float_vector::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void ScriptValue_Float_vector::PushFloat(double p_float)
{
	values_.push_back(p_float);
}

ScriptValue *ScriptValue_Float_vector::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_Float_singleton_const(values_.at(p_idx));
}

void ScriptValue_Float_vector::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << slim_terminate();
	
	values_.at(p_idx) = p_value->FloatAtIndex(0);
}

ScriptValue *ScriptValue_Float_vector::CopyValues(void) const
{
	return new ScriptValue_Float_vector(*this);
}

void ScriptValue_Float_vector::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueFloat)
		values_.push_back(p_source_script_value->FloatAtIndex(p_idx));
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_vector::PushValueFromIndexOfScriptValue): type mismatch." << slim_terminate();
}

void ScriptValue_Float_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<double>());
}


// ScriptValue_Float_singleton_const

ScriptValue_Float_singleton_const::ScriptValue_Float_singleton_const(double p_float1) : value_(p_float1)
{
}

ScriptValue_Float_singleton_const::~ScriptValue_Float_singleton_const(void)
{
}

int ScriptValue_Float_singleton_const::Count(void) const
{
	return 1;
}

void ScriptValue_Float_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << value_;
}

bool ScriptValue_Float_singleton_const::LogicalAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return (value_ == 0 ? false : true);
}

std::string ScriptValue_Float_singleton_const::StringAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t ScriptValue_Float_singleton_const::IntAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return static_cast<int64_t>(value_);
}

double ScriptValue_Float_singleton_const::FloatAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return value_;
}

ScriptValue *ScriptValue_Float_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return new ScriptValue_Float_singleton_const(value_);
}

ScriptValue *ScriptValue_Float_singleton_const::CopyValues(void) const
{
	return new ScriptValue_Float_singleton_const(value_);
}

void ScriptValue_Float_singleton_const::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::SetValueAtIndex): internal error: ScriptValue_Float_singleton_const is not modifiable." << slim_terminate();
}

void ScriptValue_Float_singleton_const::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::PushValueFromIndexOfScriptValue): internal error: ScriptValue_Float_singleton_const is not modifiable." << slim_terminate();
}

void ScriptValue_Float_singleton_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	SLIM_TERMINATION << "ERROR (ScriptValue_Float_singleton_const::Sort): internal error: ScriptValue_Float_singleton_const is not modifiable." << slim_terminate();
}


//
//	ScriptValue_Object
//
#pragma mark ScriptValue_Object

ScriptValue_Object::~ScriptValue_Object(void)
{
}

ScriptValueType ScriptValue_Object::Type(void) const
{
	return ScriptValueType::kValueObject;
}

ScriptValue *ScriptValue_Object::NewMatchingType(void) const
{
	return new ScriptValue_Object_vector;
}

void ScriptValue_Object::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::Sort): Sort() is not defined for type object." << slim_terminate();
}


// ScriptValue_Object_vector

ScriptValue_Object_vector::ScriptValue_Object_vector(const ScriptValue_Object_vector &p_original)
{
	for (auto value : p_original.values_)
		values_.push_back(value->Retain());
}

ScriptValue_Object_vector::ScriptValue_Object_vector(void)
{
}

ScriptValue_Object_vector::ScriptValue_Object_vector(std::vector<ScriptObjectElement *> &p_elementvec)
{
	values_ = p_elementvec;		// FIXME should this retain?
}

ScriptValue_Object_vector::~ScriptValue_Object_vector(void)
{
	if (values_.size() != 0)
	{
		for (auto value : values_)
			value->Release();
	}
}

const std::string *ScriptValue_Object_vector::ElementType(void) const
{
	if (values_.size() == 0)
		return &gStr_undefined;		// this is relied upon by the type-check machinery
	else
		return values_[0]->ElementType();
}

int ScriptValue_Object_vector::Count(void) const
{
	return (int)values_.size();
}

void ScriptValue_Object_vector::Print(std::ostream &p_ostream) const
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

ScriptObjectElement *ScriptValue_Object_vector::ElementAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void ScriptValue_Object_vector::PushElement(ScriptObjectElement *p_element)
{
	if ((values_.size() > 0) && (ElementType() != p_element->ElementType()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::PushElement): the type of an object cannot be changed." << slim_terminate();
	else
		values_.push_back(p_element->Retain());
}

ScriptValue *ScriptValue_Object_vector::GetValueAtIndex(const int p_idx) const
{
	return new ScriptValue_Object_singleton_const(values_.at(p_idx));
}

void ScriptValue_Object_vector::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << slim_terminate();
	
	// can't change the type of element object we collect
	if ((values_.size() > 0) && (ElementType() != p_value->ElementAtIndex(0)->ElementType()))
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SetValueAtIndex): the type of an object cannot be changed." << slim_terminate();
	
	values_.at(p_idx)->Release();
	values_.at(p_idx) = p_value->ElementAtIndex(0)->Retain();
}

ScriptValue *ScriptValue_Object_vector::CopyValues(void) const
{
	return new ScriptValue_Object_vector(*this);
}

void ScriptValue_Object_vector::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
	if (p_source_script_value->Type() == ScriptValueType::kValueObject)
	{
		if ((values_.size() > 0) && (ElementType() != p_source_script_value->ElementAtIndex(p_idx)->ElementType()))
			SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::PushValueFromIndexOfScriptValue): the type of an object cannot be changed." << slim_terminate();
		else
			values_.push_back(p_source_script_value->ElementAtIndex(p_idx)->Retain());
	}
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::PushValueFromIndexOfScriptValue): type mismatch." << slim_terminate();
}

bool CompareLogicalObjectSortPairsAscending(std::pair<bool, ScriptObjectElement*> i, std::pair<bool, ScriptObjectElement*> j);
bool CompareLogicalObjectSortPairsAscending(std::pair<bool, ScriptObjectElement*> i, std::pair<bool, ScriptObjectElement*> j)					{ return (i.first < j.first); }
bool CompareLogicalObjectSortPairsDescending(std::pair<bool, ScriptObjectElement*> i, std::pair<bool, ScriptObjectElement*> j);
bool CompareLogicalObjectSortPairsDescending(std::pair<bool, ScriptObjectElement*> i, std::pair<bool, ScriptObjectElement*> j)					{ return (i.first > j.first); }

bool CompareIntObjectSortPairsAscending(std::pair<int64_t, ScriptObjectElement*> i, std::pair<int64_t, ScriptObjectElement*> j);
bool CompareIntObjectSortPairsAscending(std::pair<int64_t, ScriptObjectElement*> i, std::pair<int64_t, ScriptObjectElement*> j)					{ return (i.first < j.first); }
bool CompareIntObjectSortPairsDescending(std::pair<int64_t, ScriptObjectElement*> i, std::pair<int64_t, ScriptObjectElement*> j);
bool CompareIntObjectSortPairsDescending(std::pair<int64_t, ScriptObjectElement*> i, std::pair<int64_t, ScriptObjectElement*> j)				{ return (i.first > j.first); }

bool CompareFloatObjectSortPairsAscending(std::pair<double, ScriptObjectElement*> i, std::pair<double, ScriptObjectElement*> j);
bool CompareFloatObjectSortPairsAscending(std::pair<double, ScriptObjectElement*> i, std::pair<double, ScriptObjectElement*> j)					{ return (i.first < j.first); }
bool CompareFloatObjectSortPairsDescending(std::pair<double, ScriptObjectElement*> i, std::pair<double, ScriptObjectElement*> j);
bool CompareFloatObjectSortPairsDescending(std::pair<double, ScriptObjectElement*> i, std::pair<double, ScriptObjectElement*> j)				{ return (i.first > j.first); }

bool CompareStringObjectSortPairsAscending(std::pair<std::string, ScriptObjectElement*> i, std::pair<std::string, ScriptObjectElement*> j);
bool CompareStringObjectSortPairsAscending(std::pair<std::string, ScriptObjectElement*> i, std::pair<std::string, ScriptObjectElement*> j)		{ return (i.first < j.first); }
bool CompareStringObjectSortPairsDescending(std::pair<std::string, ScriptObjectElement*> i, std::pair<std::string, ScriptObjectElement*> j);
bool CompareStringObjectSortPairsDescending(std::pair<std::string, ScriptObjectElement*> i, std::pair<std::string, ScriptObjectElement*> j)		{ return (i.first > j.first); }

void ScriptValue_Object_vector::SortBy(const std::string &p_property, bool p_ascending)
{
	// length 0 is already sorted
	if (values_.size() == 0)
		return;
	
	// figure out what type the property returns
	GlobalStringID property_string_id = GlobalStringIDForString(p_property);
	ScriptValue *first_result = values_[0]->GetValueForMember(property_string_id);
	ScriptValueType property_type = first_result->Type();
	
	if (first_result->IsTemporary()) delete first_result;
	
	// switch on the property type for efficiency
	switch (property_type)
	{
		case ScriptValueType::kValueNULL:
		case ScriptValueType::kValueObject:
			SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " returned " << property_type << "; a property that evaluates to logical, int, float, or string is required." << slim_terminate();
			break;
			
		case ScriptValueType::kValueLogical:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<bool, ScriptObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				ScriptValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << slim_terminate();
				if (temp_result->Type() != property_type)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << slim_terminate();
				
				sortable_pairs.push_back(std::pair<bool, ScriptObjectElement*>(temp_result->LogicalAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
			
		case ScriptValueType::kValueInt:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<int64_t, ScriptObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				ScriptValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << slim_terminate();
				if (temp_result->Type() != property_type)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << slim_terminate();
				
				sortable_pairs.push_back(std::pair<int64_t, ScriptObjectElement*>(temp_result->IntAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
			
		case ScriptValueType::kValueFloat:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<double, ScriptObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				ScriptValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << slim_terminate();
				if (temp_result->Type() != property_type)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << slim_terminate();
				
				sortable_pairs.push_back(std::pair<double, ScriptObjectElement*>(temp_result->FloatAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
			
		case ScriptValueType::kValueString:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<std::string, ScriptObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				ScriptValue *temp_result = value->GetValueForMember(property_string_id);
				
				if (temp_result->Count() != 1)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << slim_terminate();
				if (temp_result->Type() != property_type)
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << slim_terminate();
				
				sortable_pairs.push_back(std::pair<std::string, ScriptObjectElement*>(temp_result->StringAtIndex(0), value));
				
				if (temp_result->IsTemporary()) delete temp_result;
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.push_back(sorted_pair.second);
			
			break;
		}
	}
}

std::vector<std::string> ScriptValue_Object_vector::ReadOnlyMembersOfElements(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->ReadOnlyMembers();
}

std::vector<std::string> ScriptValue_Object_vector::ReadWriteMembersOfElements(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->ReadWriteMembers();
}

ScriptValue *ScriptValue_Object_vector::GetValueForMemberOfElements(GlobalStringID p_member_id) const
{
	auto values_size = values_.size();
	
	if (values_size == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::GetValueForMemberOfElements): unrecognized member name \"" << StringForGlobalStringID(p_member_id) << "\" (no elements, thus no element type defined)." << slim_terminate();
		
		return gStaticScriptValueNULLInvisible;
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		ScriptObjectElement *value = values_[0];
		ScriptValue *result = value->GetValueForMember(p_member_id);
		
		if (result->Count() != 1)
		{
			// We need to check that this property is const; if not, it is required to give a singleton return
			if (!values_[0]->MemberIsReadOnly(p_member_id))
				SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::GetValueForMemberOfElements): internal error: non-const member " << StringForGlobalStringID(p_member_id) << " produced " << result->Count() << " values for a single element." << slim_terminate();
		}
		
		return result;
	}
	else
	{
		// get the value from all members and collect the results
		vector<ScriptValue*> results;
		bool checked_const_multivalued = false;
		
		for (auto value : values_)
		{
			ScriptValue *temp_result = value->GetValueForMember(p_member_id);
			
			if (!checked_const_multivalued && (temp_result->Count() != 1))
			{
				// We need to check that this property is const; if not, it is required to give a singleton return
				if (!values_[0]->MemberIsReadOnly(p_member_id))
					SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::GetValueForMemberOfElements): internal error: non-const member " << StringForGlobalStringID(p_member_id) << " produced " << temp_result->Count() << " values for a single element." << slim_terminate();
				
				checked_const_multivalued = true;
			}
			
			results.push_back(temp_result);
		}
		
		// concatenate the results using ConcatenateScriptValues(); we pass our own name as p_function_name, which just makes errors be in our name
		ScriptValue *result = ConcatenateScriptValues(gStr_GetValueForMemberOfElements, results.data(), (int)results.size());
		
		// Now we just need to dispose of our temporary ScriptValues
		for (ScriptValue *temp_value : results)
			if (temp_value->IsTemporary()) delete temp_value;
		
		return result;
	}
}

// This somewhat odd method returns one "representative" ScriptValue for the given property, by calling the first element in the
// object.  This is used by code completion to follow the chain of object types along a key path; we don't need all of the values
// that the property would return, we just need one representative value of the proper type.  This is more efficient, of course;
// but the main reason that we don't just call GetValueForMemberOfElements() is that we need an API that will not raise.
ScriptValue *ScriptValue_Object_vector::GetRepresentativeValueOrNullForMemberOfElements(GlobalStringID p_member_id) const
{
	if (values_.size())
	{
		// check that the member is defined before we call our elements
		const std::string &member_name = StringForGlobalStringID(p_member_id);
		std::vector<std::string> constant_members = values_[0]->ReadOnlyMembers();
		
		if (std::find(constant_members.begin(), constant_members.end(), member_name) == constant_members.end())
		{
			std::vector<std::string> variable_members = values_[0]->ReadWriteMembers();
			
			if (std::find(variable_members.begin(), variable_members.end(), member_name) == variable_members.end())
				return nullptr;
		}
		
		// get a value from the first element and return it; we only need to return one representative value
		return values_[0]->GetValueForMember(p_member_id);
	}
	
	return nullptr;
}

void ScriptValue_Object_vector::SetValueForMemberOfElements(GlobalStringID p_member_id, ScriptValue *p_value)
{
	if (values_.size() == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SetValueForMemberOfElements): unrecognized member name \"" << StringForGlobalStringID(p_member_id) << "\" (no elements, thus no element type defined)." << slim_terminate();
	}
	else
	{
		int p_value_count = p_value->Count();
		
		if (p_value_count == 1)
		{
			// we have a multiplex assignment of one value to (maybe) more than one element: x.foo = 10
			for (auto value : values_)
				value->SetValueForMember(p_member_id, p_value);
		}
		else if (p_value_count == Count())
		{
			// we have a one-to-one assignment of values to elements: x.foo = 1:5 (where x has 5 elements)
			for (int value_idx = 0; value_idx < p_value_count; value_idx++)
			{
				ScriptValue *temp_rvalue = p_value->GetValueAtIndex(value_idx);
				
				values_[value_idx]->SetValueForMember(p_member_id, temp_rvalue);
				
				if (temp_rvalue->IsTemporary()) delete temp_rvalue;
			}
		}
		else
			SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SetValueForMemberOfElements): assignment to a member requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << slim_terminate();
	}
}

std::vector<std::string> ScriptValue_Object_vector::MethodsOfElements(void) const
{
	if (values_.size() == 0)
		return std::vector<std::string>();
	else
		return values_[0]->Methods();
}

const FunctionSignature *ScriptValue_Object_vector::SignatureForMethodOfElements(GlobalStringID p_method_id) const
{
	if (values_.size() == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::SignatureForMethodOfElements): unrecognized method name " << StringForGlobalStringID(p_method_id) << "." << slim_terminate();
		
		return new FunctionSignature(gStr_empty_string, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL);
	}
	else
		return values_[0]->SignatureForMethod(p_method_id);
}

ScriptValue *ScriptValue_Object_vector::ExecuteClassMethodOfElements(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	if (values_.size() == 0)
	{
		// FIXME perhaps ScriptValue_Object_vector should know its element type even when empty, so class methods can be called with no elements?
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::ExecuteClassMethodOfElements): unrecognized class method name " << StringForGlobalStringID(p_method_id) << "." << slim_terminate();
		
		return gStaticScriptValueNULLInvisible;
	}
	else
	{
		// call the method on one member only, since it is a class method
		ScriptValue* result = values_[0]->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
		
		return result;
	}
}

ScriptValue *ScriptValue_Object_vector::ExecuteInstanceMethodOfElements(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	auto values_size = values_.size();
	
	if (values_size == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_vector::ExecuteInstanceMethodOfElements): unrecognized instance method name " << StringForGlobalStringID(p_method_id) << "." << slim_terminate();
		
		return gStaticScriptValueNULLInvisible;
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		ScriptObjectElement *value = values_[0];
		ScriptValue *result = value->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
		
		return result;
	}
	else
	{
		// call the method on all members and collect the results
		vector<ScriptValue*> results;
		
		for (auto value : values_)
			results.push_back(value->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter));
		
		// concatenate the results using ConcatenateScriptValues(); we pass our own name as p_function_name, which just makes errors be in our name
		ScriptValue *result = ConcatenateScriptValues(gStr_ExecuteMethod, results.data(), (int)results.size());
		
		// Now we just need to dispose of our temporary ScriptValues
		for (ScriptValue *temp_value : results)
			if (temp_value->IsTemporary()) delete temp_value;
		
		return result;
	}
}


// ScriptValue_Object_singleton_const

ScriptValue_Object_singleton_const::ScriptValue_Object_singleton_const(ScriptObjectElement *p_element1) : value_(p_element1)
{
	p_element1->Retain();
}

ScriptValue_Object_singleton_const::~ScriptValue_Object_singleton_const(void)
{
	value_->Release();
}

const std::string *ScriptValue_Object_singleton_const::ElementType(void) const
{
	return value_->ElementType();
}

int ScriptValue_Object_singleton_const::Count(void) const
{
	return 1;
}

void ScriptValue_Object_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << *value_;
}

ScriptObjectElement *ScriptValue_Object_singleton_const::ElementAtIndex(int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_singleton_const::ElementAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return value_;
}

ScriptValue *ScriptValue_Object_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_singleton_const::GetValueAtIndex): internal error: non-zero index accessed." << slim_terminate();
	
	return new ScriptValue_Object_singleton_const(value_);
}

ScriptValue *ScriptValue_Object_singleton_const::CopyValues(void) const
{
	return new ScriptValue_Object_singleton_const(value_);
}

void ScriptValue_Object_singleton_const::SetValueAtIndex(const int p_idx, ScriptValue *p_value)
{
#pragma unused(p_idx, p_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Object_singleton_const::SetValueAtIndex): internal error: ScriptValue_Object_singleton_const is not modifiable." << slim_terminate();
}

void ScriptValue_Object_singleton_const::PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	SLIM_TERMINATION << "ERROR (ScriptValue_Object_singleton_const::PushValueFromIndexOfScriptValue): internal error: ScriptValue_Object_singleton_const is not modifiable." << slim_terminate();
}

std::vector<std::string> ScriptValue_Object_singleton_const::ReadOnlyMembersOfElements(void) const
{
	return value_->ReadOnlyMembers();
}

std::vector<std::string> ScriptValue_Object_singleton_const::ReadWriteMembersOfElements(void) const
{
	return value_->ReadWriteMembers();
}

ScriptValue *ScriptValue_Object_singleton_const::GetValueForMemberOfElements(GlobalStringID p_member_id) const
{
	ScriptValue *result = value_->GetValueForMember(p_member_id);
	
	if (result->Count() != 1)
	{
		// We need to check that this property is const; if not, it is required to give a singleton return
		if (!value_->MemberIsReadOnly(p_member_id))
			SLIM_TERMINATION << "ERROR (ScriptValue_Object_singleton_const::GetValueForMemberOfElements): internal error: non-const member " << StringForGlobalStringID(p_member_id) << " produced " << result->Count() << " values for a single element." << slim_terminate();
	}
	
	return result;
}

// This somewhat odd method returns one "representative" ScriptValue for the given property, by calling the first element in the
// object.  This is used by code completion to follow the chain of object types along a key path; we don't need all of the values
// that the property would return, we just need one representative value of the proper type.  This is more efficient, of course;
// but the main reason that we don't just call GetValueForMemberOfElements() is that we need an API that will not raise.
ScriptValue *ScriptValue_Object_singleton_const::GetRepresentativeValueOrNullForMemberOfElements(GlobalStringID p_member_id) const
{
	// check that the member is defined before we call our elements
	const std::string &member_name = StringForGlobalStringID(p_member_id);
	std::vector<std::string> constant_members = value_->ReadOnlyMembers();
	
	if (std::find(constant_members.begin(), constant_members.end(), member_name) == constant_members.end())
	{
		std::vector<std::string> variable_members = value_->ReadWriteMembers();
		
		if (std::find(variable_members.begin(), variable_members.end(), member_name) == variable_members.end())
			return nullptr;
	}
	
	// get a value from the first element and return it; we only need to return one representative value
	return value_->GetValueForMember(p_member_id);
}

void ScriptValue_Object_singleton_const::SetValueForMemberOfElements(GlobalStringID p_member_id, ScriptValue *p_value)
{
	if (p_value->Count() == 1)
		value_->SetValueForMember(p_member_id, p_value);
	else
		SLIM_TERMINATION << "ERROR (ScriptValue_Object_singleton_const::SetValueForMemberOfElements): assignment to a member requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << slim_terminate();
}

std::vector<std::string> ScriptValue_Object_singleton_const::MethodsOfElements(void) const
{
	return value_->Methods();
}

const FunctionSignature *ScriptValue_Object_singleton_const::SignatureForMethodOfElements(GlobalStringID p_method_id) const
{
	return value_->SignatureForMethod(p_method_id);
}

ScriptValue *ScriptValue_Object_singleton_const::ExecuteClassMethodOfElements(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	return value_->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}

ScriptValue *ScriptValue_Object_singleton_const::ExecuteInstanceMethodOfElements(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	return value_->ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
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

void ScriptObjectElement::Print(std::ostream &p_ostream) const
{
	p_ostream << *ElementType();
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

bool ScriptObjectElement::MemberIsReadOnly(GlobalStringID p_member_id) const
{
	SLIM_TERMINATION << "ERROR (ScriptObjectElement::MemberIsReadOnly for " << *ElementType() << "): unrecognized member name \"" << StringForGlobalStringID(p_member_id) << "\"." << slim_terminate();
	return true;
}

ScriptValue *ScriptObjectElement::GetValueForMember(GlobalStringID p_member_id)
{
	bool readonly = MemberIsReadOnly(p_member_id);	// will raise if the member does not exist at all
	
	SLIM_TERMINATION << "ERROR (ScriptObjectElement::GetValueForMember for " << *ElementType() << "): internal error: attempt to get a value for " << (readonly ? "read-only member " : "read-write member ") << StringForGlobalStringID(p_member_id) << " was not handled by subclass." << slim_terminate();
	
	return nullptr;
}

void ScriptObjectElement::SetValueForMember(GlobalStringID p_member_id, ScriptValue *p_value)
{
#pragma unused(p_value)
	bool readonly = MemberIsReadOnly(p_member_id);	// will raise if the member does not exist at all
	
	// Check whether setting a constant was attempted; we can do this on behalf of all our subclasses
	if (readonly)
		SLIM_TERMINATION << "ERROR (ScriptObjectElement::SetValueForMember for " << *ElementType() << "): attempt to set a new value for read-only member " << StringForGlobalStringID(p_member_id) << "." << slim_terminate();
	else
		SLIM_TERMINATION << "ERROR (ScriptObjectElement::SetValueForMember for " << *ElementType() << "): internal error: setting a new value for read-write member " << StringForGlobalStringID(p_member_id) << " was not handled by subclass." << slim_terminate();
}

std::vector<std::string> ScriptObjectElement::Methods(void) const
{
	std::vector<std::string> methods;
	
	methods.push_back(gStr_method);
	methods.push_back(gStr_property);
	methods.push_back(gStr_str);
	
	return methods;
}

const FunctionSignature *ScriptObjectElement::SignatureForMethod(GlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static FunctionSignature *strSig = nullptr;
	static FunctionSignature *propertySig = nullptr;
	static FunctionSignature *methodsSig = nullptr;
	
	if (!strSig)
	{
		methodsSig = (new FunctionSignature(gStr_method, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetClassMethod()->AddString_OS();
		propertySig = (new FunctionSignature(gStr_property, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetClassMethod()->AddString_OS();
		strSig = (new FunctionSignature(gStr_str, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_method:
			return methodsSig;
		case gID_property:
			return propertySig;
		case gID_str:
			return strSig;
			
			// all others, including gID_none
		default:
			// Check whether the method signature request failed due to a bad subclass implementation
			std::vector<std::string> methods = Methods();
			const std::string &method_name = StringForGlobalStringID(p_method_id);
			
			if (std::find(methods.begin(), methods.end(), method_name) != methods.end())
				SLIM_TERMINATION << "ERROR (ScriptObjectElement::SignatureForMethod for " << *ElementType() << "): internal error: method signature " << &method_name << " was not provided by subclass." << slim_terminate();
			
			// Otherwise, we have an unrecognized method, so throw
			SLIM_TERMINATION << "ERROR (ScriptObjectElement::SignatureForMethod for " << *ElementType() << "): unrecognized method name " << &method_name << "." << slim_terminate();
			return new FunctionSignature(gStr_empty_string, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL);
	}
}

ScriptValue *ScriptObjectElement::ExecuteMethod(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_interpreter)
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_str:		// instance method
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			
			output_stream << *ElementType() << ":" << endl;
			
			std::vector<std::string> read_only_member_names = ReadOnlyMembers();
			std::vector<std::string> read_write_member_names = ReadWriteMembers();
			std::vector<std::string> member_names;
			
			member_names.insert(member_names.end(), read_only_member_names.begin(), read_only_member_names.end());
			member_names.insert(member_names.end(), read_write_member_names.begin(), read_write_member_names.end());
			std::sort(member_names.begin(), member_names.end());
			
			for (auto member_name_iter = member_names.begin(); member_name_iter != member_names.end(); ++member_name_iter)
			{
				const std::string &member_name = *member_name_iter;
				GlobalStringID member_id = GlobalStringIDForString(member_name);
				ScriptValue *member_value = GetValueForMember(member_id);
				int member_count = member_value->Count();
				bool is_const = std::find(read_only_member_names.begin(), read_only_member_names.end(), member_name) != read_only_member_names.end();
				
				output_stream << "\t";
				
				if (member_count <= 2)
					output_stream << member_name << (is_const ? " => (" : " -> (") << member_value->Type() << ") " << *member_value << endl;
				else
				{
					ScriptValue *first_value = member_value->GetValueAtIndex(0);
					ScriptValue *second_value = member_value->GetValueAtIndex(1);
					
					output_stream << member_name << (is_const ? " => (" : " -> (") << member_value->Type() << ") " << *first_value << " " << *second_value << " ... (" << member_count << " values)" << endl;
					
					if (first_value->IsTemporary()) delete first_value;
					if (second_value->IsTemporary()) delete second_value;
				}
				
				if (member_value->IsTemporary()) delete member_value;
			}
			
			return gStaticScriptValueNULLInvisible;
		}
		case gID_property:		// class method
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_argument_count == 1);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0) : gStr_empty_string);
			std::vector<std::string> read_only_member_names = ReadOnlyMembers();
			std::vector<std::string> read_write_member_names = ReadWriteMembers();
			std::vector<std::string> member_names;
			bool signature_found = false;
			
			member_names.insert(member_names.end(), read_only_member_names.begin(), read_only_member_names.end());
			member_names.insert(member_names.end(), read_write_member_names.begin(), read_write_member_names.end());
			std::sort(member_names.begin(), member_names.end());
			
			for (auto member_name_iter = member_names.begin(); member_name_iter != member_names.end(); ++member_name_iter)
			{
				const std::string &member_name = *member_name_iter;
				GlobalStringID member_id = GlobalStringIDForString(member_name);
				
				if (has_match_string && (member_name.compare(match_string) != 0))
					continue;
				
				ScriptValue *member_value = GetValueForMember(member_id);
				bool is_const = std::find(read_only_member_names.begin(), read_only_member_names.end(), member_name) != read_only_member_names.end();
				
				output_stream << member_name << (is_const ? " => (" : " -> (") << member_value->Type() << ")" << endl;
				
				if (member_value->IsTemporary()) delete member_value;
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No property found for \"" << match_string << "\"." << endl;
			
			return gStaticScriptValueNULLInvisible;
		}
		case gID_method:		// class method
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_argument_count == 1);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0) : gStr_empty_string);
			std::vector<std::string> method_names = Methods();
			bool signature_found = false;
			
			std::sort(method_names.begin(), method_names.end());
			
			for (auto method_name_iter = method_names.begin(); method_name_iter != method_names.end(); ++method_name_iter)
			{
				const std::string &method_name = *method_name_iter;
				GlobalStringID method_id = GlobalStringIDForString(method_name);
				
				if (has_match_string && (method_name.compare(match_string) != 0))
					continue;
				
				const FunctionSignature *method_signature = SignatureForMethod(method_id);
				
				output_stream << *method_signature << endl;
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No method signature found for \"" << match_string << "\"." << endl;
			
			return gStaticScriptValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			std::vector<std::string> methods = Methods();
			const std::string &method_name = StringForGlobalStringID(p_method_id);
			
			if (std::find(methods.begin(), methods.end(), method_name) != methods.end())
				SLIM_TERMINATION << "ERROR (ScriptObjectElement::ExecuteMethod for " << *ElementType() << "): internal error: method " << method_name << " was not handled by subclass." << slim_terminate();
			
			// Otherwise, we have an unrecognized method, so throw
			SLIM_TERMINATION << "ERROR (ScriptObjectElement::ExecuteMethod for " << *ElementType() << "): unrecognized method name " << method_name << "." << slim_terminate();
			
			return gStaticScriptValueNULLInvisible;
		}
	}
}

void ScriptObjectElement::TypeCheckValue(const std::string &p_method_name, GlobalStringID p_member_id, ScriptValue *p_value, ScriptValueMask p_type_mask)
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
		SLIM_TERMINATION << "ERROR (ScriptObjectElement::TypeCheckValue for " << *ElementType() << "::" << p_method_name << "): type " << p_value->Type() << " is not legal for member " << StringForGlobalStringID(p_member_id) << "." << slim_terminate();
}

void ScriptObjectElement::RangeCheckValue(const std::string &p_method_name, GlobalStringID p_member_id, bool p_in_range)
{
	if (!p_in_range)
		SLIM_TERMINATION << "ERROR (ScriptObjectElement::RangeCheckValue for" << *ElementType() << "::" << p_method_name << "): new value for member " << StringForGlobalStringID(p_member_id) << " is illegal." << slim_terminate();
}

std::ostream &operator<<(std::ostream &p_outstream, const ScriptObjectElement &p_element)
{
	p_element.Print(p_outstream);	// get dynamic dispatch
	
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







































































