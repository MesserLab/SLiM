//
//  eidos_value.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
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

#include "eidos_value.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <algorithm>
#include <utility>
#include <functional>
#include "math.h"
#include "errno.h"


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::istream;
using std::ostream;


// The global object pool for EidosValue, initialized in EidosWarmup()
EidosObjectPool *gEidosValuePool = nullptr;


//
//	Global static EidosValue objects; these are effectively const, although EidosValues can't be declared as const.
//	Internally, these are implemented as subclasses that terminate if they are dealloced or modified.
//

EidosValue_NULL_SP gStaticEidosValueNULL;
EidosValue_NULL_SP gStaticEidosValueNULLInvisible;

EidosValue_Logical_SP gStaticEidosValue_Logical_ZeroVec;
EidosValue_Int_SP gStaticEidosValue_Integer_ZeroVec;
EidosValue_Float_SP gStaticEidosValue_Float_ZeroVec;
EidosValue_String_SP gStaticEidosValue_String_ZeroVec;
EidosValue_Object_SP gStaticEidosValue_Object_ZeroVec;

EidosValue_Logical_SP gStaticEidosValue_LogicalT;
EidosValue_Logical_SP gStaticEidosValue_LogicalF;

EidosValue_Int_SP gStaticEidosValue_Integer0;
EidosValue_Int_SP gStaticEidosValue_Integer1;

EidosValue_Float_SP gStaticEidosValue_Float0;
EidosValue_Float_SP gStaticEidosValue_Float0Point5;
EidosValue_Float_SP gStaticEidosValue_Float1;

EidosValue_String_SP gStaticEidosValue_StringEmpty;
EidosValue_String_SP gStaticEidosValue_StringSpace;
EidosValue_String_SP gStaticEidosValue_StringAsterisk;

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

std::string StringForEidosValueMask(const EidosValueMask p_mask, const EidosObjectClass *p_object_class, const std::string &p_name, EidosValue *p_default)
{
	//
	//	Note this logic is paralleled in +[NSAttributedString eidosAttributedStringForCallSignature:].
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
		out_string += " ";	// non-breaking space
		out_string += p_name;
	}
	
	if (is_optional)
	{
		if (p_default)
		{
			out_string += " = ";
			
			std::ostringstream default_string_stream;
			
			p_default->Print(default_string_stream);
			
			out_string += default_string_stream.str();
		}
		
		out_string += "]";
	}
	
	return out_string;
}

// returns -1 if value1[index1] < value2[index2], 0 if ==, 1 if >, with full type promotion
int CompareEidosValues(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, EidosToken *p_blame_token)
{
	EidosValueType type1 = p_value1.Type();
	EidosValueType type2 = p_value2.Type();
	
	if ((type1 == EidosValueType::kValueNULL) || (type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) comparison with NULL is illegal." << eidos_terminate(p_blame_token);
	
	// comparing one object to another is legal, but objects cannot be compared to other types
	if ((type1 == EidosValueType::kValueObject) && (type2 == EidosValueType::kValueObject))
	{
		EidosObjectElement *element1 = p_value1.ObjectElementAtIndex(p_index1, p_blame_token);
		EidosObjectElement *element2 = p_value2.ObjectElementAtIndex(p_index2, p_blame_token);
		
		return (element1 == element2) ? 0 : -1;		// no relative ordering, just equality comparison; enforced in script_interpreter
	}
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((type1 == EidosValueType::kValueString) || (type2 == EidosValueType::kValueString))
	{
		string string1 = p_value1.StringAtIndex(p_index1, p_blame_token);
		string string2 = p_value2.StringAtIndex(p_index2, p_blame_token);
		int compare_result = string1.compare(string2);		// not guaranteed to be -1 / 0 / +1, just negative / 0 / positive
		
		return (compare_result < 0) ? -1 : ((compare_result > 0) ? 1 : 0);
	}
	
	// float is the next highest type, so we promote to float if either operand is a float
	if ((type1 == EidosValueType::kValueFloat) || (type2 == EidosValueType::kValueFloat))
	{
		double float1 = p_value1.FloatAtIndex(p_index1, p_blame_token);
		double float2 = p_value2.FloatAtIndex(p_index2, p_blame_token);
		
		return (float1 < float2) ? -1 : ((float1 > float2) ? 1 : 0);
	}
	
	// int is the next highest type, so we promote to int if either operand is a int
	if ((type1 == EidosValueType::kValueInt) || (type2 == EidosValueType::kValueInt))
	{
		int64_t int1 = p_value1.IntAtIndex(p_index1, p_blame_token);
		int64_t int2 = p_value2.IntAtIndex(p_index2, p_blame_token);
		
		return (int1 < int2) ? -1 : ((int1 > int2) ? 1 : 0);
	}
	
	// logical is the next highest type, so we promote to logical if either operand is a logical
	if ((type1 == EidosValueType::kValueLogical) || (type2 == EidosValueType::kValueLogical))
	{
		eidos_logical_t logical1 = p_value1.LogicalAtIndex(p_index1, p_blame_token);
		eidos_logical_t logical2 = p_value2.LogicalAtIndex(p_index2, p_blame_token);
		
		return (logical1 < logical2) ? -1 : ((logical1 > logical2) ? 1 : 0);
	}
	
	// that's the end of the road; we should never reach this point
	EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) comparison involving type " << type1 << " and type " << type2 << " is undefined." << eidos_terminate(p_blame_token);
	return 0;
}

int CompareEidosValues_Object(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, EidosToken *p_blame_token)
{
	EidosObjectElement *element1 = p_value1.ObjectElementAtIndex(p_index1, p_blame_token);
	EidosObjectElement *element2 = p_value2.ObjectElementAtIndex(p_index2, p_blame_token);
	
	return (element1 == element2) ? 0 : -1;		// no relative ordering, just equality comparison; enforced in script_interpreter
}

int CompareEidosValues_String(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, EidosToken *p_blame_token)
{
	string string1 = p_value1.StringAtIndex(p_index1, p_blame_token);
	string string2 = p_value2.StringAtIndex(p_index2, p_blame_token);
	int compare_result = string1.compare(string2);		// not guaranteed to be -1 / 0 / +1, just negative / 0 / positive
	
	return (compare_result < 0) ? -1 : ((compare_result > 0) ? 1 : 0);
}

int CompareEidosValues_Float(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, EidosToken *p_blame_token)
{
	double float1 = p_value1.FloatAtIndex(p_index1, p_blame_token);
	double float2 = p_value2.FloatAtIndex(p_index2, p_blame_token);
	
	return (float1 < float2) ? -1 : ((float1 > float2) ? 1 : 0);
}

int CompareEidosValues_Int(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, EidosToken *p_blame_token)
{
	int64_t int1 = p_value1.IntAtIndex(p_index1, p_blame_token);
	int64_t int2 = p_value2.IntAtIndex(p_index2, p_blame_token);
	
	return (int1 < int2) ? -1 : ((int1 > int2) ? 1 : 0);
}

