//
//  eidos_value.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
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

EidosObjectClass *gEidos_UndefinedClassObject = new EidosObjectClass();


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

std::string StringForEidosValueMask(const EidosValueMask p_mask, const EidosObjectClass *p_object_class, const std::string &p_name, EidosValue *p_default)
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
		EidosObjectElement *element1 = p_value1.ObjectElementAtIndex(p_index1, p_blame_token);
		EidosObjectElement *element2 = p_value2.ObjectElementAtIndex(p_index2, p_blame_token);
		
		if (p_operator == EidosComparisonOperator::kEqual)			return (element1 == element2);
		else if (p_operator == EidosComparisonOperator::kNotEqual)	return (element1 != element2);
		else
			EIDOS_TERMINATION << "ERROR (CompareEidosValues): (internal error) objects may only be compared with == and !=." << EidosTerminate(p_blame_token);
	}
	
	// string is the highest type, so we promote to string if either operand is a string
	if ((type1 == EidosValueType::kValueString) || (type2 == EidosValueType::kValueString))
	{
		std::string string1 = p_value1.StringAtIndex(p_index1, p_blame_token);
		std::string string2 = p_value2.StringAtIndex(p_index2, p_blame_token);
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
		double float1 = p_value1.FloatAtIndex(p_index1, p_blame_token);
		double float2 = p_value2.FloatAtIndex(p_index2, p_blame_token);
		
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
		int64_t int1 = p_value1.IntAtIndex(p_index1, p_blame_token);
		int64_t int2 = p_value2.IntAtIndex(p_index2, p_blame_token);
		
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
		eidos_logical_t logical1 = p_value1.LogicalAtIndex(p_index1, p_blame_token);
		eidos_logical_t logical2 = p_value2.LogicalAtIndex(p_index2, p_blame_token);
		
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

EidosValue::EidosValue(EidosValueType p_value_type, bool p_singleton) : intrusive_ref_count_(0), cached_type_(p_value_type), invisible_(false), is_singleton_(p_singleton), dim_(nullptr)
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
	
	free(dim_);
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

void EidosValue::RaiseForUnsupportedConversionCall(const EidosToken *p_blame_token) const
{
	EIDOS_TERMINATION << "ERROR (EidosValue::RaiseForUnsupportedConversionCall): an EidosValue cannot be converted to the requested type." << EidosTerminate(p_blame_token);
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

EidosValue_SP EidosValue::VectorBasedCopy(void) const
{
	return CopyValues();
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
		
		inclusion_counts.push_back(dim_size);
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
		static int64_t *static_dim_buffer = nullptr;
		static int static_dim_buffer_size = -1;
		
		if (dimcount > static_dim_buffer_size)
		{
			static_dim_buffer = (int64_t *)realloc(static_dim_buffer, (dimcount + 1) * sizeof(int64_t));	// +1 so the zero case doesn't result in a zero-size allocation
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

void EidosValue::PrintMatrixFromIndex(int64_t p_ncol, int64_t p_nrow, int64_t p_start_index, std::ostream &p_ostream) const
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
	for (int pad_index = 0; pad_index < max_rowcol_width; pad_index++)
		p_ostream << ' ';
	
	// print the column headers
	for (int col_index = 0; col_index < p_ncol; ++col_index)
	{
		// pad before column header
		{
			int element_width = (col_index == 0) ? 4 : ((int)floor(log10(col_index)) + 4);
			int padding = (max_element_width - element_width) + 1;
			
			for (int pad_index = 0; pad_index < padding; pad_index++)
				p_ostream << ' ';
		}
		
		// column header
		p_ostream << "[," << col_index << "]";
	}
	
	// print each row
	for (int row_index = 0; row_index < p_nrow; ++row_index)
	{
		p_ostream << std::endl;
		
		// pad before row index
		{
			int element_width = (row_index == 0) ? 4 : ((int)floor(log10(row_index)) + 4);
			int padding = (max_rowcol_width - element_width);
			
			for (int pad_index = 0; pad_index < padding; pad_index++)
				p_ostream << ' ';
		}
		
		// row index
		p_ostream << '[' << row_index << ",]";
		
		// row contents
		for (int col_index = 0; col_index < p_ncol; ++col_index)
		{
			std::string &element_string = element_strings[row_index + col_index * p_nrow];
			int element_width = (int)element_string.length();
			int padding = (max_element_width - element_width) + 1;
			
			for (int pad_index = 0; pad_index < padding; pad_index++)
				p_ostream << ' ';
			
			p_ostream << element_string;
		}
	}
}

void EidosValue::Print(std::ostream &p_ostream) const
{
	int count = Count();
	
	if (count == 0)
	{
		// standard format for zero-length vectors
		EidosValueType type = Type();
		
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
		for (int value_index = 0; value_index < count; ++value_index)
		{
			if (value_index > 0)
				p_ostream << ' ';
			
			PrintValueAtIndex(value_index, p_ostream);
		}
	}
	else if (dim_[0] == 2)
	{
		PrintMatrixFromIndex(dim_[2], dim_[1], 0, p_ostream);
	}
	else if (dim_[0] > 2)
	{
		// print arrays by looping over dimensions
		int64_t dim_count = dim_[0];
		int64_t matrix_skip = dim_[1] * dim_[2];	// number of elements in each nxm matrix slice
		int64_t array_dim_count = dim_count - 2;	// this many additional dimensions of nxm matrix slices
		int64_t *array_dim_indices = (int64_t *)calloc(array_dim_count, sizeof(int64_t));
		int64_t *array_dim_skip = (int64_t *)calloc(array_dim_count, sizeof(int64_t));
		
		array_dim_skip[0] = matrix_skip;
		for (int array_dim_index = 1; array_dim_index < array_dim_count; ++array_dim_index)
			array_dim_skip[array_dim_index] = array_dim_skip[array_dim_index - 1] * dim_[array_dim_index + 2];
		
		do
		{
			// print an index line before the matrix
			p_ostream << ", ";
			
			for (int idx = 0; idx < array_dim_count; ++idx)
				p_ostream << ", " << array_dim_indices[idx];
			
			p_ostream << std::endl << std::endl;
			
			// print out the matrix for this slice
			int slice_offset = 0;
			
			for (int idx = 0; idx < array_dim_count; ++idx)
				slice_offset += array_dim_skip[idx] * array_dim_indices[idx];
			
			PrintMatrixFromIndex(dim_[2], dim_[1], slice_offset, p_ostream);
			p_ostream << std::endl;
			
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
			
			p_ostream << std::endl;
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


//
//	EidosValue_VOID
//
#pragma mark -
#pragma mark EidosValue_VOID
#pragma mark -

/* static */ EidosValue_VOID_SP EidosValue_VOID::Static_EidosValue_VOID(void)
{
	// this is a truly permanent constant object
	static EidosValue_VOID_SP static_void = EidosValue_VOID_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_VOID());
	
	static_void->invisible_ = true;		// set every time, since we don't have a constructor to set invisibility
	
	return static_void;
}

const std::string &EidosValue_VOID::ElementType(void) const
{
	return gEidosStr_void;
}

int EidosValue_VOID::Count_Virtual(void) const
{
	return 0;
}

void EidosValue_VOID::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
#pragma unused(p_idx)
	p_ostream << gEidosStr_void;
}

EidosValue_SP EidosValue_VOID::GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const
{
#pragma unused(p_idx, p_blame_token)
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::GetValueAtIndex): (internal error) illegal on void." << EidosTerminate(p_blame_token);
}

void EidosValue_VOID::SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token)
{
#pragma unused(p_idx, p_value)
	EIDOS_TERMINATION << "ERROR (EidosValue_VOID::SetValueAtIndex): (internal error) illegal on void." << EidosTerminate(p_blame_token);
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

void EidosValue_NULL::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
#pragma unused(p_idx)
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
	
	for (auto init_item : p_init_list)
		push_logical_no_check(init_item);
}

EidosValue_Logical::EidosValue_Logical(const eidos_logical_t *p_values, size_t p_count) : EidosValue(EidosValueType::kValueLogical, false)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_logical_no_check(p_values[index], index);
}

const std::string &EidosValue_Logical::ElementType(void) const
{
	return gEidosStr_logical;
}

int EidosValue_Logical::Count_Virtual(void) const
{
	return (int)count_;
}

void EidosValue_Logical::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	eidos_logical_t value = values_[p_idx];
	
	p_ostream << (value ? gEidosStr_T : gEidosStr_F);
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(values_, count_))->CopyDimensionsFromValue(this));
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(values_, count_))->CopyDimensionsFromValue(this));	// same as EidosValue_Logical::, but let's not rely on that
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

