//
//  eidos_value.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2025 Philipp Messer.  All rights reserved.
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
#include "eidos_sorting.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "json.hpp"

#include <algorithm>
#include <utility>
#include <functional>
#include <cmath>
#include "string.h"


// The global object pool for EidosValue, initialized in Eidos_WarmUp()
EidosObjectPool *gEidosValuePool = nullptr;


//
//	Global static EidosValue objects; these are effectively const, although EidosValues can't be declared as const.
//	Internally, these are implemented as subclasses that terminate if they are dealloced or modified.
//

EidosValue_VOID_SP gStaticEidosValueVOID;

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
EidosValue_Int_SP gStaticEidosValue_Integer2;
EidosValue_Int_SP gStaticEidosValue_Integer3;

EidosValue_Float_SP gStaticEidosValue_Float0;
EidosValue_Float_SP gStaticEidosValue_Float0Point5;
EidosValue_Float_SP gStaticEidosValue_Float1;
EidosValue_Float_SP gStaticEidosValue_Float10;
EidosValue_Float_SP gStaticEidosValue_FloatINF;
EidosValue_Float_SP gStaticEidosValue_FloatNAN;
EidosValue_Float_SP gStaticEidosValue_FloatE;
EidosValue_Float_SP gStaticEidosValue_FloatPI;

EidosValue_String_SP gStaticEidosValue_StringEmpty;
EidosValue_String_SP gStaticEidosValue_StringSpace;
EidosValue_String_SP gStaticEidosValue_StringAsterisk;
EidosValue_String_SP gStaticEidosValue_StringDoubleAsterisk;
EidosValue_String_SP gStaticEidosValue_StringComma;
EidosValue_String_SP gStaticEidosValue_StringTab;
EidosValue_String_SP gStaticEidosValue_StringPeriod;
EidosValue_String_SP gStaticEidosValue_StringDoubleQuote;
EidosValue_String_SP gStaticEidosValue_String_ECMAScript;
EidosValue_String_SP gStaticEidosValue_String_indices;
EidosValue_String_SP gStaticEidosValue_String_average;

EidosClass *gEidosObject_Class = nullptr;


std::string StringForEidosValueType(const EidosValueType p_type)
{
	switch (p_type)
	{
		case EidosValueType::kValueVOID:		return gEidosStr_void;
		case EidosValueType::kValueNULL:		return gEidosStr_NULL;
		case EidosValueType::kValueLogical:		return gEidosStr_logical;
		case EidosValueType::kValueString:		return gEidosStr_string;
		case EidosValueType::kValueInt:			return gEidosStr_integer;
		case EidosValueType::kValueFloat:		return gEidosStr_float;
		case EidosValueType::kValueObject:		return gEidosStr_object;
	}
	EIDOS_TERMINATION << "ERROR (StringForEidosValueType): (internal error) unrecognized EidosValueType." << EidosTerminate();
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosValueType p_type)
{
	p_outstream << StringForEidosValueType(p_type);
	
	return p_outstream;
}

std::string StringForEidosValueMask(const EidosValueMask p_mask, const EidosClass *p_object_class, const std::string &p_name, EidosValue *p_default)
{
	//
	//	Note this logic is paralleled in +[NSAttributedString eidosAttributedStringForCallSignature:].
	//	These two should be kept in synch so the user-visible format of signatures is consistent.
	//
	
	if (p_name == gEidosStr_ELLIPSIS)
		return gEidosStr_ELLIPSIS;
	
	std::string out_string;
	bool is_optional = !!(p_mask & kEidosValueMaskOptional);
	bool requires_singleton = !!(p_mask & kEidosValueMaskSingleton);
	EidosValueMask stripped_mask = p_mask & kEidosValueMaskFlagStrip;
	
	if (is_optional)
		out_string += "[";
	
	if (stripped_mask == kEidosValueMaskNone)			out_string += "?";
	else if (stripped_mask == kEidosValueMaskAny)		out_string += "*";
	else if (stripped_mask == kEidosValueMaskAnyBase)	out_string += "+";
	else if (stripped_mask == kEidosValueMaskVOID)		out_string += gEidosStr_void;
	else if (stripped_mask == kEidosValueMaskNULL)		out_string += gEidosStr_NULL;
	else if (stripped_mask == kEidosValueMaskLogical)	out_string += gEidosStr_logical;
	else if (stripped_mask == kEidosValueMaskString)	out_string += gEidosStr_string;
	else if (stripped_mask == kEidosValueMaskInt)		out_string += gEidosStr_integer;
	else if (stripped_mask == kEidosValueMaskFloat)		out_string += gEidosStr_float;
	else if (stripped_mask == kEidosValueMaskObject)	out_string += gEidosStr_object;
	else if (stripped_mask == kEidosValueMaskNumeric)	out_string += gEidosStr_numeric;
	else
	{
		if (stripped_mask & kEidosValueMaskVOID)		out_string += "v";
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
		out_string += p_object_class->ClassNameForDisplay();
		out_string += ">";
	}
	
	if (requires_singleton)
		out_string += "$";
	
	if (p_name.length() > 0)
	{
		out_string += "\u00A0";	// non-breaking space
		out_string += p_name;
	}
	
	if (is_optional)
	{
		if (p_default && (p_default != gStaticEidosValueNULLInvisible.get()))
		{
			out_string += "\u00A0=\u00A0";
			
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

EidosValueType EidosTypeForPromotion(EidosValueType p_type1, EidosValueType p_type2, const EidosToken *p_blame_token)
{
	if ((p_type1 == EidosValueType::kValueVOID) || (p_type2 == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (EidosTypeForPromotion): (internal error) comparison with void is illegal." << EidosTerminate(p_blame_token);
	if ((p_type1 == EidosValueType::kValueNULL) || (p_type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (EidosTypeForPromotion): (internal error) comparison with NULL is illegal." << EidosTerminate(p_blame_token);
	
	// comparing one object to another is legal, but objects cannot be compared to other types
	// note that comparisons between objects and non-objects are not flagged here;
	// they error out later, when the object operand is cast to the other type
	if ((p_type1 == EidosValueType::kValueObject) && (p_type2 == EidosValueType::kValueObject))
		return EidosValueType::kValueObject;
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((p_type1 == EidosValueType::kValueString) || (p_type2 == EidosValueType::kValueString))
		return EidosValueType::kValueString;
	
	// float is the next highest type, so we promote to float if either operand is a float
	if ((p_type1 == EidosValueType::kValueFloat) || (p_type2 == EidosValueType::kValueFloat))
		return EidosValueType::kValueFloat;
	
	// int is the next highest type, so we promote to int if either operand is a int
	if ((p_type1 == EidosValueType::kValueInt) || (p_type2 == EidosValueType::kValueInt))
		return EidosValueType::kValueInt;
	
	// logical is the next highest type, so we promote to logical if either operand is a logical
	if ((p_type1 == EidosValueType::kValueLogical) || (p_type2 == EidosValueType::kValueLogical))
		return EidosValueType::kValueLogical;
	
	// that's the end of the road; we should never reach this point
	EIDOS_TERMINATION << "ERROR (EidosTypeForPromotion): (internal error) promotion involving type " << p_type1 << " and type " << p_type2 << " is undefined." << EidosTerminate(p_blame_token);
}

bool CompareEidosValues(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, EidosComparisonOperator p_operator, const EidosToken *p_blame_token)
{
	EidosValueType type1 = p_value1.Type(), type2 = p_value2.Type();
	
	if ((type1 == EidosValueType::kValueVOID) || (type2 == EidosValueType::kValueVOID))
		EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) comparison with void is illegal." << EidosTerminate(p_blame_token);
	if ((type1 == EidosValueType::kValueNULL) || (type2 == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) comparison with NULL is illegal." << EidosTerminate(p_blame_token);
	
	// comparing one object to another is legal, but objects cannot be compared to other types
	if ((type1 == EidosValueType::kValueObject) && (type2 == EidosValueType::kValueObject))
	{
		EidosObject *element1 = p_value1.ObjectElementAtIndex_NOCAST(p_index1, p_blame_token);
		EidosObject *element2 = p_value2.ObjectElementAtIndex_NOCAST(p_index2, p_blame_token);
		
		if (p_operator == EidosComparisonOperator::kEqual)			return (element1 == element2);
		else if (p_operator == EidosComparisonOperator::kNotEqual)	return (element1 != element2);
		else
			EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) objects may only be compared with == and !=." << EidosTerminate(p_blame_token);
	}
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((type1 == EidosValueType::kValueString) || (type2 == EidosValueType::kValueString))
	{
		std::string string1 = p_value1.StringAtIndex_CAST(p_index1, p_blame_token);
		std::string string2 = p_value2.StringAtIndex_CAST(p_index2, p_blame_token);
		int compare_result = string1.compare(string2);		// not guaranteed to be -1 / 0 / +1, just negative / 0 / positive
		
		switch (p_operator) {
			case EidosComparisonOperator::kLess:			return (compare_result < 0);
			case EidosComparisonOperator::kLessOrEqual:		return (compare_result <= 0);
			case EidosComparisonOperator::kEqual:			return (compare_result == 0);
			case EidosComparisonOperator::kGreaterOrEqual:	return (compare_result >= 0);
			case EidosComparisonOperator::kGreater:			return (compare_result > 0);
			case EidosComparisonOperator::kNotEqual:		return (compare_result != 0);
		}
	}
	
	// float is the next highest type, so we promote to float if either operand is a float
	if ((type1 == EidosValueType::kValueFloat) || (type2 == EidosValueType::kValueFloat))
	{
		double float1 = p_value1.FloatAtIndex_CAST(p_index1, p_blame_token);
		double float2 = p_value2.FloatAtIndex_CAST(p_index2, p_blame_token);
		
		switch (p_operator) {
			case EidosComparisonOperator::kLess:			return (float1 < float2);
			case EidosComparisonOperator::kLessOrEqual:		return (float1 <= float2);
			case EidosComparisonOperator::kEqual:			return (float1 == float2);
			case EidosComparisonOperator::kGreaterOrEqual:	return (float1 >= float2);
			case EidosComparisonOperator::kGreater:			return (float1 > float2);
			case EidosComparisonOperator::kNotEqual:		return (float1 != float2);
		}
	}
	
	// int is the next highest type, so we promote to int if either operand is a int
	if ((type1 == EidosValueType::kValueInt) || (type2 == EidosValueType::kValueInt))
	{
		int64_t int1 = p_value1.IntAtIndex_CAST(p_index1, p_blame_token);
		int64_t int2 = p_value2.IntAtIndex_CAST(p_index2, p_blame_token);
		
		switch (p_operator) {
			case EidosComparisonOperator::kLess:			return (int1 < int2);
			case EidosComparisonOperator::kLessOrEqual:		return (int1 <= int2);
			case EidosComparisonOperator::kEqual:			return (int1 == int2);
			case EidosComparisonOperator::kGreaterOrEqual:	return (int1 >= int2);
			case EidosComparisonOperator::kGreater:			return (int1 > int2);
			case EidosComparisonOperator::kNotEqual:		return (int1 != int2);
		}
	}
	
	// logical is the next highest type, so we promote to logical if either operand is a logical
	if ((type1 == EidosValueType::kValueLogical) || (type2 == EidosValueType::kValueLogical))
	{
		eidos_logical_t logical1 = p_value1.LogicalAtIndex_CAST(p_index1, p_blame_token);
		eidos_logical_t logical2 = p_value2.LogicalAtIndex_CAST(p_index2, p_blame_token);
		
		switch (p_operator) {
			case EidosComparisonOperator::kLess:			return (logical1 < logical2);
			case EidosComparisonOperator::kLessOrEqual:		return (logical1 <= logical2);
			case EidosComparisonOperator::kEqual:			return (logical1 == logical2);
			case EidosComparisonOperator::kGreaterOrEqual:	return (logical1 >= logical2);
			case EidosComparisonOperator::kGreater:			return (logical1 > logical2);
			case EidosComparisonOperator::kNotEqual:		return (logical1 != logical2);
		}
	}
	
	// that's the end of the road; we should never reach this point
	EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) comparison involving type " << type1 << " and type " << type2 << " is undefined." << EidosTerminate(p_blame_token);
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

eidos_logical_t EidosValue::LogicalAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::LogicalAtIndex_CAST): operand type " << this->Type() << " cannot be converted to type logical." << EidosTerminate(p_blame_token);
}

std::string EidosValue::StringAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::StringAtIndex_CAST): operand type " << this->Type() << " cannot be converted to type string." << EidosTerminate(p_blame_token);
}

int64_t EidosValue::IntAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::IntAtIndex_CAST): operand type " << this->Type() << " cannot be converted to type integer." << EidosTerminate(p_blame_token);
}