int CompareEidosValues_Logical(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, EidosToken *p_blame_token)
{
	eidos_logical_t logical1 = p_value1.LogicalAtIndex(p_index1, p_blame_token);
	eidos_logical_t logical2 = p_value2.LogicalAtIndex(p_index2, p_blame_token);
	
	return (logical1 < logical2) ? -1 : ((logical1 > logical2) ? 1 : 0);
}

EidosCompareFunctionPtr EidosGetCompareFunctionForTypes(EidosValueType p_type1, EidosValueType p_type2, EidosToken *p_blame_token)
{
	if ((p_type1 == EidosValueType::kValueNULL) || (p_type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (EidosGetCompareFunctionForTypes): (internal error) comparison with NULL is illegal." << eidos_terminate(p_blame_token);
	
	// comparing one object to another is legal, but objects cannot be compared to other types
	if ((p_type1 == EidosValueType::kValueObject) && (p_type2 == EidosValueType::kValueObject))
		return CompareEidosValues_Object;
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((p_type1 == EidosValueType::kValueString) || (p_type2 == EidosValueType::kValueString))
		return CompareEidosValues_String;
	
	// float is the next highest type, so we promote to float if either operand is a float
	if ((p_type1 == EidosValueType::kValueFloat) || (p_type2 == EidosValueType::kValueFloat))
		return CompareEidosValues_Float;
	
	// int is the next highest type, so we promote to int if either operand is a int
	if ((p_type1 == EidosValueType::kValueInt) || (p_type2 == EidosValueType::kValueInt))
		return CompareEidosValues_Int;
	
	// logical is the next highest type, so we promote to logical if either operand is a logical
	if ((p_type1 == EidosValueType::kValueLogical) || (p_type2 == EidosValueType::kValueLogical))
		return CompareEidosValues_Logical;
	
	// that's the end of the road; we should never reach this point
	EIDOS_TERMINATION << "ERROR (EidosGetCompareFunctionForTypes): (internal error) comparison involving type " << p_type1 << " and type " << p_type2 << " is undefined." << eidos_terminate(p_blame_token);
	return 0;
}


//
//	EidosValue
//
#pragma mark -
#pragma mark EidosValue

#ifdef EIDOS_TRACK_VALUE_ALLOCATION
int EidosValue::valueTrackingCount;
std::vector<EidosValue *> EidosValue::valueTrackingVector;
#endif

EidosValue::EidosValue(EidosValueType p_value_type, bool p_singleton) : intrusive_ref_count(0), invisible_(false), cached_type_(p_value_type), is_singleton_(p_singleton)
{
#ifdef EIDOS_TRACK_VALUE_ALLOCATION
	valueTrackingCount++;
	valueTrackingVector.emplace_back(this);
#endif
}

EidosValue::~EidosValue(void)
{
#ifdef EIDOS_TRACK_VALUE_ALLOCATION
	valueTrackingVector.erase(std::remove(valueTrackingVector.begin(), valueTrackingVector.end(), this), valueTrackingVector.end());
	valueTrackingCount--;
#endif
}

eidos_logical_t EidosValue::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::LogicalAtIndex): operand type " << this->Type() << " cannot be converted to type logical." << eidos_terminate(p_blame_token);
}

std::string EidosValue::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::StringAtIndex): operand type " << this->Type() << " cannot be converted to type string." << eidos_terminate(p_blame_token);
}

int64_t EidosValue::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::IntAtIndex): operand type " << this->Type() << " cannot be converted to type integer." << eidos_terminate(p_blame_token);
}

double EidosValue::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::FloatAtIndex): operand type " << this->Type() << " cannot be converted to type float." << eidos_terminate(p_blame_token);
}

EidosObjectElement *EidosValue::ObjectElementAtIndex(int p_idx, EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::ObjectElementAtIndex): operand type " << this->Type() << " cannot be converted to type object." << eidos_terminate(p_blame_token);
}

void EidosValue::RaiseForUnimplementedVectorCall(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForUnimplementedVectorCall): (internal error) direct vector access attempted on an EidosValue type that does not support it." << eidos_terminate(nullptr);
}

EidosValue_SP EidosValue::VectorBasedCopy(void) const
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

EidosValue_NULL::~EidosValue_NULL(void)
{
}

/* static */ EidosValue_NULL_SP EidosValue_NULL::Static_EidosValue_NULL(void)
{
	// this is a truly permanent constant object
	static EidosValue_NULL_SP static_null = EidosValue_NULL_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_NULL());
	
	return static_null;
}

/* static */ EidosValue_NULL_SP EidosValue_NULL::Static_EidosValue_NULL_Invisible(void)
{
	// this is a truly permanent constant object
	static EidosValue_NULL_SP static_null = EidosValue_NULL_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_NULL());
	
	static_null->invisible_ = true;		// set every time, since we don't have a constructor to set invisibility
	
	return static_null;
}

const std::string &EidosValue_NULL::ElementType(void) const
{
	return gEidosStr_NULL;
}

int EidosValue_NULL::Count_Virtual(void) const
{
	return 0;
}

void EidosValue_NULL::Print(std::ostream &p_ostream) const
{
	p_ostream << gEidosStr_NULL;
}

EidosValue_SP EidosValue_NULL::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
#pragma unused(p_idx, p_blame_token)
	return gStaticEidosValueNULL;
}

void EidosValue_NULL::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_NULL::SetValueAtIndex): operand type " << this->Type() << " does not support setting values with the subscript operator ('[]')." << eidos_terminate(p_blame_token);
}

EidosValue_SP EidosValue_NULL::CopyValues(void) const
{
	return gStaticEidosValueNULL;
}

EidosValue_SP EidosValue_NULL::NewMatchingType(void) const
{
	return gStaticEidosValueNULL;
}

void EidosValue_NULL::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx)
	if (p_source_script_value.Type() == EidosValueType::kValueNULL)
		;	// NULL doesn't have values or indices, so this is a no-op
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_NULL::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate(p_blame_token);
}

void EidosValue_NULL::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	// nothing to do
}


//
//	EidosValue_Logical
//
#pragma mark -
#pragma mark EidosValue_Logical

EidosValue_Logical::EidosValue_Logical(void) : EidosValue(EidosValueType::kValueLogical, false)
{
}

EidosValue_Logical::EidosValue_Logical(const std::vector<eidos_logical_t> &p_logicalvec) : EidosValue(EidosValueType::kValueLogical, false)
{
	values_ = p_logicalvec;
}

EidosValue_Logical::EidosValue_Logical(eidos_logical_t p_logical1) : EidosValue(EidosValueType::kValueLogical, false)	// protected
{
	values_.emplace_back(p_logical1);
}

EidosValue_Logical::EidosValue_Logical(std::initializer_list<eidos_logical_t> p_init_list) : EidosValue(EidosValueType::kValueLogical, false)
{
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		values_.emplace_back(*init_item);
}

EidosValue_Logical::~EidosValue_Logical(void)
{
}

const std::string &EidosValue_Logical::ElementType(void) const
{
	return gEidosStr_logical;
}

int EidosValue_Logical::Count_Virtual(void) const
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
		
		for (eidos_logical_t value : values_)
		{
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << (value ? gEidosStr_T : gEidosStr_F);
		}
	}
}

EidosValue_Logical *EidosValue_Logical::Reserve(int p_reserved_size)
{
	values_.reserve(p_reserved_size);
	return this;
}

