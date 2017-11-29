//
//  eidos_value.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2017 Philipp Messer.  All rights reserved.
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
#include <cmath>
#include "errno.h"
#include "string.h"


// The global object pool for EidosValue, initialized in Eidos_WarmUp()
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
EidosValue_Float_SP gStaticEidosValue_FloatINF;
EidosValue_Float_SP gStaticEidosValue_FloatNAN;

EidosValue_String_SP gStaticEidosValue_StringEmpty;
EidosValue_String_SP gStaticEidosValue_StringSpace;
EidosValue_String_SP gStaticEidosValue_StringAsterisk;
EidosValue_String_SP gStaticEidosValue_StringDoubleAsterisk;

EidosObjectClass *gEidos_UndefinedClassObject = new EidosObjectClass();


std::string StringForEidosValueType(const EidosValueType p_type)
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
		if (p_default && (p_default != gStaticEidosValueNULLInvisible.get()))
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


#pragma mark -
#pragma mark Comparing EidosValues
#pragma mark -

// returns -1 if value1[index1] < value2[index2], 0 if ==, 1 if >, with full type promotion
int CompareEidosValues(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token)
{
	EidosValueType type1 = p_value1.Type();
	EidosValueType type2 = p_value2.Type();
	
	if ((type1 == EidosValueType::kValueNULL) || (type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) comparison with NULL is illegal." << EidosTerminate(p_blame_token);
	
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
		std::string string1 = p_value1.StringAtIndex(p_index1, p_blame_token);
		std::string string2 = p_value2.StringAtIndex(p_index2, p_blame_token);
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
	EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) comparison involving type " << type1 << " and type " << type2 << " is undefined." << EidosTerminate(p_blame_token);
	return 0;
}

int CompareEidosValues_Object(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token)
{
	EidosObjectElement *element1 = p_value1.ObjectElementAtIndex(p_index1, p_blame_token);
	EidosObjectElement *element2 = p_value2.ObjectElementAtIndex(p_index2, p_blame_token);
	
	return (element1 == element2) ? 0 : -1;		// no relative ordering, just equality comparison; enforced in script_interpreter
}

int CompareEidosValues_String(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token)
{
	std::string string1 = p_value1.StringAtIndex(p_index1, p_blame_token);
	std::string string2 = p_value2.StringAtIndex(p_index2, p_blame_token);
	int compare_result = string1.compare(string2);		// not guaranteed to be -1 / 0 / +1, just negative / 0 / positive
	
	return (compare_result < 0) ? -1 : ((compare_result > 0) ? 1 : 0);
}

int CompareEidosValues_Float(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token)
{
	double float1 = p_value1.FloatAtIndex(p_index1, p_blame_token);
	double float2 = p_value2.FloatAtIndex(p_index2, p_blame_token);
	
	return (float1 < float2) ? -1 : ((float1 > float2) ? 1 : 0);
}

int CompareEidosValues_Int(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token)
{
	int64_t int1 = p_value1.IntAtIndex(p_index1, p_blame_token);
	int64_t int2 = p_value2.IntAtIndex(p_index2, p_blame_token);
	
	return (int1 < int2) ? -1 : ((int1 > int2) ? 1 : 0);
}

int CompareEidosValues_Logical(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token)
{
	eidos_logical_t logical1 = p_value1.LogicalAtIndex(p_index1, p_blame_token);
	eidos_logical_t logical2 = p_value2.LogicalAtIndex(p_index2, p_blame_token);
	
	return (logical1 < logical2) ? -1 : ((logical1 > logical2) ? 1 : 0);
}

EidosCompareFunctionPtr Eidos_GetCompareFunctionForTypes(EidosValueType p_type1, EidosValueType p_type2, const EidosToken *p_blame_token)
{
	if ((p_type1 == EidosValueType::kValueNULL) || (p_type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Eidos_GetCompareFunctionForTypes): (internal error) comparison with NULL is illegal." << EidosTerminate(p_blame_token);
	
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
	EIDOS_TERMINATION << "ERROR (Eidos_GetCompareFunctionForTypes): (internal error) comparison involving type " << p_type1 << " and type " << p_type2 << " is undefined." << EidosTerminate(p_blame_token);
	return 0;
}


//
//	EidosValue
//
#pragma mark -
#pragma mark EidosValue
#pragma mark -

#ifdef EIDOS_TRACK_VALUE_ALLOCATION
int EidosValue::valueTrackingCount;
std::vector<EidosValue *> EidosValue::valueTrackingVector;
#endif

EidosValue::EidosValue(EidosValueType p_value_type, bool p_singleton) : intrusive_ref_count_(0), invisible_(false), cached_type_(p_value_type), is_singleton_(p_singleton)
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

eidos_logical_t EidosValue::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::LogicalAtIndex): operand type " << this->Type() << " cannot be converted to type logical." << EidosTerminate(p_blame_token);
}

std::string EidosValue::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::StringAtIndex): operand type " << this->Type() << " cannot be converted to type string." << EidosTerminate(p_blame_token);
}

int64_t EidosValue::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::IntAtIndex): operand type " << this->Type() << " cannot be converted to type integer." << EidosTerminate(p_blame_token);
}

double EidosValue::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::FloatAtIndex): operand type " << this->Type() << " cannot be converted to type float." << EidosTerminate(p_blame_token);
}

EidosObjectElement *EidosValue::ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::ObjectElementAtIndex): operand type " << this->Type() << " cannot be converted to type object." << EidosTerminate(p_blame_token);
}

void EidosValue::RaiseForUnimplementedVectorCall(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForUnimplementedVectorCall): (internal error) direct vector access attempted on an EidosValue type that does not support it." << EidosTerminate(nullptr);
}

void EidosValue::RaiseForCapacityViolation(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForCapacityViolation): (internal error) access violated the current capacity of an EidosValue." << EidosTerminate(nullptr);
}

void EidosValue::RaiseForRangeViolation(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForCapacityViolation): (internal error) access violated the current size of an EidosValue." << EidosTerminate(nullptr);
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
#pragma mark -

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

EidosValue_SP EidosValue_NULL::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx, p_blame_token)
	return gStaticEidosValueNULL;
}

void EidosValue_NULL::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_NULL::SetValueAtIndex): operand type " << this->Type() << " does not support setting values with the subscript operator ('[]')." << EidosTerminate(p_blame_token);
}

EidosValue_SP EidosValue_NULL::CopyValues(void) const
{
	return gStaticEidosValueNULL;
}

EidosValue_SP EidosValue_NULL::NewMatchingType(void) const
{
	return gStaticEidosValueNULL;
}

void EidosValue_NULL::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx)
	if (p_source_script_value.Type() == EidosValueType::kValueNULL)
		;	// NULL doesn't have values or indices, so this is a no-op
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_NULL::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
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
#pragma mark -

EidosValue_Logical::EidosValue_Logical(void) : EidosValue(EidosValueType::kValueLogical, false)
{
}

EidosValue_Logical::EidosValue_Logical(const std::vector<eidos_logical_t> &p_logicalvec) : EidosValue(EidosValueType::kValueLogical, false)
{
	size_t count = p_logicalvec.size();
	const eidos_logical_t *values = p_logicalvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_logical_no_check(values[index], index);
}

EidosValue_Logical::EidosValue_Logical(eidos_logical_t p_logical1) : EidosValue(EidosValueType::kValueLogical, false)	// protected
{
	push_logical(p_logical1);
}

EidosValue_Logical::EidosValue_Logical(std::initializer_list<eidos_logical_t> p_init_list) : EidosValue(EidosValueType::kValueLogical, false)
{
	reserve(p_init_list.size());
	
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		push_logical_no_check(*init_item);
}