double EidosValue::FloatAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::FloatAtIndex_CAST): operand type " << this->Type() << " cannot be converted to type float." << EidosTerminate(p_blame_token);
}

EidosObject *EidosValue::ObjectElementAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx)
	EIDOS_TERMINATION << "ERROR (EidosValue::ObjectElementAtIndex_CAST): operand type " << this->Type() << " cannot be converted to type object." << EidosTerminate(p_blame_token);
}

void EidosValue::RaiseForIncorrectTypeCall(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForIncorrectTypeCall): (internal error) direct vector access attempted on an EidosValue type of the incorrect type." << EidosTerminate(nullptr);
}

void EidosValue::RaiseForImmutabilityCall(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForImmutabilityCall): (internal error) mutable direct vector access attempted on an immutable EidosValue." << EidosTerminate(nullptr);
}

void EidosValue::RaiseForCapacityViolation(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForCapacityViolation): (internal error) access violated the current capacity of an EidosValue." << EidosTerminate(nullptr);
}

void EidosValue::RaiseForRangeViolation(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForRangeViolation): (internal error) access violated the current size of an EidosValue." << EidosTerminate(nullptr);
}

void EidosValue::RaiseForRetainReleaseViolation(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForRetainReleaseViolation): (internal error) access violated the retain/release policy of an EidosValue." << EidosTerminate(nullptr);
}

bool EidosValue::MatchingDimensions(const EidosValue *p_value1, const EidosValue *p_value2)
{
	int x_dimcount = p_value1->DimensionCount();
	int y_dimcount = p_value2->DimensionCount();
	
	if (x_dimcount != y_dimcount)
		return false;
	
	const int64_t *x_dims = p_value1->Dimensions();
	const int64_t *y_dims = p_value2->Dimensions();
	
	if ((x_dims && !y_dims) || (!x_dims && y_dims))		// should never happen
		return false;
	
	if (x_dims && y_dims)
	{
		for (int dim_index = 0; dim_index < x_dimcount; ++dim_index)
			if (x_dims[dim_index] != y_dims[dim_index])
				return false;
	}
	
	return true;
}

void EidosValue::_CopyDimensionsFromValue(const EidosValue *p_value)
{
	WILL_MODIFY(this);
	
	int64_t *source_dims = p_value->dim_;
	
	// First check that the source's dimensions will work for us; we assume they work for the source, so rather than
	// multiplying them out we can just compare total sizes.  We only need to check this if the source is an array.
	if (source_dims)
	{
		int count = Count();
		int source_count = p_value->Count();
		
		if (count != source_count)
			EIDOS_TERMINATION << "ERROR (EidosValue::_CopyDimensionsFromValue): mismatch between vector length and requested dimensions." << EidosTerminate(nullptr);
		
		// OK, the source's dimensions work; assume that we need to throw out our existing dimensions (virtually
		// always true, since this is generally called on new EidosValues), and malloc and copy a new dim_ buffer.
		free(dim_);
		dim_ = nullptr;
		
		int64_t dim_count = *source_dims;
		
		dim_ = (int64_t *)malloc((dim_count + 1) * sizeof(int64_t));
		if (!dim_)
			EIDOS_TERMINATION << "ERROR (EidosValue::_CopyDimensionsFromValue): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		memcpy(dim_, source_dims, (dim_count + 1) * sizeof(int64_t));
	}
	else
	{
		// The source has no dimensions, so we just need to throw out any dimensions we have.
		free(dim_);
		dim_ = nullptr;
	}
}

void EidosValue::SetDimensions(int64_t p_dim_count, const int64_t *p_dim_buffer)
{
	WILL_MODIFY(this);
	
	if ((p_dim_count == 1) && !p_dim_buffer)
	{
		// Make the value a plain vector; throw out any dimensions we have.
		free(dim_);
		dim_ = nullptr;
	}
	else if ((p_dim_count >= 2) && p_dim_buffer)
	{
		// A matrix or array is requested; first check that our size fits the request
		int64_t dim_product = 1;
		
		for (int dim_index = 0; dim_index < p_dim_count; ++dim_index)
		{
			int64_t dim = p_dim_buffer[dim_index];
			
			if (dim <= 0)
				EIDOS_TERMINATION << "ERROR (EidosValue::SetDimensions): dimension <= 0 requested, which is not allowed." << EidosTerminate(nullptr);
			
			int64_t old_product = dim_product;
			
			dim_product *= dim;
			if (dim_product / dim != old_product)
				EIDOS_TERMINATION << "ERROR (EidosValue::SetDimensions): dimension overflow; product of dimensions exceeds maximum capacity." << EidosTerminate(nullptr);
		}
		
		int count = Count();
		
		if (dim_product != count)
			EIDOS_TERMINATION << "ERROR (EidosValue::SetDimensions): mismatch between vector length and requested dimensions." << EidosTerminate(nullptr);
		
		// OK, the size works and the individual dimensions check out, so make our dim_ buffer
		free(dim_);
		dim_ = nullptr;
		
		dim_ = (int64_t *)malloc((p_dim_count + 1) * sizeof(int64_t));
		if (!dim_)
			EIDOS_TERMINATION << "ERROR (EidosValue::SetDimensions): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
		dim_[0] = p_dim_count;
		memcpy(dim_ + 1, p_dim_buffer, p_dim_count * sizeof(int64_t));
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue::SetDimensions): nonsensical requested dimensions." << EidosTerminate(nullptr);
}

EidosValue_SP EidosValue::Subset(std::vector<std::vector<int64_t>> &p_inclusion_indices, bool p_drop, const EidosToken *p_blame_token)
{
	EidosValue_SP result_SP;
	
	int dimcount = DimensionCount();
	const int64_t *dims = Dimensions();
	std::vector<int> inclusion_counts;
	bool empty_dimension = false;
	
	if ((int)p_inclusion_indices.size() != dimcount)
		EIDOS_TERMINATION << "ERROR (EidosValue::Subset): (internal error) size of p_inclusion_indices does not match dimension count of value." << EidosTerminate(nullptr);
	
	for (int dim_index = 0; dim_index < dimcount; ++dim_index)
	{
		int dim_size = (int)p_inclusion_indices[dim_index].size();
		
		if (dim_size == 0)
			empty_dimension = true;
		
		inclusion_counts.emplace_back(dim_size);
	}
	
	if (empty_dimension)
	{
		// If there was a dimension where no index was included, the result is an empty vector of the right type
		result_SP = NewMatchingType();
	}
	else
	{
		// We have tabulated the subsets; now copy the included values into the result.  To do this, we count up in the base
		// system established by inclusion_counts, and add the referenced value at each step along the way.
		result_SP = NewMatchingType();
		
		std::vector<int> generating_counter(dimcount, 0);
		
		do
		{
			// add the value referenced by generating_counter in inclusion_indices
			int64_t referenced_index = 0;
			int64_t dim_skip = 1;
			
			for (int counter_index = 0; counter_index < dimcount; ++counter_index)
			{
				int counter_value = generating_counter[counter_index];
				int64_t inclusion_index_value = p_inclusion_indices[counter_index][counter_value];
				
				referenced_index += inclusion_index_value * dim_skip;
				
				dim_skip *= dims[counter_index];
			}
			
			result_SP->PushValueFromIndexOfEidosValue((int)referenced_index, *this, p_blame_token);
			
			// increment generating_counter in the base system of inclusion_counts
			int generating_counter_index = 0;
			
			do
			{
				if (++generating_counter[generating_counter_index] == inclusion_counts[generating_counter_index])
				{
					generating_counter[generating_counter_index] = 0;
					generating_counter_index++;		// carry
				}
				else
					break;
			}
			while (generating_counter_index < dimcount);
			
			// if we carried out off the top, we are done adding included values
			if (generating_counter_index == dimcount)
				break;
		}
		while (true);
		
		// Finally, set the dimensionality of the result, considering dropped dimensions.  This basically follows the structure
		// of the indexed operand's dimensions, but (a) resizes to match the size of p_inclusion_indices for the given dimension,
		// and (b) omits any dimension that has a count of exactly 1, if dropping is requested.
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("EidosValue::Subset(): usage of statics");
		
		static int64_t *static_dim_buffer = nullptr;
		static int static_dim_buffer_size = -1;
		
		if (dimcount > static_dim_buffer_size)
		{
			static_dim_buffer = (int64_t *)realloc(static_dim_buffer, (dimcount + 1) * sizeof(int64_t));	// +1 so the zero case doesn't result in a zero-size allocation		// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
			if (!static_dim_buffer)
				EIDOS_TERMINATION << "ERROR (EidosValue::Subset): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			static_dim_buffer_size = dimcount;
		}
		
		int final_dim_count = 0;
		
		for (int subset_index = 0; subset_index < dimcount; ++subset_index)
		{
			int dim_size = inclusion_counts[subset_index];
			
			if (!p_drop || (dim_size > 1))
				static_dim_buffer[final_dim_count++] = dim_size;
		}
		
		if (final_dim_count > 1)
			result_SP->SetDimensions(final_dim_count, static_dim_buffer);
	}
	
	return result_SP;
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosValue &p_value)
{
	p_value.Print(p_outstream);		// get dynamic dispatch
	
	return p_outstream;
}