void EidosValue_Logical_const::_CopyDimensionsFromValue(const EidosValue *p_value)
{
	if (p_value->DimensionCount() != 1)
	{
		EIDOS_TERMINATION << "ERROR (EidosValue_Logical_const::_CopyDimensionsFromValue): (internal error) EidosValue_Logical_const is not modifiable." << EidosTerminate(nullptr);
	}
}


//
//	EidosValue_String
//
#pragma mark -
#pragma mark EidosValue_String
#pragma mark -

const std::string &EidosValue_String::ElementType(void) const
{
	return gEidosStr_string;
}

EidosValue_SP EidosValue_String::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
}

void EidosValue_String::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	const std::string &value = StringAtIndex(p_idx, nullptr);
	
	// Emit a quoted string.  Note that we do not attempt to escape characters so that the emitted string
	// is a legal string for input into Eidos that would reproduce the original string, at present.
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


// EidosValue_String_vector
#pragma mark EidosValue_String_vector

EidosValue_String_vector::EidosValue_String_vector(const std::vector<std::string> &p_stringvec) : EidosValue_String(false), values_(p_stringvec)
{
}

EidosValue_String_vector::EidosValue_String_vector(std::initializer_list<const std::string> p_init_list) : EidosValue_String(false)
{
	for (auto init_item = p_init_list.begin(); init_item != p_init_list.end(); init_item++)
		values_.emplace_back(*init_item);
}

int EidosValue_String_vector::Count_Virtual(void) const
{
	return (int)values_.size();
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
	if ((converted_value < (double)INT64_MIN) || (converted_value >= (double)INT64_MAX))
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector(values_))->CopyDimensionsFromValue(this));
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

int EidosValue_String_singleton::Count_Virtual(void) const
{
	return 1;
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
	if ((converted_value < (double)INT64_MIN) || (converted_value >= (double)INT64_MAX))
		EIDOS_TERMINATION << "ERROR (EidosValue_String_singleton::IntAtIndex): \"" << value_ << "\" could not be represented as an integer (out of range)." << EidosTerminate(p_blame_token);
	
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(value_))->CopyDimensionsFromValue(this));
}

EidosValue_SP EidosValue_String_singleton::VectorBasedCopy(void) const
{
	EidosValue_String_vector_SP new_vec = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
	
	new_vec->PushString(value_);
	new_vec->CopyDimensionsFromValue(this);
	
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

const std::string &EidosValue_Int::ElementType(void) const
{
	return gEidosStr_integer;
}

EidosValue_SP EidosValue_Int::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
}

void EidosValue_Int::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	int64_t value = IntAtIndex(p_idx, nullptr);
	
	p_ostream << value;
}


// EidosValue_Int_vector
#pragma mark EidosValue_Int_vector

EidosValue_Int_vector::EidosValue_Int_vector(const std::vector<int16_t> &p_intvec) : EidosValue_Int(false)
{
	size_t count = p_intvec.size();
	const int16_t *values = p_intvec.data();
	
	resize_no_initialize(count);
	
	for (size_t index = 0; index < count; ++index)
		set_int_no_check(values[index], index);
}

