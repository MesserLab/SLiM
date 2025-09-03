//
//  eidos_functions_stats.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15; split from eidos_functions.cpp 09/26/2022
//  Copyright (c) 2015-2025 Benjamin C. Haller.  All rights reserved.
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
#include "eidos_sorting.h"

#include <utility>
#include <limits>
#include <string>
#include <algorithm>
#include <vector>


// ************************************************************************************
//
//	statistics functions
//
#pragma mark -
#pragma mark Statistics functions
#pragma mark -

static double _Eidos_CalcCorrelation(size_t count, EidosValue *x_value, EidosValue *y_value, size_t x_offset, size_t y_offset)
{
	// A thin wrapper for Eidos_Correlation() that can be used with both integer and float EidosValues,
	// and takes an offset for x and y that allow a particular column of a matrix to be selected
	if (x_value->Type() == EidosValueType::kValueInt)
	{
		if (y_value->Type() == EidosValueType::kValueInt)
			return Eidos_Correlation<const int64_t, const int64_t>(x_value->IntData() + x_offset, y_value->IntData() + y_offset, count);
		else				// EidosValueType::kValueFloat
			return Eidos_Correlation<const int64_t, const double>(x_value->IntData() + x_offset, y_value->FloatData() + y_offset, count);
	}
	else					// EidosValueType::kValueFloat
	{
		if (y_value->Type() == EidosValueType::kValueInt)
			return Eidos_Correlation<const double, const int64_t>(x_value->FloatData() + x_offset, y_value->IntData() + y_offset, count);
		else				// EidosValueType::kValueFloat
			return Eidos_Correlation<const double, const double>(x_value->FloatData() + x_offset, y_value->FloatData() + y_offset, count);
	}
}

//	(float)cor(numeric x, [Nif y = NULL])
EidosValue_SP Eidos_ExecuteFunction_cor(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	bool x_is_matrix = x_value->IsMatrixOrArray();
	bool y_is_matrix = y_value->IsMatrixOrArray();
	
	if (x_is_matrix || y_is_matrix)
	{
		// correlation involving at least one matrix (treated by column); y=NULL means do cor(x, x) for matrix x
		if (y_value->Type() == EidosValueType::kValueNULL)
		{
			y_value = x_value;
			y_is_matrix = x_is_matrix;
		}
		
		// arrays are not allowed, just matrices and vectors
		if ((x_value->DimensionCount() > 2) || (y_value->DimensionCount() > 2))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): function cor() does not allow x or y to be an array." << EidosTerminate(nullptr);
		
		// get the lengths of the vectors we're calculating correlation on: vector length or matrix row count
		size_t x_vec_length = x_is_matrix ? x_value->Dimensions()[0] : x_value->Count();
		size_t y_vec_length = y_is_matrix ? y_value->Dimensions()[0] : y_value->Count();
		
		if (x_vec_length != y_vec_length)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): incompatible dimensions in cor()." << EidosTerminate(nullptr);
		
		size_t vec_length = x_vec_length;
		
		if (vec_length == 0)
			return gStaticEidosValue_FloatNAN;
		
		// so we're making a correlation matrix; let's determine its size first
		int64_t nrows = x_is_matrix ? x_value->Dimensions()[1] : 1;
		int64_t ncols = y_is_matrix ? y_value->Dimensions()[1] : 1;
		
		EidosValue_Float *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(nrows * ncols);
		double *result_data = result->FloatData_Mutable();
		result_SP = EidosValue_SP(result);
		
		if (x_value == y_value)
		{
			// if x_value and y_value are the same, we're making a correlation matrix for x_value with itself
			// the result will be a symmetric matrix, so we can save time by calculating only one triangle
			for (int64_t row = 0; row < nrows; ++row)
			{
				for (int64_t col = 0; col < ncols; ++col)
				{
					if (row == col)
					{
						result_data[col * nrows + row] = 1.0;
					}
					else if (row < col)
					{
						size_t x_offset = row * vec_length;
						size_t y_offset = col * vec_length;
						double cor = _Eidos_CalcCorrelation(vec_length, x_value, y_value, x_offset, y_offset);
						
						result_data[col * nrows + row] = cor;
						result_data[row * nrows + col] = cor;
					}
				}
			}
		}
		else
		{
			// general case: loop over the elements of the result and calculate each one
			for (int64_t row = 0; row < nrows; ++row)
			{
				for (int64_t col = 0; col < ncols; ++col)
				{
					size_t x_offset = row * vec_length;
					size_t y_offset = col * vec_length;
					double cor = _Eidos_CalcCorrelation(vec_length, x_value, y_value, x_offset, y_offset);
					
					result_data[col * nrows + row] = cor;
				}
			}
		}
		
		const int64_t dim_buf[2] = {nrows, ncols};
		result->SetDimensions(2, dim_buf);
	}
	else
	{
		// correlation of two vectors x and y; in this case, y is not allowed to be NULL
		if (y_value->Type() == EidosValueType::kValueNULL)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): function cor() requires both x and y to be supplied, or a matrix x." << EidosTerminate(nullptr);
		
		int count = x_value->Count();
		
		if (count != y_value->Count())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): function cor() requires that x and y be the same size." << EidosTerminate(nullptr);
		if (count <= 1)
			return gStaticEidosValue_FloatNAN;
		
		// calculate correlation between x and y
		double cor = _Eidos_CalcCorrelation(count, x_value, y_value, 0, 0);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(cor));
	}
	
	return result_SP;
}