void EidosValue::PrintMatrixFromIndex(int64_t p_ncol, int64_t p_nrow, int64_t p_start_index, std::ostream &p_ostream, const std::string &p_indent) const
{
	int64_t element_count = p_ncol * p_nrow;
	
	// assemble all of the value strings, for printing with alignment
	std::vector<std::string> element_strings;
	
	for (int64_t element_index = 0; element_index < element_count; ++element_index)
	{
		std::ostringstream oss;
		
		PrintValueAtIndex((int)(element_index + p_start_index), oss);
		element_strings.emplace_back(oss.str());
	}
	
	// find the widest element, including column headers
	int max_element_width = 0;
	
	for (int64_t element_index = 0; element_index < element_count; ++element_index)
		max_element_width = std::max(max_element_width, (int)element_strings[element_index].length());
	
	max_element_width = std::max(max_element_width, (p_ncol == 1) ? 4 : ((int)floor(log10(p_ncol - 1)) + 4));
	
	int max_rowcol_width = (p_nrow == 1) ? 4 : ((int)floor(log10(p_nrow - 1)) + 4);
	
	// print the upper left corner padding
	p_ostream << p_indent << std::string(max_rowcol_width, ' ');
	
	// print the column headers
	for (int col_index = 0; col_index < p_ncol; ++col_index)
	{
		// pad before column header
		{
			int element_width = (col_index == 0) ? 4 : ((int)floor(log10(col_index)) + 4);
			
			p_ostream << std::string((max_element_width - element_width) + 1, ' ');
		}
		
		// column header
		p_ostream << "[," << col_index << "]";
	}
	
	// print each row
	for (int row_index = 0; row_index < p_nrow; ++row_index)
	{
		p_ostream << std::endl << p_indent;
		
		// pad before row index
		{
			int element_width = (row_index == 0) ? 4 : ((int)floor(log10(row_index)) + 4);
			
			p_ostream << std::string(max_rowcol_width - element_width, ' ');
		}
		
		// row index
		p_ostream << '[' << row_index << ",]";
		
		// row contents
		for (int col_index = 0; col_index < p_ncol; ++col_index)
		{
			std::string &element_string = element_strings[row_index + col_index * p_nrow];
			int element_width = (int)element_string.length();
			
			p_ostream << std::string((max_element_width - element_width) + 1, ' ') << element_string;
		}
	}
}

void EidosValue::Print(std::ostream &p_ostream, const std::string &p_indent) const
{
	int count = Count();
	
	if (count == 0)
	{
		// standard format for zero-length vectors
		EidosValueType type = Type();
		
		p_ostream << p_indent;
		
		switch (type)
		{
			case EidosValueType::kValueVOID:	p_ostream << gEidosStr_void; break;
			case EidosValueType::kValueNULL:	p_ostream << gEidosStr_NULL; break;
			case EidosValueType::kValueLogical:
			case EidosValueType::kValueInt:
			case EidosValueType::kValueFloat:
			case EidosValueType::kValueString:	p_ostream << ElementType() << "(0)"; break;
			case EidosValueType::kValueObject:	p_ostream << "object()<" << ElementType() << ">"; break;
		}
	}
	else if (!dim_)
	{
		// print vectors by just spewing out individual values
		p_ostream << p_indent;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			if (value_index > 0)
				p_ostream << ' ';
			
			PrintValueAtIndex(value_index, p_ostream);
		}
	}
	else if (dim_[0] == 2)
	{
		PrintMatrixFromIndex(dim_[2], dim_[1], 0, p_ostream, p_indent);
	}
	else if (dim_[0] > 2)
	{
		// print arrays by looping over dimensions
		int64_t dim_count = dim_[0];
		int64_t matrix_skip = dim_[1] * dim_[2];	// number of elements in each nxm matrix slice
		int64_t array_dim_count = dim_count - 2;	// this many additional dimensions of nxm matrix slices
		int64_t *array_dim_indices = (int64_t *)calloc(array_dim_count, sizeof(int64_t));
		int64_t *array_dim_skip = (int64_t *)calloc(array_dim_count, sizeof(int64_t));
		
		if (!array_dim_indices || !array_dim_skip)
			EIDOS_TERMINATION << "ERROR (EidosValue::Print): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		array_dim_skip[0] = matrix_skip;
		for (int array_dim_index = 1; array_dim_index < array_dim_count; ++array_dim_index)
			array_dim_skip[array_dim_index] = array_dim_skip[array_dim_index - 1] * dim_[array_dim_index + 2];
		
		do
		{
			// print an index line before the matrix
			p_ostream << p_indent << ", ";
			
			for (int idx = 0; idx < array_dim_count; ++idx)
				p_ostream << ", " << array_dim_indices[idx];
			
			p_ostream << std::endl << p_indent << std::endl;
			
			// print out the matrix for this slice
			int slice_offset = 0;
			
			for (int idx = 0; idx < array_dim_count; ++idx)
				slice_offset += array_dim_skip[idx] * array_dim_indices[idx];
			
			PrintMatrixFromIndex(dim_[2], dim_[1], slice_offset, p_ostream, p_indent);
			
			// increment dim_indices; this is counting up in a weird mixed-base system
			int count_index;
			
			for (count_index = 0; count_index < array_dim_count; ++count_index)
			{
				if (++(array_dim_indices[count_index]) == dim_[count_index + 3])
					array_dim_indices[count_index] = 0;		// carry
				else
					break;
			}
			
			if (count_index == array_dim_count)
				break;
			
			p_ostream << std::endl << p_indent << std::endl;
		}
		while (true);
		
		free(array_dim_indices);
		free(array_dim_skip);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosValue::Print): (internal error) illegal dimension count " << dim_[0] << "." << EidosTerminate(nullptr);
	}
}

void EidosValue::PrintStructure(std::ostream &p_ostream, int max_values) const
{
	EidosValueType x_type = Type();
	int x_count = Count();
	int x_dimcount = DimensionCount();
	const int64_t *x_dims = Dimensions();
	
	if (x_count == 0)
	{
		// zero-length vectors get printed according to the standard code in EidosValue
		Print(p_ostream);
	}
	else
	{
		// start with the type, and then the class for object-type values
		p_ostream << x_type;
		
		if (x_type == EidosValueType::kValueObject)
			p_ostream << "<" << ElementType() << ">";
		
		// then print the ranges for each dimension
		p_ostream << " [";
		
		if (x_dimcount == 1)
			p_ostream << "0:" << (x_count - 1) << "]";
		else
		{
			for (int dim_index = 0; dim_index < x_dimcount; ++dim_index)
			{
				if (dim_index > 0)
					p_ostream << ", ";
				p_ostream << "0:" << (x_dims[dim_index] - 1);
			}
			
			p_ostream << "]";
		}
		
		// finally, print up to max_values values, if available, followed by an ellipsis if not all values were printed
		if (max_values > 0)
		{
			p_ostream << " ";
			
			int output_count = std::min(max_values, x_count);
			
			for (int output_index = 0; output_index < output_count; ++output_index)
			{
				EidosValue_SP value = GetValueAtIndex(output_index, nullptr);
				
				if (output_index > 0)
					p_ostream << gEidosStr_space_string;
				
				p_ostream << *value;
			}
			
			if (x_count > output_count)
				p_ostream << " ...";
		}
	}
}


//
//	EidosValue_VOID
//
#pragma mark -
#pragma mark EidosValue_VOID
#pragma mark -

const std::string &EidosValue_VOID::ElementType(void) const
{
	return gEidosStr_void;
}

void EidosValue_VOID::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
#pragma unused(p_idx)
	p_ostream << gEidosStr_void;
}

nlohmann::json EidosValue_VOID::JSONRepresentation(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::JSONRepresentation): (internal error) illegal on void." << EidosTerminate(nullptr);
}

EidosValue_SP EidosValue_VOID::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx, p_blame_token)
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::GetValueAtIndex): (internal error) illegal on void." << EidosTerminate(p_blame_token);
}

EidosValue_SP EidosValue_VOID::CopyValues(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::CopyValues): (internal error) illegal on void." << EidosTerminate(nullptr);
}

EidosValue_SP EidosValue_VOID::NewMatchingType(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::NewMatchingType): (internal error) illegal on void." << EidosTerminate(nullptr);
}

void EidosValue_VOID::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_source_script_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::PushValueFromIndexOfEidosValue): (internal error) illegal on void." << EidosTerminate(p_blame_token);
}

void EidosValue_VOID::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::Sort): (internal error) illegal on void." << EidosTerminate(nullptr);
}


//
//	EidosValue_NULL
//
#pragma mark -
#pragma mark EidosValue_NULL
#pragma mark -

const std::string &EidosValue_NULL::ElementType(void) const
{
	return gEidosStr_NULL;
}

void EidosValue_NULL::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
#pragma unused(p_idx)
	p_ostream << gEidosStr_NULL;
}

nlohmann::json EidosValue_NULL::JSONRepresentation(void) const
{
	nlohmann::json json_object;		// this will come out as a JSON "null"
	
	return json_object;
}