EidosValue_Int_vector::EidosValue_Int_vector(const std::vector<int32_t> &p_intvec) : EidosValue_Int(false)
{
	size_t count = p_intvec.size();
	const int32_t *values = p_intvec.data();
	
	resize_no_initialize(count);
	
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
	
	for (auto init_item : p_init_list)
		push_int_no_check(init_item);
}

EidosValue_Int_vector::EidosValue_Int_vector(const int64_t *p_values, size_t p_count) : EidosValue_Int(false)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_int_no_check(p_values[index], index);
}

int EidosValue_Int_vector::Count_Virtual(void) const
{
	return (int)count_;
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(values_, count_))->CopyDimensionsFromValue(this));
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

int EidosValue_Int_singleton::Count_Virtual(void) const
{
	return 1;
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(value_))->CopyDimensionsFromValue(this));
}

EidosValue_SP EidosValue_Int_singleton::VectorBasedCopy(void) const
{
	// We intentionally don't reserve a size of 1 here, on the assumption that further values are likely to be added
	EidosValue_Int_vector_SP new_vec = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
	
	new_vec->push_int(value_);
	new_vec->CopyDimensionsFromValue(this);
	
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

const std::string &EidosValue_Float::ElementType(void) const
{
	return gEidosStr_float;
}

EidosValue_SP EidosValue_Float::NewMatchingType(void) const
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
}

void EidosValue_Float::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	p_ostream << EidosStringForFloat(FloatAtIndex(p_idx, nullptr));
}


// EidosValue_Float_vector
#pragma mark EidosValue_Float_vector

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
	
	for (auto init_item : p_init_list)
		push_float_no_check(init_item);
}

EidosValue_Float_vector::EidosValue_Float_vector(const double *p_values, size_t p_count) : EidosValue_Float(false)
{
	resize_no_initialize(p_count);
	
	for (size_t index = 0; index < p_count; ++index)
		set_float_no_check(p_values[index], index);
}

int EidosValue_Float_vector::Count_Virtual(void) const
{
	return (int)count_;
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
	
	return EidosStringForFloat(values_[p_idx]);
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
	if ((value < (double)INT64_MIN) || (value >= (double)INT64_MAX))
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(values_, count_))->CopyDimensionsFromValue(this));
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
		std::sort(values_, values_ + count_, [](const double& a, const double& b) { return std::isnan(b) || (a < b); });
	else
		std::sort(values_, values_ + count_, [](const double& a, const double& b) { return std::isnan(b) || (a > b); });
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

int EidosValue_Float_singleton::Count_Virtual(void) const
{
	return 1;
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
	
	return EidosStringForFloat(value_);
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
	if ((value_ < (double)INT64_MIN) || (value_ >= (double)INT64_MAX))
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
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(value_))->CopyDimensionsFromValue(this));
}

EidosValue_SP EidosValue_Float_singleton::VectorBasedCopy(void) const
{
	// We intentionally don't reserve a size of 1 here, on the assumption that further values are likely to be added
	EidosValue_Float_vector_SP new_vec = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
	
	new_vec->push_float(value_);
	new_vec->CopyDimensionsFromValue(this);
	
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

// See comments on EidosValue_Object::EidosValue_Object() below
std::vector<EidosValue_Object *> gEidosValue_Object_Mutation_Registry;
std::vector<EidosValue_Object *> gEidosValue_Object_Genome_Registry;
std::vector<EidosValue_Object *> gEidosValue_Object_Individual_Registry;

EidosValue_Object::EidosValue_Object(bool p_singleton, const EidosObjectClass *p_class) : EidosValue(EidosValueType::kValueObject, p_singleton), class_(p_class),
	class_uses_retain_release_(p_class == gEidos_UndefinedClassObject ? true : p_class->UsesRetainRelease())
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
	
	// BCH 11 January 2018: Sadly, I have found it necessary to extend this hack to Individual and Genome as well.
	// See Subpopulation::ExecuteMethod_takeMigrants() for the rationale; basically, references to those classes
	// need to be found and patched when migration occurs in nonWF models, for technical reasons.  I apologize
	// in advance to anyone who encounters this hack.  I have also added registered_for_patching_ here, to allow
	// us to avoid patching self-references; see the comments on the constructor below.
	const std::string *element_type = &(class_->ElementType());
										
	if (element_type == &gEidosStr_Mutation)
	{
		gEidosValue_Object_Mutation_Registry.push_back(this);
		registered_for_patching_ = true;
		
		//std::cout << "pushed Mutation EidosValue_Object, count == " << gEidosValue_Object_Mutation_Registry.size() << std::endl;
	}
	else if (element_type == &gEidosStr_Genome)
	{
		gEidosValue_Object_Genome_Registry.push_back(this);
		registered_for_patching_ = true;
		
		//std::cout << "pushed Genome EidosValue_Object, count == " << gEidosValue_Object_Genome_Registry.size() << std::endl;
	}
	else if (element_type == &gEidosStr_Individual)
	{
		gEidosValue_Object_Individual_Registry.push_back(this);
		registered_for_patching_ = true;
		
		//std::cout << "pushed Individual EidosValue_Object, count == " << gEidosValue_Object_Individual_Registry.size() << std::endl;
	}
	else
	{
		// save a bit of time for other classes by avoiding the class check in the destructor, since it's free to do so
		registered_for_patching_ = false;
	}
}

EidosValue_Object::EidosValue_Object(bool p_singleton, const EidosObjectClass *p_class, __attribute__((unused)) bool p_register_for_patching) : EidosValue(EidosValueType::kValueObject, p_singleton), class_(p_class),
	class_uses_retain_release_(p_class == gEidos_UndefinedClassObject ? true : p_class->UsesRetainRelease())
{
	// This special constructor variant skips the registration done in the body of the standard constructor above.
	// Note that the value of p_register_for_patching is UNUSED; its purpose is merely to select this alternative
	// constructor, its value is irrelevant.  The reason we want to skip registration for some EidosValues is that
	// they are self-pointers; some objects, such as Individual and Genome, contain a cached EidosValue that refers
	// to the object, as an EidosValue self-reference.  Regardless of what pointer-patching may need to be done,
	// such self-pointers should never be patched; you never want the self-pointer of one object to point to a
	// different object!  Also, we make many of these self-pointers and they clog up the registries and make them
	// slow, so we want to avoid registering them anyway.  See the constructor above for further comments.
	registered_for_patching_ = false;
}