eidos_logical_t EidosValue_Logical::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::LogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return values_[p_idx];
}

std::string EidosValue_Logical::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::StringAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (values_[p_idx] ? gEidosStr_T : gEidosStr_F);
}

int64_t EidosValue_Logical::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::IntAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (values_[p_idx] ? 1 : 0);
}

double EidosValue_Logical::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::FloatAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (values_[p_idx] ? 1.0 : 0.0);
}

void EidosValue_Logical::PushLogical(eidos_logical_t p_logical)
{
	values_.emplace_back(p_logical);
}

void EidosValue_Logical::SetLogicalAtIndex(const int p_idx, eidos_logical_t p_logical, EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::SetLogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	values_[p_idx] = p_logical;
}

EidosValue_SP EidosValue_Logical::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (values_[p_idx] ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

void EidosValue_Logical::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	values_[p_idx] = p_value.LogicalAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_Logical::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(values_));
}

EidosValue_SP EidosValue_Logical::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
}

void EidosValue_Logical::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueLogical)
		values_.emplace_back(p_source_script_value.LogicalAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate(p_blame_token);
}

void EidosValue_Logical::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<eidos_logical_t>());
}

// EidosValue_Logical_const
#pragma mark EidosValue_Logical_const

EidosValue_Logical_const::EidosValue_Logical_const(eidos_logical_t p_bool1) : EidosValue_Logical(p_bool1)
{
	is_singleton_ = true;	// because EidosValue_Logical has a different class structure from the other types, this has to be patched after construction; no harm done...
}

EidosValue_Logical_const::~EidosValue_Logical_const(void)
{
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::~EidosValue_Logical_const): (internal error) global constant deallocated." << eidos_terminate(nullptr);
}

/* static */ EidosValue_Logical_SP EidosValue_Logical_const::Static_EidosValue_Logical_T(void)
{
	// this is a truly permanent constant object
	static EidosValue_Logical_const_SP static_T = EidosValue_Logical_const_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical_const(true));
	
	return static_T;
}

/* static */ EidosValue_Logical_SP EidosValue_Logical_const::Static_EidosValue_Logical_F(void)
{
	// this is a truly permanent constant object
	static EidosValue_Logical_const_SP static_F = EidosValue_Logical_const_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical_const(false));
	
	return static_F;
}

EidosValue_SP EidosValue_Logical_const::VectorBasedCopy(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(values_));	// same as EidosValue_Logical::, but let's not rely on that
}

EidosValue_Logical *EidosValue_Logical_const::Reserve(int p_reserved_size)
{
#pragma unused(p_reserved_size)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::Reserve): (internal error) EidosValue_Logical_const is not modifiable." << eidos_terminate(nullptr);
}

void EidosValue_Logical_const::PushLogical(eidos_logical_t p_logical)
{
#pragma unused(p_logical)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::PushLogical): (internal error) EidosValue_Logical_const is not modifiable." << eidos_terminate(nullptr);
}

void EidosValue_Logical_const::SetLogicalAtIndex(const int p_idx, eidos_logical_t p_logical, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_logical)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::SetLogicalAtIndex): (internal error) EidosValue_Logical_const is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Logical_const::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::SetValueAtIndex): (internal error) EidosValue_Logical_const is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Logical_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Logical_const is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Logical_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::Sort): (internal error) EidosValue_Logical_const is not modifiable." << eidos_terminate(nullptr);
}


//
//	EidosValue_String
//
#pragma mark -
#pragma mark EidosValue_String

EidosValue_String::~EidosValue_String(void)
{
}

const std::string &EidosValue_String::ElementType(void) const
{
	return gEidosStr_string;
}

EidosValue_SP EidosValue_String::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
}


// EidosValue_String_vector
#pragma mark EidosValue_String_vector

EidosValue_String_vector::EidosValue_String_vector(void) : EidosValue_String(false)
{
}

EidosValue_String_vector::EidosValue_String_vector(const std::vector<std::string> &p_stringvec) : EidosValue_String(false)
{
	values_ = p_stringvec;
}

EidosValue_String_vector::EidosValue_String_vector(std::initializer_list<const std::string> p_init_list) : EidosValue_String(false)
{
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		values_.emplace_back(*init_item);
}

EidosValue_String_vector::~EidosValue_String_vector(void)
{
}

int EidosValue_String_vector::Count_Virtual(void) const
{
	return (int)values_.size();
}

void EidosValue_String_vector::Print(std::ostream &p_ostream) const
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
			
			// Emit a quoted string.  Note that we do not attempt to escape characters so that the emitted string is a
			// legal string for input into Eidos that would reproduce the original string, at present.
			if (value.find('"') != std::string::npos)
			{
				// contains ", so let's use '
				p_ostream << '\'' << value << '\'';
			}
			else
			{
				// does not contain ", so let's use that; double quotes are the default/standard
				p_ostream << '"' << value << '"';
			}
		}
	}
}

eidos_logical_t EidosValue_String_vector::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::LogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (values_[p_idx].length() > 0);
}

std::string EidosValue_String_vector::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::StringAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return values_[p_idx];
}

int64_t EidosValue_String_vector::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::IntAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	// We don't use IntForString because an integer has been specifically requested, so even if the string appears to contain a
	// float value we want to force it into being an int; the way to do that is to use FloatForString and then convert to int64_t
	double converted_value = EidosInterpreter::FloatForString(values_[p_idx], p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::IntAtIndex): \"" << values_[p_idx] << "\" could not be represented as an integer (out of range)." << eidos_terminate(p_blame_token);
	
	return static_cast<int64_t>(converted_value);
}

double EidosValue_String_vector::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::FloatAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosInterpreter::FloatForString(values_[p_idx], p_blame_token);
}

EidosValue_SP EidosValue_String_vector::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(values_[p_idx]));
}

void EidosValue_String_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	values_[p_idx] = p_value.StringAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_String_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector(values_));
}

void EidosValue_String_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueString)
		values_.emplace_back(p_source_script_value.StringAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate(p_blame_token);
}

void EidosValue_String_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<std::string>());
}


// EidosValue_String_singleton
#pragma mark EidosValue_String_singleton

EidosValue_String_singleton::EidosValue_String_singleton(const std::string &p_string1) : value_(p_string1), EidosValue_String(true)
{
}

EidosValue_String_singleton::~EidosValue_String_singleton(void)
{
	delete cached_script_;
}

int EidosValue_String_singleton::Count_Virtual(void) const
{
	return 1;
}

void EidosValue_String_singleton::Print(std::ostream &p_ostream) const
{
	// Emit a quoted string.  Note that we do not attempt to escape characters so that the emitted string is a
	// legal string for input into Eidos that would reproduce the original string, at present.
	if (value_.find('"') != std::string::npos)
	{
		// contains ", so let's use '
		p_ostream << '\'' << value_ << '\'';
	}
	else
	{
		// does not contain ", so let's use that; double quotes are the default/standard
		p_ostream << '"' << value_ << '"';
	}
}

eidos_logical_t EidosValue_String_singleton::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::LogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (value_.length() > 0);
}

