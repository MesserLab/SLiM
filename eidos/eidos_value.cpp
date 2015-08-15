//
//  eidos_value.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
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

#include "eidos_value.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


//
//	Global static EidosValue objects; these are effectively const, although EidosValues can't be declared as const.
//	Internally, thse are implemented as subclasses that terminate if they are dealloced or modified.
//

EidosValue_NULL *gStaticEidosValueNULL = EidosValue_NULL_const::Static_EidosValue_NULL();
EidosValue_NULL *gStaticEidosValueNULLInvisible = EidosValue_NULL_const::Static_EidosValue_NULL_Invisible();

EidosValue_Logical *gStaticEidosValue_LogicalT = EidosValue_Logical_const::Static_EidosValue_Logical_T();
EidosValue_Logical *gStaticEidosValue_LogicalF = EidosValue_Logical_const::Static_EidosValue_Logical_F();

EidosObjectClass *gEidos_UndefinedClassObject = new EidosObjectClass();


string StringForEidosValueType(const EidosValueType p_type)
{
	switch (p_type)
	{
		case EidosValueType::kValueNULL:		return gEidosStr_NULL;
		case EidosValueType::kValueLogical:		return gEidosStr_logical;
		case EidosValueType::kValueString:		return gEidosStr_string;
		case EidosValueType::kValueInt:			return gEidosStr_integer;
		case EidosValueType::kValueFloat:		return gEidosStr_float;
		case EidosValueType::kValueObject:		return gEidosStr_object;
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosValueType p_type)
{
	p_outstream << StringForEidosValueType(p_type);
	
	return p_outstream;
}

std::string StringForEidosValueMask(const EidosValueMask p_mask, const EidosObjectClass *p_object_class, const std::string &p_name)
{
	//
	//	Note this logic is paralleled in -[EidosConsoleWindowController updateStatusLineWithSignature:].
	//	These two should be kept in synch so the user-visible format of signatures is consistent.
	//
	
	std::string out_string;
	bool is_optional = !!(p_mask & kEidosValueMaskOptional);
	bool requires_singleton = !!(p_mask & kEidosValueMaskSingleton);
	EidosValueMask stripped_mask = p_mask & kEidosValueMaskFlagStrip;
	
	if (is_optional)
		out_string += "[";
	
	if (stripped_mask == kEidosValueMaskNone)			out_string += "?";
	else if (stripped_mask == kEidosValueMaskAny)		out_string += "*";
	else if (stripped_mask == kEidosValueMaskAnyBase)	out_string += "+";
	else if (stripped_mask == kEidosValueMaskNULL)		out_string += gEidosStr_void;
	else if (stripped_mask == kEidosValueMaskLogical)	out_string += gEidosStr_logical;
	else if (stripped_mask == kEidosValueMaskString)	out_string += gEidosStr_string;
	else if (stripped_mask == kEidosValueMaskInt)		out_string += gEidosStr_integer;
	else if (stripped_mask == kEidosValueMaskFloat)		out_string += gEidosStr_float;
	else if (stripped_mask == kEidosValueMaskObject)	out_string += gEidosStr_object;
	else if (stripped_mask == kEidosValueMaskNumeric)	out_string += gEidosStr_numeric;
	else
	{
		if (stripped_mask & kEidosValueMaskNULL)		out_string += "N";
		if (stripped_mask & kEidosValueMaskLogical)		out_string += "l";
		if (stripped_mask & kEidosValueMaskInt)			out_string += "i";
		if (stripped_mask & kEidosValueMaskFloat)		out_string += "f";
		if (stripped_mask & kEidosValueMaskString)		out_string += "s";
		if (stripped_mask & kEidosValueMaskObject)		out_string += "o";
	}
	
	if (p_object_class && (stripped_mask & kEidosValueMaskObject))
	{
		out_string += "<";
		out_string += p_object_class->ElementType();
		out_string += ">";
	}
	
	if (requires_singleton)
		out_string += "$";
	
	if (p_name.length() > 0)
	{
		out_string += " ";
		out_string += p_name;
	}
	
	if (is_optional)
		out_string += "]";
	
	return out_string;
}

// returns -1 if value1[index1] < value2[index2], 0 if ==, 1 if >, with full type promotion
int CompareEidosValues(const EidosValue *p_value1, int p_index1, const EidosValue *p_value2, int p_index2)
{
	EidosValueType type1 = p_value1->Type();
	EidosValueType type2 = p_value2->Type();
	
	if ((type1 == EidosValueType::kValueNULL) || (type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (CompareEidosValues): comparison with NULL is illegal." << eidos_terminate();
	
	// comparing one object to another is legal, but objects cannot be compared to other types
	if ((type1 == EidosValueType::kValueObject) && (type2 == EidosValueType::kValueObject))
	{
		EidosObjectElement *element1 = p_value1->ObjectElementAtIndex(p_index1);
		EidosObjectElement *element2 = p_value2->ObjectElementAtIndex(p_index2);
		
		return (element1 == element2) ? 0 : -1;		// no relative ordering, just equality comparison; enforced in script_interpreter
	}
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((type1 == EidosValueType::kValueString) || (type2 == EidosValueType::kValueString))
	{
		string string1 = p_value1->StringAtIndex(p_index1);
		string string2 = p_value2->StringAtIndex(p_index2);
		int compare_result = string1.compare(string2);		// not guaranteed to be -1 / 0 / +1, just negative / 0 / positive
		
		return (compare_result < 0) ? -1 : ((compare_result > 0) ? 1 : 0);
	}
	
	// float is the next highest type, so we promote to float if either operand is a float
	if ((type1 == EidosValueType::kValueFloat) || (type2 == EidosValueType::kValueFloat))
	{
		double float1 = p_value1->FloatAtIndex(p_index1);
		double float2 = p_value2->FloatAtIndex(p_index2);
		
		return (float1 < float2) ? -1 : ((float1 > float2) ? 1 : 0);
	}
	
	// int is the next highest type, so we promote to int if either operand is a int
	if ((type1 == EidosValueType::kValueInt) || (type2 == EidosValueType::kValueInt))
	{
		int64_t int1 = p_value1->IntAtIndex(p_index1);
		int64_t int2 = p_value2->IntAtIndex(p_index2);
		
		return (int1 < int2) ? -1 : ((int1 > int2) ? 1 : 0);
	}
	
	// logical is the next highest type, so we promote to logical if either operand is a logical
	if ((type1 == EidosValueType::kValueLogical) || (type2 == EidosValueType::kValueLogical))
	{
		bool logical1 = p_value1->LogicalAtIndex(p_index1);
		bool logical2 = p_value2->LogicalAtIndex(p_index2);
		
		return (logical1 < logical2) ? -1 : ((logical1 > logical2) ? 1 : 0);
	}
	
	// that's the end of the road; we should never reach this point
	EIDOS_TERMINATION << "ERROR (CompareEidosValues): comparison involving type " << type1 << " and type " << type2 << " is undefined." << eidos_terminate();
	return 0;
}


//
//	EidosValue
//
#pragma mark -
#pragma mark EidosValue

EidosValue::EidosValue(const EidosValue &p_original) : external_temporary_(false), external_permanent_(false), invisible_(false)	// doesn't use original for these flags
{
#pragma unused(p_original)
}

EidosValue::EidosValue(void)
{
}

EidosValue::~EidosValue(void)
{
}

bool EidosValue::LogicalAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::LogicalAtIndex): operand type " << this->Type() << " cannot be converted to type logical." << eidos_terminate();
	return false;
}

std::string EidosValue::StringAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::StringAtIndex): operand type " << this->Type() << " cannot be converted to type string." << eidos_terminate();
	return std::string();
}