EidosValue_Logical::EidosValue_Logical(const eidos_logical_t *p_values, size_t p_count) : EidosValue(EidosValueType::kValueLogical, false)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_logical_no_check(p_values[index], index);
}

EidosValue_Logical::~EidosValue_Logical(void)
{
	free(values_);
}

const std::string &EidosValue_Logical::ElementType(void) const
{
	return gEidosStr_logical;
}

int EidosValue_Logical::Count_Virtual(void) const
{
	return (int)count_;
}

void EidosValue_Logical::Print(std::ostream &p_ostream) const
{
	if (size() == 0)
	{
		p_ostream << "logical(0)";
	}
	else
	{
		bool first = true;
		
		for (size_t index = 0; index < count_; ++index)
		{
			eidos_logical_t value = values_[index];
			
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << (value ? gEidosStr_T : gEidosStr_F);
		}
	}
}

eidos_logical_t EidosValue_Logical::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::LogicalAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

std::string EidosValue_Logical::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::StringAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? gEidosStr_T : gEidosStr_F);
}

int64_t EidosValue_Logical::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::IntAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? 1 : 0);
}

double EidosValue_Logical::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::FloatAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? 1.0 : 0.0);
}

EidosValue_SP EidosValue_Logical::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

void EidosValue_Logical::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::SetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	values_[p_idx] = p_value.LogicalAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_Logical::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(values_, count_));
}

EidosValue_SP EidosValue_Logical::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
}

void EidosValue_Logical::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueLogical)
		push_logical(p_source_script_value.LogicalAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_Logical::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_, values_ + count_);
	else
		std::sort(values_, values_ + count_, std::greater<eidos_logical_t>());
}

EidosValue_Logical *EidosValue_Logical::reserve(size_t p_reserved_size)
{
	if (p_reserved_size > capacity_)
	{
		values_ = (eidos_logical_t *)realloc(values_, p_reserved_size * sizeof(eidos_logical_t));
		capacity_ = p_reserved_size;
	}
	
	return this;
}

EidosValue_Logical *EidosValue_Logical::resize_no_initialize(size_t p_new_size)
{
	reserve(p_new_size);	// might set a capacity greater than p_new_size; no guarantees
	count_ = p_new_size;	// regardless of the capacity set, set the size to exactly p_new_size
	
	return this;
}

void EidosValue_Logical::expand(void)
{
	if (capacity_ == 0)
		reserve(16);		// if no reserve() call was made, start out with a bit of room
	else
		reserve(capacity_ << 1);
}

void EidosValue_Logical::erase_index(size_t p_index)
{
	if (p_index >= count_)
		RaiseForRangeViolation();
	
	if (p_index == count_ - 1)
		--count_;
	else
	{
		eidos_logical_t *element_ptr = values_ + p_index;
		eidos_logical_t *next_element_ptr = values_ + p_index + 1;
		eidos_logical_t *past_end_element_ptr = values_ + count_;
		size_t element_copy_count = past_end_element_ptr - next_element_ptr;
		
		memmove(element_ptr, next_element_ptr, element_copy_count * sizeof(eidos_logical_t));
		
		--count_;
	}
}

// EidosValue_Logical_const
#pragma mark EidosValue_Logical_const

EidosValue_Logical_const::EidosValue_Logical_const(eidos_logical_t p_bool1) : EidosValue_Logical(p_bool1)
{
	is_singleton_ = true;	// because EidosValue_Logical has a different class structure from the other types, this has to be patched after construction; no harm done...
}

EidosValue_Logical_const::~EidosValue_Logical_const(void)
{
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::~EidosValue_Logical_const): (internal error) global constant deallocated." << EidosTerminate(nullptr);
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
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(values_, count_));	// same as EidosValue_Logical::, but let's not rely on that
}

void EidosValue_Logical_const::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::SetValueAtIndex): (internal error) EidosValue_Logical_const is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_Logical_const::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Logical_const is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_Logical_const::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::Sort): (internal error) EidosValue_Logical_const is not modifiable." << EidosTerminate(nullptr);
}


//
//	EidosValue_String
//
#pragma mark -
#pragma mark EidosValue_String
#pragma mark -

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
		
		for (const std::string &value : values_)
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

eidos_logical_t EidosValue_String_vector::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::LogicalAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx].length() > 0);
}

std::string EidosValue_String_vector::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::StringAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

int64_t EidosValue_String_vector::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::IntAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	// We don't use IntForString because an integer has been specifically requested, so even if the string appears to contain a
	// float value we want to force it into being an int; the way to do that is to use FloatForString and then convert to int64_t
	double converted_value = EidosInterpreter::FloatForString(values_[p_idx], p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::IntAtIndex): \"" << values_[p_idx] << "\" could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
	
	return static_cast<int64_t>(converted_value);
}

double EidosValue_String_vector::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::FloatAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosInterpreter::FloatForString(values_[p_idx], p_blame_token);
}

EidosValue_SP EidosValue_String_vector::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(values_[p_idx]));
}

void EidosValue_String_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	values_[p_idx] = p_value.StringAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_String_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector(values_));
}

void EidosValue_String_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueString)
		values_.emplace_back(p_source_script_value.StringAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
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

eidos_logical_t EidosValue_String_singleton::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::LogicalAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (value_.length() > 0);
}

std::string EidosValue_String_singleton::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::StringAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return value_;
}

int64_t EidosValue_String_singleton::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::IntAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	// We don't use IntForString because an integer has been specifically requested, so even if the string appears to contain a
	// float value we want to force it into being an int; the way to do that is to use FloatForString and then convert to int64_t
	double converted_value = EidosInterpreter::FloatForString(value_, p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((converted_value < INT64_MIN) || (converted_value >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_vector::IntAtIndex): \"" << value_ << "\" could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
	
	return static_cast<int64_t>(converted_value);
}

double EidosValue_String_singleton::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::FloatAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosInterpreter::FloatForString(value_, p_blame_token);
}

EidosValue_SP EidosValue_String_singleton::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
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

void EidosValue_String_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::SetValueAtIndex): (internal error) EidosValue_String_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_String_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_String_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_String_singleton::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::Sort): (internal error) EidosValue_String_singleton is not modifiable." << EidosTerminate(nullptr);
}


//
//	EidosValue_Int
//
#pragma mark -
#pragma mark EidosValue_Int
#pragma mark -

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
	size_t count = p_intvec.size();
	const int16_t *values = p_intvec.data();
	
	resize_no_initialize(p_intvec.size());
	
	for (size_t index = 0; index < count; ++index)
		set_int_no_check(values[index], index);
}

EidosValue_Int_vector::EidosValue_Int_vector(const std::vector<int32_t> &p_intvec) : EidosValue_Int(false)
{
	size_t count = p_intvec.size();
	const int32_t *values = p_intvec.data();
	
	resize_no_initialize(p_intvec.size());
	
	for (size_t index = 0; index < count; ++index)
		set_int_no_check(values[index], index);
}

EidosValue_Int_vector::EidosValue_Int_vector(const std::vector<int64_t> &p_intvec) : EidosValue_Int(false)
{
	size_t count = p_intvec.size();
	const int64_t *values = p_intvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_int_no_check(values[index], index);
}

EidosValue_Int_vector::EidosValue_Int_vector(std::initializer_list<int64_t> p_init_list) : EidosValue_Int(false)
{
	reserve(p_init_list.size());
	
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		push_int_no_check(*init_item);
}

EidosValue_Int_vector::EidosValue_Int_vector(const int64_t *p_values, size_t p_count) : EidosValue_Int(false)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_int_no_check(p_values[index], index);
}