EidosValue_Object::~EidosValue_Object(void)
{
	// See comment on EidosValue_Object::EidosValue_Object() above
	if (registered_for_patching_)
	{
		const std::string *element_type = &(class_->ElementType());
		
		if (element_type == &gEidosStr_Mutation)
		{
			auto erase_iter = std::find(gEidosValue_Object_Mutation_Registry.begin(), gEidosValue_Object_Mutation_Registry.end(), this);
			
			if (erase_iter != gEidosValue_Object_Mutation_Registry.end())
				gEidosValue_Object_Mutation_Registry.erase(erase_iter);
			else
				EIDOS_TERMINATION << "ERROR (EidosValue_Object::~EidosValue_Object): (internal error) unregistered EidosValue_Object of class Mutation." << EidosTerminate(nullptr);
			
			//std::cout << "popped Mutation EidosValue_Object, count == " << gEidosValue_Object_Mutation_Registry.size() << std::endl;
		}
		else if (element_type == &gEidosStr_Genome)
		{
			auto erase_iter = std::find(gEidosValue_Object_Genome_Registry.begin(), gEidosValue_Object_Genome_Registry.end(), this);
			
			if (erase_iter != gEidosValue_Object_Genome_Registry.end())
				gEidosValue_Object_Genome_Registry.erase(erase_iter);
			else
				EIDOS_TERMINATION << "ERROR (EidosValue_Object::~EidosValue_Object): (internal error) unregistered EidosValue_Object of class Genome." << EidosTerminate(nullptr);
			
			//std::cout << "popped Genome EidosValue_Object, count == " << gEidosValue_Object_Genome_Registry.size() << std::endl;
		}
		else if (element_type == &gEidosStr_Individual)
		{
			auto erase_iter = std::find(gEidosValue_Object_Individual_Registry.begin(), gEidosValue_Object_Individual_Registry.end(), this);
			
			if (erase_iter != gEidosValue_Object_Individual_Registry.end())
				gEidosValue_Object_Individual_Registry.erase(erase_iter);
			else
				EIDOS_TERMINATION << "ERROR (EidosValue_Object::~EidosValue_Object): (internal error) unregistered EidosValue_Object of class Individual." << EidosTerminate(nullptr);
			
			//std::cout << "popped Individual EidosValue_Object, count == " << gEidosValue_Object_Individual_Registry.size() << std::endl;
		}
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
	EIDOS_TERMINATION << "ERROR (EidosValue_Object::RaiseForClassMismatch): the type of an object cannot be changed." << EidosTerminate(nullptr);
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

void EidosValue_Object::PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const
{
	EidosObjectElement *value = ObjectElementAtIndex(p_idx, nullptr);
	
	p_ostream << *value;
}


// EidosValue_Object_vector
#pragma mark EidosValue_Object_vector

EidosValue_Object_vector::EidosValue_Object_vector(const EidosValue_Object_vector &p_original) : EidosValue_Object(false, p_original.Class())
{
	size_t count = p_original.size();
	EidosObjectElement * const *values = p_original.data();
	
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

EidosValue_Object_vector::EidosValue_Object_vector(const std::vector<EidosObjectElement *> &p_elementvec, const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
{
	size_t count = p_elementvec.size();
	EidosObjectElement * const *values = p_elementvec.data();
	
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

EidosValue_Object_vector::EidosValue_Object_vector(std::initializer_list<EidosObjectElement *> p_init_list, const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
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

EidosValue_Object_vector::EidosValue_Object_vector(EidosObjectElement **p_values, size_t p_count, const EidosObjectClass *p_class) : EidosValue_Object(false, p_class)
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

EidosValue_Object_vector::~EidosValue_Object_vector(void)
{
	if (class_uses_retain_release_)
	{
		for (size_t index = 0; index < count_; ++index)
		{
			EidosObjectElement *value = values_[index];
			
			if (value)
				static_cast<EidosObjectElement_Retained *>(value)->Release();		// unsafe cast to avoid virtual function overhead
		}
	}
	
	free(values_);
}

int EidosValue_Object_vector::Count_Virtual(void) const
{
	return (int)size();
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
	
	if (class_uses_retain_release_)
	{
		static_cast<EidosObjectElement_Retained *>(new_value)->Retain();				// unsafe cast to avoid virtual function overhead
		if (values_[p_idx])
			static_cast<EidosObjectElement_Retained *>(values_[p_idx])->Release();		// unsafe cast to avoid virtual function overhead
	}
	
	values_[p_idx] = new_value;
}

EidosValue_SP EidosValue_Object_vector::CopyValues(void) const
{
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(*this))->CopyDimensionsFromValue(this));
}

void EidosValue_Object_vector::PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token)
{
	if (p_source_script_value.Type() == EidosValueType::kValueObject)
		push_object_element_CRR(p_source_script_value.ObjectElementAtIndex(p_idx, p_blame_token));
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
		case EidosValueType::kValueVOID:
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
			resize_no_initialize_RR(0);
			
			if (class_uses_retain_release_)
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_RR(sorted_pair.second);
			}
			else
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_NORR(sorted_pair.second);
			}
			
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
			resize_no_initialize_RR(0);
			
			if (class_uses_retain_release_)
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_RR(sorted_pair.second);
			}
			else
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_NORR(sorted_pair.second);
			}
			
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
			resize_no_initialize_RR(0);
			
			if (class_uses_retain_release_)
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_RR(sorted_pair.second);
			}
			else
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_NORR(sorted_pair.second);
			}
			
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
			resize_no_initialize_RR(0);
			
			if (class_uses_retain_release_)
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_RR(sorted_pair.second);
			}
			else
			{
				for (auto sorted_pair : sortable_pairs)
					push_object_element_no_check_NORR(sorted_pair.second);
			}
			
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
			const EidosObjectClass *value_class = signature->value_class_;
			
			if (value_class)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(value_class));
			else
				return gStaticEidosValue_Object_ZeroVec;
		}
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::GetPropertyOfElements): property " << Eidos_StringForGlobalStringID(p_property_id) << " does not specify an unambiguous value type, and thus cannot be accessed on a zero-length vector." << EidosTerminate(nullptr);
	}
	else if (values_size == 1)
	{
		// the singleton case is very common, so it should be special-cased for speed
		// accelerated property is not used for singletons because we want to generate a singleton-class result
		// of course the accelerated getters could be smart enough to do that, but that makes them more complex; not a clear win
		EidosObjectElement *value = values_[0];
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
				EidosObjectElement *value = values_[value_index];
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
#endif
		
		// concatenate the results using ConcatenateEidosValues()
		EidosValue_SP result = ConcatenateEidosValues(results, true, false);	// allow NULL, don't allow VOID
		
		// Access of singleton properties retains the matrix/array structure of the target
		if (signature->value_mask_ & kEidosValueMaskSingleton)
			result->CopyDimensionsFromValue(this);
		
		return result;
	}
}