int64_t EidosValue::IntAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::IntAtIndex): operand type " << this->Type() << " cannot be converted to type integer." << eidos_terminate();
	return 0;
}

double EidosValue::FloatAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::FloatAtIndex): operand type " << this->Type() << " cannot be converted to type float." << eidos_terminate();
	return 0.0;
}

EidosObjectElement *EidosValue::ObjectElementAtIndex(int p_idx) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::ObjectElementAtIndex): operand type " << this->Type() << " cannot be converted to type object." << eidos_terminate();
	return nullptr;
}

bool EidosValue::IsMutable(void) const
{
	return true;
}

EidosValue *EidosValue::MutableCopy(void) const
{
	return CopyValues();
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosValue &p_value)
{
	p_value.Print(p_outstream);		// get dynamic dispatch
	
	return p_outstream;
}


//
//	EidosValue_NULL
//
#pragma mark -
#pragma mark EidosValue_NULL

EidosValue_NULL::EidosValue_NULL(void)
{
}

EidosValue_NULL::~EidosValue_NULL(void)
{
}

EidosValueType EidosValue_NULL::Type(void) const
{
	return EidosValueType::kValueNULL;
}

const std::string &EidosValue_NULL::ElementType(void) const
{
	return gEidosStr_NULL;
}

int EidosValue_NULL::Count(void) const
{
	return 0;
}

void EidosValue_NULL::Print(std::ostream &p_ostream) const
{
	p_ostream << gEidosStr_NULL;
}

EidosValue *EidosValue_NULL::GetValueAtIndex(const int p_idx) const
{
#pragma unused(p_idx)
	return new EidosValue_NULL;
}

void EidosValue_NULL::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_NULL::SetValueAtIndex): operand type " << this->Type() << " does not support setting values with the subscript operator ('[]')." << eidos_terminate();
}

EidosValue *EidosValue_NULL::CopyValues(void) const
{
	return new EidosValue_NULL(*this);
}

EidosValue *EidosValue_NULL::NewMatchingType(void) const
{
	return new EidosValue_NULL;
}

void EidosValue_NULL::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx)
	if (p_source_script_value->Type() == EidosValueType::kValueNULL)
		;	// NULL doesn't have values or indices, so this is a no-op
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_NULL::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_NULL::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	// nothing to do
}


EidosValue_NULL_const::~EidosValue_NULL_const(void)
{
	EIDOS_TERMINATION << "ERROR (EidosValue_NULL_const::~EidosValue_NULL_const): internal error: global constant deallocated." << eidos_terminate();
}

/* static */ EidosValue_NULL *EidosValue_NULL_const::Static_EidosValue_NULL(void)
{
	static EidosValue_NULL_const *static_null = nullptr;
	
	if (!static_null)
	{
		// this is a truly permanent constant object
		static_null = new EidosValue_NULL_const();
		static_null->SetExternalPermanent();
	}
	
	return static_null;
}

/* static */ EidosValue_NULL *EidosValue_NULL_const::Static_EidosValue_NULL_Invisible(void)
{
	static EidosValue_NULL_const *static_null = nullptr;
	
	if (!static_null)
	{
		// this is a truly permanent constant object
		static_null = new EidosValue_NULL_const();
		static_null->invisible_ = true;
		static_null->SetExternalPermanent();
	}
	
	return static_null;
}



//
//	EidosValue_Logical
//
#pragma mark -
#pragma mark EidosValue_Logical

EidosValue_Logical::EidosValue_Logical(void)
{
}

EidosValue_Logical::EidosValue_Logical(std::vector<bool> &p_boolvec)
{
	values_ = p_boolvec;
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1)
{
	values_.push_back(p_bool1);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
	values_.push_back(p_bool5);
}

EidosValue_Logical::EidosValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5, bool p_bool6)
{
	values_.push_back(p_bool1);
	values_.push_back(p_bool2);
	values_.push_back(p_bool3);
	values_.push_back(p_bool4);
	values_.push_back(p_bool5);
	values_.push_back(p_bool6);
}