std::string EidosValue_String_singleton::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::StringAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return value_;
}

int64_t EidosValue_String_singleton::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::IntAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	// We don't use IntForString because an integer has been specifically requested, so even if the string appears to contain a
	// float value we want to force it into being an int; the way to do that is to use FloatForString and then convert to int64_t
	double converted_value = EidosInterpreter::FloatForString(value_, p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::IntAtIndex): \"" << value_ << "\" could not be represented as an integer (out of range)." << eidos_terminate(p_blame_token);
	
	return static_cast<int64_t>(converted_value);
}

double EidosValue_String_singleton::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::FloatAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosInterpreter::FloatForString(value_, p_blame_token);
}

EidosValue_SP EidosValue_String_singleton::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(value_));
}

EidosValue_SP EidosValue_String_singleton::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(value_));
}

EidosValue_SP EidosValue_String_singleton::VectorBasedCopy(void) const
{
	EidosValue_String_vector_SP new_vec = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
	
	new_vec->PushString(value_);
	
	return new_vec;
}

void EidosValue_String_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::SetValueAtIndex): (internal error) EidosValue_String_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_String_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_String_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_String_singleton::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::Sort): (internal error) EidosValue_String_singleton is not modifiable." << eidos_terminate(nullptr);
}


//
//	EidosValue_Int
//
#pragma mark -
#pragma mark EidosValue_Int

EidosValue_Int::~EidosValue_Int(void)
{
}

const std::string &EidosValue_Int::ElementType(void) const
{
	return gEidosStr_integer;
}

EidosValue_SP EidosValue_Int::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
}


// EidosValue_Int_vector
#pragma mark EidosValue_Int_vector

EidosValue_Int_vector::EidosValue_Int_vector(void) : EidosValue_Int(false)
{
}

EidosValue_Int_vector::EidosValue_Int_vector(const std::vector<int16_t> &p_intvec) : EidosValue_Int(false)
{
	values_.reserve(p_intvec.size());
	
	for (auto intval : p_intvec)
		values_.emplace_back(intval);
}

EidosValue_Int_vector::EidosValue_Int_vector(const std::vector<int32_t> &p_intvec) : EidosValue_Int(false)
{
	values_.reserve(p_intvec.size());
	
	for (auto intval : p_intvec)
		values_.emplace_back(intval);
}

EidosValue_Int_vector::EidosValue_Int_vector(const std::vector<int64_t> &p_intvec) : EidosValue_Int(false)
{
	values_ = p_intvec;
}

EidosValue_Int_vector::EidosValue_Int_vector(std::initializer_list<int64_t> p_init_list) : EidosValue_Int(false)
{
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		values_.emplace_back(*init_item);
}

EidosValue_Int_vector::~EidosValue_Int_vector(void)
{
}

int EidosValue_Int_vector::Count_Virtual(void) const
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

eidos_logical_t EidosValue_Int_vector::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::LogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (values_[p_idx] == 0 ? false : true);
}

std::string EidosValue_Int_vector::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::StringAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	// with C++11, could use std::to_string(values_[p_idx])
	std::ostringstream ss;
	
	ss << values_[p_idx];
	
	return ss.str();
}

int64_t EidosValue_Int_vector::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::IntAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return values_[p_idx];
}

double EidosValue_Int_vector::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::FloatAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Int_vector::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(values_[p_idx]));
}

void EidosValue_Int_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	values_[p_idx] = p_value.IntAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_Int_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(values_));
}

void EidosValue_Int_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueInt)
		values_.emplace_back(p_source_script_value.IntAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate(p_blame_token);
}

void EidosValue_Int_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<int64_t>());
}



// EidosValue_Int_singleton
#pragma mark EidosValue_Int_singleton

EidosValue_Int_singleton::EidosValue_Int_singleton(int64_t p_int1) : value_(p_int1), EidosValue_Int(true)
{
}

EidosValue_Int_singleton::~EidosValue_Int_singleton(void)
{
}

int EidosValue_Int_singleton::Count_Virtual(void) const
{
	return 1;
}

void EidosValue_Int_singleton::Print(std::ostream &p_ostream) const
{
	p_ostream << value_;
}

eidos_logical_t EidosValue_Int_singleton::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::LogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Int_singleton::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::StringAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	// with C++11, could use std::to_string(value_)
	std::ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Int_singleton::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::IntAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return value_;
}

double EidosValue_Int_singleton::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::FloatAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return value_;
}

EidosValue_SP EidosValue_Int_singleton::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(value_));
}

EidosValue_SP EidosValue_Int_singleton::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(value_));
}

EidosValue_SP EidosValue_Int_singleton::VectorBasedCopy(void) const
{
	EidosValue_Int_vector_SP new_vec = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
	
	new_vec->PushInt(value_);
	
	return new_vec;
}

void EidosValue_Int_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::SetValueAtIndex): (internal error) EidosValue_Float_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Int_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Float_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Int_singleton::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::Sort): (internal error) EidosValue_Float_singleton is not modifiable." << eidos_terminate(nullptr);
}


//
//	EidosValue_Float
//
#pragma mark -
#pragma mark EidosValue_Float

EidosValue_Float::~EidosValue_Float(void)
{
}

const std::string &EidosValue_Float::ElementType(void) const
{
	return gEidosStr_float;
}

EidosValue_SP EidosValue_Float::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
}


// EidosValue_Float_vector
#pragma mark EidosValue_Float_vector

EidosValue_Float_vector::EidosValue_Float_vector(void) : EidosValue_Float(false)
{
}

EidosValue_Float_vector::EidosValue_Float_vector(const std::vector<double> &p_doublevec) : EidosValue_Float(false)
{
	values_ = p_doublevec;
}

EidosValue_Float_vector::EidosValue_Float_vector(double *p_doublebuf, int p_buffer_length) : EidosValue_Float(false)
{
	for (int index = 0; index < p_buffer_length; index++)
		values_.emplace_back(p_doublebuf[index]);
}

EidosValue_Float_vector::EidosValue_Float_vector(std::initializer_list<double> p_init_list) : EidosValue_Float(false)
{
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		values_.emplace_back(*init_item);
}

EidosValue_Float_vector::~EidosValue_Float_vector(void)
{
}

int EidosValue_Float_vector::Count_Virtual(void) const
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
			
			// Customize our output a bit to look like Eidos, not C++
			if (isinf(value))
			{
				if (std::signbit(value))
					p_ostream << gEidosStr_MINUS_INF;
				else
					p_ostream << gEidosStr_INF;
			}
			else if (isnan(value))
				p_ostream << gEidosStr_NAN;
			else
				p_ostream << value;
		}
	}
}

eidos_logical_t EidosValue_Float_vector::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::LogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	double value = values_[p_idx];
	
	if (isnan(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::LogicalAtIndex): NAN cannot be converted to logical type." << eidos_terminate(p_blame_token);
	
	return (value == 0 ? false : true);
}

std::string EidosValue_Float_vector::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::StringAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	// with C++11, could use std::to_string(values_[p_idx])
	std::ostringstream ss;
	double value = values_[p_idx];
	
	// Customize our output a bit to look like Eidos, not C++
	if (isinf(value))
	{
		if (std::signbit(value))
			ss << gEidosStr_MINUS_INF;
		else
			ss << gEidosStr_INF;
	}
	else if (isnan(value))
		ss << gEidosStr_NAN;
	else
		ss << value;
	
	return ss.str();
}