EidosValue_SP EidosValue_NULL::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx, p_blame_token)
	return gStaticEidosValueNULL;
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
	WILL_MODIFY(this);
	
	if (p_source_script_value.Type() == EidosValueType::kValueNULL)
		;	// NULL doesn't have values or indices, so this is a no-op
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_NULL::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_NULL::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	WILL_MODIFY(this);
	
	// nothing to do
}


//
//	EidosValue_Logical
//
#pragma mark -
#pragma mark EidosValue_Logical
#pragma mark -

EidosValue_Logical::EidosValue_Logical(const std::vector<eidos_logical_t> &p_logicalvec) : EidosValue(EidosValueType::kValueLogical)
{
	size_t count = p_logicalvec.size();
	const eidos_logical_t *values = p_logicalvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_logical_no_check(values[index], index);
}

EidosValue_Logical::EidosValue_Logical(eidos_logical_t p_logical1) : EidosValue(EidosValueType::kValueLogical)	// protected
{
	push_logical(p_logical1);
}

EidosValue_Logical::EidosValue_Logical(std::initializer_list<eidos_logical_t> p_init_list) : EidosValue(EidosValueType::kValueLogical)
{
	reserve(p_init_list.size());
	
	for (auto init_item : p_init_list)
		push_logical_no_check(init_item);
}

EidosValue_Logical::EidosValue_Logical(const eidos_logical_t *p_values, size_t p_count) : EidosValue(EidosValueType::kValueLogical)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_logical_no_check(p_values[index], index);
}

const std::string &EidosValue_Logical::ElementType(void) const
{
	return gEidosStr_logical;
}

void EidosValue_Logical::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	eidos_logical_t value = values_[p_idx];
	
	p_ostream << (value ? gEidosStr_T : gEidosStr_F);
}

nlohmann::json EidosValue_Logical::JSONRepresentation(void) const
{
	nlohmann::json json_object = nlohmann::json::array();	// always write as an array, for consistency; makes automated parsing easier
	int count = Count();
	
	for (int i = 0; i < count; ++i)
		json_object.emplace_back(values_[i] ? true : false);
	
	return json_object;
}

eidos_logical_t EidosValue_Logical::LogicalAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::LogicalAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

eidos_logical_t EidosValue_Logical::LogicalAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::LogicalAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

std::string EidosValue_Logical::StringAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::StringAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? gEidosStr_T : gEidosStr_F);
}

int64_t EidosValue_Logical::IntAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::IntAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? 1 : 0);
}

double EidosValue_Logical::FloatAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::FloatAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? 1.0 : 0.0);
}

EidosValue_SP EidosValue_Logical::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

EidosValue_SP EidosValue_Logical::CopyValues(void) const
{
	// note that constness, invisibility, etc. do not get copied
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(values_, count_))->CopyDimensionsFromValue(this));
}

EidosValue_SP EidosValue_Logical::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
}

void EidosValue_Logical::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	WILL_MODIFY(this);
	
	if (p_source_script_value.Type() == EidosValueType::kValueLogical)
		push_logical(p_source_script_value.LogicalAtIndex_NOCAST(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_Logical::Sort(bool p_ascending)
{
	WILL_MODIFY(this);
	
	if (p_ascending)
		std::sort(values_, values_ + count_);
	else
		std::sort(values_, values_ + count_, std::greater<eidos_logical_t>());
}

EidosValue_Logical *EidosValue_Logical::reserve(size_t p_reserved_size)
{
	WILL_MODIFY(this);
	
	if (p_reserved_size > capacity_)
	{
		values_ = (eidos_logical_t *)realloc(values_, p_reserved_size * sizeof(eidos_logical_t));
		if (!values_)
			EIDOS_TERMINATION << "ERROR (EidosValue_Logical::reserve): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		capacity_ = p_reserved_size;
	}
	
	return this;
}

void EidosValue_Logical::erase_index(size_t p_index)
{
	WILL_MODIFY(this);
	
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


//
//	EidosValue_String
//
#pragma mark -
#pragma mark EidosValue_String
#pragma mark -

EidosValue_String::EidosValue_String(const std::vector<std::string> &p_stringvec) : EidosValue(EidosValueType::kValueString), values_(p_stringvec)
{
}

EidosValue_String::EidosValue_String(std::initializer_list<const std::string> p_init_list) : EidosValue(EidosValueType::kValueString)
{
	for (const auto &init_item : p_init_list)
		values_.emplace_back(init_item);
}

EidosValue_String::EidosValue_String(std::initializer_list<const char *> p_init_list) : EidosValue(EidosValueType::kValueString)
{
	for (const auto &init_item : p_init_list)
		values_.emplace_back(init_item);
}

const std::string &EidosValue_String::ElementType(void) const
{
	return gEidosStr_string;
}

EidosValue_SP EidosValue_String::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String());
}

void EidosValue_String::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	const std::string &value = StringRefAtIndex_NOCAST(p_idx, nullptr);
	
	// Emit a quoted string with backslash escapes as needed
	p_ostream << Eidos_string_escaped(value, EidosStringQuoting::kChooseQuotes);
}

nlohmann::json EidosValue_String::JSONRepresentation(void) const
{
	nlohmann::json json_object = nlohmann::json::array();	// always write as an array, for consistency; makes automated parsing easier
	int count = Count();
	
	for (int i = 0; i < count; ++i)
		json_object.emplace_back(StringAtIndex_NOCAST(i, nullptr));
	
	return json_object;
}

std::string EidosValue_String::StringAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::StringAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

const std::string &EidosValue_String::StringRefAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::StringRefAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

eidos_logical_t EidosValue_String::LogicalAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::LogicalAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx].length() > 0);
}

std::string EidosValue_String::StringAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::StringAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

int64_t EidosValue_String::IntAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::IntAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	// We don't use IntForString because an integer has been specifically requested, so even if the string appears to contain a
	// float value we want to force it into being an int; the way to do that is to use FloatForString and then convert to int64_t
	double converted_value = EidosInterpreter::FloatForString(values_[p_idx], p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((converted_value < (double)INT64_MIN) || (converted_value >= (double)INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::IntAtIndex_CAST): '" << values_[p_idx] << "' could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
	
	return static_cast<int64_t>(converted_value);
}

double EidosValue_String::FloatAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::FloatAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosInterpreter::FloatForString(values_[p_idx], p_blame_token);
}

EidosValue_SP EidosValue_String::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)values_.size()))
		EIDOS_TERMINATION << "ERROR (EidosValue_String::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(values_[p_idx]));
}

EidosValue_SP EidosValue_String::CopyValues(void) const
{
	// note that constness, invisibility, etc. do not get copied
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_String(values_))->CopyDimensionsFromValue(this));
}

void EidosValue_String::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	WILL_MODIFY(this);
	UncacheScript();
	
	if (p_source_script_value.Type() == EidosValueType::kValueString)
		values_.emplace_back(p_source_script_value.StringAtIndex_NOCAST(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_String::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_String::Sort(bool p_ascending)
{
	WILL_MODIFY(this);
	UncacheScript();
	
	if (values_.size() < 2)
		return;
	
	// This will sort in parallel if the task is large enough (and we're running parallel)
	Eidos_ParallelSort(values_.data(), values_.size(), p_ascending);
}


//
//	EidosValue_Int
//
#pragma mark -
#pragma mark EidosValue_Int
#pragma mark -

EidosValue_Int::EidosValue_Int(const std::vector<int16_t> &p_intvec) : EidosValue(EidosValueType::kValueInt), values_(&singleton_value_), count_(0), capacity_(1)
{
	size_t count = p_intvec.size();
	const int16_t *values = p_intvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_int_no_check(values[index], index);
}

EidosValue_Int::EidosValue_Int(const std::vector<int32_t> &p_intvec) : EidosValue(EidosValueType::kValueInt), values_(&singleton_value_), count_(0), capacity_(1)
{
	size_t count = p_intvec.size();
	const int32_t *values = p_intvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_int_no_check(values[index], index);
}

EidosValue_Int::EidosValue_Int(const std::vector<int64_t> &p_intvec) : EidosValue(EidosValueType::kValueInt), values_(&singleton_value_), count_(0), capacity_(1)
{
	size_t count = p_intvec.size();
	const int64_t *values = p_intvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_int_no_check(values[index], index);
}

EidosValue_Int::EidosValue_Int(std::initializer_list<int64_t> p_init_list) : EidosValue(EidosValueType::kValueInt), values_(&singleton_value_), count_(0), capacity_(1)
{
	reserve(p_init_list.size());
	
	for (auto init_item : p_init_list)
		push_int_no_check(init_item);
}

EidosValue_Int::EidosValue_Int(const int64_t *p_values, size_t p_count) : EidosValue(EidosValueType::kValueInt), values_(&singleton_value_), count_(0), capacity_(1)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_int_no_check(p_values[index], index);
}

const std::string &EidosValue_Int::ElementType(void) const
{
	return gEidosStr_integer;
}

EidosValue_SP EidosValue_Int::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int());
}

void EidosValue_Int::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	int64_t value = IntAtIndex_NOCAST(p_idx, nullptr);
	
	p_ostream << value;
}

nlohmann::json EidosValue_Int::JSONRepresentation(void) const
{
	nlohmann::json json_object = nlohmann::json::array();	// always write as an array, for consistency; makes automated parsing easier
	int count = Count();
	
	for (int i = 0; i < count; ++i)
		json_object.emplace_back(IntAtIndex_NOCAST(i, nullptr));
	
	return json_object;
}

int64_t EidosValue_Int::IntAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::IntAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

double EidosValue_Int::NumericAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	// casts integer to float, otherwise does not cast; considered _NOCAST
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::NumericAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

eidos_logical_t EidosValue_Int::LogicalAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::LogicalAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return (values_[p_idx] == 0 ? false : true);
}

std::string EidosValue_Int::StringAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::StringAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return std::to_string(values_[p_idx]);		// way faster than std::ostringstream
}

int64_t EidosValue_Int::IntAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::IntAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

double EidosValue_Int::FloatAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::FloatAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Int::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(values_[p_idx]));
}

EidosValue_SP EidosValue_Int::CopyValues(void) const
{
	// note that constness, invisibility, etc. do not get copied
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Int(values_, count_))->CopyDimensionsFromValue(this));
}