EidosValue_Logical::~EidosValue_Logical(void)
{
}

EidosValueType EidosValue_Logical::Type(void) const
{
	return EidosValueType::kValueLogical;
}

const std::string &EidosValue_Logical::ElementType(void) const
{
	return gEidosStr_logical;
}

int EidosValue_Logical::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Logical::Print(std::ostream &p_ostream) const
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
			
			p_ostream << (value ? gEidosStr_T : gEidosStr_F);
		}
	}
}

const std::vector<bool> &EidosValue_Logical::LogicalVector(void) const
{
	return values_;
}

bool EidosValue_Logical::LogicalAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

std::string EidosValue_Logical::StringAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? gEidosStr_T : gEidosStr_F);
}

int64_t EidosValue_Logical::IntAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? 1 : 0);
}

double EidosValue_Logical::FloatAtIndex(int p_idx) const
{
	return (values_.at(p_idx) ? 1.0 : 0.0);
}

void EidosValue_Logical::PushLogical(bool p_logical)
{
	values_.push_back(p_logical);
}

void EidosValue_Logical::SetLogicalAtIndex(const int p_idx, bool p_logical)
{
	values_.at(p_idx) = p_logical;
}

EidosValue *EidosValue_Logical::GetValueAtIndex(const int p_idx) const
{
	return (values_.at(p_idx) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

void EidosValue_Logical::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->LogicalAtIndex(0);
}

EidosValue *EidosValue_Logical::CopyValues(void) const
{
	return new EidosValue_Logical(*this);
}

EidosValue *EidosValue_Logical::NewMatchingType(void) const
{
	return new EidosValue_Logical;
}

void EidosValue_Logical::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueLogical)
		values_.push_back(p_source_script_value->LogicalAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_Logical::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<bool>());
}


EidosValue_Logical_const::EidosValue_Logical_const(bool p_bool1) : EidosValue_Logical(p_bool1)
{
}

EidosValue_Logical_const::~EidosValue_Logical_const(void)
{
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::~EidosValue_Logical_const): internal error: global constant deallocated." << eidos_terminate();
}

/* static */ EidosValue_Logical *EidosValue_Logical_const::Static_EidosValue_Logical_T(void)
{
	static EidosValue_Logical_const *static_T = nullptr;
	
	if (!static_T)
	{
		// this is a truly permanent constant object
		static_T = new EidosValue_Logical_const(true);
		static_T->SetExternalPermanent();
	}
	
	return static_T;
}

/* static */ EidosValue_Logical *EidosValue_Logical_const::Static_EidosValue_Logical_F(void)
{
	static EidosValue_Logical_const *static_F = nullptr;
	
	if (!static_F)
	{
		// this is a truly permanent constant object
		static_F = new EidosValue_Logical_const(false);
		static_F->SetExternalPermanent();
	}
	
	return static_F;
}

bool EidosValue_Logical_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Logical_const::MutableCopy(void) const
{
	return new EidosValue_Logical(*this);	// same as EidosValue_Logical::, but let's not rely on that
}

void EidosValue_Logical_const::PushLogical(bool p_logical)
{
#pragma unused(p_logical)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::PushLogical): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::SetLogicalAtIndex(const int p_idx, bool p_logical)
{
#pragma unused(p_idx, p_logical)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::SetLogicalAtIndex): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::SetValueAtIndex): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}

void EidosValue_Logical_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::Sort): internal error: EidosValue_Logical_const is not modifiable." << eidos_terminate();
}


//
//	EidosValue_String
//
#pragma mark -
#pragma mark EidosValue_String

EidosValue_String::EidosValue_String(void)
{
}

EidosValue_String::EidosValue_String(std::vector<std::string> &p_stringvec)
{
	values_ = p_stringvec;
}

EidosValue_String::EidosValue_String(const std::string &p_string1)
{
	values_.push_back(p_string1);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
	values_.push_back(p_string5);
}

EidosValue_String::EidosValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5, const std::string &p_string6)
{
	values_.push_back(p_string1);
	values_.push_back(p_string2);
	values_.push_back(p_string3);
	values_.push_back(p_string4);
	values_.push_back(p_string5);
	values_.push_back(p_string6);
}

EidosValue_String::~EidosValue_String(void)
{
}

EidosValueType EidosValue_String::Type(void) const
{
	return EidosValueType::kValueString;
}

const std::string &EidosValue_String::ElementType(void) const
{
	return gEidosStr_string;
}

int EidosValue_String::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_String::Print(std::ostream &p_ostream) const
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

const std::vector<std::string> &EidosValue_String::StringVector(void) const
{
	return values_;
}

bool EidosValue_String::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx).length() > 0);
}

std::string EidosValue_String::StringAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

int64_t EidosValue_String::IntAtIndex(int p_idx) const
{
	const string &index_str = values_.at(p_idx);
	
	return strtoq(index_str.c_str(), nullptr, 10);
}

double EidosValue_String::FloatAtIndex(int p_idx) const
{
	const string &index_str = values_.at(p_idx);
	
	return strtod(index_str.c_str(), nullptr);
}

void EidosValue_String::PushString(const std::string &p_string)
{
	values_.push_back(p_string);
}

EidosValue *EidosValue_String::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_String(values_.at(p_idx));
}

void EidosValue_String::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->StringAtIndex(0);
}

EidosValue *EidosValue_String::CopyValues(void) const
{
	return new EidosValue_String(*this);
}

EidosValue *EidosValue_String::NewMatchingType(void) const
{
	return new EidosValue_String;
}

void EidosValue_String::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueString)
		values_.push_back(p_source_script_value->StringAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_String::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_String::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<std::string>());
}


//
//	EidosValue_Int
//
#pragma mark -
#pragma mark EidosValue_Int