int64_t EidosValue_Float_vector::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	double value = values_[p_idx];
	
	if (isnan(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): NAN cannot be converted to integer type." << eidos_terminate(p_blame_token);
	if (isinf(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): INF cannot be converted to integer type." << eidos_terminate(p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((value < INT64_MIN) || (value >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): float value " << value << " is too large to be converted to integer type." << eidos_terminate(p_blame_token);
	
	return static_cast<int64_t>(value);
}

double EidosValue_Float_vector::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::FloatAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Float_vector::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(values_[p_idx]));
}

void EidosValue_Float_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	values_[p_idx] = p_value.FloatAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_Float_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(values_));
}

void EidosValue_Float_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueFloat)
		values_.emplace_back(p_source_script_value.FloatAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate(p_blame_token);
}

void EidosValue_Float_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_.begin(), values_.end());
	else
		std::sort(values_.begin(), values_.end(), std::greater<double>());
}


// EidosValue_Float_singleton
#pragma mark EidosValue_Float_singleton

EidosValue_Float_singleton::EidosValue_Float_singleton(double p_float1) : value_(p_float1), EidosValue_Float(true)
{
}

EidosValue_Float_singleton::~EidosValue_Float_singleton(void)
{
}

int EidosValue_Float_singleton::Count_Virtual(void) const
{
	return 1;
}

void EidosValue_Float_singleton::Print(std::ostream &p_ostream) const
{
	// Customize our output a bit to look like Eidos, not C++
	if (isinf(value_))
	{
		if (std::signbit(value_))
			p_ostream << gEidosStr_MINUS_INF;
		else
			p_ostream << gEidosStr_INF;
	}
	else if (isnan(value_))
		p_ostream << gEidosStr_NAN;
	else
		p_ostream << value_;
}

eidos_logical_t EidosValue_Float_singleton::LogicalAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::LogicalAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	if (isnan(value_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::LogicalAtIndex): NAN cannot be converted to logical type." << eidos_terminate(p_blame_token);
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Float_singleton::StringAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::StringAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	// with C++11, could use std::to_string(value_)
	std::ostringstream ss;
	
	// Customize our output a bit to look like Eidos, not C++
	if (isinf(value_))
	{
		if (std::signbit(value_))
			ss << gEidosStr_MINUS_INF;
		else
			ss << gEidosStr_INF;
	}
	else if (isnan(value_))
		ss << gEidosStr_NAN;
	else
		ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Float_singleton::IntAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	if (isnan(value_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): NAN cannot be converted to integer type." << eidos_terminate(p_blame_token);
	if (isinf(value_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): INF cannot be converted to integer type." << eidos_terminate(p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((value_ < INT64_MIN) || (value_ >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): float value " << value_ << " is too large to be converted to integer type." << eidos_terminate(p_blame_token);
	
	return static_cast<int64_t>(value_);
}

double EidosValue_Float_singleton::FloatAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::FloatAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return value_;
}

EidosValue_SP EidosValue_Float_singleton::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(value_));
}

EidosValue_SP EidosValue_Float_singleton::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(value_));
}

EidosValue_SP EidosValue_Float_singleton::VectorBasedCopy(void) const
{
	EidosValue_Float_vector_SP new_vec = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
	
	new_vec->PushFloat(value_);
	
	return new_vec;
}

void EidosValue_Float_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::SetValueAtIndex): (internal error) EidosValue_Float_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Float_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Float_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Float_singleton::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::Sort): (internal error) EidosValue_Float_singleton is not modifiable." << eidos_terminate(nullptr);
}


//
//	EidosValue_Object
//
#pragma mark -
#pragma mark EidosValue_Object

EidosValue_Object::~EidosValue_Object(void)
{
}

const std::string &EidosValue_Object::ElementType(void) const
{
	return Class()->ElementType();
}

EidosValue_SP EidosValue_Object::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(Class()));
}

void EidosValue_Object::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object::Sort): Sort() is not defined for type object." << eidos_terminate(nullptr);
}


// EidosValue_Object_vector
#pragma mark EidosValue_Object_vector

EidosValue_Object_vector::EidosValue_Object_vector(const EidosValue_Object_vector &p_original) : EidosValue_Object(false, p_original.Class())
{
	for (auto value : p_original.values_)
	{
#if DEBUG
		if (value->Class() != class_)
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::EidosValue_Object_vector): class mismatch." << eidos_terminate(nullptr);
#endif
		
		values_.emplace_back(value->Retain());
	}
}

EidosValue_Object_vector::EidosValue_Object_vector(const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
{
}

EidosValue_Object_vector::EidosValue_Object_vector(const std::vector<EidosObjectElement *> &p_elementvec, const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
{
	values_ = p_elementvec;
	
	for (auto value : values_)
	{
#if DEBUG
		if (value->Class() != class_)
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::EidosValue_Object_vector): class mismatch." << eidos_terminate(nullptr);
#endif
		
		value->Retain();
	}
}

EidosValue_Object_vector::EidosValue_Object_vector(std::initializer_list<EidosObjectElement *> p_init_list, const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
{
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
	{
#if DEBUG
		if ((*init_item)->Class() != class_)
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::EidosValue_Object_vector): class mismatch." << eidos_terminate(nullptr);
#endif
		
		values_.emplace_back((*init_item)->Retain());
	}
}

EidosValue_Object_vector::~EidosValue_Object_vector(void)
{
	if (values_.size() != 0)
	{
		for (auto value : values_)
			value->Release();
	}
}

int EidosValue_Object_vector::Count_Virtual(void) const
{
	return (int)values_.size();
}

void EidosValue_Object_vector::Print(std::ostream &p_ostream) const
{
	if (values_.size() == 0)
	{
		p_ostream << "object()<" << class_->ElementType() << ">";
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

EidosObjectElement *EidosValue_Object_vector::ObjectElementAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ObjectElementAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return values_[p_idx];
}

void EidosValue_Object_vector::PushObjectElement(EidosObjectElement *p_element)
{
	const EidosObjectClass *element_class = p_element->Class();
	
	if (class_ != element_class)
	{
		if (class_ == gEidos_UndefinedClassObject)
			class_ = element_class;
		else
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushObjectElement): the type of an object cannot be changed." << eidos_terminate(nullptr);
	}
	
	values_.emplace_back(p_element->Retain());
}

EidosValue_SP EidosValue_Object_vector::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(values_[p_idx], Class()));
}

void EidosValue_Object_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	// can't change the type of element object we collect
	const EidosObjectClass *element_class = ((EidosValue_Object &)p_value).Class();
	
	if (class_ != element_class)
	{
		if (class_ == gEidos_UndefinedClassObject)
			class_ = element_class;
		else
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueAtIndex): the type of an object cannot be changed." << eidos_terminate(p_blame_token);
	}
	
	EidosObjectElement *new_value = p_value.ObjectElementAtIndex(0, p_blame_token);
	
	new_value->Retain();
	values_[p_idx]->Release();
	values_[p_idx] = new_value;
}