void EidosValue_Int::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	WILL_MODIFY(this);
	
	if (p_source_script_value.Type() == EidosValueType::kValueInt)
		push_int(p_source_script_value.IntAtIndex_NOCAST(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Int::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_Int::Sort(bool p_ascending)
{
	WILL_MODIFY(this);
	
	if (count_ < 2)
		return;
	
	// This will sort in parallel if the task is large enough (and we're running parallel)
	Eidos_ParallelSort(values_, count_, p_ascending);
}

EidosValue_Int *EidosValue_Int::reserve(size_t p_reserved_size)
{
	WILL_MODIFY(this);
	
	if (p_reserved_size > capacity_)
	{
		// this is a reservation for an explicit size, so we give that size exactly, to avoid wasting space
		
		if (values_ == &singleton_value_)
		{
			values_ = (int64_t *)malloc(p_reserved_size * sizeof(int64_t));
			
			if (!values_)
				EIDOS_TERMINATION << "ERROR (EidosValue_Int::reserve): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			values_[0] = singleton_value_;
		}
		else
		{
			values_ = (int64_t *)realloc(values_, p_reserved_size * sizeof(int64_t));
			
			if (!values_)
				EIDOS_TERMINATION << "ERROR (EidosValue_Int::reserve): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		}
		
		capacity_ = p_reserved_size;
	}
	
	return this;
}

void EidosValue_Int::erase_index(size_t p_index)
{
	WILL_MODIFY(this);
	
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


//
//	EidosValue_Float
//
#pragma mark -
#pragma mark EidosValue_Float
#pragma mark -

EidosValue_Float::EidosValue_Float(const std::vector<double> &p_doublevec) : EidosValue(EidosValueType::kValueFloat), values_(&singleton_value_), count_(0), capacity_(1)
{
	size_t count = p_doublevec.size();
	const double *values = p_doublevec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_float_no_check(values[index], index);
}

EidosValue_Float::EidosValue_Float(std::initializer_list<double> p_init_list) : EidosValue(EidosValueType::kValueFloat), values_(&singleton_value_), count_(0), capacity_(1)
{
	reserve(p_init_list.size());
	
	for (auto init_item : p_init_list)
		push_float_no_check(init_item);
}

EidosValue_Float::EidosValue_Float(const double *p_values, size_t p_count) : EidosValue(EidosValueType::kValueFloat), values_(&singleton_value_), count_(0), capacity_(1)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_float_no_check(p_values[index], index);
}

const std::string &EidosValue_Float::ElementType(void) const
{
	return gEidosStr_float;
}

EidosValue_SP EidosValue_Float::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
}

void EidosValue_Float::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	p_ostream << EidosStringForFloat(FloatAtIndex_NOCAST(p_idx, nullptr));
}

nlohmann::json EidosValue_Float::JSONRepresentation(void) const
{
	nlohmann::json json_object = nlohmann::json::array();	// always write as an array, for consistency; makes automated parsing easier
	int count = Count();
	
	for (int i = 0; i < count; ++i)
		json_object.emplace_back(FloatAtIndex_NOCAST(i, nullptr));
	
	return json_object;
}

double EidosValue_Float::FloatAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::FloatAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

double EidosValue_Float::NumericAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	// casts integer to float, otherwise does not cast; considered _NOCAST
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::NumericAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

eidos_logical_t EidosValue_Float::LogicalAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::LogicalAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	double value = values_[p_idx];
	
	if (std::isnan(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::LogicalAtIndex_CAST): NAN cannot be converted to logical type." << EidosTerminate(p_blame_token);
	
	return (value == 0 ? false : true);
}

std::string EidosValue_Float::StringAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::StringAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosStringForFloat(values_[p_idx]);
}

int64_t EidosValue_Float::IntAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::IntAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	double value = values_[p_idx];
	
	if (std::isnan(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::IntAtIndex_CAST): NAN cannot be converted to integer type." << EidosTerminate(p_blame_token);
	if (std::isinf(value))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::IntAtIndex_CAST): INF cannot be converted to integer type." << EidosTerminate(p_blame_token);
	
	// nwellnhof on stackoverflow points out that the >= here is correct even though it looks wrong, because reasons...
	if ((value < (double)INT64_MIN) || (value >= (double)INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::IntAtIndex_CAST): float value " << value << " is too large to be converted to integer type." << EidosTerminate(p_blame_token);
	
	return static_cast<int64_t>(value);
}

double EidosValue_Float::FloatAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::FloatAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Float::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(values_[p_idx]));
}

EidosValue_SP EidosValue_Float::CopyValues(void) const
{
	// note that constness, invisibility, etc. do not get copied
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Float(values_, count_))->CopyDimensionsFromValue(this));
}

void EidosValue_Float::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	WILL_MODIFY(this);
	
	if (p_source_script_value.Type() == EidosValueType::kValueFloat)
		push_float(p_source_script_value.FloatAtIndex_NOCAST(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Float::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_Float::Sort(bool p_ascending)
{
	WILL_MODIFY(this);
	
	if (count_ < 2)
		return;
	
	// Unfortunately a custom comparator is needed to make the sort order with NANs match that of R
	if (p_ascending)
		Eidos_ParallelSort_Comparator(values_, count_, [](const double& a, const double& b) { return std::isnan(b) || (a < b); });
	else
		Eidos_ParallelSort_Comparator(values_, count_, [](const double& a, const double& b) { return std::isnan(b) || (a > b); });
}

EidosValue_Float *EidosValue_Float::reserve(size_t p_reserved_size)
{
	WILL_MODIFY(this);
	
	if (p_reserved_size > capacity_)
	{
		// this is a reservation for an explicit size, so we give that size exactly, to avoid wasting space
		
		if (values_ == &singleton_value_)
		{
			values_ = (double *)malloc(p_reserved_size * sizeof(double));
			
			if (!values_)
				EIDOS_TERMINATION << "ERROR (EidosValue_Float::reserve): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			values_[0] = singleton_value_;
		}
		else
		{
			values_ = (double *)realloc(values_, p_reserved_size * sizeof(double));
			
			if (!values_)
				EIDOS_TERMINATION << "ERROR (EidosValue_Float::reserve): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		}
		
		capacity_ = p_reserved_size;
	}
	
	return this;
}

void EidosValue_Float::erase_index(size_t p_index)
{
	WILL_MODIFY(this);
	
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


//
//	EidosValue_Object
//
#pragma mark -
#pragma mark EidosValue_Object
#pragma mark -

// See comments on EidosValue_Object::EidosValue_Object() below.  Note this is shared by all species.
std::vector<EidosValue_Object *> gEidosValue_Object_Mutation_Registry;

EidosValue_Object::EidosValue_Object(const EidosClass *p_class) : EidosValue(EidosValueType::kValueObject),
	values_(&singleton_value_), count_(0), capacity_(1), class_(p_class)
{
	class_uses_retain_release_ = (class_ == gEidosObject_Class ? true : class_->UsesRetainRelease());
	
	// BCH 7 May 2017: OK, so, this is a hack of breathtaking disgustingness.  Here is the problem.  In SLiM we
	// need to reallocate the block in which all Mutation objects live, which invalidates all their pointers.
	// SLiM can mostly handle that for itself, but there is a big problem when it comes to EidosValue_Object
	// instances, which can contain Mutation * elements that must be updated.  So we need to find every single
	// EidosValue_Object in existence which has a class of Mutation.  To that end, we here register this object
	// in a global list if it is of class Mutation; in ~EidosValue_Object we will deregister it.  Maybe there
	// is some way to do this without pushing the hack down into Eidos, but at the moment I'm not seeing it.
	// On the bright side, this scheme actually seems pretty robust; the only way it fails is if somebody avoids
	// using the constructor or the destructor for EidosValue_Object, I think, which seems unlikely.
	// Note this is shared by all species, since the mutation block itself is shared by all species.
	const std::string *element_type = &(class_->ClassName());
										
	if (element_type == &gEidosStr_Mutation)
	{
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("EidosValue_Object::EidosValue_Object(): gEidosValue_Object_Mutation_Registry change");
		
		gEidosValue_Object_Mutation_Registry.emplace_back(this);
		registered_for_patching_ = true;
		
		//std::cout << "pushed Mutation EidosValue_Object, count == " << gEidosValue_Object_Mutation_Registry.size() << std::endl;
	}
	else
	{
		registered_for_patching_ = false;
	}
}

EidosValue_Object::EidosValue_Object(EidosObject *p_element1, const EidosClass *p_class) : EidosValue_Object(p_class)
{
	// we want to allow nullptr as a momentary placeholder, although in general a value should exist
	// this is a bit gross; I think it is only in EidosInterpreter::Evaluate_For(), which wants to
	// set up the loop index variable and *then* put the first value into it; could probably be cleaned up.
	if (p_element1)
		push_object_element_CRR(p_element1);
	else
	{
		// this puts nullptr into singleton_value_, but does it following the standard procedure
		if (count_ == capacity_)
			expand();
		
		values_[count_++] = nullptr;
	}
}

EidosValue_Object::EidosValue_Object(const EidosValue_Object &p_original) : EidosValue_Object(p_original.Class())
{
	size_t count = p_original.Count();
	EidosObject * const *values = p_original.data();
	
	resize_no_initialize_RR(count);
	
	if (class_uses_retain_release_)
	{
		for (size_t index = 0; index < count; ++index)
			set_object_element_no_check_no_previous_RR(values[index], index);
	}
	else
	{
		for (size_t index = 0; index < count; ++index)
			set_object_element_no_check_NORR(values[index], index);
	}
}

EidosValue_Object::EidosValue_Object(const std::vector<EidosObject *> &p_elementvec, const EidosClass *p_class) : EidosValue_Object(p_class)
{
	size_t count = p_elementvec.size();
	EidosObject * const *values = p_elementvec.data();
	
	resize_no_initialize_RR(count);
	
	if (class_uses_retain_release_)
	{
		for (size_t index = 0; index < count; ++index)
			set_object_element_no_check_no_previous_RR(values[index], index);
	}
	else
	{
		for (size_t index = 0; index < count; ++index)
			set_object_element_no_check_NORR(values[index], index);
	}
}

EidosValue_Object::EidosValue_Object(std::initializer_list<EidosObject *> p_init_list, const EidosClass *p_class) : EidosValue_Object(p_class)
{
	reserve(p_init_list.size());
	
	if (class_uses_retain_release_)
	{
		for (auto init_item : p_init_list)
			push_object_element_no_check_RR(init_item);
	}
	else
	{
		for (auto init_item : p_init_list)
			push_object_element_no_check_NORR(init_item);
	}
}

EidosValue_Object::EidosValue_Object(EidosObject **p_values, size_t p_count, const EidosClass *p_class) : EidosValue_Object(p_class)
{
	resize_no_initialize_RR(p_count);
	
	if (class_uses_retain_release_)
	{
		for (size_t index = 0; index < p_count; ++index)
			set_object_element_no_check_no_previous_RR(p_values[index], index);
	}
	else
	{
		for (size_t index = 0; index < p_count; ++index)
			set_object_element_no_check_NORR(p_values[index], index);
	}
}

EidosValue_Object::~EidosValue_Object(void)
{
	// See comment on EidosValue_Object::EidosValue_Object() above
	if (registered_for_patching_)
	{
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("EidosValue_Object::~EidosValue_Object(): gEidosValue_Object_Mutation_Registry change");
		
		auto erase_iter = std::find(gEidosValue_Object_Mutation_Registry.begin(), gEidosValue_Object_Mutation_Registry.end(), this);
		
		if (erase_iter != gEidosValue_Object_Mutation_Registry.end())
			gEidosValue_Object_Mutation_Registry.erase(erase_iter);
		else
			EIDOS_TERMINATION << "ERROR (EidosValue_Object::~EidosValue_Object): (internal error) unregistered EidosValue_Object of class Mutation." << EidosTerminate(nullptr);
		
		//std::cout << "popped Mutation EidosValue_Object, count == " << gEidosValue_Object_Mutation_Registry.size() << std::endl;
	}
	
	if (class_uses_retain_release_)
	{
		for (size_t index = 0; index < count_; ++index)
		{
			EidosObject *value = values_[index];
			
			if (value)
				static_cast<EidosDictionaryRetained *>(value)->Release();		// unsafe cast to avoid virtual function overhead
		}
	}
	
	if (values_ != &singleton_value_)
		free(values_);
}

// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
void EidosValue_Object::PatchPointersByAdding(std::uintptr_t p_pointer_difference)
{
	for (size_t i = 0; i < count_; ++i)
	{
		std::uintptr_t old_element_ptr = reinterpret_cast<std::uintptr_t>(values_[i]);
		std::uintptr_t new_element_ptr = old_element_ptr + p_pointer_difference;
		
		values_[i] = reinterpret_cast<EidosObject *>(new_element_ptr);				// NOLINT(*-no-int-to-ptr)
	}
}

// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
void EidosValue_Object::PatchPointersBySubtracting(std::uintptr_t p_pointer_difference)
{
	for (size_t i = 0; i < count_; ++i)
	{
		std::uintptr_t old_element_ptr = reinterpret_cast<std::uintptr_t>(values_[i]);
		std::uintptr_t new_element_ptr = old_element_ptr - p_pointer_difference;
		
		values_[i] = reinterpret_cast<EidosObject *>(new_element_ptr);				// NOLINT(*-no-int-to-ptr)
	}
}

void EidosValue_Object::RaiseForClassMismatch(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue_Object::RaiseForClassMismatch): the type of an object cannot be changed." << EidosTerminate(nullptr);
}

const std::string &EidosValue_Object::ElementType(void) const
{
	return Class()->ClassNameForDisplay();
}

EidosValue_SP EidosValue_Object::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(Class()));
}

void EidosValue_Object::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	EidosObject *value = ObjectElementAtIndex_NOCAST(p_idx, nullptr);
	
	p_ostream << *value;
}