EidosValue_Int::~EidosValue_Int(void)
{
}

EidosValueType EidosValue_Int::Type(void) const
{
	return EidosValueType::kValueInt;
}

const std::string &EidosValue_Int::ElementType(void) const
{
	return gEidosStr_integer;
}

EidosValue *EidosValue_Int::NewMatchingType(void) const
{
	return new EidosValue_Int_vector;
}


// EidosValue_Int_vector

EidosValue_Int_vector::EidosValue_Int_vector(void)
{
}

EidosValue_Int_vector::EidosValue_Int_vector(std::vector<int16_t> &p_intvec)
{
	for (auto intval : p_intvec)
		values_.push_back(intval);
}

EidosValue_Int_vector::EidosValue_Int_vector(std::vector<int32_t> &p_intvec)
{
	for (auto intval : p_intvec)
		values_.push_back(intval);
}

EidosValue_Int_vector::EidosValue_Int_vector(std::vector<int64_t> &p_intvec)
{
	values_ = p_intvec;
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
}

EidosValue_Int_vector::EidosValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5, int64_t p_int6)
{
	values_.push_back(p_int1);
	values_.push_back(p_int2);
	values_.push_back(p_int3);
	values_.push_back(p_int4);
	values_.push_back(p_int5);
	values_.push_back(p_int6);
}

EidosValue_Int_vector::~EidosValue_Int_vector(void)
{
}

int EidosValue_Int_vector::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Int_vector::Print(std::ostream &p_ostream) const
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

const std::vector<int64_t> &EidosValue_Int_vector::IntVector(void) const
{
	return values_;
}

bool EidosValue_Int_vector::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string EidosValue_Int_vector::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t EidosValue_Int_vector::IntAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

double EidosValue_Int_vector::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void EidosValue_Int_vector::PushInt(int64_t p_int)
{
	values_.push_back(p_int);
}

EidosValue *EidosValue_Int_vector::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_Int_singleton_const(values_.at(p_idx));
}

void EidosValue_Int_vector::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->IntAtIndex(0);
}

EidosValue *EidosValue_Int_vector::CopyValues(void) const
{
	return new EidosValue_Int_vector(*this);
}

void EidosValue_Int_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueInt)
		values_.push_back(p_source_script_value->IntAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_Int_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<int64_t>());
}



// EidosValue_Int_singleton_const

EidosValue_Int_singleton_const::EidosValue_Int_singleton_const(int64_t p_int1) : value_(p_int1)
{
}

EidosValue_Int_singleton_const::~EidosValue_Int_singleton_const(void)
{
}

int EidosValue_Int_singleton_const::Count(void) const
{
	return 1;
}

void EidosValue_Int_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << value_;
}

bool EidosValue_Int_singleton_const::LogicalAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Int_singleton_const::StringAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::StringAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Int_singleton_const::IntAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::IntAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

double EidosValue_Int_singleton_const::FloatAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::FloatAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

EidosValue *EidosValue_Int_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::GetValueAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return new EidosValue_Int_singleton_const(value_);
}

EidosValue *EidosValue_Int_singleton_const::CopyValues(void) const
{
	return new EidosValue_Int_singleton_const(value_);
}

bool EidosValue_Int_singleton_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Int_singleton_const::MutableCopy(void) const
{
	EidosValue_Int_vector *new_vec = new EidosValue_Int_vector();
	
	new_vec->PushInt(value_);
	
	return new_vec;
}

void EidosValue_Int_singleton_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::SetValueAtIndex): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Int_singleton_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Int_singleton_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton_const::Sort): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}


//
//	EidosValue_Float
//
#pragma mark -
#pragma mark EidosValue_Float

EidosValue_Float::~EidosValue_Float(void)
{
}

EidosValueType EidosValue_Float::Type(void) const
{
	return EidosValueType::kValueFloat;
}

const std::string &EidosValue_Float::ElementType(void) const
{
	return gEidosStr_float;
}

EidosValue *EidosValue_Float::NewMatchingType(void) const
{
	return new EidosValue_Float_vector;
}


// EidosValue_Float_vector

EidosValue_Float_vector::EidosValue_Float_vector(void)
{
}

EidosValue_Float_vector::EidosValue_Float_vector(std::vector<double> &p_doublevec)
{
	values_ = p_doublevec;
}

EidosValue_Float_vector::EidosValue_Float_vector(double *p_doublebuf, int p_buffer_length)
{
	for (int index = 0; index < p_buffer_length; index++)
		values_.push_back(p_doublebuf[index]);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
}

EidosValue_Float_vector::EidosValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5, double p_float6)
{
	values_.push_back(p_float1);
	values_.push_back(p_float2);
	values_.push_back(p_float3);
	values_.push_back(p_float4);
	values_.push_back(p_float5);
	values_.push_back(p_float6);
}

EidosValue_Float_vector::~EidosValue_Float_vector(void)
{
}

int EidosValue_Float_vector::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Float_vector::Print(std::ostream &p_ostream) const
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

const std::vector<double> &EidosValue_Float_vector::FloatVector(void) const
{
	return values_;
}

bool EidosValue_Float_vector::LogicalAtIndex(int p_idx) const
{
	return (values_.at(p_idx) == 0 ? false : true);
}

std::string EidosValue_Float_vector::StringAtIndex(int p_idx) const
{
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << values_.at(p_idx);
	
	return ss.str();
}

int64_t EidosValue_Float_vector::IntAtIndex(int p_idx) const
{
	return static_cast<int64_t>(values_.at(p_idx));
}

double EidosValue_Float_vector::FloatAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void EidosValue_Float_vector::PushFloat(double p_float)
{
	values_.push_back(p_float);
}

EidosValue *EidosValue_Float_vector::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_Float_singleton_const(values_.at(p_idx));
}