EidosValue_Int_vector::~EidosValue_Int_vector(void)
{
	free(values_);
}

int EidosValue_Int_vector::Count_Virtual(void) const
{
	return (int)count_;
}

void EidosValue_Int_vector::Print(std::ostream &p_ostream) const
{
	if (size() == 0)
	{
		p_ostream << "integer(0)";
	}
	else
	{
		bool first = true;
		
		for (size_t index = 0; index < count_; ++index)
		{
			int64_t value = values_[index];
			
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << value;
		}
	}
}

eidos_logical_t EidosValue_Int_vector::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::LogicalAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] == 0 ? false : true);
}

std::string EidosValue_Int_vector::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::StringAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	// with C++11, could use std::to_string(values_[p_idx])
	std::ostringstream ss;
	
	ss << values_[p_idx];
	
	return ss.str();
}

int64_t EidosValue_Int_vector::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::IntAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

double EidosValue_Int_vector::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::FloatAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Int_vector::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(values_[p_idx]));
}

void EidosValue_Int_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	values_[p_idx] = p_value.IntAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_Int_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(values_, count_));
}

void EidosValue_Int_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueInt)
		push_int(p_source_script_value.IntAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_vector::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_Int_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_, values_ + count_);
	else
		std::sort(values_, values_ + count_, std::greater<int64_t>());
}

EidosValue_Int_vector *EidosValue_Int_vector::reserve(size_t p_reserved_size)
{
	if (p_reserved_size > capacity_)
	{
		values_ = (int64_t *)realloc(values_, p_reserved_size * sizeof(int64_t));
		capacity_ = p_reserved_size;
	}
	
	return this;
}

EidosValue_Int_vector *EidosValue_Int_vector::resize_no_initialize(size_t p_new_size)
{
	reserve(p_new_size);	// might set a capacity greater than p_new_size; no guarantees
	count_ = p_new_size;	// regardless of the capacity set, set the size to exactly p_new_size
	
	return this;
}

void EidosValue_Int_vector::expand(void)
{
	if (capacity_ == 0)
		reserve(16);		// if no reserve() call was made, start out with a bit of room
	else
		reserve(capacity_ << 1);
}

void EidosValue_Int_vector::erase_index(size_t p_index)
{
	if (p_index >= count_)
		RaiseForRangeViolation();
	
	if (p_index == count_ - 1)
		--count_;
	else
	{
		int64_t *element_ptr = values_ + p_index;
		int64_t *next_element_ptr = values_ + p_index + 1;
		int64_t *past_end_element_ptr = values_ + count_;
		size_t element_copy_count = past_end_element_ptr - next_element_ptr;
		
		memmove(element_ptr, next_element_ptr, element_copy_count * sizeof(int64_t));
		
		--count_;
	}
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

eidos_logical_t EidosValue_Int_singleton::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::LogicalAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Int_singleton::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::StringAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	// with C++11, could use std::to_string(value_)
	std::ostringstream ss;
	
	ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Int_singleton::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::IntAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return value_;
}

double EidosValue_Int_singleton::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::FloatAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return value_;
}

EidosValue_SP EidosValue_Int_singleton::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(value_));
}

EidosValue_SP EidosValue_Int_singleton::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(value_));
}

EidosValue_SP EidosValue_Int_singleton::VectorBasedCopy(void) const
{
	// We intentionally don't reserve a size of 1 here, on the assumption that further values are likely to be added
	EidosValue_Int_vector_SP new_vec = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
	
	new_vec->push_int(value_);
	
	return new_vec;
}

void EidosValue_Int_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::SetValueAtIndex): (internal error) EidosValue_Float_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_Int_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Float_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_Int_singleton::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Int_singleton::Sort): (internal error) EidosValue_Float_singleton is not modifiable." << EidosTerminate(nullptr);
}


//
//	EidosValue_Float
//
#pragma mark -
#pragma mark EidosValue_Float
#pragma mark -

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
	size_t count = p_doublevec.size();
	const double *values = p_doublevec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_float_no_check(values[index], index);
}

EidosValue_Float_vector::EidosValue_Float_vector(std::initializer_list<double> p_init_list) : EidosValue_Float(false)
{
	reserve(p_init_list.size());
	
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		push_float_no_check(*init_item);
}

EidosValue_Float_vector::EidosValue_Float_vector(const double *p_values, size_t p_count) : EidosValue_Float(false)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_float_no_check(p_values[index], index);
}

EidosValue_Float_vector::~EidosValue_Float_vector(void)
{
	free(values_);
}

int EidosValue_Float_vector::Count_Virtual(void) const
{
	return (int)count_;
}

void EidosValue_Float_vector::Print(std::ostream &p_ostream) const
{
	if (size() == 0)
	{
		p_ostream << "float(0)";
	}
	else
	{
		bool first = true;
		
		for (size_t index = 0; index < count_; ++index)
		{
			double value = values_[index];
			
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			// Customize our output a bit to look like Eidos, not C++
			if (std::isinf(value))
			{
				if (std::signbit(value))
					p_ostream << gEidosStr_MINUS_INF;
				else
					p_ostream << gEidosStr_INF;
			}
			else if (std::isnan(value))
				p_ostream << gEidosStr_NAN;
			else
				p_ostream << value;
		}
	}
}

eidos_logical_t EidosValue_Float_vector::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::LogicalAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	double value = values_[p_idx];
	
	if (std::isnan(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::LogicalAtIndex): NAN cannot be converted to logical type." << EidosTerminate(p_blame_token);
	
	return (value == 0 ? false : true);
}

std::string EidosValue_Float_vector::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::StringAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	// with C++11, could use std::to_string(values_[p_idx])
	std::ostringstream ss;
	double value = values_[p_idx];
	
	// Customize our output a bit to look like Eidos, not C++
	if (std::isinf(value))
	{
		if (std::signbit(value))
			ss << gEidosStr_MINUS_INF;
		else
			ss << gEidosStr_INF;
	}
	else if (std::isnan(value))
		ss << gEidosStr_NAN;
	else
		ss << value;
	
	return ss.str();
}

int64_t EidosValue_Float_vector::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	double value = values_[p_idx];
	
	if (std::isnan(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): NAN cannot be converted to integer type." << EidosTerminate(p_blame_token);
	if (std::isinf(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): INF cannot be converted to integer type." << EidosTerminate(p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((value < INT64_MIN) || (value >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::IntAtIndex): float value " << value << " is too large to be converted to integer type." << EidosTerminate(p_blame_token);
	
	return static_cast<int64_t>(value);
}

double EidosValue_Float_vector::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::FloatAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Float_vector::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(values_[p_idx]));
}

void EidosValue_Float_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	values_[p_idx] = p_value.FloatAtIndex(0, p_blame_token);
}

EidosValue_SP EidosValue_Float_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(values_, count_));
}