nlohmann::json EidosValue_Object::JSONRepresentation(void) const
{
	nlohmann::json json_object = nlohmann::json::array();	// always write as an array, for consistency; makes automated parsing easier
	int count = Count();
	
	for (int i = 0; i < count; ++i)
		json_object.emplace_back(ObjectElementAtIndex_NOCAST(i, nullptr)->JSONRepresentation());
	
	return json_object;
}

EidosObject *EidosValue_Object::ObjectElementAtIndex_NOCAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::ObjectElementAtIndex_NOCAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

EidosObject *EidosValue_Object::ObjectElementAtIndex_CAST(int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::ObjectElementAtIndex_CAST): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return values_[p_idx];
}

EidosValue_SP EidosValue_Object::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
	if ((p_idx < 0) || (p_idx >= (int)count_))
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::GetValueAtIndex): subscript " << p_idx << " out of range." << EidosTerminate(p_blame_token);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(values_[p_idx], Class()));
}

EidosValue_SP EidosValue_Object::CopyValues(void) const
{
	// note that constness, invisibility, etc. do not get copied
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Object(*this))->CopyDimensionsFromValue(this));
}

void EidosValue_Object::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	WILL_MODIFY(this);
	
	if (p_source_script_value.Type() == EidosValueType::kValueObject)
		push_object_element_CRR(p_source_script_value.ObjectElementAtIndex_NOCAST(p_idx, p_blame_token));
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::PushValueFromIndexOfEidosValue): type mismatch." << EidosTerminate(p_blame_token);
}

void EidosValue_Object::Sort(bool p_ascending)
{
#pragma unused(p_ascending)
	EIDOS_TERMINATION << "ERROR (EidosValue_Object::Sort): Sort() is not defined for type object." << EidosTerminate(nullptr);
}

static bool CompareLogicalObjectSortPairsAscending(std::pair<eidos_logical_t, EidosObject*> i, std::pair<eidos_logical_t, EidosObject*> j)	{ return (i.first < j.first); }
static bool CompareLogicalObjectSortPairsDescending(std::pair<eidos_logical_t, EidosObject*> i, std::pair<eidos_logical_t, EidosObject*> j)	{ return (i.first > j.first); }

static bool CompareIntObjectSortPairsAscending(std::pair<int64_t, EidosObject*> i, std::pair<int64_t, EidosObject*> j)				{ return (i.first < j.first); }
static bool CompareIntObjectSortPairsDescending(std::pair<int64_t, EidosObject*> i, std::pair<int64_t, EidosObject*> j)				{ return (i.first > j.first); }

static bool CompareFloatObjectSortPairsAscending(std::pair<double, EidosObject*> i, std::pair<double, EidosObject*> j)				{ return (i.first < j.first); }
static bool CompareFloatObjectSortPairsDescending(std::pair<double, EidosObject*> i, std::pair<double, EidosObject*> j)				{ return (i.first > j.first); }

static bool CompareStringObjectSortPairsAscending(const std::pair<std::string, EidosObject*> &i, const std::pair<std::string, EidosObject*> &j)		{ return (i.first < j.first); }
static bool CompareStringObjectSortPairsDescending(const std::pair<std::string, EidosObject*> &i, const std::pair<std::string, EidosObject*> &j)	{ return (i.first > j.first); }

void EidosValue_Object::SortBy(const std::string &p_property, bool p_ascending)
{
	WILL_MODIFY(this);
	
	// At present this is called only by the sortBy() Eidos function, so we do not need a
	// blame token; all errors will be attributed to that function automatically.
	
	// length 0 or 1 is already sorted
	if (count_ <= 1)
		return;
	
	// figure out what type the property returns
	EidosGlobalStringID property_string_id = EidosStringRegistry::GlobalStringIDForString(p_property);
	EidosValue_SP first_result = values_[0]->GetProperty(property_string_id);
	EidosValueType property_type = first_result->Type();
	
	// switch on the property type for efficiency
	switch (property_type)
	{
		case EidosValueType::kValueVOID:
		case EidosValueType::kValueNULL:
		case EidosValueType::kValueObject:
			EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " returned " << property_type << "; a property that evaluates to logical, int, float, or string is required." << EidosTerminate(nullptr);
			break;
			
		case EidosValueType::kValueLogical:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<eidos_logical_t, EidosObject*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObject *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(temp_result->LogicalAtIndex_NOCAST(0, nullptr), value);
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareLogicalObjectSortPairsDescending);
			
			// read out our new element vector; the elements are already retained, if they are under retain/release
			resize_no_initialize_RR(0);
			
			for (auto sorted_pair : sortable_pairs)
				push_object_element_no_check_already_retained(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueInt:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<int64_t, EidosObject*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObject *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(temp_result->IntAtIndex_NOCAST(0, nullptr), value);
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareIntObjectSortPairsDescending);
			
			// read out our new element vector; the elements are already retained, if they are under retain/release
			resize_no_initialize_RR(0);
			
			for (auto sorted_pair : sortable_pairs)
				push_object_element_no_check_already_retained(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueFloat:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<double, EidosObject*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObject *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(temp_result->FloatAtIndex_NOCAST(0, nullptr), value);
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareFloatObjectSortPairsDescending);
			
			// read out our new element vector; the elements are already retained, if they are under retain/release
			resize_no_initialize_RR(0);
			
			for (auto sorted_pair : sortable_pairs)
				push_object_element_no_check_already_retained(sorted_pair.second);
			
			break;
		}
			
		case EidosValueType::kValueString:
		{
			// make a vector of pairs: first is the value returned for the sorting property, second is the object element
			std::vector<std::pair<std::string, EidosObject*>> sortable_pairs;
			
			for (size_t value_index = 0; value_index < count_; value_index++)
			{
				EidosObject *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(property_string_id);
				
				if (temp_result->Count() != 1)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " produced " << temp_result->Count() << " values for a single element; a property that produces one value per element is required for sorting." << EidosTerminate(nullptr);
				if (temp_result->Type() != property_type)
					EIDOS_TERMINATION << "ERROR (EidosValue_Object::SortBy): sorting property " << p_property << " did not produce a consistent result type; a single type is required for a sorting key." << EidosTerminate(nullptr);
				
				sortable_pairs.emplace_back(temp_result->StringAtIndex_NOCAST(0, nullptr), value);
			}
			
			// sort the vector of pairs
			if (p_ascending)
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsAscending);
			else
				std::sort(sortable_pairs.begin(), sortable_pairs.end(), CompareStringObjectSortPairsDescending);
			
			// read out our new element vector; the elements are already retained, if they are under retain/release
			resize_no_initialize_RR(0);
			
			for (const auto &sorted_pair : sortable_pairs)
				push_object_element_no_check_already_retained(sorted_pair.second);
			
			break;
		}
	}
}