static double _Eidos_CalcCovariance(size_t count, EidosValue *x_value, EidosValue *y_value, size_t x_offset, size_t y_offset)
{
	// A thin wrapper for Eidos_Covariance() that can be used with both integer and float EidosValues,
	// and takes an offset for x and y that allow a particular column of a matrix to be selected
	if (x_value->Type() == EidosValueType::kValueInt)
	{
		if (y_value->Type() == EidosValueType::kValueInt)
			return Eidos_Covariance<const int64_t, const int64_t>(x_value->IntData() + x_offset, y_value->IntData() + y_offset, count);
		else				// EidosValueType::kValueFloat
			return Eidos_Covariance<const int64_t, const double>(x_value->IntData() + x_offset, y_value->FloatData() + y_offset, count);
	}
	else					// EidosValueType::kValueFloat
	{
		if (y_value->Type() == EidosValueType::kValueInt)
			return Eidos_Covariance<const double, const int64_t>(x_value->FloatData() + x_offset, y_value->IntData() + y_offset, count);
		else				// EidosValueType::kValueFloat
			return Eidos_Covariance<const double, const double>(x_value->FloatData() + x_offset, y_value->FloatData() + y_offset, count);
	}
}

//	(float)cov(numeric x, [Nif y = NULL])
EidosValue_SP Eidos_ExecuteFunction_cov(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	bool x_is_matrix = x_value->IsMatrixOrArray();
	bool y_is_matrix = y_value->IsMatrixOrArray();
	
	if (x_is_matrix || y_is_matrix)
	{
		// covariance involving at least one matrix (treated by column); y=NULL means do cov(x, x) for matrix x
		if (y_value->Type() == EidosValueType::kValueNULL)
		{
			y_value = x_value;
			y_is_matrix = x_is_matrix;
		}
		
		// arrays are not allowed, just matrices and vectors
		if ((x_value->DimensionCount() > 2) || (y_value->DimensionCount() > 2))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): function cov() does not allow x or y to be an array." << EidosTerminate(nullptr);
		
		// get the lengths of the vectors we're calculating covariance on: vector length or matrix row count
		size_t x_vec_length = x_is_matrix ? x_value->Dimensions()[0] : x_value->Count();
		size_t y_vec_length = y_is_matrix ? y_value->Dimensions()[0] : y_value->Count();
		
		if (x_vec_length != y_vec_length)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): incompatible dimensions in cov()." << EidosTerminate(nullptr);
		
		size_t vec_length = x_vec_length;
		
		if (vec_length == 0)
			return gStaticEidosValue_FloatNAN;
		
		// so we're making a covariance matrix; let's determine its size first
		int64_t nrows = x_is_matrix ? x_value->Dimensions()[1] : 1;
		int64_t ncols = y_is_matrix ? y_value->Dimensions()[1] : 1;
		
		EidosValue_Float *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(nrows * ncols);
		double *result_data = result->FloatData_Mutable();
		result_SP = EidosValue_SP(result);
		
		if (x_value == y_value)
		{
			// if x_value and y_value are the same, we're making a covariance matrix for x_value with itself
			// the result will be a symmetric matrix, so we can save time by calculating only one triangle
			for (int64_t row = 0; row < nrows; ++row)
			{
				for (int64_t col = 0; col < ncols; ++col)
				{
					if (row <= col)
					{
						size_t x_offset = row * vec_length;
						size_t y_offset = col * vec_length;
						double cov = _Eidos_CalcCovariance(vec_length, x_value, y_value, x_offset, y_offset);
						
						result_data[col * nrows + row] = cov;
						result_data[row * nrows + col] = cov;
					}
				}
			}
		}
		else
		{
			// general case: loop over the elements of the result and calculate each one
			for (int64_t row = 0; row < nrows; ++row)
			{
				for (int64_t col = 0; col < ncols; ++col)
				{
					size_t x_offset = row * vec_length;
					size_t y_offset = col * vec_length;
					double cov = _Eidos_CalcCovariance(vec_length, x_value, y_value, x_offset, y_offset);
					
					result_data[col * nrows + row] = cov;
				}
			}
		}
		
		const int64_t dim_buf[2] = {nrows, ncols};
		result->SetDimensions(2, dim_buf);
	}
	else
	{
		// covariance of two vectors x and y; in this case, y is not allowed to be NULL
		if (y_value->Type() == EidosValueType::kValueNULL)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): function cov() requires both x and y to be supplied, or a matrix x." << EidosTerminate(nullptr);
		
		int count = x_value->Count();
		
		if (count != y_value->Count())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): function cov() requires that x and y be the same size." << EidosTerminate(nullptr);
		if (count <= 1)
			return gStaticEidosValue_FloatNAN;
		
		// calculate covariance between x and y
		double cov = _Eidos_CalcCovariance(count, x_value, y_value, 0, 0);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(cov));
	}
	
	return result_SP;
}