EidosValue_SP EidosValue_Object_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(*this));
}

void EidosValue_Object_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueObject)
	{
		const EidosObjectClass *element_class = ((EidosValue_Object &)p_source_script_value).Class();
		
		if (class_ != element_class)
		{
			if (class_ == gEidos_UndefinedClassObject)
				class_ = element_class;
			else
				EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushValueFromIndexOfEidosValue): the type of an object cannot be changed." << eidos_terminate(p_blame_token);
		}
		
		values_.emplace_back(p_source_script_value.ObjectElementAtIndex(p_idx, p_blame_token)->Retain());
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushValueFromIndexOfEidosValue): type mismatch." << eidos_terminate(p_blame_token);
}

static bool CompareLogicalObjectSortPairsAscending(std::pair<eidos_logical_t, EidosObjectElement*> i, std::pair<eidos_logical_t, EidosObjectElement*> j);
static bool CompareLogicalObjectSortPairsAscending(std::pair<eidos_logical_t, EidosObjectElement*> i, std::pair<eidos_logical_t, EidosObjectElement*> j)	{ return (i.first < j.first); }
static bool CompareLogicalObjectSortPairsDescending(std::pair<eidos_logical_t, EidosObjectElement*> i, std::pair<eidos_logical_t, EidosObjectElement*> j);
static bool CompareLogicalObjectSortPairsDescending(std::pair<eidos_logical_t, EidosObjectElement*> i, std::pair<eidos_logical_t, EidosObjectElement*> j)	{ return (i.first > j.first); }

static bool CompareIntObjectSortPairsAscending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j);
static bool CompareIntObjectSortPairsAscending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j)				{ return (i.first < j.first); }
static bool CompareIntObjectSortPairsDescending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j);
static bool CompareIntObjectSortPairsDescending(std::pair<int64_t, EidosObjectElement*> i, std::pair<int64_t, EidosObjectElement*> j)				{ return (i.first > j.first); }

static bool CompareFloatObjectSortPairsAscending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j);
static bool CompareFloatObjectSortPairsAscending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j)				{ return (i.first < j.first); }
static bool CompareFloatObjectSortPairsDescending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j);
static bool CompareFloatObjectSortPairsDescending(std::pair<double, EidosObjectElement*> i, std::pair<double, EidosObjectElement*> j)				{ return (i.first > j.first); }

static bool CompareStringObjectSortPairsAscending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j);
static bool CompareStringObjectSortPairsAscending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j)		{ return (i.first < j.first); }
static bool CompareStringObjectSortPairsDescending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j);
static bool CompareStringObjectSortPairsDescending(std::pair<std::string, EidosObjectElement*> i, std::pair<std::string, EidosObjectElement*> j)	{ return (i.first > j.first); }

void EidosValue_Object_vector::SortBy(const std::string &p_property, bool p_ascending)
{
	// At present this is called only by the sortBy() Eidos function, so we do not need a
	// blame token; all errors will be attributed to that function automatically.
	
	// length 0 is already sorted
	if (values_.size() == 0)
		return;
	
	// figure out what type the property returns
	EidosGlobalStringID property_string_id = EidosGlobalStringIDForString(p_property);
	EidosValue_SP first_result = values_[0]->GetProperty(property_string_id);
	EidosValueType property_type = first_result->Type();
	
	// switch on the property type for efficiency
	switch (property_type)
	{
		case EidosValueType::kValueNULL:
		case EidosValueType::kValueObject:
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " returned " << property_type << "; a property that evaluates to logical, int, float, or string is required." << eidos_terminate(nullptr);
			break;
			
		case EidosValueType::kValueLogical:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<eidos_logical_t, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<eidos_logical_t, EidosObjectElement*>(temp_result->LogicalAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.emplace_back(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueInt:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<int64_t, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<int64_t, EidosObjectElement*>(temp_result->IntAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.emplace_back(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueFloat:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<double, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<double, EidosObjectElement*>(temp_result->FloatAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.emplace_back(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueString:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			vector<std::pair<std::string, EidosObjectElement*>> sortable_pairs;
			
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << eidos_terminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << eidos_terminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<std::string, EidosObjectElement*>(temp_result->StringAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsDescending);
			
			// read out our new element vector
			values_.clear();
			
			for (auto sorted_pair : sortable_pairs)
				values_.emplace_back(sorted_pair.second);
			
			break;
		}
	}
}

EidosValue_SP EidosValue_Object_vector::GetPropertyOfElements(EidosGlobalStringID p_property_id) const
{
	auto values_size = values_.size();
	const EidosPropertySignature *signature = class_->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate(nullptr);
	
	if (values_size == 0)
	{
		// With a zero-length vector, the return is a zero-length vector of the type specified by the property signature.  This
		// allows code to proceed without errors without having to check for the zero-length case, by simply propagating the
		// zero-length-ness forward while preserving the expected type for the expression.  If multiple return types are possible,
		// we return a zero-length vector for the simplest type supported.
		EidosValueMask sig_mask = (signature->value_mask_ & kEidosValueMaskFlagStrip);
		
		if (sig_mask & kEidosValueMaskNULL) return gStaticEidosValueNULL;	// we assume visible NULL is the NULL desired, to avoid masking errors
		if (sig_mask & kEidosValueMaskLogical) return gStaticEidosValue_Logical_ZeroVec;
		if (sig_mask & kEidosValueMaskInt) return gStaticEidosValue_Integer_ZeroVec;
		if (sig_mask & kEidosValueMaskFloat) return gStaticEidosValue_Float_ZeroVec;
		if (sig_mask & kEidosValueMaskString) return gStaticEidosValue_String_ZeroVec;
		if (sig_mask & kEidosValueMaskObject)
		{
			const EidosObjectClass *value_class = signature->value_class_;
			
			if (value_class)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(value_class));
			else
				return gStaticEidosValue_Object_ZeroVec;
		}
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " does not specify a value type, and thus cannot be accessed on a zero-length vector." << eidos_terminate(nullptr);
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		// comment this case out to fully test the accelerated property code below
		EidosObjectElement *value = values_[0];
		EidosValue_SP result = value->GetProperty(p_property_id);
		
		signature->CheckResultValue(*result);
		return result;
	}
	else if (signature->accelerated_)
	{
		// Accelerated property access is enabled for this property, so we will switch on the type of the property, and
		// assemble the result value directly from the C++ type values we get from the accelerated access methods...
		EidosValueMask sig_mask = (signature->value_mask_ & kEidosValueMaskFlagStrip);
		
		switch (sig_mask)
		{
			case kEidosValueMaskLogical:
			{
				EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve((int)values_size);
				std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
				
				for (auto value : values_)
					logical_result_vec.emplace_back(value->GetProperty_Accelerated_Logical(p_property_id));
				
				return EidosValue_SP(logical_result);
			}
			case kEidosValueMaskInt:
			{
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve((int)values_size);
				
				for (auto value : values_)
					int_result->PushInt(value->GetProperty_Accelerated_Int(p_property_id));
				
				return EidosValue_SP(int_result);
			}
			case kEidosValueMaskFloat:
			{
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve((int)values_size);
				
				for (auto value : values_)
					float_result->PushFloat(value->GetProperty_Accelerated_Float(p_property_id));
				
				return EidosValue_SP(float_result);
			}
			case kEidosValueMaskString:
			{
				EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)values_size);
				
				for (auto value : values_)
					string_result->PushString(value->GetProperty_Accelerated_String(p_property_id));
				
				return EidosValue_SP(string_result);
			}
			case kEidosValueMaskObject:
			{
				const EidosObjectClass *value_class = signature->value_class_;
				
				if (value_class)
				{
					EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(value_class))->Reserve((int)values_size);
					
					for (auto value : values_)
						object_result->PushObjectElement(value->GetProperty_Accelerated_ObjectElement(p_property_id));
					
					return EidosValue_SP(object_result);
				}
				else
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): (internal error) missing object element class for accelerated property access." << eidos_terminate(nullptr);
			}
			default:
				EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): (internal error) unsupported value type for accelerated property access." << eidos_terminate(nullptr);
		}
	}
	else
	{
		// get the value from all properties and collect the results
		vector<EidosValue_SP> results;
		
		if (values_size < 10)
		{
			// with small objects, we check every value
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->GetProperty(p_property_id);
				
				signature->CheckResultValue(*temp_result);
				results.emplace_back(temp_result);
			}
		}
		else
		{
			// with large objects, we just spot-check the first value, for speed
			bool checked_multivalued = false;
			
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->GetProperty(p_property_id);
				
				if (!checked_multivalued)
				{
					signature->CheckResultValue(*temp_result);
					checked_multivalued = true;
				}
				
				results.emplace_back(temp_result);
			}
		}
		
		// concatenate the results using ConcatenateEidosValues()
		EidosValue_SP result = ConcatenateEidosValues(results.data(), (int)results.size(), true);
		
		return result;
	}
}