EidosValue_SP EidosValue_Object::GetPropertyOfElements(EidosGlobalStringID p_property_id) const
{
	size_t values_size = count_;
	const EidosPropertySignature *signature = class_->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::GetPropertyOfElements): property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << EidosTerminate(nullptr);
	
	if (values_size == 0)
	{
		// With a zero-length vector, the return is a zero-length vector of the type specified by the property signature.  This
		// allows code to proceed without errors without having to check for the zero-length case, by simply propagating the
		// zero-length-ness forward while preserving the expected type for the expression.  If multiple return types are possible,
		// we return a zero-length vector for the simplest type supported.
		// BCH 24 March 2018: Decided this policy is a bit unsafe; now we only propagate a zero-length result forward if the
		// return type is unambiguous, otherwise it is an error.
		EidosValueMask sig_mask = (signature->value_mask_ & kEidosValueMaskFlagStrip);
		
		if (sig_mask == kEidosValueMaskVOID) return gStaticEidosValueVOID;	// (not legal for properties anyway)
		if (sig_mask == kEidosValueMaskNULL) return gStaticEidosValueNULL;	// (not legal for properties anyway)
		if (sig_mask == kEidosValueMaskLogical) return gStaticEidosValue_Logical_ZeroVec;
		if (sig_mask == kEidosValueMaskInt) return gStaticEidosValue_Integer_ZeroVec;
		if (sig_mask == kEidosValueMaskFloat) return gStaticEidosValue_Float_ZeroVec;
		if (sig_mask == kEidosValueMaskString) return gStaticEidosValue_String_ZeroVec;
		if (sig_mask == kEidosValueMaskObject)
		{
			const EidosClass *value_class = signature->value_class_;
			
			if (value_class)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(value_class));
			else
				return gStaticEidosValue_Object_ZeroVec;
		}
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::GetPropertyOfElements): property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " does not specify an unambiguous value type, and thus cannot be accessed on a zero-length vector." << EidosTerminate(nullptr);
	}
	else if (values_size == 1)
	{
		// The singleton case is very common, so it should be special-cased for speed
		EidosObject *value = values_[0];
		EidosValue_SP result = value->GetProperty(p_property_id);
		
		// Access of singleton properties retains the matrix/array structure of the target
		if (signature->value_mask_ & kEidosValueMaskSingleton)
			result->CopyDimensionsFromValue(this);
		
#if DEBUG
		// This is time-consuming, and will fail only due to internal bugs, so we should do it only in DEBUG
		signature->CheckResultValue(*result);
#endif
		return result;
	}
	else if (signature->accelerated_get_)
	{
		// Accelerated property access is enabled for this property, so the class will do all the work for us
		// We put this case below the (values_size == 1) case so the accelerated getter can focus on the vectorized case
		EidosValue_SP result = EidosValue_SP(signature->accelerated_getter(values_, values_size));
		
		// Access of singleton properties retains the matrix/array structure of the target
		if (signature->value_mask_ & kEidosValueMaskSingleton)
			result->CopyDimensionsFromValue(this);
		
#if DEBUG
		// This is time-consuming, and will fail only due to internal bugs, so we should do it only in DEBUG
		signature->CheckAggregateResultValue(*result, values_size);
#endif
		return result;
	}
	else
	{
		// get the value from all properties and collect the results
		std::vector<EidosValue_SP> results;
		
#if DEBUG
		if (values_size < 10)
		{
#endif
			// with small objects, we check every value
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosObject *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(p_property_id);
				
#if DEBUG
				// This is time-consuming, and will fail only due to internal bugs, so we should do it only in DEBUG
				signature->CheckResultValue(*temp_result);
#endif
				results.emplace_back(temp_result);
			}
#if DEBUG
		}
		else
		{
			// with large objects, we just spot-check the first value, for speed
			bool checked_multivalued = false;
			
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosObject *value = values_[value_index];
				EidosValue_SP temp_result = value->GetProperty(p_property_id);
				
				if (!checked_multivalued)
				{
					signature->CheckResultValue(*temp_result);
					checked_multivalued = true;
				}
				
				results.emplace_back(temp_result);
			}
		}
#endif
		
		// concatenate the results using ConcatenateEidosValues()
		EidosValue_SP result = ConcatenateEidosValues(results, true, false);	// allow NULL, don't allow VOID
		
		// Access of singleton properties retains the matrix/array structure of the target
		if (signature->value_mask_ & kEidosValueMaskSingleton)
			result->CopyDimensionsFromValue(this);
		
		return result;
	}
}