//	(float)filter(numeric x, float filter, [lif$ outside = F])
EidosValue_SP Eidos_ExecuteFunction_filter(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// this is patterned after the R function filter(), but only for method="convolution", sides=2, circular=F
	// so for now we support only a centered filter convolved over x with a non-circular buffer
	// values where the filter extends beyond the range of x are handled according to the `outside` parameter
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *filter_value = p_arguments[1].get();
	EidosValue *outside_value = p_arguments[2].get();
	int x_count = x_value->Count();
	int filter_count = filter_value->Count();
	
	// the maximum filter length is arbitrary, but it seems like a good idea to flag weird bugs?
	if ((filter_count <= 0) | (filter_count > 999) | (filter_count % 2 == 0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_filter): function filter() requires filter to have a length that is odd and within the interval [1, 999]." << EidosTerminate(nullptr);
	
	// decode the value of outside, which must be T (use NAN), F (exclude), or a numeric value (use constant)
	typedef enum _OutsideValue {
		kUseNAN = 0,
		kExcludeOuter,
		kUseConstant
	} OutsideValue;
	
	OutsideValue outside;
	double outsideConstant = 0.0;	// only used for OutsideValue::kUseConstant
	
	if (outside_value->Type() == EidosValueType::kValueLogical)
	{
		if (outside_value->LogicalAtIndex_NOCAST(0, nullptr) == false)
		{
			// outside=F is the default: use NAN for all positions where the filter extends beyond x
			outside = OutsideValue::kUseNAN;
		}
		else
		{
			// outside=T: exclude positions where the filter extends beyond x, and rescale to compensate
			outside = OutsideValue::kExcludeOuter;
		}
	}
	else
	{
		// outside is integer or float: it gives the mean/expected value to be used for all values beyond x
		outside = OutsideValue::kUseConstant;
		outsideConstant = outside_value->NumericAtIndex_NOCAST(0, nullptr);
	}
	
	// half rounded down; e.g., for a filter of length 5, this is 2; this is the number of
	// positions at the start/end of the result where the filter extends past the end of x
	int half_filter = filter_count / 2;
	
	// the result is the same length as x, in all cases
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(x_count);
	EidosValue_SP result_SP(float_result);
	double *result_data = float_result->FloatData_Mutable();
	
	// test for a simple moving average, with equal weights summing to 1.0, to special-case it
	const double *filter_data = filter_value->FloatData();
	bool is_simple_moving_average = true;
	
	for (int index = 0; index < filter_count; ++index)
	{
		if (std::abs(filter_data[index] - (1.0 / filter_count)) > 1e-15)	// 1e-15 is a roundoff epsilon
		{
			is_simple_moving_average = false;
			break;
		}
	}
	
	// if outside is kExcludeOuter, we need to know the sum of the filter's absolute values for scaling
	double sum_abs_filter = 0.0;
	
	if (outside == OutsideValue::kExcludeOuter)
	{
		for (int pos = 0; pos < filter_count; ++pos)
		{
			double filter_element = filter_data[pos];
			
			if ((filter_element == 0) && ((pos == 0) || (pos == filter_count - 1)))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_filter): when outside=T, function filter() requires the first and last values of filter to be non-zero to avoid numerical issues." << EidosTerminate(nullptr);
			
			sum_abs_filter += std::abs(filter_element);
		}
	}
	
	// now that we've checked all error cases, short-circuit for zero-length x
	if (x_count == 0)
		return result_SP;
	
	// now we start generating the result vector; we will use the variable result_pos to track the result
	// position we are calculating throughout, and will handle it three sections, left/middle/right
	int result_pos = 0;
	
	// handle the half-filter length at the start, where positions on the left lie outside x
	// this section also handles cases where positions on the right are also outside, because x_count is small
	switch (outside)
	{
		case OutsideValue::kUseNAN:			// use NAN for all positions where the filter extends beyond x
		{
			for ( ; (result_pos < half_filter) && (result_pos < x_count); ++result_pos)
				result_data[result_pos] = std::numeric_limits<double>::quiet_NaN();
			
			break;
		}
		case OutsideValue::kExcludeOuter:	// exclude positions where the filter extends beyond x
		{
			for ( ; (result_pos < half_filter) && (result_pos < x_count); ++result_pos)
			{
				double filtered_total = 0.0;
				double sum_abs_filter_inside = 0.0;
				
				for (int filter_pos = 0; filter_pos < filter_count; filter_pos++)
				{
					double filter_datum = filter_data[filter_pos];
					int x_pos = result_pos + (filter_pos - half_filter);
					
					if ((x_pos >= 0) && (x_pos < x_count))
					{
						double x_datum = x_value->NumericAtIndex_NOCAST(x_pos, nullptr);
						
						filtered_total += filter_datum * x_datum;
						sum_abs_filter_inside += std::abs(filter_datum);
					}
				}
				
				result_data[result_pos] = filtered_total * (sum_abs_filter / sum_abs_filter_inside);
			}
			break;
		}
		case OutsideValue::kUseConstant:	// use the given numeric value for values beyond x
		{
			for ( ; (result_pos < half_filter) && (result_pos < x_count); ++result_pos)
			{
				double filtered_total = 0.0;
				
				for (int filter_pos = 0; filter_pos < filter_count; filter_pos++)
				{
					double filter_datum = filter_data[filter_pos];
					int x_pos = result_pos + (filter_pos - half_filter);
					double x_datum;
					
					if ((x_pos >= 0) && (x_pos < x_count))
						x_datum = x_value->NumericAtIndex_NOCAST(x_pos, nullptr);
					else
						x_datum = outsideConstant;
					
					filtered_total += filter_datum * x_datum;
				}
				
				result_data[result_pos] = filtered_total;
			}
			break;
		}
	}
	
	// now we branch depending on whether x is integer or float, and special-case simple moving averages
	// for this middle loop we don't need to worry about the x position being outside bounds
	if (x_value->Type() == EidosValueType::kValueFloat)
	{
		const double *x_data = x_value->FloatData();
		
		if (is_simple_moving_average)
		{
			// the first position after the half-filter length sets up a moving total
			double moving_total = 0.0;
			
			if (result_pos < x_count - half_filter)
			{
				for (int filter_pos = 0; filter_pos < filter_count; ++filter_pos)
					moving_total += x_data[filter_pos];
				
				result_data[result_pos] = moving_total / filter_count;
				++result_pos;
			}
			
			// the remaining positions modify the moving total
			for ( ; result_pos < x_count - half_filter; ++result_pos)
			{
				moving_total -= x_data[result_pos - half_filter - 1];
				moving_total += x_data[result_pos + half_filter];
				
				result_data[result_pos] = moving_total / filter_count;
			}
		}
		else
		{
			// we compute the filter over the appropriate range of x at each position
			for ( ; result_pos < x_count - half_filter; ++result_pos)
			{
				double filter_total = 0.0;
				
				for (int filter_pos = 0; filter_pos < filter_count; filter_pos++)
					filter_total += filter_data[filter_pos] * x_data[filter_pos + result_pos - half_filter];
				
				result_data[result_pos] = filter_total;
			}
		}
	}
	else
	{
		const int64_t *x_data = x_value->IntData();
		
		if (is_simple_moving_average)
		{
			// the first position after the half-filter length sets up a moving total
			double moving_total = 0.0;
			
			if (result_pos < x_count - half_filter)
			{
				for (int filter_pos = 0; filter_pos < filter_count; ++filter_pos)
					moving_total += x_data[filter_pos];
				
				result_data[result_pos] = moving_total / filter_count;
				++result_pos;
			}
			
			// the remaining non-NAN positions modify the moving total
			for ( ; result_pos < x_count - half_filter; ++result_pos)
			{
				moving_total -= x_data[result_pos - half_filter - 1];
				moving_total += x_data[result_pos + half_filter];
				
				result_data[result_pos] = moving_total / filter_count;
			}
		}
		else
		{
			// we compute the filter over the appropriate range of x at each position
			for ( ; result_pos < x_count - half_filter; ++result_pos)
			{
				double filter_total = 0.0;
				
				for (int filter_pos = 0; filter_pos < filter_count; filter_pos++)
					filter_total += filter_data[filter_pos] * x_data[filter_pos + result_pos - half_filter];
				
				result_data[result_pos] = filter_total;
			}
		}
	}
	
	// the remaining positions at the end have positions outside x on the right, but never on the left
	switch (outside)
	{
		case OutsideValue::kUseNAN:			// use NAN for all positions where the filter extends beyond x
		{
			for ( ; result_pos < x_count; ++result_pos)
				result_data[result_pos] = std::numeric_limits<double>::quiet_NaN();
			break;
		}
		case OutsideValue::kExcludeOuter:	// exclude positions where the filter extends beyond x
		{
			for ( ; result_pos < x_count; ++result_pos)
			{
				double filtered_total = 0.0;
				double sum_abs_filter_inside = 0.0;
				
				for (int filter_pos = 0; filter_pos < filter_count; filter_pos++)
				{
					double filter_datum = filter_data[filter_pos];
					int x_pos = result_pos + (filter_pos - half_filter);
					
					if (x_pos < x_count)
					{
						double x_datum = x_value->NumericAtIndex_NOCAST(x_pos, nullptr);
						
						filtered_total += filter_datum * x_datum;
						sum_abs_filter_inside += std::abs(filter_datum);
					}
				}
				
				result_data[result_pos] = filtered_total * (sum_abs_filter / sum_abs_filter_inside);
			}
			break;
		}
		case OutsideValue::kUseConstant:	// use the given numeric value for values beyond x
		{
			for ( ; result_pos < x_count; ++result_pos)
			{
				double filtered_total = 0.0;
				
				for (int filter_pos = 0; filter_pos < filter_count; filter_pos++)
				{
					double filter_datum = filter_data[filter_pos];
					int x_pos = result_pos + (filter_pos - half_filter);
					double x_datum;
					
					if (x_pos < x_count)
						x_datum = x_value->NumericAtIndex_NOCAST(x_pos, nullptr);
					else
						x_datum = outsideConstant;
					
					filtered_total += filter_datum * x_datum;
				}
				
				result_data[result_pos] = filtered_total;
			}
			break;
		}
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
			const eidos_logical_t *logical_data = arg_value->LogicalData();
			
			for (int value_index = 0; value_index < arg_count; ++value_index)
				if (logical_data[value_index] == true)
					return gStaticEidosValue_LogicalT;
		}
		
		result_SP = gStaticEidosValue_LogicalF;
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		int64_t max = p_arguments[first_nonempty_argument]->IntAtIndex_NOCAST(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntData()[0];
				if (max < temp)
					max = temp;
			}
			else if (arg_count > 1)
			{
				const int64_t *int_data = arg_value->IntData();
				int64_t loop_max = INT64_MIN;
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MAX_INT);
#pragma omp parallel for schedule(static) default(none) shared(arg_count) firstprivate(int_data) reduction(max: loop_max) if(arg_count >= EIDOS_OMPMIN_MAX_INT) num_threads(thread_count)
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (loop_max < temp)
						loop_max = temp;
				}
				
				if (max < loop_max)
					max = loop_max;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(max));
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		double max = p_arguments[first_nonempty_argument]->FloatAtIndex_NOCAST(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatData()[0];
				
				// if there is a NAN the result is always NAN, so we don't need to scan further
				if (std::isnan(temp))
					return gStaticEidosValue_FloatNAN;
				
				if (max < temp)
					max = temp;
			}
			else
			{
				const double *float_data = arg_value->FloatData();
				double loop_max = -std::numeric_limits<double>::infinity();
				bool saw_NAN = false;
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MAX_FLOAT);
#pragma omp parallel for schedule(static) default(none) shared(arg_count) firstprivate(float_data) reduction(max: loop_max) reduction(||: saw_NAN) if(arg_count >= EIDOS_OMPMIN_MAX_FLOAT) num_threads(thread_count)
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					double temp = float_data[value_index];
					
					// if there is a NAN the result is always NAN, but we can't return from a parallel region
					if (std::isnan(temp))
						saw_NAN = true;
					
					if (loop_max < temp)
						loop_max = temp;
				}
				
				if (saw_NAN)
					return gStaticEidosValue_FloatNAN;
				
				if (max < loop_max)
					max = loop_max;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(max));
	}
	else if (x_type == EidosValueType::kValueString)
	{
		const std::string *max = &(((EidosValue_String *)(p_arguments[first_nonempty_argument].get()))->StringRefAtIndex_NOCAST(0, nullptr));
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue_String *arg_value = (EidosValue_String *)(p_arguments[arg_index].get());
			int arg_count = arg_value->Count();
			const std::string *string_vec = arg_value->StringData();
			
			for (int value_index = 0; value_index < arg_count; ++value_index)
			{
				const std::string &temp = string_vec[value_index];
				if (*max < temp)
					max = &temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(*max));
	}
	
	return result_SP;
}