void EidosValue_Object_vector::SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate(nullptr);
	
	signature->CheckAssignedValue(p_value);
	
	// We have to check the count ourselves; the signature does not do that for us
	int p_value_count = p_value.Count();
	
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
			EidosValue_SP temp_rvalue = p_value.GetValueAtIndex(value_idx, nullptr);
			
			values_[value_idx]->SetProperty(p_property_id, *temp_rvalue);
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << eidos_terminate(nullptr);
}

EidosValue_SP EidosValue_Object_vector::ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosMethodSignature *p_method_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	// This is an instance method, so it gets dispatched to all of our elements
	auto values_size = values_.size();
	
	if (values_size == 0)
	{
		// With a zero-length vector, the return is a zero-length vector of the type specified by the method signature.  This
		// allows code to proceed without errors without having to check for the zero-length case, by simply propagating the
		// zero-length-ness forward while preserving the expected type for the expression.  If multiple return types are possible,
		// we return a zero-length vector for the simplest type supported.
		EidosValueMask sig_mask = (p_method_signature->return_mask_ & kEidosValueMaskFlagStrip);
		
		if (sig_mask & kEidosValueMaskNULL) return gStaticEidosValueNULL;	// we assume visible NULL is the NULL desired, to avoid masking errors
		if (sig_mask & kEidosValueMaskLogical) return gStaticEidosValue_Logical_ZeroVec;
		if (sig_mask & kEidosValueMaskInt) return gStaticEidosValue_Integer_ZeroVec;
		if (sig_mask & kEidosValueMaskFloat) return gStaticEidosValue_Float_ZeroVec;
		if (sig_mask & kEidosValueMaskString) return gStaticEidosValue_String_ZeroVec;
		if (sig_mask & kEidosValueMaskObject)
		{
			const EidosObjectClass *return_class = p_method_signature->return_class_;
			
			if (return_class)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(return_class));
			else
				return gStaticEidosValue_Object_ZeroVec;
		}
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ExecuteMethodCall): method " << StringForEidosGlobalStringID(p_method_id) << " does not specify a return type, and thus cannot be called on a zero-length vector." << eidos_terminate(nullptr);
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		EidosObjectElement *value = values_[0];
		EidosValue_SP result = value->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
		
		p_method_signature->CheckReturn(*result);
		return result;
	}
	else
	{
		// call the method on all elements and collect the results
		vector<EidosValue_SP> results;
		
		if (values_size < 10)
		{
			// with small objects, we check every value
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
				
				p_method_signature->CheckReturn(*temp_result);
				results.emplace_back(temp_result);
			}
		}
		else
		{
			// with large objects, we just spot-check the first value, for speed
			bool checked_multivalued = false;
			
			for (auto value : values_)
			{
				EidosValue_SP temp_result = value->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
				
				if (!checked_multivalued)
				{
					p_method_signature->CheckReturn(*temp_result);
					checked_multivalued = true;
				}
				
				results.emplace_back(temp_result);
			}
		}
		
		// concatenate the results using ConcatenateEidosValues()
		EidosValue_SP result = ConcatenateEidosValues(results.data(), (int)results.size(), true);
		
		return result;
	}
}


// EidosValue_Object_singleton
#pragma mark EidosValue_Object_singleton

EidosValue_Object_singleton::EidosValue_Object_singleton(EidosObjectElement *p_element1, const EidosObjectClass *p_class) : value_(p_element1), EidosValue_Object(true, p_class)
{
	// we want to allow nullptr as a momentary placeholder, although in general a value should exist
	if (p_element1)
	{
#if DEBUG
		if (p_element1->Class() != class_)
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::EidosValue_Object_vector): class mismatch." << eidos_terminate(nullptr);
#endif
		
		p_element1->Retain();
	}
}

EidosValue_Object_singleton::~EidosValue_Object_singleton(void)
{
	value_->Release();
}

int EidosValue_Object_singleton::Count_Virtual(void) const
{
	return 1;
}

void EidosValue_Object_singleton::Print(std::ostream &p_ostream) const
{
	p_ostream << *value_;
}

EidosObjectElement *EidosValue_Object_singleton::ObjectElementAtIndex(int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::ObjectElementAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return value_;
}

EidosValue_SP EidosValue_Object_singleton::GetValueAtIndex(const int p_idx, EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << eidos_terminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(value_, Class()));
}

void EidosValue_Object_singleton::SetValue(EidosObjectElement *p_element)
{
#if DEBUG
	if (p_element->Class() != class_)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::SetValue): class mismatch." << eidos_terminate(nullptr);
#endif
	
	p_element->Retain();
	if (value_) value_->Release();		// we might have been initialized with nullptr with the aim of setting a value here
	value_ = p_element;
}

EidosValue_SP EidosValue_Object_singleton::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(value_, Class()));
}

EidosValue_SP EidosValue_Object_singleton::VectorBasedCopy(void) const
{
	EidosValue_Object_vector_SP new_vec = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(Class()));
	
	new_vec->PushObjectElement(value_);
	
	return new_vec;
}

void EidosValue_Object_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::SetValueAtIndex): (internal error) EidosValue_Object_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

void EidosValue_Object_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Object_singleton is not modifiable." << eidos_terminate(p_blame_token);
}