void EidosValue_Float_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueFloat)
		push_float(p_source_script_value.FloatAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_vector::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_Float_vector::Sort(bool p_ascending)
{
	if (p_ascending)
		std::sort(values_, values_ + count_);
	else
		std::sort(values_, values_ + count_, std::greater<double>());
}

EidosValue_Float_vector *EidosValue_Float_vector::reserve(size_t p_reserved_size)
{
	if (p_reserved_size > capacity_)
	{
		values_ = (double *)realloc(values_, p_reserved_size * sizeof(double));
		capacity_ = p_reserved_size;
	}
	
	return this;
}

EidosValue_Float_vector *EidosValue_Float_vector::resize_no_initialize(size_t p_new_size)
{
	reserve(p_new_size);	// might set a capacity greater than p_new_size; no guarantees
	count_ = p_new_size;	// regardless of the capacity set, set the size to exactly p_new_size
	
	return this;
}

void EidosValue_Float_vector::expand(void)
{
	if (capacity_ == 0)
		reserve(16);		// if no reserve() call was made, start out with a bit of room
	else
		reserve(capacity_ << 1);
}

void EidosValue_Float_vector::erase_index(size_t p_index)
{
	if (p_index >= count_)
		RaiseForRangeViolation();
	
	if (p_index == count_ - 1)
		--count_;
	else
	{
		double *element_ptr = values_ + p_index;
		double *next_element_ptr = values_ + p_index + 1;
		double *past_end_element_ptr = values_ + count_;
		size_t element_copy_count = past_end_element_ptr - next_element_ptr;
		
		memmove(element_ptr, next_element_ptr, element_copy_count * sizeof(double));
		
		--count_;
	}
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
	if (std::isinf(value_))
	{
		if (std::signbit(value_))
			p_ostream << gEidosStr_MINUS_INF;
		else
			p_ostream << gEidosStr_INF;
	}
	else if (std::isnan(value_))
		p_ostream << gEidosStr_NAN;
	else
		p_ostream << value_;
}

eidos_logical_t EidosValue_Float_singleton::LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::LogicalAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	if (std::isnan(value_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::LogicalAtIndex): NAN cannot be converted to logical type." << EidosTerminate(p_blame_token);
	
	return (value_ == 0 ? false : true);
}

std::string EidosValue_Float_singleton::StringAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::StringAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	// with C++11, could use std::to_string(value_)
	std::ostringstream ss;
	
	// Customize our output a bit to look like Eidos, not C++
	if (std::isinf(value_))
	{
		if (std::signbit(value_))
			ss << gEidosStr_MINUS_INF;
		else
			ss << gEidosStr_INF;
	}
	else if (std::isnan(value_))
		ss << gEidosStr_NAN;
	else
		ss << value_;
	
	return ss.str();
}

int64_t EidosValue_Float_singleton::IntAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	if (std::isnan(value_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): NAN cannot be converted to integer type." << EidosTerminate(p_blame_token);
	if (std::isinf(value_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): INF cannot be converted to integer type." << EidosTerminate(p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((value_ < INT64_MIN) || (value_ >= INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::IntAtIndex): float value " << value_ << " is too large to be converted to integer type." << EidosTerminate(p_blame_token);
	
	return static_cast<int64_t>(value_);
}

double EidosValue_Float_singleton::FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::FloatAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return value_;
}

EidosValue_SP EidosValue_Float_singleton::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(value_));
}

EidosValue_SP EidosValue_Float_singleton::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(value_));
}

EidosValue_SP EidosValue_Float_singleton::VectorBasedCopy(void) const
{
	// We intentionally don't reserve a size of 1 here, on the assumption that further values are likely to be added
	EidosValue_Float_vector_SP new_vec = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
	
	new_vec->push_float(value_);
	
	return new_vec;
}

void EidosValue_Float_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::SetValueAtIndex): (internal error) EidosValue_Float_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_Float_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Float_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_Float_singleton::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Float_singleton::Sort): (internal error) EidosValue_Float_singleton is not modifiable." << EidosTerminate(nullptr);
}


//
//	EidosValue_Object
//
#pragma mark -
#pragma mark EidosValue_Object
#pragma mark -

// See comment on EidosValue_Object::EidosValue_Object() below
std::vector<EidosValue_Object *> gEidosValue_Object_Mutation_Registry;

EidosValue_Object::EidosValue_Object(bool p_singleton, const EidosObjectClass *p_class) : EidosValue(EidosValueType::kValueObject, p_singleton), class_(p_class)
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
, class_needs_retain_release_(p_class == gEidos_UndefinedClassObject ? true : p_class->NeedsRetainRelease())
#endif
{
	// BCH 7 May 2017: OK, so, this is a hack of breathtaking disgustingness.  Here is the problem.  In SLiM we
	// need to reallocate the block in which all Mutation objects live, which invalidates all their pointers.
	// SLiM can mostly handle that for itself, but there is a big problem when it comes to EidosValue_Object
	// instances, which can contain Mutation * elements that must be updated.  So we need to find every single
	// EidosValue_Object in existence which has a class of Mutation.  To that end, we here register this object
	// in a global list if it is of class Mutation; in ~EidosValue_Object we will deregister it.  Maybe there
	// is some way to do this without pushing the hack down into Eidos, but at the moment I'm not seeing it.
	// On the bright side, this scheme actually seems pretty robust; the only way it fails is if somebody avoids
	// using the constructor or the destructor for EidosValue_Object, I think, which seems unlikely.
	if (&(class_->ElementType()) == &gEidosStr_Mutation)
		gEidosValue_Object_Mutation_Registry.push_back(this);
}

EidosValue_Object::~EidosValue_Object(void)
{
	// See comment on EidosValue_Object::EidosValue_Object() above
	if (&(class_->ElementType()) == &gEidosStr_Mutation)
	{
		auto erase_iter = std::find(gEidosValue_Object_Mutation_Registry.begin(), gEidosValue_Object_Mutation_Registry.end(), this);
		
		if (erase_iter != gEidosValue_Object_Mutation_Registry.end())
			gEidosValue_Object_Mutation_Registry.erase(erase_iter);
		else
			EIDOS_TERMINATION << "ERROR (EidosValue_Object::~EidosValue_Object): (internal error) unregistered EidosValue_Object of class Mutation." << EidosTerminate(nullptr);
	}
}

// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
void EidosValue_Object_vector::PatchPointersByAdding(std::uintptr_t p_pointer_difference)
{
	size_t value_count = size();
	
	for (size_t i = 0; i < value_count; ++i)
	{
		std::uintptr_t old_element_ptr = reinterpret_cast<std::uintptr_t>(values_[i]);
		std::uintptr_t new_element_ptr = old_element_ptr + p_pointer_difference;
		
		values_[i] = reinterpret_cast<EidosObjectElement *>(new_element_ptr);
	}
}

// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
void EidosValue_Object_vector::PatchPointersBySubtracting(std::uintptr_t p_pointer_difference)
{
	size_t value_count = size();
	
	for (size_t i = 0; i < value_count; ++i)
	{
		std::uintptr_t old_element_ptr = reinterpret_cast<std::uintptr_t>(values_[i]);
		std::uintptr_t new_element_ptr = old_element_ptr - p_pointer_difference;
		
		values_[i] = reinterpret_cast<EidosObjectElement *>(new_element_ptr);
	}
}

// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
void EidosValue_Object_singleton::PatchPointersByAdding(std::uintptr_t p_pointer_difference)
{
	std::uintptr_t old_element_ptr = reinterpret_cast<std::uintptr_t>(value_);
	std::uintptr_t new_element_ptr = old_element_ptr + p_pointer_difference;
	
	value_ = reinterpret_cast<EidosObjectElement *>(new_element_ptr);
}

// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
void EidosValue_Object_singleton::PatchPointersBySubtracting(std::uintptr_t p_pointer_difference)
{
	std::uintptr_t old_element_ptr = reinterpret_cast<std::uintptr_t>(value_);
	std::uintptr_t new_element_ptr = old_element_ptr - p_pointer_difference;
	
	value_ = reinterpret_cast<EidosObjectElement *>(new_element_ptr);
}

void EidosValue_Object::RaiseForClassMismatch(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForClassMismatch): the type of an object cannot be changed." << EidosTerminate(nullptr);
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
	EIDOS_TERMINATION << "ERROR (EidosValue_Object::Sort): Sort() is not defined for type object." << EidosTerminate(nullptr);
}


// EidosValue_Object_vector
#pragma mark EidosValue_Object_vector