void EidosValue_Object_vector::SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): property " << Eidos_StringForGlobalStringID(p_property_id) << " is not defined for object element type " << ElementType() << "." << EidosTerminate(nullptr);
	
	bool exact_match = signature->CheckAssignedValue(p_value);
	
	// We have to check the count ourselves; the signature does not do that for us
	size_t p_value_count = p_value.Count();
	size_t values_size = (size_t)size();
	
	if (p_value_count == 1)
	{
		// we have a multiplex assignment of one value to (maybe) more than one element: x.foo = 10
		if (signature->accelerated_set_)
		{
			// Accelerated property writing is enabled for this property, so we call the setter directly
			// Unlike the vector case below, this should work with implicit promotion, because accelerated
			// setters will get the singleton value safely, not by direct vector access
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
				// However, accelerated setting of vectors requires that the value type matches the property
				// type, without promotion, so we check that condition first and fall through if not
				if (exact_match)
				{
					signature->accelerated_setter(values_, values_size, p_value, p_value_count);
					return;
				}
			}
			
			// we have a one-to-one assignment of values to elements: x.foo = 1:5 (where x has 5 elements)
			for (size_t value_idx = 0; value_idx < p_value_count; value_idx++)
			{
				EidosValue_SP temp_rvalue = p_value.GetValueAtIndex((int)value_idx, nullptr);
				
				values_[value_idx]->SetProperty(p_property_id, *temp_rvalue);
			}
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::SetPropertyOfElements): assignment to a property requires an rvalue that is a singleton (multiplex assignment) or that has a .size() matching the .size of the lvalue." << EidosTerminate(nullptr);
}

EidosValue_SP EidosValue_Object_vector::ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosInstanceMethodSignature *p_method_signature, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// This is an instance method, so it gets dispatched to all of our elements
	auto values_size = size();
	
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
			const EidosObjectClass *return_class = p_method_signature->return_class_;
			
			if (return_class)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(return_class));
			else
				return gStaticEidosValue_Object_ZeroVec;
		}
		
		EIDOS_TERMINATION << "ERROR (EidosValue_Object_vector::ExecuteMethodCall): method " << Eidos_StringForGlobalStringID(p_method_id) << " does not specify an unambiguous return type, and thus cannot be called on a zero-length vector." << EidosTerminate(nullptr);
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
		EidosObjectElement *value = values_[0];
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
		// I imagine.  In the singleton case we also avoid a virtual function call per element by checking for the
		// return's IsSingleton() and doing a cast if it returns YES, allowing direct access to its value.  Yes, I have
		// timed all of this, and it really does make a difference; calling a method on a large object vector is very
		// common, and these optimizations make a substantial difference.  In fact, I'm tempted to go even further,
		// down the road I went down with accelerated properties, but I will hold off on that for now.  :->
		EidosValueMask sig_mask = (p_method_signature->return_mask_ & kEidosValueMaskFlagStrip);
		bool return_is_singleton = (p_method_signature->return_mask_ & kEidosValueMaskSingleton);
		
#if 0
		// Log vectorized calls to methods, to assess which methods are most worth accelerating
		if (values_size > 10)
			std::cerr << "Vector call to method " << Eidos_StringForGlobalStringID(p_method_id) << " on class " << class_->ElementType() << " (" << values_size << " elements)" << std::endl;