EidosValue_SP EidosValue_Object_singleton::GetPropertyOfElements(EidosGlobalStringID p_property_id) const
{
	const EidosPropertySignature *signature = value_->Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::GetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate(nullptr);
	
	EidosValue_SP result = value_->GetProperty(p_property_id);
	
	signature->CheckResultValue(*result);
	
	return result;
}

void EidosValue_Object_singleton::SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	const EidosPropertySignature *signature = value_->Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::SetPropertyOfElements): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << eidos_terminate(nullptr);
	
	signature->CheckAssignedValue(p_value);
	
	// We have to check the count ourselves; the signature does not do that for us
	if (p_value.Count() == 1)
	{
		value_->SetProperty(p_property_id, p_value);
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << eidos_terminate(nullptr);
}

EidosValue_SP EidosValue_Object_singleton::ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosMethodSignature *p_method_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	// This is an instance method, so it gets dispatched to our element
	EidosValue_SP result = value_->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	
	p_method_signature->CheckReturn(*result);
	return result;
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

EidosValue_SP EidosObjectElement::GetProperty(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty for " << Class()->ElementType() << "): (internal error) attempt to get a value for property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate(nullptr);
}

eidos_logical_t EidosObjectElement::GetProperty_Accelerated_Logical(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_Logical for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate(nullptr);
}

int64_t EidosObjectElement::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_Int for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate(nullptr);
}

double EidosObjectElement::GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_Float for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate(nullptr);
}

std::string EidosObjectElement::GetProperty_Accelerated_String(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_String for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate(nullptr);
}

EidosObjectElement *EidosObjectElement::GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_ObjectElement for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate(nullptr);
}

void EidosObjectElement::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
#pragma unused(p_value)
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty): property " << StringForEidosGlobalStringID(p_property_id) << " is not defined for object element type " << Class()->ElementType() << "." << eidos_terminate(nullptr);
	
	bool readonly = signature->read_only_;
	
	// Check whether setting a constant was attempted; we can do this on behalf of all our subclasses
	if (readonly)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty for " << Class()->ElementType() << "): attempt to set a new value for read-only property " << StringForEidosGlobalStringID(p_property_id) << "." << eidos_terminate(nullptr);
	else
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty for " << Class()->ElementType() << "): (internal error) setting a new value for read-write property " << StringForEidosGlobalStringID(p_property_id) << " was not handled by subclass." << eidos_terminate(nullptr);
}

EidosValue_SP EidosObjectElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_argument_count, p_interpreter)
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			
			//
			//	*********************	– (void)str(void)
			//
#pragma mark -str()
			
		case gEidosID_str:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			
			output_stream << Class()->ElementType() << ":" << endl;
			
			const std::vector<const EidosPropertySignature *> *properties = Class()->Properties();
			
			for (auto property_sig : *properties)
			{
				const std::string &property_name = property_sig->property_name_;
				EidosGlobalStringID property_id = property_sig->property_id_;
				EidosValue_SP property_value = GetProperty(property_id);
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
					EidosValue_SP first_value = property_value->GetValueAtIndex(0, nullptr);
					EidosValue_SP second_value = property_value->GetValueAtIndex(1, nullptr);
					
					output_stream << *first_value << " " << *second_value << " ... (" << property_count << " values)" << endl;
				}
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
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << eidos_terminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): unrecognized method name " << method_name << "." << eidos_terminate(nullptr);
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
		EIDOS_TERMINATION << "ERROR (EidosObjectClass::SignatureForPropertyOrRaise for " << ElementType() << "): (internal error) missing property " << StringForEidosGlobalStringID(p_property_id) << "." << eidos_terminate(nullptr);
	
	return signature;
}

const std::vector<const EidosMethodSignature *> *EidosObjectClass::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>;
		
		// keep alphabetical order here
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID_method));
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID_property));
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID_size));
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID_str));
	}
	
	return methods;
}

const EidosMethodSignature *EidosObjectClass::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosClassMethodSignature *methodSig = nullptr;
	static EidosClassMethodSignature *propertySig = nullptr;
	static EidosClassMethodSignature *sizeSig = nullptr;
	static EidosInstanceMethodSignature *strSig = nullptr;
	
	if (!methodSig)
	{
		methodSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_method, kEidosValueMaskNULL))->AddString_OSN("methodName", gStaticEidosValueNULL);
		propertySig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_property, kEidosValueMaskNULL))->AddString_OSN("propertyName", gStaticEidosValueNULL);
		sizeSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_size, kEidosValueMaskInt | kEidosValueMaskSingleton));
		strSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_str, kEidosValueMaskNULL));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID_method:	return methodSig;
		case gEidosID_property:	return propertySig;
		case gEidosID_size:		return sizeSig;
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
		EIDOS_TERMINATION << "ERROR (EidosObjectClass::SignatureForMethodOrRaise for " << ElementType() << "): (internal error) missing method " << StringForEidosGlobalStringID(p_method_id) << "." << eidos_terminate(nullptr);
	
	return signature;
}

EidosValue_SP EidosObjectClass::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused(p_argument_count)
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	+ (void)property([Ns$ propertyName = NULL])
			//
#pragma mark +property()
			
		case gEidosID_property:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
			const std::vector<const EidosPropertySignature *> *properties = Properties();
			bool signature_found = false;
			
			for (auto property_sig : *properties)
			{
				const std::string &property_name = property_sig->property_name_;
				
				if (has_match_string && (property_name.compare(match_string) != 0))
					continue;
				
				output_stream << property_name << " " << property_sig->PropertySymbol() << " (" << StringForEidosValueMask(property_sig->value_mask_, property_sig->value_class_, "", nullptr) << ")" << endl;
				
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No property found for \"" << match_string << "\"." << endl;
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	+ (void)method([Ns$ methodName = NULL])
			//
#pragma mark +method()
			
		case gEidosID_method:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
			string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
			const std::vector<const EidosMethodSignature *> *methods = Methods();
			bool signature_found = false;
			
			// Output class methods first
			for (auto method_sig : *methods)
			{
				if (!method_sig->is_class_method)
					continue;
				
				const std::string &method_name = method_sig->call_name_;
				
				if (has_match_string && (method_name.compare(match_string) != 0))
					continue;
				
				output_stream << *method_sig << endl;
				signature_found = true;
			}
			
			// Then instance methods
			for (auto method_sig : *methods)
			{
				if (method_sig->is_class_method)
					continue;
				
				const std::string &method_name = method_sig->call_name_;
				
				if (has_match_string && (method_name.compare(match_string) != 0))
					continue;
				
				output_stream << *method_sig << endl;
				signature_found = true;
			}
			
			if (has_match_string && !signature_found)
				output_stream << "No method signature found for \"" << match_string << "\"." << endl;
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	+ (integer$)size(void)
			//
#pragma mark +size()
			
		case gEidosID_size:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(p_target->Count()));
		}
			
			// all others, including gID_none
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<const EidosMethodSignature *> *methods = Methods();
			const std::string &method_name = StringForEidosGlobalStringID(p_method_id);
			
			for (auto method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << eidos_terminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): unrecognized method name " << method_name << "." << eidos_terminate(nullptr);
		}
	}
}







































