EidosValue_Object_vector::EidosValue_Object_vector(const EidosValue_Object_vector &p_original) : EidosValue_Object(false, p_original.Class())
{
	size_t count = p_original.size();
	EidosObjectElement * const *values = p_original.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_object_element_no_check(values[index], index);
}

EidosValue_Object_vector::EidosValue_Object_vector(const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
{
}

EidosValue_Object_vector::EidosValue_Object_vector(const std::vector<EidosObjectElement *> &p_elementvec, const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
{
	size_t count = p_elementvec.size();
	EidosObjectElement * const *values = p_elementvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_object_element_no_check(values[index], index);
}

EidosValue_Object_vector::EidosValue_Object_vector(std::initializer_list<EidosObjectElement *> p_init_list, const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
{
	reserve(p_init_list.size());
	
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		push_object_element_no_check(*init_item);
}

EidosValue_Object_vector::~EidosValue_Object_vector(void)
{
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
	{
		for (size_t index = 0; index < count_; ++index)
		{
			EidosObjectElement *value = values_[index];
			
			if (value)
				value->Release();
		}
	}
#endif
	
	free(values_);
}

int EidosValue_Object_vector::Count_Virtual(void) const
{
	return (int)size();
}

void EidosValue_Object_vector::Print(std::ostream &p_ostream) const
{
	if (size() == 0)
	{
		p_ostream << "object()<" << class_->ElementType() << ">";
	}
	else
	{
		bool first = true;
		
		for (size_t index = 0; index < count_; ++index)
		{
			EidosObjectElement *value = values_[index];
			
			if (first)
				first = false;
			else
				p_ostream << ' ';
			
			p_ostream << *value;
		}
	}
}

EidosObjectElement *EidosValue_Object_vector::ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ObjectElementAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Object_vector::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(values_[p_idx], Class()));
}

void EidosValue_Object_vector::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
	if ((p_idx < 0) || (p_idx >= (int)size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	EidosObjectElement *new_value = p_value.ObjectElementAtIndex(0, p_blame_token);
	
	DeclareClassFromElement(new_value);
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
	{
		new_value->Retain();
		if (values_[p_idx])
			values_[p_idx]->Release();
	}
#endif
	
	values_[p_idx] = new_value;
}

EidosValue_SP EidosValue_Object_vector::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(*this));
}