//	(float$)mean(lif x)
EidosValue_SP Eidos_ExecuteFunction_mean(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
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
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(x_value->FloatAtIndex_CAST(0, nullptr)));
	}
	else
	{
		// Call sum() to do the addition for us, since it takes exactly the same arguments; it will return numeric$ which we treat as float$
		// Note this means we inherit the parallelization/vectorization behavior of sum(); we have no separate benchmarks for mean()
		EidosValue_SP sum_value = Eidos_ExecuteFunction_sum(p_arguments, p_interpreter);
		double sum = sum_value->FloatAtIndex_CAST(0, nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(sum / x_count));
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
			const eidos_logical_t *logical_data = arg_value->LogicalData();
			
			for (int value_index = 0; value_index < arg_count; ++value_index)
				if (logical_data[value_index] == false)
					return gStaticEidosValue_LogicalF;
		}
		
		result_SP = gStaticEidosValue_LogicalT;
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		int64_t min = p_arguments[first_nonempty_argument]->IntAtIndex_NOCAST(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntData()[0];
				if (min > temp)
					min = temp;
			}
			else if (arg_count > 1)
			{
				const int64_t *int_data = arg_value->IntData();
				int64_t loop_min = INT64_MAX;
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MIN_INT);
#pragma omp parallel for schedule(static) default(none) shared(arg_count) firstprivate(int_data) reduction(min: loop_min) if(arg_count >= EIDOS_OMPMIN_MIN_INT) num_threads(thread_count)
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (loop_min > temp)
						loop_min = temp;
				}
				
				if (min > loop_min)
					min = loop_min;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(min));
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		double min = p_arguments[first_nonempty_argument]->FloatAtIndex_NOCAST(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatData()[0];
				
				// if there is a NAN the result is always NAN, so we don't need to scan further
				if (std::isnan(temp))
					return gStaticEidosValue_FloatNAN;
				
				if (min > temp)
					min = temp;
			}
			else
			{
				const double *float_data = arg_value->FloatData();
				double loop_min = std::numeric_limits<double>::infinity();
				bool saw_NAN = false;
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MIN_FLOAT);
#pragma omp parallel for schedule(static) default(none) shared(arg_count) firstprivate(float_data) reduction(min: loop_min) reduction(||: saw_NAN) if(arg_count >= EIDOS_OMPMIN_MIN_FLOAT) num_threads(thread_count)
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					double temp = float_data[value_index];
					
					// if there is a NAN the result is always NAN, but we can't return from a parallel region
					if (std::isnan(temp))
						saw_NAN = true;
					
					if (loop_min > temp)
						loop_min = temp;
				}
				
				if (saw_NAN)
					return gStaticEidosValue_FloatNAN;
				
				if (min > loop_min)
					min = loop_min;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(min));
	}
	else if (x_type == EidosValueType::kValueString)
	{
		const std::string *min = &(((EidosValue_String *)(p_arguments[first_nonempty_argument].get()))->StringRefAtIndex_NOCAST(0, nullptr));
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue_String *arg_value = (EidosValue_String *)(p_arguments[arg_index].get());
			int arg_count = arg_value->Count();
			const std::string *string_vec = arg_value->StringData();
			
			for (int value_index = 0; value_index < arg_count; ++value_index)
			{
				const std::string &temp = string_vec[value_index];
				if (*min > temp)
					min = &temp;
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(*min));
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
			if (std::isnan(x_value->FloatAtIndex_NOCAST(0, nullptr)) || std::isnan(y_value->FloatAtIndex_NOCAST(0, nullptr)))
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
			const eidos_logical_t *logical0_data = x_value->LogicalData();
			eidos_logical_t y_singleton_value = y_value->LogicalAtIndex_NOCAST(0, nullptr);
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] || y_singleton_value, value_index); // || is logical max
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntData();
			int64_t y_singleton_value = y_value->IntAtIndex_NOCAST(0, nullptr);
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data_mutable();
			result_SP = EidosValue_SP(int_result);
			
			// BCH 12/27/2022: This and the corresponding loop in pmin() show an unusually high variance in execution time, for the same data.
			// The benchmark model typically runs in either 7-8 seconds, or 11-12 seconds, not in between.  Timing indicates that this is due
			// to an overall slowdown of the whole process, not a 3-4 second pause with a massive one-time stall.  I suspect that there is
			// something about this loop that tends to encourage macOS to move the thread to an efficiency core, as bizarre as that sounds.
			// (I'm... too sexy for my core... too sexy for my core... core's going to offload me...)  Well, it's the only hypothesis left.
			// I tried upping the QoS ("quality of service") setting for the process to encourage it to stay on the performance cores, but
			// that actually made things worse, maybe by increasing contention with UI-based apps.  Removing SIMD from this loop makes no
			// difference.  I looked at the alignment of int0_data and int_result_data, and that is uncorrelated with the performance issue.
			// I haven't figured out how to confirm my hypothesis with profiling tools yet.  It's a mystery.  Leaving this comment here
			// for posterity.  It's not a big deal in the grand scheme of things, but I would love to know what's going on.  FIXME
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMAX_INT_1);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(int0_data, int_result_data, y_singleton_value) if(parallel:x_count >= EIDOS_OMPMIN_PMAX_INT_1) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int0_value = int0_data[value_index];
				int_result_data[value_index] = ((int0_value > y_singleton_value) ? int0_value : y_singleton_value);
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double * __restrict__ float0_data = x_value->FloatData();
			double y_singleton_value = y_value->FloatAtIndex_NOCAST(0, nullptr);
			EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data_mutable();
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMAX_FLOAT_1);
#pragma omp parallel for schedule(static) default(none) shared(x_count) firstprivate(float0_data, float_result_data, y_singleton_value) if(x_count >= EIDOS_OMPMIN_PMAX_FLOAT_1) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				double float0_value = float0_data[value_index];
				if (std::isnan(float0_value) || std::isnan(y_singleton_value))
					float_result_data[value_index] = std::numeric_limits<double>::quiet_NaN();
				else
					float_result_data[value_index] = ((float0_value > y_singleton_value) ? float0_value : y_singleton_value);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string *string0_vec = x_value->StringData();
			const std::string &y_singleton_value = ((EidosValue_String *)y_value)->StringRefAtIndex_NOCAST(0, nullptr);
			EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve(x_count);
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
			const eidos_logical_t *logical0_data = x_value->LogicalData();
			const eidos_logical_t *logical1_data = y_value->LogicalData();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] || logical1_data[value_index], value_index); // || is logical max
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntData();
			const int64_t * __restrict__ int1_data = y_value->IntData();
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data_mutable();
			result_SP = EidosValue_SP(int_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMAX_INT_2);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(int0_data, int1_data, int_result_data) if(parallel:x_count >= EIDOS_OMPMIN_PMAX_INT_2) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int0_value = int0_data[value_index];
				int64_t int1_value = int1_data[value_index];
				int_result_data[value_index] = (int0_value > int1_value) ? int0_value : int1_value;
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double * __restrict__ float0_data = x_value->FloatData();
			const double * __restrict__ float1_data = y_value->FloatData();
			EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data_mutable();
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMAX_FLOAT_2);
#pragma omp parallel for schedule(static) default(none) shared(x_count) firstprivate(float0_data, float1_data, float_result_data) if(x_count >= EIDOS_OMPMIN_PMAX_FLOAT_2) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				double float0_value = float0_data[value_index];
				double float1_value = float1_data[value_index];
				if (std::isnan(float0_data[value_index]) || std::isnan(float1_data[value_index]))
					float_result_data[value_index] = std::numeric_limits<double>::quiet_NaN();
				else
					float_result_data[value_index] = ((float0_value > float1_value) ? float0_value : float1_value);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string *string0_vec = x_value->StringData();
			const std::string *string1_vec = y_value->StringData();
			EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve(x_count);
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
			if (std::isnan(x_value->FloatAtIndex_NOCAST(0, nullptr)) || std::isnan(y_value->FloatAtIndex_NOCAST(0, nullptr)))
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
			const eidos_logical_t *logical0_data = x_value->LogicalData();
			eidos_logical_t y_singleton_value = y_value->LogicalAtIndex_NOCAST(0, nullptr);
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] && y_singleton_value, value_index); // && is logical min
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntData();
			int64_t y_singleton_value = y_value->IntAtIndex_NOCAST(0, nullptr);
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data_mutable();
			result_SP = EidosValue_SP(int_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMIN_INT_1);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(int0_data, int_result_data, y_singleton_value) if(parallel:x_count >= EIDOS_OMPMIN_PMIN_INT_1) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int0_value = int0_data[value_index];
				int_result_data[value_index] = ((int0_value < y_singleton_value) ? int0_value : y_singleton_value);
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double * __restrict__ float0_data = x_value->FloatData();
			double y_singleton_value = y_value->FloatAtIndex_NOCAST(0, nullptr);
			EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data_mutable();
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMIN_FLOAT_1);
#pragma omp parallel for schedule(static) default(none) shared(x_count) firstprivate(float0_data, float_result_data, y_singleton_value) if(x_count >= EIDOS_OMPMIN_PMIN_FLOAT_1) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				double float0_value = float0_data[value_index];
				if (std::isnan(float0_value) || std::isnan(y_singleton_value))
					float_result_data[value_index] = std::numeric_limits<double>::quiet_NaN();
				else
					float_result_data[value_index] = ((float0_value < y_singleton_value) ? float0_value : y_singleton_value);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string *string0_vec = x_value->StringData();
			const std::string &y_singleton_value = ((EidosValue_String *)y_value)->StringRefAtIndex_NOCAST(0, nullptr);
			EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve(x_count);
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
			const eidos_logical_t *logical0_data = x_value->LogicalData();
			const eidos_logical_t *logical1_data = y_value->LogicalData();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] && logical1_data[value_index], value_index); // && is logical min
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntData();
			const int64_t * __restrict__ int1_data = y_value->IntData();
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data_mutable();
			result_SP = EidosValue_SP(int_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMIN_INT_2);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(int0_data, int1_data, int_result_data) if(parallel:x_count >= EIDOS_OMPMIN_PMIN_INT_2) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int0_value = int0_data[value_index];
				int64_t int1_value = int1_data[value_index];
				int_result_data[value_index] = (int0_value < int1_value) ? int0_value : int1_value;
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double * __restrict__ float0_data = x_value->FloatData();
			const double * __restrict__ float1_data = y_value->FloatData();
			EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data_mutable();
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_PMIN_FLOAT_2);
#pragma omp parallel for schedule(static) default(none) shared(x_count) firstprivate(float0_data, float1_data, float_result_data) if(x_count >= EIDOS_OMPMIN_PMIN_FLOAT_2) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// if there is a NAN the result is always NAN
				double float0_value = float0_data[value_index];
				double float1_value = float1_data[value_index];
				if (std::isnan(float0_data[value_index]) || std::isnan(float1_data[value_index]))
					float_result_data[value_index] = std::numeric_limits<double>::quiet_NaN();
				else
					float_result_data[value_index] = ((float0_value < float1_value) ? float0_value : float1_value);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string *string0_vec = x_value->StringData();
			const std::string *string1_vec = y_value->StringData();
			EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve(x_count);
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
			double prob = probs_value->FloatAtIndex_NOCAST(probs_index, nullptr);
			
			if ((prob < 0.0) || (prob > 1.0))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): function quantile() requires probabilities to be in [0, 1]." << EidosTerminate(nullptr);
			
			probs.emplace_back(prob);
		}
	}
	
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(probs_count);
	result_SP = EidosValue_SP(float_result);
	
	if (x_count == 1)
	{
		// All quantiles of a singleton are the value of the singleton; the probabilities don't matter as long as they're in range (checked above)
		double x_singleton = x_value->NumericAtIndex_NOCAST(0, nullptr);
		
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
			const double *float_data = x_value->FloatData();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (std::isnan(float_data[value_index]))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): quantiles of NAN are undefined." << EidosTerminate(nullptr);
		}
		
		// Next we get an order vector for x, which can be integer or float
		std::vector<int64_t> order;
		
		if (x_type == EidosValueType::kValueInt)
			order = EidosSortIndexes(x_value->IntData(), x_count, true);
		else if (x_type == EidosValueType::kValueFloat)
			order = EidosSortIndexes(x_value->FloatData(), x_count, true);
		
		// Now loop over the requested probabilities and calculate them
		for (int probs_index = 0; probs_index < probs_count; ++probs_index)
		{
			double prob = probs[probs_index];
			double index = (x_count - 1) * prob;
			int64_t lo = (int64_t)std::floor(index);
			int64_t hi = (int64_t)std::ceil(index);
			
			double quantile = x_value->NumericAtIndex_NOCAST((int)order[lo], nullptr);
			if (lo != hi) {
				double h = index - lo;
				quantile *= (1.0 - h);
				quantile += h * x_value->NumericAtIndex_NOCAST((int)order[hi], nullptr);
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
		EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(2);
		result_SP = EidosValue_SP(int_result);
		
		int64_t max = p_arguments[first_nonempty_argument]->IntAtIndex_NOCAST(0, nullptr);
		int64_t min = max;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			const int64_t *int_data = arg_value->IntData();
			
			for (int value_index = 0; value_index < arg_count; ++value_index)
			{
				int64_t temp = int_data[value_index];
				if (max < temp)
					max = temp;
				else if (min > temp)
					min = temp;
			}
		}
		
		int_result->set_int_no_check(min, 0);
		int_result->set_int_no_check(max, 1);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(2);
		result_SP = EidosValue_SP(float_result);
		
		double max = p_arguments[first_nonempty_argument]->FloatAtIndex_NOCAST(0, nullptr);
		double min = max;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			const double *float_data = arg_value->FloatData();
			
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
		
		float_result->set_float_no_check(min, 0);
		float_result->set_float_no_check(max, 1);
	}
	
	return result_SP;
}