#endif
		
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
						const eidos_logical_t *return_data = temp_result->LogicalVector()->data();
						
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
			EidosValue_Int_vector *integer_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			
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
					{
						if (temp_result->IsSingleton())
							integer_result->push_int_no_check(((EidosValue_Int_singleton *)temp_result.get())->IntValue());
						else
							integer_result->push_int_no_check(temp_result->IntAtIndex(0, nullptr));		// should not generally get hit since the method should return a singleton
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
					if (temp_result->Type() == EidosValueType::kValueInt)
					{
						int return_count = temp_result->Count();
						
						if ((return_count == 1) && temp_result->IsSingleton())
						{
							integer_result->push_int(((EidosValue_Int_singleton *)temp_result.get())->IntValue());
						}
						else
						{
							const int64_t *return_data = temp_result->IntVector()->data();
							
							for (int return_index = 0; return_index < return_count; return_index++)
								integer_result->push_int(return_data[return_index]);
						}
					}
					// else it is a NULL, discard it
				}
			}
			
			return EidosValue_SP(integer_result);
		}
		else if (sig_mask == kEidosValueMaskFloat)
		{
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			
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
					{
						if (temp_result->IsSingleton())
							float_result->push_float_no_check(((EidosValue_Float_singleton *)temp_result.get())->FloatValue());
						else
							float_result->push_float_no_check(temp_result->FloatAtIndex(0, nullptr));		// should not generally get hit since the method should return a singleton
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
					if (temp_result->Type() == EidosValueType::kValueFloat)
					{
						int return_count = temp_result->Count();
						
						if ((return_count == 1) && temp_result->IsSingleton())
						{
							float_result->push_float(((EidosValue_Float_singleton *)temp_result.get())->FloatValue());
						}
						else
						{
							const double *return_data = temp_result->FloatVector()->data();
							
							for (int return_index = 0; return_index < return_count; return_index++)
								float_result->push_float(return_data[return_index]);
						}
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
			const EidosObjectClass *return_class = p_method_signature->return_class_;
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(return_class);
			
			if (return_is_singleton)
			{
				object_result->reserve(values_size);
				
				for (size_t value_index = 0; value_index < values_size; ++value_index)
				{
					EidosValue_SP temp_result = values_[value_index]->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
#if DEBUG
					p_method_signature->CheckReturn(*temp_result);
#endif
					if (temp_result->Type() == EidosValueType::kValueObject)
					{
						if (temp_result->IsSingleton())
							object_result->push_object_element_no_check_CRR(((EidosValue_Object_singleton *)temp_result.get())->ObjectElementValue());
						else
							object_result->push_object_element_no_check_CRR(temp_result->ObjectElementAtIndex(0, nullptr));		// should not generally get hit since the method should return a singleton
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
						
						if ((return_count == 1) && temp_result->IsSingleton())
						{
							object_result->push_object_element_CRR(((EidosValue_Object_singleton *)temp_result.get())->ObjectElementValue());
						}
						else
						{
							EidosObjectElement * const *return_data = temp_result->ObjectElementVector()->data();
							
							for (int return_index = 0; return_index < return_count; return_index++)
								object_result->push_object_element_CRR(return_data[return_index]);
						}
					}
					// else it is a NULL, discard it
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
	
	// deal with retain/release
	if (class_uses_retain_release_)
	{
		if (p_new_size < count_)
		{
			// shrinking; need to release the elements made redundant
			for (size_t element_index = p_new_size; element_index < count_; ++element_index)
			{
				EidosObjectElement *value = values_[element_index];
				
				if (value)
					static_cast<EidosObjectElement_Retained *>(value)->Release();		// unsafe cast to avoid virtual function overhead
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

EidosValue_Object_vector *EidosValue_Object_vector::resize_no_initialize_RR(size_t p_new_size)
{
	reserve(p_new_size);	// might set a capacity greater than p_new_size; no guarantees
	
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
	
	if (class_uses_retain_release_)
		if (values_[p_index])
			static_cast<EidosObjectElement_Retained *>(values_[p_index])->Release();		// unsafe cast to avoid virtual function overhead
	
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

EidosValue_Object_singleton::EidosValue_Object_singleton(EidosObjectElement *p_element1, const EidosObjectClass *p_class) : EidosValue_Object(true, p_class), value_(p_element1)
{
	// we want to allow nullptr as a momentary placeholder, although in general a value should exist
	if (p_element1)
	{
		DeclareClassFromElement(p_element1);
		
		if (class_uses_retain_release_)
			static_cast<EidosObjectElement_Retained *>(p_element1)->Retain();		// unsafe cast to avoid virtual function overhead
	}
}

EidosValue_Object_singleton::EidosValue_Object_singleton(EidosObjectElement *p_element1, const EidosObjectClass *p_class, bool p_register_for_patching) : EidosValue_Object(true, p_class, p_register_for_patching), value_(p_element1)
{
	// This is a special variant constructor used for EidosValues that are self-pointers and should not be
	// patched by the address-patching mechanism; see EidosValue_Object::EidosValue_Object() for comments.
	// This variant should not be used by anybody who does not know exactly what they're doing!
	
	// we want to allow nullptr as a momentary placeholder, although in general a value should exist
	if (p_element1)
	{
		DeclareClassFromElement(p_element1);
		
		if (class_uses_retain_release_)
			static_cast<EidosObjectElement_Retained *>(p_element1)->Retain();		// unsafe cast to avoid virtual function overhead
	}
}

EidosValue_Object_singleton::~EidosValue_Object_singleton(void)
{
	if (class_uses_retain_release_)
		if (value_)
			static_cast<EidosObjectElement_Retained *>(value_)->Release();		// unsafe cast to avoid virtual function overhead
}

int EidosValue_Object_singleton::Count_Virtual(void) const
{
	return 1;
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
	
	if (class_uses_retain_release_)
	{
		static_cast<EidosObjectElement_Retained *>(p_element)->Retain();		// unsafe cast to avoid virtual function overhead
		
		// we might have been initialized with nullptr with the aim of setting a value here
		if (value_)
			static_cast<EidosObjectElement_Retained *>(value_)->Release();		// unsafe cast to avoid virtual function overhead
	}
	
	value_ = p_element;
}

EidosValue_SP EidosValue_Object_singleton::CopyValues(void) const
{
	return EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(value_, Class()))->CopyDimensionsFromValue(this));
}

EidosValue_SP EidosValue_Object_singleton::VectorBasedCopy(void) const
{
	// We intentionally don't reserve a size of 1 here, on the assumption that further values are likely to be added
	EidosValue_Object_vector_SP new_vec = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(Class()));
	
	new_vec->push_object_element_CRR(value_);
	new_vec->CopyDimensionsFromValue(this);
	
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
	
	// Access of singleton properties retains the matrix/array structure of the target
	if (signature->value_mask_ & kEidosValueMaskSingleton)
		result->CopyDimensionsFromValue(this);
	
#if DEBUG
	// This is time-consuming, and will fail only due to internal bugs, so we should do it only in DEBUG
	signature->CheckResultValue(*result);
#endif
	
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

EidosValue_SP EidosValue_Object_singleton::ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosInstanceMethodSignature *p_method_signature, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// This is an instance method, so it gets dispatched to our element
	if (p_method_signature->accelerated_imp_)
	{
		// the method is accelerated, so call through its accelerated imp
		EidosValue_SP result = p_method_signature->accelerated_imper_(&value_, 1, p_method_id, p_arguments, p_interpreter);
		
		p_method_signature->CheckReturn(*result);
		return result;
	}
	else
	{
		// not accelerated, so ExecuteInstanceMethod() handles it
		EidosValue_SP result = value_->ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
		
		p_method_signature->CheckReturn(*result);
		return result;
	}
}


//
//	EidosObjectElement
//
#pragma mark -
#pragma mark EidosObjectElement
#pragma mark -

void EidosObjectElement::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType();
}

EidosValue_SP EidosObjectElement::GetProperty(EidosGlobalStringID p_property_id)
{
	// This is the backstop, called by subclasses
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::GetProperty for " << Class()->ElementType() << "): attempt to get a value for property " << Eidos_StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObjectElement::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
#pragma unused(p_value)
	// This is the backstop, called by subclasses
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

EidosValue_SP EidosObjectElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_interpreter)
	switch (p_method_id)
	{
		case gEidosID_str:	return ExecuteMethod_str(p_method_id, p_arguments, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<EidosMethodSignature_CSP> *methods = Class()->Methods();
			const std::string &method_name = Eidos_StringForGlobalStringID(p_method_id);
			
			for (const EidosMethodSignature_CSP &method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectElement::ExecuteInstanceMethod for " << Class()->ElementType() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
		}
	}
}

//	*********************	– (void)str(void)
//
EidosValue_SP EidosObjectElement::ExecuteMethod_str(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << Class()->ElementType() << ":" << std::endl;
	
	const std::vector<EidosPropertySignature_CSP> *properties = Class()->Properties();
	
	for (const EidosPropertySignature_CSP &property_sig : *properties)
	{
		const std::string &property_name = property_sig->property_name_;
		EidosGlobalStringID property_id = property_sig->property_id_;
		EidosValue_SP property_value;
		bool oldSuppressWarnings = gEidosSuppressWarnings;
		
		gEidosSuppressWarnings = true;		// prevent warnings from questionable property accesses from producing warnings in the user's output pane
		
		try {
			property_value = GetProperty(property_id);
		} catch (...) {
			// throw away the raise message
			gEidosTermination.clear();
			gEidosTermination.str(gEidosStr_empty_string);
		}
		
		gEidosSuppressWarnings = oldSuppressWarnings;
		
		if (property_value)
		{
			EidosValueType property_type = property_value->Type();
			int property_count = property_value->Count();
			int property_dimcount = property_value->DimensionCount();
			const int64_t *property_dims = property_value->Dimensions();
			
			output_stream << "\t" << property_name << " " << property_sig->PropertySymbol() << " ";
			
			if (property_count == 0)
			{
				// zero-length vectors get printed according to the standard code in EidosValue
				property_value->Print(output_stream);
			}
			else
			{
				// start with the type, and then the class for object-type values
				output_stream << property_type;
				
				if (property_type == EidosValueType::kValueObject)
					output_stream << "<" << property_value->ElementType() << ">";
				
				// then print the ranges for each dimension
				output_stream << " [";
				
				if (property_dimcount == 1)
					output_stream << "0:" << (property_count - 1) << "] ";
				else
				{
					for (int dim_index = 0; dim_index < property_dimcount; ++dim_index)
					{
						if (dim_index > 0)
							output_stream << ", ";
						output_stream << "0:" << (property_dims[dim_index] - 1);
					}
					
					output_stream << "] ";
				}
				
				// finally, print up to two values, if available, followed by an ellipsis if not all values were printed
				int output_count = std::min(2, property_count);
				
				for (int output_index = 0; output_index < output_count; ++output_index)
				{
					EidosValue_SP value = property_value->GetValueAtIndex(output_index, nullptr);
					
					if (output_index > 0)
						output_stream << gEidosStr_space_string;
					
					output_stream << *value;
				}
				
				if (property_count > output_count)
					output_stream << " ...";
			}
			
			output_stream << std::endl;
		}
		else
		{
			// The property threw an error when we tried to access it, which is allowed
			// for properties that are only valid in specific circumstances
			output_stream << "\t" << property_name << " " << property_sig->PropertySymbol() << " <inaccessible>" << std::endl;
		}
	}
	
	return gStaticEidosValueVOID;
}

EidosValue_SP EidosObjectElement::ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_function_name, p_arguments, p_interpreter)
	EIDOS_TERMINATION << "ERROR (EidosObjectElement::ContextDefinedFunctionDispatch for " << Class()->ElementType() << "): (internal error) unimplemented Context function dispatch." << EidosTerminate(nullptr);
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosObjectElement &p_element)
{
	p_element.Print(p_outstream);	// get dynamic dispatch
	
	return p_outstream;
}


//
//	EidosObjectElement_Retained
//
#pragma mark -
#pragma mark EidosObjectElement_Retained
#pragma mark -

/*
EidosObjectElement_Retained::EidosObjectElement_Retained(void)
{
//	std::cerr << "EidosObjectElement_Retained::EidosObjectElement_Retained allocated " << this << " with refcount == 1" << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}

EidosObjectElement_Retained::~EidosObjectElement_Retained(void)
{
//	std::cerr << "EidosObjectElement_Retained::~EidosObjectElement_Retained deallocated " << this << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}
*/

void EidosObjectElement_Retained::SelfDelete(void)
{
	// called when our refcount reaches zero; can be overridden by subclasses to provide custom behavior
	delete this;
}


//
//	EidosObjectClass
//
#pragma mark -
#pragma mark EidosObjectClass
#pragma mark -

bool EidosObjectClass::UsesRetainRelease(void) const
{
	return false;
}

const std::string &EidosObjectClass::ElementType(void) const
{
	return gEidosStr_undefined;
}

void EidosObjectClass::CacheDispatchTables(void)
{
	if (dispatches_cached_)
		EIDOS_TERMINATION << "ERROR (EidosObjectClass::CacheDispatchTables): (internal error) dispatch tables already initialized for class " << ElementType() << "." << EidosTerminate(nullptr);
	
	{
		const std::vector<EidosPropertySignature_CSP> *properties = Properties();
		int32_t last_id = -1;
		
		for (const EidosPropertySignature_CSP &sig : *properties)
			last_id = std::max(last_id, (int32_t)sig->property_id_);
		
		property_signatures_dispatch_capacity_ = last_id + 1;
		
		// this limit may need to be lifted someday, but for now it's a sanity check if the uniquing code changes
		if (property_signatures_dispatch_capacity_ > 512)
			EIDOS_TERMINATION << "ERROR (EidosObjectClass::CacheDispatchTables): (internal error) property dispatch table unreasonably large for class " << ElementType() << "." << EidosTerminate(nullptr);
		
		property_signatures_dispatch_ = (EidosPropertySignature_CSP *)calloc(property_signatures_dispatch_capacity_, sizeof(EidosPropertySignature_CSP));
		
		for (const EidosPropertySignature_CSP &sig : *properties)
			property_signatures_dispatch_[sig->property_id_] = sig;
	}
	
	{
		const std::vector<EidosMethodSignature_CSP> *methods = Methods();
		int32_t last_id = -1;
		
		for (const EidosMethodSignature_CSP &sig : *methods)
			last_id = std::max(last_id, (int32_t)sig->call_id_);
		
		method_signatures_dispatch_capacity_ = last_id + 1;
		
		// this limit may need to be lifted someday, but for now it's a sanity check if the uniquing code changes
		if (method_signatures_dispatch_capacity_ > 512)
			EIDOS_TERMINATION << "ERROR (EidosObjectClass::CacheDispatchTables): (internal error) method dispatch table unreasonably large for class " << ElementType() << "." << EidosTerminate(nullptr);
		
		method_signatures_dispatch_ = (EidosMethodSignature_CSP *)calloc(method_signatures_dispatch_capacity_, sizeof(EidosMethodSignature_CSP));
		
		for (const EidosMethodSignature_CSP &sig : *methods)
			method_signatures_dispatch_[sig->call_id_] = sig;
	}
	
	//std::cout << ElementType() << " property/method lookup table capacities: " << property_signatures_dispatch_capacity_ << " / " << method_signatures_dispatch_capacity_ << " (" << ((property_signatures_dispatch_capacity_ + method_signatures_dispatch_capacity_) * sizeof(EidosPropertySignature *)) << " bytes)" << std::endl;
	
	dispatches_cached_ = true;
}

void EidosObjectClass::RaiseForDispatchUninitialized(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosObjectClass::RaiseForDispatchUninitialized): (internal error) dispatch tables not initialized for class " << ElementType() << "." << EidosTerminate(nullptr);
}

const std::vector<EidosPropertySignature_CSP> *EidosObjectClass::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>;
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosObjectClass::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>;
		
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_methodSignature, kEidosValueMaskVOID))->AddString_OSN("methodName", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_propertySignature, kEidosValueMaskVOID))->AddString_OSN("propertyName", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_size, kEidosValueMaskInt | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_length, kEidosValueMaskInt | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_str, kEidosValueMaskVOID)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP EidosObjectClass::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gEidosID_propertySignature:	return ExecuteMethod_propertySignature(p_method_id, p_target, p_arguments, p_interpreter);
		case gEidosID_methodSignature:		return ExecuteMethod_methodSignature(p_method_id, p_target, p_arguments, p_interpreter);
		case gEidosID_size:					return ExecuteMethod_size_length(p_method_id, p_target, p_arguments, p_interpreter);
		case gEidosID_length:				return ExecuteMethod_size_length(p_method_id, p_target, p_arguments, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<EidosMethodSignature_CSP> *methods = Methods();
			const std::string &method_name = Eidos_StringForGlobalStringID(p_method_id);
			
			for (const EidosMethodSignature_CSP &method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObjectClass::ExecuteClassMethod for " << ElementType() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
		}
	}
}