void EidosValue_Float_vector::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	values_.at(p_idx) = p_value->FloatAtIndex(0);
}

EidosValue *EidosValue_Float_vector::CopyValues(void) const
{
	return new EidosValue_Float_vector(*this);
}

void EidosValue_Float_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueFloat)
		values_.push_back(p_source_script_value->FloatAtIndex(p_idx));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

void EidosValue_Float_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<double>());
}


// EidosValue_Float_singleton_const

EidosValue_Float_singleton_const::EidosValue_Float_singleton_const(double p_float1) : value_(p_float1)
{
}

EidosValue_Float_singleton_const::~EidosValue_Float_singleton_const(void)
{
}

int EidosValue_Float_singleton_const::Count(void) const
{
	return 1;
}

void EidosValue_Float_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << value_;
}

bool EidosValue_Float_singleton_const::LogicalAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::LogicalAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Float_singleton_const::StringAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::StringAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	// with C++11, could use std::to_string(values_.at(p_idx))
	ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Float_singleton_const::IntAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::IntAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return static_cast<int64_t>(value_);
}

double EidosValue_Float_singleton_const::FloatAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::FloatAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

EidosValue *EidosValue_Float_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::GetValueAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return new EidosValue_Float_singleton_const(value_);
}

EidosValue *EidosValue_Float_singleton_const::CopyValues(void) const
{
	return new EidosValue_Float_singleton_const(value_);
}

bool EidosValue_Float_singleton_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Float_singleton_const::MutableCopy(void) const
{
	EidosValue_Float_vector *new_vec = new EidosValue_Float_vector();
	
	new_vec->PushFloat(value_);
	
	return new_vec;
}

void EidosValue_Float_singleton_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::SetValueAtIndex): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Float_singleton_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Float_singleton_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton_const::Sort): internal error: EidosValue_Float_singleton_const is not modifiable." << eidos_terminate();
}


//
//	EidosValue_Object
//
#pragma mark -
#pragma mark EidosValue_Object

EidosValue_Object::~EidosValue_Object(void)
{
}

EidosValueType EidosValue_Object::Type(void) const
{
	return EidosValueType::kValueObject;
}

const std::string &EidosValue_Object::ElementType(void) const
{
	return Class()->ElementType();
}

EidosValue *EidosValue_Object::NewMatchingType(void) const
{
	return new EidosValue_Object_vector;
}

void EidosValue_Object::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object::Sort): Sort() is not defined for type object." << eidos_terminate();
}


// EidosValue_Object_vector

EidosValue_Object_vector::EidosValue_Object_vector(const EidosValue_Object_vector &p_original)
{
	for (auto value : p_original.values_)
		values_.push_back(value->Retain());
}

EidosValue_Object_vector::EidosValue_Object_vector(void)
{
}

EidosValue_Object_vector::EidosValue_Object_vector(std::vector<EidosObjectElement *> &p_elementvec)
{
	values_ = p_elementvec;		// FIXME should this retain?
}

EidosValue_Object_vector::~EidosValue_Object_vector(void)
{
	if (values_.size() != 0)
	{
		for (auto value : values_)
			value->Release();
	}
}

const EidosObjectClass *EidosValue_Object_vector::Class(void) const
{
	if (values_.size() == 0)
		return gEidos_UndefinedClassObject;		// this is relied upon by the type-check machinery
	else
		return values_[0]->Class();
}

int EidosValue_Object_vector::Count(void) const
{
	return (int)values_.size();
}

void EidosValue_Object_vector::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "object(0)";
	}
	else
	{
		bool first = true;
		
		for (EidosObjectElement *value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << *value;
		}
	}
}

EidosObjectElement *EidosValue_Object_vector::ObjectElementAtIndex(int p_idx) const
{
	return values_.at(p_idx);
}

void EidosValue_Object_vector::PushElement(EidosObjectElement *p_element)
{
	if ((values_.size() > 0) && (Class() != p_element->Class()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushElement): the type of an object cannot be changed." << eidos_terminate();
	else
		values_.push_back(p_element->Retain());
}

EidosValue *EidosValue_Object_vector::GetValueAtIndex(const int p_idx) const
{
	return new EidosValue_Object_singleton_const(values_.at(p_idx));
}

void EidosValue_Object_vector::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
	if ((p_idx < 0) || (p_idx >= values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate();
	
	// can't change the type of element object we collect
	if ((values_.size() > 0) && (Class() != p_value->ObjectElementAtIndex(0)->Class()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueAtIndex): the type of an object cannot be changed." << eidos_terminate();
	
	values_.at(p_idx)->Release();
	values_.at(p_idx) = p_value->ObjectElementAtIndex(0)->Retain();
}

EidosValue *EidosValue_Object_vector::CopyValues(void) const
{
	return new EidosValue_Object_vector(*this);
}

void EidosValue_Object_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
	if (p_source_script_value->Type() == EidosValueType::kValueObject)
	{
		if ((values_.size() > 0) && (Class() != p_source_script_value->ObjectElementAtIndex(p_idx)->Class()))
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushValueFromIndexOfEidosValue): the type of an object cannot be changed." << eidos_terminate();
		else
			values_.push_back(p_source_script_value->ObjectElementAtIndex(p_idx)->Retain());
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate();
}

bool CompareLogicalObjectSortPairsAscending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j);
bool CompareLogicalObjectSortPairsAscending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j)					{ return (i.first < j.first); }
bool CompareLogicalObjectSortPairsDescending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j);
bool CompareLogicalObjectSortPairsDescending(std::pair<bool, EidosObjectElement*> i, std::pair<bool, EidosObjectElement*> j)					{ return (i.first > j.first); }

bool CompareIntObjectSortPairsAscending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j);
bool CompareIntObjectSortPairsAscending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j)					{ return (i.first < j.first); }
bool CompareIntObjectSortPairsDescending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j);
bool CompareIntObjectSortPairsDescending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j)				{ return (i.first > j.first); }