void EidosValue_Object_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueObject)
		push_object_element(p_source_script_value.ObjectElementAtIndex(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
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
	if (size() == 0)
		return;
	
	// figure out what type the property returns
	EidosGlobalStringID property_string_id = Eidos_GlobalStringIDForString(p_property);
	EidosValue_SP first_result = values_[0]->GetProperty(property_string_id);
	EidosValueType property_type = first_result->Type();
	
	// switch on the property type for efficiency
	switch (property_type)
	{
		case EidosValueType::kValueNULL:
		case EidosValueType::kValueObject:
			EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " returned " << property_type << "; a property that evaluates to logical, int, float, or string is required." << EidosTerminate(nullptr);
			break;
			
		case EidosValueType::kValueLogical:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<eidos_logical_t, EidosObjectElement*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObjectElement *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<eidos_logical_t, EidosObjectElement*>(temp_result->LogicalAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsDescending);
			
			// read out our new element vector
			resize_no_initialize(0);
			
			for (auto sorted_pair : sortable_pairs)
				push_object_element_no_check(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueInt:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<int64_t, EidosObjectElement*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObjectElement *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<int64_t, EidosObjectElement*>(temp_result->IntAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsDescending);
			
			// read out our new element vector
			resize_no_initialize(0);
			
			for (auto sorted_pair : sortable_pairs)
				push_object_element_no_check(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueFloat:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<double, EidosObjectElement*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObjectElement *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<double, EidosObjectElement*>(temp_result->FloatAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsDescending);
			
			// read out our new element vector
			resize_no_initialize(0);
			
			for (auto sorted_pair : sortable_pairs)
				push_object_element_no_check(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueString:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<std::string, EidosObjectElement*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObjectElement *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(std::pair<std::string, EidosObjectElement*>(temp_result->StringAtIndex(0, nullptr), value));
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsDescending);
			
			// read out our new element vector
			resize_no_initialize(0);
			
			for (auto sorted_pair : sortable_pairs)
				push_object_element_no_check(sorted_pair.second);
			
			break;
		}
	}
}

EidosValue_SP EidosValue_Object_vector::GetPropertyOfElements(EidosGlobalStringID p_property_id) const
{
	size_t values_size = (size_t)size();
	const EidosPropertySignature *signature = class_->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): property " << Eidos_StringForGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << EidosTerminate(nullptr);
	
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
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): property " << Eidos_StringForGlobalStringID(p_property_id) << " does not specify a value type, and thus cannot be accessed on a zero-length vector." << EidosTerminate(nullptr);
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
	else if (signature->accelerated_get_)
	{
		// Accelerated property access is enabled for this property, so we will switch on the type of the property, and
		// assemble the result value directly from the C++ type values we get from the accelerated access methods...
		EidosValueMask sig_mask = (signature->value_mask_ & kEidosValueMaskFlagStrip);
		
		switch (sig_mask)
		{
			case kEidosValueMaskLogical:
			{
				EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
					logical_result->set_logical_no_check(values_[value_index]->GetProperty_Accelerated_Logical(p_property_id), value_index);
				
				return EidosValue_SP(logical_result);
			}
			case kEidosValueMaskInt:
			{
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
					int_result->set_int_no_check(values_[value_index]->GetProperty_Accelerated_Int(p_property_id), value_index);
				
				return EidosValue_SP(int_result);
			}
			case kEidosValueMaskFloat:
			{
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
					float_result->set_float_no_check(values_[value_index]->GetProperty_Accelerated_Float(p_property_id), value_index);
				
				return EidosValue_SP(float_result);
			}
			case kEidosValueMaskString:
			{
				EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
					string_result->PushString(values_[value_index]->GetProperty_Accelerated_String(p_property_id));
				
				return EidosValue_SP(string_result);
			}
			case kEidosValueMaskObject:
			{
				const EidosObjectClass *value_class = signature->value_class_;
				
				if (value_class)
				{
					EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(value_class))->resize_no_initialize((int)values_size);
					
					for (size_t value_index = 0; value_index < values_size; ++value_index)
						object_result->set_object_element_no_check(values_[value_index]->GetProperty_Accelerated_ObjectElement(p_property_id), value_index);
					
					return EidosValue_SP(object_result);
				}
				else
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): (internal error) missing object element class for accelerated property access." << EidosTerminate(nullptr);
			}
			default:
				EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): (internal error) unsupported value type for accelerated property access." << EidosTerminate(nullptr);
		}
	}
	else
	{
		// get the value from all properties and collect the results
		std::vector<EidosValue_SP> results;
		
		if (values_size < 10)
		{
			// with small objects, we check every value
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosObjectElement *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(p_property_id);
				
				signature->CheckResultValue(*temp_result);
				results.emplace_back(temp_result);
			}
		}
		else
		{
			// with large objects, we just spot-check the first value, for speed
			bool checked_multivalued = false;
			
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosObjectElement *value = values_[value_index];
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
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): property " << Eidos_StringForGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << EidosTerminate(nullptr);
	
	signature->CheckAssignedValue(p_value);
	
	// We have to check the count ourselves; the signature does not do that for us
	int p_value_count = p_value.Count();
	size_t values_size = (size_t)size();
	
	if (p_value_count == 1)
	{
		// we have a multiplex assignment of one value to (maybe) more than one element: x.foo = 10
		if (signature->accelerated_set_ && (Count() > 1))
		{
			// Accelerated property writing is enabled for this property, so we will switch on the type of the property, and
			// assemble the written values directly from the C++ type values we get from direct access of p_value...
			EidosValueMask sig_mask = (signature->value_mask_ & kEidosValueMaskFlagStrip);
			
			switch (sig_mask)
			{
				case kEidosValueMaskLogical:	// p_value must be logical
				{
					eidos_logical_t set_value = p_value.LogicalAtIndex(0, nullptr);
					
					for (size_t value_index = 0; value_index < values_size; ++value_index)
						values_[value_index]->SetProperty_Accelerated_Logical(p_property_id, set_value);
					
					break;
				}
				case kEidosValueMaskInt:		// p_value could be integer or logical
				{
					int64_t set_value = p_value.IntAtIndex(0, nullptr);
					
					for (size_t value_index = 0; value_index < values_size; ++value_index)
						values_[value_index]->SetProperty_Accelerated_Int(p_property_id, set_value);
					
					break;
				}
				case kEidosValueMaskFloat:		// p_value could be float, integer, or logical
				{
					double set_value = p_value.FloatAtIndex(0, nullptr);
					
					for (size_t value_index = 0; value_index < values_size; ++value_index)
						values_[value_index]->SetProperty_Accelerated_Float(p_property_id, set_value);
					
					break;
				}
				case kEidosValueMaskString:		// p_value must be string
				{
					const std::string &set_value = p_value.StringAtIndex(0, nullptr);
					
					for (size_t value_index = 0; value_index < values_size; ++value_index)
						values_[value_index]->SetProperty_Accelerated_String(p_property_id, set_value);
					
					break;
				}
				case kEidosValueMaskObject:		// p_value must be object, and must match in class
				{
					const EidosObjectElement *set_value = p_value.ObjectElementAtIndex(0, nullptr);
					
					for (size_t value_index = 0; value_index < values_size; ++value_index)
						values_[value_index]->SetProperty_Accelerated_ObjectElement(p_property_id, set_value);
					
					break;
				}
				default:
					EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): (internal error) unsupported value type for accelerated property writing; should have been caught by CheckAssignedValue()." << EidosTerminate(nullptr);
					break;
			}
		}
		else
		{
			for (size_t value_index = 0; value_index < values_size; ++value_index)
				values_[value_index]->SetProperty(p_property_id, p_value);
		}
	}
	else if (p_value_count == Count())
	{
		if (p_value_count)
		{
			if (signature->accelerated_set_)
			{
				// Accelerated property writing is enabled for this property, so we will switch on the type of the property, and
				// assemble the written values directly from the C++ type values we get from direct access of p_value...
				EidosValueMask sig_mask = (signature->value_mask_ & kEidosValueMaskFlagStrip);
				
				switch (sig_mask)
				{
					case kEidosValueMaskLogical:	// p_value must be logical
					{
						const eidos_logical_t *value_ptr = p_value.LogicalVector()->data();
						
						for (size_t value_index = 0; value_index < values_size; ++value_index)
							values_[value_index]->SetProperty_Accelerated_Logical(p_property_id, *(value_ptr++));
						
						return;
					}
					case kEidosValueMaskInt:		// p_value could be integer or logical
					{
						EidosValueType set_value_type = p_value.Type();
						
						if (set_value_type == EidosValueType::kValueInt)
						{
							const int64_t *value_ptr = p_value.IntVector()->data();
							
							for (size_t value_index = 0; value_index < values_size; ++value_index)
								values_[value_index]->SetProperty_Accelerated_Int(p_property_id, *(value_ptr++));
							
							return;
						}
						else if (set_value_type == EidosValueType::kValueLogical)
						{
							const eidos_logical_t *value_ptr = p_value.LogicalVector()->data();
							
							for (size_t value_index = 0; value_index < values_size; ++value_index)
								values_[value_index]->SetProperty_Accelerated_Int(p_property_id, *(value_ptr++) ? 1 : 0);
							
							return;
						}
						
						break;
					}
					case kEidosValueMaskFloat:		// p_value could be float, integer, or logical
					{
						EidosValueType set_value_type = p_value.Type();
						
						if (set_value_type == EidosValueType::kValueFloat)
						{
							const double *value_ptr = p_value.FloatVector()->data();
							
							for (size_t value_index = 0; value_index < values_size; ++value_index)
								values_[value_index]->SetProperty_Accelerated_Float(p_property_id, *(value_ptr++));
							
							return;
						}
						else if (set_value_type == EidosValueType::kValueInt)
						{
							const int64_t *value_ptr = p_value.IntVector()->data();
							
							for (size_t value_index = 0; value_index < values_size; ++value_index)
								values_[value_index]->SetProperty_Accelerated_Float(p_property_id, *(value_ptr++));
							
							return;
						}
						else if (set_value_type == EidosValueType::kValueLogical)
						{
							const eidos_logical_t *value_ptr = p_value.LogicalVector()->data();
							
							for (size_t value_index = 0; value_index < values_size; ++value_index)
								values_[value_index]->SetProperty_Accelerated_Float(p_property_id, *(value_ptr++) ? 1.0 : 0.0);
							
							return;
						}
						
						break;
					}
					case kEidosValueMaskString:		// p_value must be string
					{
						const std::vector<std::string> *value_vec = p_value.StringVector();
						const std::string *value_ptr = value_vec->data();
						
						for (size_t value_index = 0; value_index < values_size; ++value_index)
							values_[value_index]->SetProperty_Accelerated_String(p_property_id, *(value_ptr++));
						
						return;
					}
					case kEidosValueMaskObject:		// p_value must be object, and must match in class
					{
						const EidosValue_Object_vector *value_vec = p_value.ObjectElementVector();
						EidosObjectElement * const *value_ptr = value_vec->data();
						
						for (size_t value_index = 0; value_index < values_size; ++value_index)
							values_[value_index]->SetProperty_Accelerated_ObjectElement(p_property_id, *(value_ptr++));
						
						return;
					}
					default:
						break;
				}
				
				EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): (internal error) unsupported value type for accelerated property writing; should have been caught by CheckAssignedValue()." << EidosTerminate(nullptr);
			}
			else
			{
				// we have a one-to-one assignment of values to elements: x.foo = 1:5 (where x has 5 elements)
				for (int value_idx = 0; value_idx < p_value_count; value_idx++)
				{
					EidosValue_SP temp_rvalue = p_value.GetValueAtIndex(value_idx, nullptr);
					
					values_[value_idx]->SetProperty(p_property_id, *temp_rvalue);
				}
			}
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << EidosTerminate(nullptr);
}