//	*********************	+ (void)propertySignature([Ns$ propertyName = NULL])
//
EidosValue_SP EidosObjectClass::ExecuteMethod_propertySignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
	std::string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	const std::vector<EidosPropertySignature_CSP> *properties = Properties();
	bool signature_found = false;
	
	for (const EidosPropertySignature_CSP &property_sig : *properties)
	{
		const std::string &property_name = property_sig->property_name_;
		
		if (has_match_string && (property_name.compare(match_string) != 0))
			continue;
		
		output_stream << property_name << " " << property_sig->PropertySymbol() << " (" << StringForEidosValueMask(property_sig->value_mask_, property_sig->value_class_, "", nullptr) << ")" << std::endl;
		
		signature_found = true;
	}
	
	if (has_match_string && !signature_found)
		output_stream << "No property found for \"" << match_string << "\"." << std::endl;
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (void)methodSignature([Ns$ methodName = NULL])
//
EidosValue_SP EidosObjectClass::ExecuteMethod_methodSignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
	std::string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	const std::vector<EidosMethodSignature_CSP> *methods = Methods();
	bool signature_found = false;
	
	// Output class methods first
	for (const EidosMethodSignature_CSP &method_sig : *methods)
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
	for (const EidosMethodSignature_CSP &method_sig : *methods)
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
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (integer$)size(void)
//	*********************	+ (integer$)length(void)
//
EidosValue_SP EidosObjectClass::ExecuteMethod_size_length(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(p_target->Count()));
}


//
//	EidosObjectClass_Retained
//
#pragma mark -
#pragma mark EidosObjectClass_Retained
#pragma mark -

EidosObjectClass *gEidos_EidosObjectClass_Retained = new EidosObjectClass_Retained();


const std::string &EidosObjectClass_Retained::ElementType(void) const
{
	return gEidosStr_EidosObjectClass_Retained;
}

bool EidosObjectClass_Retained::UsesRetainRelease(void) const
{
	return true;
}







































