bool CompareFloatObjectSortPairsAscending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j);
bool CompareFloatObjectSortPairsAscending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j)					{ return (i.first < j.first); }
bool CompareFloatObjectSortPairsDescending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j);
bool CompareFloatObjectSortPairsDescending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j)				{ return (i.first > j.first); }

bool CompareStringObjectSortPairsAscending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j);
bool CompareStringObjectSortPairsAscending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j)		{ return (i.first < j.first); }
bool CompareStringObjectSortPairsDescending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j);
bool CompareStringObjectSortPairsDescending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j)		{ return (i.first > j.first); }

void EidosValue_Object_vector::SortBy(const std::string &p_property, bool p_ascending)
{
	// length 0 is already sorted
	if (values_.size() == 0)
		return;
	
	// figure out what type the property returns
	EidosGlobalStringID property_string_id = EidosGlobalStringIDForString(p_property);
	EidosValue *first_result = values_[0]->GetProperty(property_string_id);
	EidosValueType property_type = first_result->Type();
	
	if (first_result->IsTemporary()) delete first_result;
	
	// switch on the property type for efficiency
	switch (property_type)
	{
		case EidosValueType::kValueNULL:
		case EidosValueType::kValueObject:
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " returned " << property_type << "; a property that evaluates to logical, int, float, or string is required." << eidos_terminate();
			break;
			
		case EidosValueType::kValueLogical:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<bool, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<bool, EidosObjectElement*>(temp_result->LogicalAtIndex(0), value));
				
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
			
		case EidosValueType::kValueInt:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<int64_t, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<int64_t, EidosObjectElement*>(temp_result->IntAtIndex(0), value));
				
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
			
		case EidosValueType::kValueFloat:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<double, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<double, EidosObjectElement*>(temp_result->FloatAtIndex(0), value));
				
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
			
		case EidosValueType::kValueString:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<std::string, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate();
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate();
				
				sortable_pairs.push_back(std::pair<std::string, EidosObjectElement*>(temp_result->StringAtIndex(0), value));
				
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

EidosValue *EidosValue_Object_vector::GetPropertyOfElements(EidosGlobalStringID p_property_id) const
{
	auto values_size = values_.size();
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate();
	
	if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		EidosObjectElement *value = values_[0];
		EidosValue *result = value->GetProperty(p_property_id);
		
		signature->CheckResultValue(result);
		return result;
	}
	else
	{
		// get the value from all properties and collect the results
		vector<EidosValue*> results;
		
		if (values_size < 10)
		{
			// with small objects, we check every value
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetProperty(p_property_id);
				
				signature->CheckResultValue(temp_result);
				results.push_back(temp_result);
			}
		}
		else
		{
			// with large objects, we just spot-check the first value, for speed
			bool checked_multivalued = false;
			
			for (auto value : values_)
			{
				EidosValue *temp_result = value->GetProperty(p_property_id);
				
				if (!checked_multivalued)
				{
					signature->CheckResultValue(temp_result);
					checked_multivalued = true;
				}
				
				results.push_back(temp_result);
			}
		}
		
		// concatenate the results using ConcatenateEidosValues(); we pass our own name as p_function_name, which just makes errors be in our name
		EidosValue *result = ConcatenateEidosValues(gEidosStr_GetPropertyOfElements, results.data(), (int)results.size());
		
		// Now we just need to dispose of our temporary EidosValues
		for (EidosValue *temp_value : results)
			if (temp_value->IsTemporary()) delete temp_value;
		
		return result;
	}
}

void EidosValue_Object_vector::SetPropertyOfElements(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate();
	
	signature->CheckAssignedValue(p_value);
	
	// We have to check the count ourselves; the signature does not do that for us
	int p_value_count = p_value->Count();
	
	if (p_value_count == 1)
	{
		// we have a multiplex assignment of one value to (maybe) more than one element: x.foo = 10
		for (auto value : values_)
			value->SetProperty(p_property_id, p_value);
	}
	else if (p_value_count == Count())
	{
		// we have a one-to-one assignment of values to elements: x.foo = 1:5 (where x has 5 elements)
		for (int value_idx = 0; value_idx < p_value_count; value_idx++)
		{
			EidosValue *temp_rvalue = p_value->GetValueAtIndex(value_idx);
			
			values_[value_idx]->SetProperty(p_property_id, temp_rvalue);
			
			if (temp_rvalue->IsTemporary()) delete temp_rvalue;
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << eidos_terminate();
}

EidosValue *EidosValue_Object_vector::ExecuteInstanceMethodOfElements(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	auto values_size = values_.size();
	
	if (values_size == 0)
	{
		// We special-case str() here as a bit of a hack.  It is defined on the base object element type, so it is always available.
		// Calling it should thus not result in an error, even though we don't know the class of the object; it should just do nothing.
		// This situation cannot arise for any other method, since str() is the only instance method supported by the base class.
		if (p_method_id != gEidosID_str)
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ExecuteInstanceMethodOfElements): method " << StringForEidosGlobalStringID(p_method_id) << " is not recognized because the object vector is empty." << eidos_terminate();
		
		return gStaticEidosValueNULLInvisible;
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		EidosObjectElement *value = values_[0];
		EidosValue *result = value->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
		
		return result;
	}
	else
	{
		// call the method on all elements and collect the results
		vector<EidosValue*> results;
		
		for (auto value : values_)
			results.push_back(value->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter));
		
		// concatenate the results using ConcatenateEidosValues(); we pass our own name as p_function_name, which just makes errors be in our name
		EidosValue *result = ConcatenateEidosValues(gEidosStr_ExecuteInstanceMethod, results.data(), (int)results.size());
		
		// Now we just need to dispose of our temporary EidosValues
		for (EidosValue *temp_value : results)
			if (temp_value->IsTemporary()) delete temp_value;
		
		return result;
	}
}