EidosValue_SP EidosValue_Object_vector::ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosMethodSignature *p_method_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	// This is an instance method, so it gets dispatched to all of our elements
	auto values_size = size();
	
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
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ExecuteMethodCall): method " << Eidos_StringForGlobalStringID(p_method_id) << " does not specify a return type, and thus cannot be called on a zero-length vector." << EidosTerminate(nullptr);
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
		std::vector<EidosValue_SP> results;
		
		if (values_size < 10)
		{
			// with small objects, we check every value
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosObjectElement *value = values_[value_index];
				EidosValue_SP temp_result = value->ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
				
				p_method_signature->CheckReturn(*temp_result);
				results.emplace_back(temp_result);
			}
		}
		else
		{
			// with large objects, we just spot-check the first value, for speed
			bool checked_multivalued = false;
			
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosObjectElement *value = values_[value_index];
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

EidosValue_Object_vector *EidosValue_Object_vector::reserve(size_t p_reserved_size)
{
	if (p_reserved_size > capacity_)
	{
		values_ = (EidosObjectElement **)realloc(values_, p_reserved_size * sizeof(EidosObjectElement *));
		capacity_ = p_reserved_size;
	}
	
	return this;
}

EidosValue_Object_vector *EidosValue_Object_vector::resize_no_initialize(size_t p_new_size)
{
	reserve(p_new_size);	// might set a capacity greater than p_new_size; no guarantees
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	// deal with retain/release
	if (class_needs_retain_release_)
	{
		if (p_new_size < count_)
		{
			// shrinking; need to release the elements made redundant
			for (size_t element_index = p_new_size; element_index < count_; ++element_index)
			{
				EidosObjectElement *value = values_[element_index];
				
				if (value)
					value->Release();
			}
		}
		else if (p_new_size > count_)
		{
			// expanding; need to zero out the new slots, despite our name (but only with class_needs_retain_release_!)
			for (size_t element_index = count_; element_index < p_new_size; ++element_index)
				values_[element_index] = nullptr;
		}
	}
#endif
	
	count_ = p_new_size;	// regardless of the capacity set, set the size to exactly p_new_size
	
	return this;
}

void EidosValue_Object_vector::expand(void)
{
	if (capacity_ == 0)
		reserve(16);		// if no reserve() call was made, start out with a bit of room
	else
		reserve(capacity_ << 1);
}

void EidosValue_Object_vector::erase_index(size_t p_index)
{
	if (p_index >= count_)
		RaiseForRangeViolation();
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
		if (values_[p_index])
			values_[p_index]->Release();
#endif
	
	if (p_index == count_ - 1)
		--count_;
	else
	{
		EidosObjectElement **element_ptr = values_ + p_index;
		EidosObjectElement **next_element_ptr = values_ + p_index + 1;
		EidosObjectElement **past_end_element_ptr = values_ + count_;
		size_t element_copy_count = past_end_element_ptr - next_element_ptr;
		
		memmove(element_ptr, next_element_ptr, element_copy_count * sizeof(EidosObjectElement *));
		
		--count_;
	}
}


// EidosValue_Object_singleton
#pragma mark EidosValue_Object_singleton

EidosValue_Object_singleton::EidosValue_Object_singleton(EidosObjectElement *p_element1, const EidosObjectClass *p_class) : value_(p_element1), EidosValue_Object(true, p_class)
{
	// we want to allow nullptr as a momentary placeholder, although in general a value should exist
	if (p_element1)
	{
		DeclareClassFromElement(p_element1);
		
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
		if (class_needs_retain_release_)
			p_element1->Retain();
#endif
	}
}

EidosValue_Object_singleton::~EidosValue_Object_singleton(void)
{
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
		if (value_)
			value_->Release();
#endif
}

int EidosValue_Object_singleton::Count_Virtual(void) const
{
	return 1;
}

void EidosValue_Object_singleton::Print(std::ostream &p_ostream) const
{
	p_ostream << *value_;
}

EidosObjectElement *EidosValue_Object_singleton::ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::ObjectElementAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return value_;
}

EidosValue_SP EidosValue_Object_singleton::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if (p_idx != 0)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(value_, Class()));
}

void EidosValue_Object_singleton::SetValue(EidosObjectElement *p_element)
{
	DeclareClassFromElement(p_element);
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
	{
		p_element->Retain();
		if (value_)
			value_->Release();		// we might have been initialized with nullptr with the aim of setting a value here
	}
#endif
	
	value_ = p_element;
}

EidosValue_SP EidosValue_Object_singleton::CopyValues(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(value_, Class()));
}

EidosValue_SP EidosValue_Object_singleton::VectorBasedCopy(void) const
{
	// We intentionally don't reserve a size of 1 here, on the assumption that further values are likely to be added
	EidosValue_Object_vector_SP new_vec = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(Class()));
	
	new_vec->push_object_element(value_);
	
	return new_vec;
}

void EidosValue_Object_singleton::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::SetValueAtIndex): (internal error) EidosValue_Object_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

void EidosValue_Object_singleton::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::PushValueFromIndexOfEidosValue): (internal error) EidosValue_Object_singleton is not modifiable." << EidosTerminate(p_blame_token);
}

EidosValue_SP EidosValue_Object_singleton::GetPropertyOfElements(EidosGlobalStringID p_property_id) const
{
	const EidosPropertySignature *signature = value_->Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::GetPropertyOfElements): property " << Eidos_StringForGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << EidosTerminate(nullptr);
	
	EidosValue_SP result = value_->GetProperty(p_property_id);
	
	signature->CheckResultValue(*result);
	
	return result;
}

void EidosValue_Object_singleton::SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	const EidosPropertySignature *signature = value_->Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::SetPropertyOfElements): property " << Eidos_StringForGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << EidosTerminate(nullptr);
	
	signature->CheckAssignedValue(p_value);
	
	// We have to check the count ourselves; the signature does not do that for us
	if (p_value.Count() == 1)
	{
		value_->SetProperty(p_property_id, p_value);
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_singleton::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << EidosTerminate(nullptr);
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
#pragma mark -

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

#ifdef EIDOS_OBJECT_RETAIN_RELEASE
EidosObjectElement *EidosObjectElement::Retain(void)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::Retain for " << Class()->ElementType() << "): (internal error) call to Retain() for non-shared class." << EidosTerminate(nullptr);
}

EidosObjectElement *EidosObjectElement::Release(void)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::Release for " << Class()->ElementType() << "): (internal error) call to Release() for non-shared class." << EidosTerminate(nullptr);
}
#endif

EidosValue_SP EidosObjectElement::GetProperty(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty for " << Class()->ElementType() << "): (internal error) attempt to get a value for property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

eidos_logical_t EidosObjectElement::GetProperty_Accelerated_Logical(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_Logical for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

int64_t EidosObjectElement::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_Int for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

double EidosObjectElement::GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_Float for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

std::string EidosObjectElement::GetProperty_Accelerated_String(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_String for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

EidosObjectElement *EidosObjectElement::GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id)
{
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty_Accelerated_ObjectElement for " << Class()->ElementType() << "): (internal error) attempt to get a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObjectElement::SetProperty_Accelerated_Logical(EidosGlobalStringID p_property_id, eidos_logical_t p_value)
{
#pragma unused(p_value)
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty_Accelerated_Logical for " << Class()->ElementType() << "): (internal error) attempt to set a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObjectElement::SetProperty_Accelerated_Int(EidosGlobalStringID p_property_id, int64_t p_value)
{
#pragma unused(p_value)
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty_Accelerated_Int for " << Class()->ElementType() << "): (internal error) attempt to set a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObjectElement::SetProperty_Accelerated_Float(EidosGlobalStringID p_property_id, double p_value)
{
#pragma unused(p_value)
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty_Accelerated_Float for " << Class()->ElementType() << "): (internal error) attempt to set a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObjectElement::SetProperty_Accelerated_String(EidosGlobalStringID p_property_id, const std::string &p_value)
{
#pragma unused(p_value)
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty_Accelerated_String for " << Class()->ElementType() << "): (internal error) attempt to set a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObjectElement::SetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id, const EidosObjectElement *p_value)
{
#pragma unused(p_value)
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty_Accelerated_ObjectElement for " << Class()->ElementType() << "): (internal error) attempt to set a value for accelerated property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObjectElement::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
#pragma unused(p_value)
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty): property " << Eidos_StringForGlobalStringID(p_property_id) << " is not defined for object element type " << Class()->ElementType() << "." << EidosTerminate(nullptr);
	
	bool readonly = signature->read_only_;
	
	// Check whether setting a constant was attempted; we can do this on behalf of all our subclasses
	if (readonly)
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty for " << Class()->ElementType() << "): attempt to set a new value for read-only property " << Eidos_StringForGlobalStringID(p_property_id) << "." << EidosTerminate(nullptr);
	else
		EIDOS_TERMINATION << "ERROR (EidosObjectElement::SetProperty for " << Class()->ElementType() << "): (internal error) setting a new value for read-write property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