//	(float$)sd(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sd(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	// This is different from the behavior of var(), cor(), and cov(), but follows R
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count <= 1)
		return gStaticEidosValue_FloatNAN;
	
	double mean = 0;
	double sd = 0;
	
	for (int value_index = 0; value_index < x_count; ++value_index)
		mean += x_value->NumericAtIndex_NOCAST(value_index, nullptr);
	
	mean /= x_count;
	
	for (int value_index = 0; value_index < x_count; ++value_index)
	{
		double temp = (x_value->NumericAtIndex_NOCAST(value_index, nullptr) - mean);
		sd += temp * temp;
	}
	
	sd = sqrt(sd / (x_count - 1));
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(sd));
	
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
	
	const double *vec1 = x_value->FloatData();
	double pvalue = 0.0;
	
	if (y_type != EidosValueType::kValueNULL)
	{
		// This is the x & y case, which is a two-sample Welch's t-test
		if (y_count <= 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ttest): function ttest() requires enough elements in y to compute variance." << EidosTerminate(nullptr);
		
		const double *vec2 = y_value->FloatData();
		
		// Right now this function only provides a two-sample t-test; we could add an optional mu argument and make y optional in order to allow a one-sample test as well
		// If we got into that, we'd probably want to provide one-sided t-tests as well, yada yada...
		pvalue = Eidos_TTest_TwoSampleWelch(vec1, x_count, vec2, y_count, nullptr, nullptr);
	}
	else if (mu_type != EidosValueType::kValueNULL)
	{
		// This is the x & mu case, which is a one-sample t-test
		double mu = mu_value->FloatAtIndex_NOCAST(0, nullptr);
		
		pvalue = Eidos_TTest_OneSample(vec1, x_count, mu, nullptr);
	}
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(pvalue));
	
	return result_SP;
}

//	(float$)var(numeric x)
EidosValue_SP Eidos_ExecuteFunction_var(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	
	if (x_value->IsMatrixOrArray())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_var): function var() does not support a matrix/array argument; use cov() to calculate variance-covariance matrices." << EidosTerminate(nullptr);
	
	int x_count = x_value->Count();
	
	if (x_count <= 1)
		return gStaticEidosValue_FloatNAN;
	
	// calculate variance of x (covariance between x and itself)
	double cov = _Eidos_CalcCovariance(x_count, x_value, x_value, 0, 0);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(cov));
}






