void EidosValue_Object::SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value, EidosToken *p_property_token)
{
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	// BCH 9 Sept. 2022: if the property does not exist, raise an error on the token for the property name.
	// Note that other errors stemming from this call will refer to whatever the current error range is.
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::SetPropertyOfElements): property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << EidosTerminate(p_property_token);
	
	signature->CheckAssignedValue(p_value);		// will raise if the type being assigned in is not an exact match
	
	// We have to check the count ourselves; the signature does not do that for us
	size_t p_value_count = p_value.Count();
	size_t values_size = count_;
	
	if (p_value_count == 1)
	{
		// we have a multiplex assignment of one value to (maybe) more than one element: x.foo = 10
		// Here (unlike the accelerated getter), the accelerated version is probably a win so we always use it
		if (signature->accelerated_set_)
		{
			// Accelerated property writing is enabled for this property, so we call the setter directly
			signature->accelerated_setter(values_, values_size, p_value, p_value_count);
		}
		else
		{
			for (size_t value_index = 0; value_index < values_size; ++value_index)
				values_[value_index]->SetProperty(p_property_id, p_value);
		}
	}
	else if (p_value_count == values_size)
	{
		if (p_value_count)
		{
			if (signature->accelerated_set_)
			{
				// Accelerated property writing is enabled for this property, so we call the setter directly
				signature->accelerated_setter(values_, values_size, p_value, p_value_count);
			}
			else
			{
				// we have a one-to-one assignment of values to elements: x.foo = 1:5 (where x has 5 elements)
				for (size_t value_idx = 0; value_idx < p_value_count; value_idx++)
				{
					EidosValue_SP temp_rvalue = p_value.GetValueAtIndex((int)value_idx, nullptr);
					
					values_[value_idx]->SetProperty(p_property_id, *temp_rvalue);
				}
			}
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << EidosTerminate(nullptr);
}

EidosValue_SP EidosValue_Object::ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosInstanceMethodSignature *p_method_signature, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// This is an instance method, so it gets dispatched to all of our elements
	auto values_size = count_;
	
	if (values_size == 0)
	{
		// With a zero-length vector, the return is a zero-length vector of the type specified by the method signature.  This
		// allows code to proceed without errors without having to check for the zero-length case, by simply propagating the
		// zero-length-ness forward while preserving the expected type for the expression.  If multiple return types are possible,
		// we return a zero-length vector for the simplest type supported.
		// BCH 24 March 2018: Decided this policy is a bit unsafe; now we only propagate a zero-length result forward if the
		// return type is unambiguous, otherwise it is an error.
		EidosValueMask sig_mask = (p_method_signature->return_mask_ & kEidosValueMaskFlagStrip);
		
		if (sig_mask == kEidosValueMaskVOID) return gStaticEidosValueVOID;
		if (sig_mask == kEidosValueMaskNULL) return gStaticEidosValueNULL;	// we assume visible NULL is the NULL desired, to avoid masking errors
		if (sig_mask == kEidosValueMaskLogical) return gStaticEidosValue_Logical_ZeroVec;
		if (sig_mask == kEidosValueMaskInt) return gStaticEidosValue_Integer_ZeroVec;
		if (sig_mask == kEidosValueMaskFloat) return gStaticEidosValue_Float_ZeroVec;
		if (sig_mask == kEidosValueMaskString) return gStaticEidosValue_String_ZeroVec;
		if (sig_mask == kEidosValueMaskObject)
		{
			const EidosClass *return_class = p_method_signature->return_class_;
			
			if (return_class)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(return_class));
			else
				return gStaticEidosValue_Object_ZeroVec;
		}
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object::ExecuteMethodCall): method " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << " does not specify an unambiguous return type, and thus cannot be called on a zero-length vector." << EidosTerminate(nullptr);
	}
	else if (p_method_signature->accelerated_imp_)
	{
		// the method is accelerated, so call through its accelerated imp; note we do this for singletons too,
		// because accelerated methods have no non-accelerated implementation, unlike accelerated properties
		EidosValue_SP result = p_method_signature->accelerated_imper_(values_, values_size, p_method_id, p_arguments, p_interpreter);
		
		p_method_signature->CheckAggregateReturn(*result, values_size);
		return result;
	}
	else if (values_size == 1)
	{
		// The singleton case is very common, so it should be special-cased for speed
		EidosObject *value = values_[0];
		EidosValue_SP result = value->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
		
		p_method_signature->CheckReturn(*result);
		return result;
	}
	else
	{
		// Call the method on all elements and collect the results.  This is a huge bottleneck for Eidos in many cases,
		// so it is worthwhile to try to be smart about it.  If the signature indicates that only one return type is
		// allowed, then we will use a special-case branch optimized for that one return type, avoiding quite a bit
		// of work.  In these single-return-type special-cases we will check the returns only when in DEBUG mode; if
		// a method fails to comply with its sole declared return type, that's not a bug that will escape notice.  :->
		// We do, however, have to deal with the fact that methods are allowed to return NULL without stating it in
		// their signature (see CheckReturn()), so we do have to do some minimal type-checking to elide NULLs.
		// When a single-return-type method is declared as returning singleton, we special-case that even further, by
		// reserving a buffer size equal to the size of the target vector and then using push_X_no_check(); if a lot
		// of NULLs are returned we will waste some memory, but the payoff of avoiding reallocs is worth it overall,
		// I imagine.  Yes, I have timed all of this, and it really does make a difference; calling a method on a
		// large object vector is very common, and these optimizations make a substantial difference.  In fact, I'm
		// tempted to go even further, down the road I went down with accelerated properties, but I will hold off on
		// that for now.  :->
		// BCH 12/20/2023: the XData() methods now allow us to avoid IsSingleton() and treat the cases jointly.
		EidosValueMask sig_mask = (p_method_signature->return_mask_ & kEidosValueMaskFlagStrip);
		bool return_is_singleton = (p_method_signature->return_mask_ & kEidosValueMaskSingleton);
		
		if (sig_mask == kEidosValueMaskVOID)
		{
			// No need to gather results at all, since they should all be void.
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
				p_method_signature->CheckReturn(*temp_result);
#endif
			}
			
			return gStaticEidosValueVOID;
		}
		else if (sig_mask == kEidosValueMaskNULL)
		{
			// We still need to make the method calls, but we don't need to gather results at all, except to note the invisible flag
			// ConcatenateEidosValues() returns invisible NULL only when all values concatenated are invisible; we mirror that.
			bool all_invisible = true;
			
			for (size_t value_index = 0; value_index < values_size; ++value_index)
			{
				EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
				p_method_signature->CheckReturn(*temp_result);
#endif
				if (!temp_result->Invisible())
					all_invisible = false;
			}
			
			return (all_invisible ? gStaticEidosValueNULLInvisible : gStaticEidosValueNULL);
		}
		else if (sig_mask == kEidosValueMaskLogical)
		{
			EidosValue_Logical *logical_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Logical();
			
			if (return_is_singleton)
			{
				logical_result->reserve(values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
					p_method_signature->CheckReturn(*temp_result);
#endif
					if (temp_result == gStaticEidosValue_LogicalF)
						logical_result->push_logical_no_check(false);
					else if (temp_result == gStaticEidosValue_LogicalT)
						logical_result->push_logical_no_check(true);
					else if (temp_result->Type() == EidosValueType::kValueLogical)
						logical_result->push_logical_no_check(((EidosValue_Logical *)temp_result.get())->data()[0]);
					// else it is a NULL, discard it
				}
			}
			else
			{
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
					p_method_signature->CheckReturn(*temp_result);
#endif
					if (temp_result == gStaticEidosValue_LogicalF)
						logical_result->push_logical(false);
					else if (temp_result == gStaticEidosValue_LogicalT)
						logical_result->push_logical(true);
					else if (temp_result->Type() == EidosValueType::kValueLogical)
					{
						// No singleton special-case for logical; EidosValue_Logical is always a vector
						int return_count = temp_result->Count();
						const eidos_logical_t *return_data = temp_result->LogicalData();
						
						for (int return_index = 0; return_index < return_count; return_index++)
							logical_result->push_logical(return_data[return_index]);
					}
					// else it is a NULL, discard it
				}
			}
			
			return EidosValue_SP(logical_result);
		}
		else if (sig_mask == kEidosValueMaskInt)
		{
			EidosValue_Int *integer_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int();
			
			if (return_is_singleton)
			{
				integer_result->reserve(values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
					p_method_signature->CheckReturn(*temp_result);
#endif
					if (temp_result->Type() == EidosValueType::kValueInt)
						integer_result->push_int_no_check(temp_result->IntData()[0]);
					// else it is a NULL, discard it
				}
			}
			else
			{
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
					p_method_signature->CheckReturn(*temp_result);
#endif
					if (temp_result->Type() == EidosValueType::kValueInt)
					{
						int return_count = temp_result->Count();
						const int64_t *return_data = temp_result->IntData();
						
						for (int return_index = 0; return_index < return_count; return_index++)
							integer_result->push_int(return_data[return_index]);
					}
					// else it is a NULL, discard it
				}
			}
			
			return EidosValue_SP(integer_result);
		}
		else if (sig_mask == kEidosValueMaskFloat)
		{
			EidosValue_Float *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float();
			
			if (return_is_singleton)
			{
				float_result->reserve(values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
					p_method_signature->CheckReturn(*temp_result);
#endif
					if (temp_result->Type() == EidosValueType::kValueFloat)
						float_result->push_float_no_check(temp_result->FloatData()[0]);
					// else it is a NULL, discard it
				}
			}
			else
			{
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
					p_method_signature->CheckReturn(*temp_result);
#endif
					if (temp_result->Type() == EidosValueType::kValueFloat)
					{
						int return_count = temp_result->Count();
						const double *return_data = temp_result->FloatData();
						
						for (int return_index = 0; return_index < return_count; return_index++)
							float_result->push_float(return_data[return_index]);
					}
					// else it is a NULL, discard it
				}
			}
			
			return EidosValue_SP(float_result);
		}
		//else if (sig_mask == kEidosValueMaskString)	// no special-case for string for now, since EidosValue_String_vector is different from the others...
		//{
		//}
		else if (sig_mask == kEidosValueMaskObject && p_method_signature->return_class_)
		{
			const EidosClass *return_class = p_method_signature->return_class_;
			EidosValue_Object *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(return_class);
			
			if (return_is_singleton)
			{
				object_result->reserve(values_size);
				
				if (return_class->UsesRetainRelease())
				{
					for (size_t value_index = 0; value_index < values_size; ++value_index)
					{
						EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
						p_method_signature->CheckReturn(*temp_result);
#endif
						if (temp_result->Type() == EidosValueType::kValueObject)
							object_result->push_object_element_no_check_RR(temp_result->ObjectData()[0]);
						// else it is a NULL, discard it
					}
				}
				else
				{
					for (size_t value_index = 0; value_index < values_size; ++value_index)
					{
						EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
						p_method_signature->CheckReturn(*temp_result);
#endif
						if (temp_result->Type() == EidosValueType::kValueObject)
							object_result->push_object_element_no_check_NORR(temp_result->ObjectData()[0]);
						// else it is a NULL, discard it
					}
				}
			}
			else
			{
				if (return_class->UsesRetainRelease())
				{
					for (size_t value_index = 0; value_index < values_size; ++value_index)
					{
						EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
						p_method_signature->CheckReturn(*temp_result);
#endif
						if (temp_result->Type() == EidosValueType::kValueObject)
						{
							int return_count = temp_result->Count();
							EidosObject * const *return_data = temp_result->ObjectData();
							
							for (int return_index = 0; return_index < return_count; return_index++)
								object_result->push_object_element_RR(return_data[return_index]);
						}
						// else it is a NULL, discard it
					}
				}
				else
				{
					for (size_t value_index = 0; value_index < values_size; ++value_index)
					{
						EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
						p_method_signature->CheckReturn(*temp_result);
#endif
						if (temp_result->Type() == EidosValueType::kValueObject)
						{
							int return_count = temp_result->Count();
							EidosObject * const *return_data = temp_result->ObjectData();
							
							for (int return_index = 0; return_index < return_count; return_index++)
								object_result->push_object_element_NORR(return_data[return_index]);
						}
						// else it is a NULL, discard it
					}
				}
			}
			
			return EidosValue_SP(object_result);
		}
		else
		{
			std::vector<EidosValue_SP> results;
			
			if (values_size < 10)
			{
				// with small objects, we check every value
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
					
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
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
					
					if (!checked_multivalued)
					{
						p_method_signature->CheckReturn(*temp_result);
						checked_multivalued = true;
					}
					
					results.emplace_back(temp_result);
				}
			}
			
			// concatenate the results using ConcatenateEidosValues()
			EidosValue_SP result = ConcatenateEidosValues(results, true, false);	// allow NULL, don't allow VOID
			
			return result;
		}
	}
}

void EidosValue_Object::clear(void)
{
	WILL_MODIFY(this);
	
	if (class_uses_retain_release_)
	{
		for (size_t index = 0; index < count_; ++index)
		{
			EidosObject *value = values_[index];
			
			if (value)
				static_cast<EidosDictionaryRetained *>(value)->Release();		// unsafe cast to avoid virtual function overhead
		}
	}
	
	count_ = 0;
}

EidosValue_Object *EidosValue_Object::reserve(size_t p_reserved_size)
{
	WILL_MODIFY(this);
	
	if (p_reserved_size > capacity_)
	{
		// this is a reservation for an explicit size, so we give that size exactly, to avoid wasting space
		
		if (values_ == &singleton_value_)
		{
			values_ = (EidosObject **)malloc(p_reserved_size * sizeof(EidosObject *));
			
			if (!values_)
				EIDOS_TERMINATION << "ERROR (EidosValue_Object::reserve): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			values_[0] = singleton_value_;
		}
		else
		{
			values_ = (EidosObject **)realloc(values_, p_reserved_size * sizeof(EidosObject *));
			if (!values_)
				EIDOS_TERMINATION << "ERROR (EidosValue_Object::reserve): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		}
		
		capacity_ = p_reserved_size;
	}
	
	return this;
}

EidosValue_Object *EidosValue_Object::resize_no_initialize(size_t p_new_size)
{
	WILL_MODIFY(this);
	
	reserve(p_new_size);	// might set a capacity greater than p_new_size; no guarantees
	
	// deal with retain/release
	if (class_uses_retain_release_)
	{
		if (p_new_size < count_)
		{
			// shrinking; need to release the elements made redundant
			for (size_t element_index = p_new_size; element_index < count_; ++element_index)
			{
				EidosObject *value = values_[element_index];
				
				if (value)
					static_cast<EidosDictionaryRetained *>(value)->Release();		// unsafe cast to avoid virtual function overhead
			}
		}
		else if (p_new_size > count_)
		{
			// expanding; need to zero out the new slots, despite our name (but only with class_uses_retain_release_!)
			for (size_t element_index = count_; element_index < p_new_size; ++element_index)
				values_[element_index] = nullptr;
		}
	}
	
	count_ = p_new_size;	// regardless of the capacity set, set the size to exactly p_new_size
	
	return this;
}

void EidosValue_Object::erase_index(size_t p_index)
{
	WILL_MODIFY(this);
	
	if (p_index >= count_)
		RaiseForRangeViolation();
	
	if (class_uses_retain_release_)
		if (values_[p_index])
			static_cast<EidosDictionaryRetained *>(values_[p_index])->Release();		// unsafe cast to avoid virtual function overhead
	
	if (p_index == count_ - 1)
		--count_;
	else
	{
		EidosObject **element_ptr = values_ + p_index;
		EidosObject **next_element_ptr = values_ + p_index + 1;
		EidosObject **past_end_element_ptr = values_ + count_;
		size_t element_copy_count = past_end_element_ptr - next_element_ptr;
		
		memmove(element_ptr, next_element_ptr, element_copy_count * sizeof(EidosObject *));
		
		--count_;
	}
}















