EidosValue_SP EidosObjectElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_argument_count, p_interpreter)
	switch (p_method_id)
	{
		case gEidosID_str:	return ExecuteMethod_str(p_method_id, p_arguments, p_argument_count, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<const EidosMethodSignature *> *methods = Class()->Methods();
			const std::string &method_name = Eidos_StringForGlobalStringID(p_method_id);
			
			for (auto method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
		}
	}
}

//	*********************	– (void)str(void)
//
EidosValue_SP EidosObjectElement::ExecuteMethod_str(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << Class()->ElementType() << ":" << std::endl;
	
	const std::vector<const EidosPropertySignature *> *properties = Class()->Properties();
	
	for (auto property_sig : *properties)
	{
		const std::string &property_name = property_sig->property_name_;
		EidosGlobalStringID property_id = property_sig->property_id_;
		EidosValue_SP property_value;
		
		try {
			property_value = GetProperty(property_id);
		} catch (...) {
		}
		
		if (property_value)
		{
			int property_count = property_value->Count();
			EidosValueType property_type = property_value->Type();
			
			output_stream << "\t" << property_name << " " << property_sig->PropertySymbol() << " (" << property_type;
			
			if (property_type == EidosValueType::kValueObject)
				output_stream << "<" << property_value->ElementType() << ">) ";
			else
				output_stream << ") ";
			
			if (property_count <= 2)
				output_stream << *property_value << std::endl;
			else
			{
				EidosValue_SP first_value = property_value->GetValueAtIndex(0, nullptr);
				EidosValue_SP second_value = property_value->GetValueAtIndex(1, nullptr);
				
				output_stream << *first_value << " " << *second_value << " ... (" << property_count << " values)" << std::endl;
			}
		}
		else
		{
			// The property threw an error when we tried to access it, which is allowed
			// for properties that are only valid in specific circumstances
			output_stream << "\t" << property_name << " " << property_sig->PropertySymbol() << " <inaccessible>" << std::endl;
		}
	}
	
	return gStaticEidosValueNULLInvisible;
}

EidosValue_SP EidosObjectElement::ContextDefinedFunctionDispatch(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused(p_function_name, p_arguments, p_argument_count, p_interpreter)
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::ContextDefinedFunctionDispatch for " << Class()->ElementType() << "): (internal error) unimplemented Context function dispatch." << EidosTerminate(nullptr);
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosObjectElement &p_element)
{
	p_element.Print(p_outstream);	// get dynamic dispatch
	
	return p_outstream;
}


//
//	EidosObjectElementInternal
//
#pragma mark EidosObjectElementInternal

EidosObjectElementInternal::EidosObjectElementInternal(void)
{
//	std::cerr << "EidosObjectElementInternal::EidosObjectElementInternal allocated " << this << " with refcount == 1" << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}

EidosObjectElementInternal::~EidosObjectElementInternal(void)
{
//	std::cerr << "EidosObjectElementInternal::~EidosObjectElementInternal deallocated " << this << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}

#ifdef EIDOS_OBJECT_RETAIN_RELEASE
EidosObjectElement *EidosObjectElementInternal::Retain(void)
{
	refcount_++;
	
//	std::cerr << "EidosObjectElementInternal::Retain for " << this << ", new refcount == " << refcount_ << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
	
	return this;
}

EidosObjectElement *EidosObjectElementInternal::Release(void)
{
	refcount_--;
	
//	std::cerr << "EidosObjectElementInternal::Release for " << this << ", new refcount == " << refcount_ << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
	
	if (refcount_ == 0)
	{
		delete this;
		return nullptr;
	}
	
	return this;
}
#endif


//
//	EidosObjectClass
//
#pragma mark -
#pragma mark EidosObjectClass
#pragma mark -

EidosObjectClass::EidosObjectClass(void)
{
}

EidosObjectClass::~EidosObjectClass(void)
{
}

#ifdef EIDOS_OBJECT_RETAIN_RELEASE
bool EidosObjectClass::NeedsRetainRelease(void) const
{
	return false;
}
#endif

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
		EIDOS_TERMINATION << "ERROR (EidosObjectClass::SignatureForPropertyOrRaise for " << ElementType() << "): (internal error) missing property " << Eidos_StringForGlobalStringID(p_property_id) << "." << EidosTerminate(nullptr);
	
	return signature;
}

const std::vector<const EidosMethodSignature *> *EidosObjectClass::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>;
		
		// keep alphabetical order here
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID_methodSignature));
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID_propertySignature));
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
		methodSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_methodSignature, kEidosValueMaskNULL))->AddString_OSN("methodName", gStaticEidosValueNULL);
		propertySig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_propertySignature, kEidosValueMaskNULL))->AddString_OSN("propertyName", gStaticEidosValueNULL);
		sizeSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_size, kEidosValueMaskInt | kEidosValueMaskSingleton));
		strSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_str, kEidosValueMaskNULL));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID_methodSignature:		return methodSig;
		case gEidosID_propertySignature:	return propertySig;
		case gEidosID_size:					return sizeSig;
		case gEidosID_str:					return strSig;
			
			// all others, including gID_none
		default:
			return nullptr;
	}
}

const EidosMethodSignature *EidosObjectClass::SignatureForMethodOrRaise(EidosGlobalStringID p_method_id) const
{
	const EidosMethodSignature *signature = SignatureForMethod(p_method_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObjectClass::SignatureForMethodOrRaise for " << ElementType() << "): (internal error) missing method " << Eidos_StringForGlobalStringID(p_method_id) << "." << EidosTerminate(nullptr);
	
	return signature;
}

EidosValue_SP EidosObjectClass::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gEidosID_propertySignature:	return ExecuteMethod_propertySignature(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		case gEidosID_methodSignature:		return ExecuteMethod_methodSignature(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		case gEidosID_size:					return ExecuteMethod_size(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<const EidosMethodSignature *> *methods = Methods();
			const std::string &method_name = Eidos_StringForGlobalStringID(p_method_id);
			
			for (auto method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
		}
	}
}

//	*********************	+ (void)propertySignature([Ns$ propertyName = NULL])
//
EidosValue_SP EidosObjectClass::ExecuteMethod_propertySignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
	std::string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	const std::vector<const EidosPropertySignature *> *properties = Properties();
	bool signature_found = false;
	
	for (auto property_sig : *properties)
	{
		const std::string &property_name = property_sig->property_name_;
		
		if (has_match_string && (property_name.compare(match_string) != 0))
			continue;
		
		output_stream << property_name << " " << property_sig->PropertySymbol() << " (" << StringForEidosValueMask(property_sig->value_mask_, property_sig->value_class_, "", nullptr) << ")" << std::endl;
		
		signature_found = true;
	}
	
	if (has_match_string && !signature_found)
		output_stream << "No property found for \"" << match_string << "\"." << std::endl;
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	+ (void)methodSignature([Ns$ methodName = NULL])
//
EidosValue_SP EidosObjectClass::ExecuteMethod_methodSignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	
	std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
	bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
	std::string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
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
		
		output_stream << *method_sig << std::endl;
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
		
		output_stream << *method_sig << std::endl;
		signature_found = true;
	}
	
	if (has_match_string && !signature_found)
		output_stream << "No method signature found for \"" << match_string << "\"." << std::endl;
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	+ (integer$)size(void)
//
EidosValue_SP EidosObjectClass::ExecuteMethod_size(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(p_target->Count()));
}







































