// EidosValue_Object_singleton_const

EidosValue_Object_singleton_const::EidosValue_Object_singleton_const(EidosObjectElement *p_element1) : value_(p_element1)
{
	p_element1->Retain();
}

EidosValue_Object_singleton_const::~EidosValue_Object_singleton_const(void)
{
	value_->Release();
}

const EidosObjectClass *EidosValue_Object_singleton_const::Class(void) const
{
	return value_->Class();
}

int EidosValue_Object_singleton_const::Count(void) const
{
	return 1;
}

void EidosValue_Object_singleton_const::Print(std::ostream &p_ostream) const
{
	p_ostream << *value_;
}

EidosObjectElement *EidosValue_Object_singleton_const::ObjectElementAtIndex(int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::ObjectElementAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return value_;
}

EidosValue *EidosValue_Object_singleton_const::GetValueAtIndex(const int p_idx) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::GetValueAtIndex): internal error: non-zero index accessed." << eidos_terminate();
	
	return new EidosValue_Object_singleton_const(value_);
}

EidosValue *EidosValue_Object_singleton_const::CopyValues(void) const
{
	return new EidosValue_Object_singleton_const(value_);
}

bool EidosValue_Object_singleton_const::IsMutable(void) const
{
	return false;
}

EidosValue *EidosValue_Object_singleton_const::MutableCopy(void) const
{
	EidosValue_Object_vector *new_vec = new EidosValue_Object_vector();
	
	new_vec->PushElement(value_);
	
	return new_vec;
}

void EidosValue_Object_singleton_const::SetValueAtIndex(const int p_idx, EidosValue *p_value)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::SetValueAtIndex): internal error: EidosValue_Object_singleton_const is not modifiable." << eidos_terminate();
}

void EidosValue_Object_singleton_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue *p_source_script_value)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::PushValueFromIndexOfEidosValue): internal error: EidosValue_Object_singleton_const is not modifiable." << eidos_terminate();
}

EidosValue *EidosValue_Object_singleton_const::GetPropertyOfElements(EidosGlobalStringID p_property_id) const
{
	const EidosPropertySignature *signature = value_->Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::GetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate();
	
	EidosValue *result = value_->GetProperty(p_property_id);
	
	signature->CheckResultValue(result);
	
	return result;
}

void EidosValue_Object_singleton_const::SetPropertyOfElements(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	const EidosPropertySignature *signature = value_->Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::SetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate();
	
	signature->CheckAssignedValue(p_value);
	
	// We have to check the count ourselves; the signature does not do that for us
	if (p_value->Count() == 1)
	{
		value_->SetProperty(p_property_id, p_value);
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton_const::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << eidos_terminate();
}

EidosValue *EidosValue_Object_singleton_const::ExecuteInstanceMethodOfElements(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	return value_->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	EidosObjectElement
//
#pragma mark -
#pragma mark EidosObjectElement

EidosObjectElement::EidosObjectElement(void)
{
}

EidosObjectElement::~EidosObjectElement(void)
{
}

void EidosObjectElement::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType();
}

EidosObjectElement *EidosObjectElement::Retain(void)
{
	// no-op; our lifetime is controlled externally
	return this;
}

EidosObjectElement *EidosObjectElement::Release(void)
{
	// no-op; our lifetime is controlled externally
	return this;
}

EidosValue *EidosObjectElement::GetProperty(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty for " << Class()->ElementType() << "): internal error: attempt to get a value for property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate();
	
	return nullptr;
}

void EidosObjectElement::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
#pragma unused(p_value)
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << Class()->ElementType() << "." << eidos_terminate();
	
	bool readonly = signature->read_only_;
	
	// Check whether setting a constant was attempted; we can do this on behalf of all our subclasses
	if (readonly)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty for " << Class()->ElementType() << "): attempt to set a new value for read-only property " << StringForEidosGlobalStringID(p_property_id) << "." << eidos_terminate();
	else
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty for " << Class()->ElementType() << "): internal error: setting a new value for read-write property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate();
}

EidosValue *EidosObjectElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_argument_count, p_interpreter)
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID_str:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			
			output_stream << Class()->ElementType() << ":" << endl;
			
			const std::vector<const EidosPropertySignature *> *properties = Class()->Properties();
			
			for (auto property_sig : *properties)
			{
				const std::string &property_name = property_sig->property_name_;
				EidosGlobalStringID property_id = property_sig->property_id_;
				EidosValue *property_value = GetProperty(property_id);
				int property_count = property_value->Count();
				EidosValueType property_type = property_value->Type();
				
				output_stream << "\t" << property_name << " " << property_sig->PropertySymbol() << " (" << property_type;
				
				if (property_type == EidosValueType::kValueObject)
					output_stream << "<" << property_value->ElementType() << ">) ";
				else
					output_stream << ") ";
				
				if (property_count <= 2)
					output_stream << *property_value << endl;
				else
				{
					EidosValue *first_value = property_value->GetValueAtIndex(0);
					EidosValue *second_value = property_value->GetValueAtIndex(1);
					
					output_stream << *first_value << " " << *second_value << " ... (" << property_count << " values)" << endl;
					
					if (first_value->IsTemporary()) delete first_value;
					if (second_value->IsTemporary()) delete second_value;
				}
				
				if (property_value->IsTemporary()) delete property_value;
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<const EidosMethodSignature *> *methods = Class()->Methods();
			const std::string &method_name = StringForEidosGlobalStringID(p_method_id);
			
			for (auto method_sig : *methods)
				if (method_sig->function_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): internal error: method " << method_name << " was not handled by subclass." << eidos_terminate();
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): unrecognized method name " << method_name << "." << eidos_terminate();
			
			return gStaticEidosValueNULLInvisible;
		}
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosObjectElement &p_element)
{
	p_element.Print(p_outstream);	// get dynamic dispatch
	
	return p_outstream;
}


//
//	EidosObjectElementInternal
//
#pragma mark -
#pragma mark EidosObjectElementInternal

EidosObjectElementInternal::EidosObjectElementInternal(void)
{
//	std::cerr << "EidosObjectElementInternal::EidosObjectElementInternal allocated " << this << " with refcount == 1" << endl;
//	eidos_print_stacktrace(stderr, 10);
}

EidosObjectElementInternal::~EidosObjectElementInternal(void)
{
//	std::cerr << "EidosObjectElementInternal::~EidosObjectElementInternal deallocated " << this << endl;
//	eidos_print_stacktrace(stderr, 10);
}

EidosObjectElement *EidosObjectElementInternal::Retain(void)
{
	refcount_++;
	
//	std::cerr << "EidosObjectElementInternal::Retain for " << this << ", new refcount == " << refcount_ << endl;
//	eidos_print_stacktrace(stderr, 10);
	
	return this;
}

EidosObjectElement *EidosObjectElementInternal::Release(void)
{
	refcount_--;
	
//	std::cerr << "EidosObjectElementInternal::Release for " << this << ", new refcount == " << refcount_ << endl;
//	eidos_print_stacktrace(stderr, 10);
	
	if (refcount_ == 0)
	{
		delete this;
		return nullptr;
	}
	
	return this;
}


//
//	EidosObjectClass
//
#pragma mark -
#pragma mark EidosObjectClass

EidosObjectClass::EidosObjectClass(void)
{
}

EidosObjectClass::~EidosObjectClass(void)
{
}

const std::string &EidosObjectClass::ElementType(void) const
{
	return gEidosStr_undefined;
}

const std::vector<const EidosPropertySignature *> *EidosObjectClass::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *empty_sigs = nullptr;
	
	if (!empty_sigs)
	{
		empty_sigs = new std::vector<const EidosPropertySignature *>;
		
		// keep alphabetical order here
	}
	
	return empty_sigs;
}

const EidosPropertySignature *EidosObjectClass::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
#pragma unused(p_property_id)
	return nullptr;
}

const EidosPropertySignature *EidosObjectClass::SignatureForPropertyOrRaise(EidosGlobalStringID p_property_id) const
{
	const EidosPropertySignature *signature = SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObjectClass::SignatureForPropertyOrRaise for " << ElementType() << "): internal error: missing property " << StringForEidosGlobalStringID(p_property_id) << "." << eidos_terminate();
	
	return signature;
}

const std::vector<const EidosMethodSignature *> *EidosObjectClass::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>;
		
		// keep alphabetical order here
		methods->push_back(SignatureForMethodOrRaise(gEidosID_method));
		methods->push_back(SignatureForMethodOrRaise(gEidosID_property));
		methods->push_back(SignatureForMethodOrRaise(gEidosID_str));
	}
	
	return methods;
}

const EidosMethodSignature *EidosObjectClass::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *strSig = nullptr;
	static EidosClassMethodSignature *propertySig = nullptr;
	static EidosClassMethodSignature *methodSig = nullptr;
	
	if (!strSig)
	{
		methodSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_method, kEidosValueMaskNULL))->AddString_OS("methodName");
		propertySig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_property, kEidosValueMaskNULL))->AddString_OS("propertyName");
		strSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_str, kEidosValueMaskNULL));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID_method:	return methodSig;
		case gEidosID_property:	return propertySig;
		case gEidosID_str:		return strSig;
			
			// all others, including gID_none
		default:
			return nullptr;
	}
}

const EidosMethodSignature *EidosObjectClass::SignatureForMethodOrRaise(EidosGlobalStringID p_method_id) const
{
	const EidosMethodSignature *signature = SignatureForMethod(p_method_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObjectClass::SignatureForMethodOrRaise for " << ElementType() << "): internal error: missing method " << StringForEidosGlobalStringID(p_method_id) << "." << eidos_terminate();
	
	return signature;
}

EidosValue *EidosObjectClass::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused(p_arguments, p_interpreter)
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID_property:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_argument_count == 1);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0) : gEidosStr_empty_string);
			const std::vector<const EidosPropertySignature *> *properties = Properties();
			bool signature_found = false;
			
			for (auto property_sig : *properties)
			{
				const std::string &property_name = property_sig->property_name_;
				
				if (has_match_string && (property_name.compare(match_string) != 0))
					continue;
				
				output_stream << property_name << " " << property_sig->PropertySymbol() << " (" << StringForEidosValueMask(property_sig->value_mask_, property_sig->value_class_, "") << ")" << endl;
				
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No property found for \"" << match_string << "\"." << endl;
			
			return gStaticEidosValueNULLInvisible;
		}
		case gEidosID_method:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_argument_count == 1);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0) : gEidosStr_empty_string);
			const std::vector<const EidosMethodSignature *> *methods = Methods();
			bool signature_found = false;
			
			for (auto method_sig : *methods)
			{
				const std::string &method_name = method_sig->function_name_;
				
				if (has_match_string && (method_name.compare(match_string) != 0))
					continue;
				
				output_stream << *method_sig << endl;
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No method signature found for \"" << match_string << "\"." << endl;
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<const EidosMethodSignature *> *methods = Methods();
			const std::string &method_name = StringForEidosGlobalStringID(p_method_id);
			
			for (auto method_sig : *methods)
				if (method_sig->function_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): internal error: method " << method_name << " was not handled by subclass." << eidos_terminate();
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): unrecognized method name " << method_name << "." << eidos_terminate();
			
			return gStaticEidosValueNULLInvisible;
		}
	}
}







































































