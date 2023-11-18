//
//  eidos_functions_math.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15; split from eidos_functions.cpp 09/26/2022
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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

#include <utility>
#include <string>
#include <vector>

// BCH 20 October 2016: continuing to try to fix problems with gcc 5.4.0 on Linux without breaking other
// builds.  We will switch to including <cmath> and using the std:: namespace math functions, since on
// that platform <cmath> is included as a side effect even if we don't include it ourselves, and on
// that platform that actually (incorrectly) undefines the global functions defined by math.h.  On other
// platforms, we get the global math.h functions defined as well, so we can't use using to select the
// <cmath> functions, we have to specify them explicitly.
// BCH 21 May 2017: since this continues to come up as an issue, it's worth adding a bit more commentary.
// New code introduced on OS X may not correctly use the std:: namespace qualifier for math functions,
// because it is not required in Xcode, and then the build breaks on Linux.  This problem is made worse
// by the fact that gsl_math.h includes math.h, so that brings in the C functions in the global namespace.
// We can't change that, because the GSL is C code and needs to use the C math library; it has not been
// validated against the C++ math library as far as I know, so changing it to use cmath would be
// dangerous.  So I think we need to just tolerate this build issue and fix it when it arises.
#include <cmath>


// ************************************************************************************
//
//	math functions
//
#pragma mark -
#pragma mark Math functions
#pragma mark -


//	(numeric)abs(numeric x)
EidosValue_SP Eidos_ExecuteFunction_abs(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			// This is an overflow-safe version of:
			//result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(llabs(x_value->IntAtIndex(0, nullptr)));
			
			int64_t operand = x_value->IntAtIndex(0, nullptr);
			
			// the absolute value of INT64_MIN cannot be represented in int64_t
			if (operand == INT64_MIN)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_abs): function abs() cannot take the absolute value of the most negative integer." << EidosTerminate(nullptr);
			
			int64_t abs_result = llabs(operand);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(abs_result));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				// This is an overflow-safe version of:
				//int_result->set_int_no_check(llabs(int_vec[value_index]), value_index);
				
				int64_t operand = int_data[value_index];
				
				// the absolute value of INT64_MIN cannot be represented in int64_t
				if (operand == INT64_MIN)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_abs): function abs() cannot take the absolute value of the most negative integer." << EidosTerminate(nullptr);
				
				int64_t abs_result = llabs(operand);
				
				int_result->set_int_no_check(abs_result, value_index);
			}
		}
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fabs(x_value->FloatAtIndex(0, nullptr))));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			double *float_result_data = float_result->data();
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_ABS_FLOAT);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_ABS_FLOAT) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
				float_result_data[value_index] = fabs(float_data[value_index]);
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)acos(numeric x)
EidosValue_SP Eidos_ExecuteFunction_acos(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(acos(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(acos(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)asin(numeric x)
EidosValue_SP Eidos_ExecuteFunction_asin(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(asin(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(asin(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)atan(numeric x)
EidosValue_SP Eidos_ExecuteFunction_atan(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(atan(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(atan(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)atan2(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_atan2(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	int y_count = y_value->Count();
	
	if (x_count != y_count)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_atan2): function atan2() requires arguments of equal length." << EidosTerminate(nullptr);
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int x_dimcount = x_value->DimensionCount();
	int y_dimcount = y_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(x_value, y_value));
	
	if ((x_dimcount > 1) && (y_dimcount > 1) && !EidosValue::MatchingDimensions(x_value, y_value))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_atan2): non-conformable array operands in atan2()." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(atan2(x_value->FloatAtIndex(0, nullptr), y_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(atan2(x_value->FloatAtIndex(value_index, nullptr), y_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	return result_SP;
}

//	(float)ceil(float x)
EidosValue_SP Eidos_ExecuteFunction_ceil(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(ceil(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_CEIL);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_CEIL) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = ceil(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)cos(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cos(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cos(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(cos(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric)cumProduct(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cumProduct(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t product = 1;
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t operand = int_data[value_index];
				
				bool overflow = Eidos_mul_overflow(product, operand, &product);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cumProduct): integer multiplication overflow in function cumProduct()." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(product, value_index);
			}
		}
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double product = 1.0;
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				product *= float_data[value_index];
				float_result->set_float_no_check(product, value_index);
			}
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric)cumSum(numeric x)
EidosValue_SP Eidos_ExecuteFunction_cumSum(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t sum = 0;
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t operand = int_data[value_index];
				
				bool overflow = Eidos_add_overflow(sum, operand, &sum);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cumSum): integer addition overflow in function cumSum()." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(sum, value_index);
			}
		}
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double sum = 0.0;
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				sum += float_data[value_index];
				float_result->set_float_no_check(sum, value_index);
			}
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)exp(numeric x)
EidosValue_SP Eidos_ExecuteFunction_exp(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(exp(x_value->FloatAtIndex(0, nullptr))));
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(exp(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_EXP_FLOAT);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_EXP_FLOAT) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = exp(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)floor(float x)
EidosValue_SP Eidos_ExecuteFunction_floor(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(floor(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_FLOOR);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_FLOOR) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = floor(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(integer)integerDiv(integer x, integer y)
EidosValue_SP Eidos_ExecuteFunction_integerDiv(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	int y_count = y_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int x_dimcount = x_value->DimensionCount();
	int y_dimcount = y_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(x_value, y_value));
	
	if ((x_dimcount > 1) && (y_dimcount > 1) && !EidosValue::MatchingDimensions(x_value, y_value))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): non-conformable array arguments to integerDiv()." << EidosTerminate(nullptr);
	
	if ((x_count == 1) && (y_count == 1))
	{
		int64_t int1 = x_value->IntAtIndex(0, nullptr);
		int64_t int2 = y_value->IntAtIndex(0, nullptr);
		
		if (int2 == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int1 / int2));
	}
	else
	{
		if (x_count == y_count)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int1 = int1_data[value_index];
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 / int2, value_index);
			}
		}
		else if (x_count == 1)
		{
			int64_t int1 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(y_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < y_count; ++value_index)
			{
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 / int2, value_index);
			}
		}
		else if (y_count == 1)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			int64_t int2 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			if (int2 == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() cannot perform division by 0." << EidosTerminate(nullptr);
			
			// Special-case division by 2, since it is common
			// BCH 13 April 2017: Removing this optimization; it produces inconsistent behavior for negative numerators.
			// This optimization was originally committed on 2 March 2017; it was never in any release version of SLiM.
//			if (int2 == 2)
//			{
//				for (int value_index = 0; value_index < x_count; ++value_index)
//					int_result->set_int_no_check(int1_vec[value_index] >> 1, value_index);
//			}
//			else
//			{
				for (int value_index = 0; value_index < x_count; ++value_index)
					int_result->set_int_no_check(int1_data[value_index] / int2, value_index);
//			}
		}
		else	// if ((x_count != y_count) && (x_count != 1) && (y_count != 1))
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerDiv): function integerDiv() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(nullptr);
		}
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	return result_SP;
}

//	(integer)integerMod(integer x, integer y)
EidosValue_SP Eidos_ExecuteFunction_integerMod(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *y_value = p_arguments[1].get();
	int y_count = y_value->Count();
	
	// matrices/arrays must be conformable, and we need to decide here which operand's dimensionality will be used for the result
	int x_dimcount = x_value->DimensionCount();
	int y_dimcount = y_value->DimensionCount();
	EidosValue_SP result_dim_source(EidosValue::BinaryOperationDimensionSource(x_value, y_value));
	
	if ((x_dimcount > 1) && (y_dimcount > 1) && !EidosValue::MatchingDimensions(x_value, y_value))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): non-conformable array arguments to integerMod()." << EidosTerminate(nullptr);
	
	if ((x_count == 1) && (y_count == 1))
	{
		int64_t int1 = x_value->IntAtIndex(0, nullptr);
		int64_t int2 = y_value->IntAtIndex(0, nullptr);
		
		if (int2 == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int1 % int2));
	}
	else
	{
		if (x_count == y_count)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t int1 = int1_data[value_index];
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 % int2, value_index);
			}
		}
		else if (x_count == 1)
		{
			int64_t int1 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int2_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(y_count);
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index = 0; value_index < y_count; ++value_index)
			{
				int64_t int2 = int2_data[value_index];
				
				if (int2 == 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(int1 % int2, value_index);
			}
		}
		else if (y_count == 1)
		{
			const int64_t *int1_data = x_value->IntVector()->data();
			int64_t int2 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
			
			if (int2 == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() cannot perform modulo by 0." << EidosTerminate(nullptr);
			
			// Special-case modulo by 2, since it is common
			// BCH 13 April 2017: Removing this optimization; it produces inconsistent behavior for negative numerators.
			// This optimization was originally committed on 2 March 2017; it was never in any release version of SLiM.
//			if (int2 == 2)
//			{
//				for (int value_index = 0; value_index < x_count; ++value_index)
//					int_result->set_int_no_check(int1_vec[value_index] & (int64_t)0x01, value_index);
//			}
//			else
//			{
				for (int value_index = 0; value_index < x_count; ++value_index)
					int_result->set_int_no_check(int1_data[value_index] % int2, value_index);
//			}
		}
		else	// if ((x_count != y_count) && (x_count != 1) && (y_count != 1))
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integerMod): function integerMod() requires that either (1) both operands have the same size(), or (2) one operand has size() == 1." << EidosTerminate(nullptr);
		}
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(result_dim_source.get());
	
	return result_SP;
}

//	(logical)isFinite(float x)
EidosValue_SP Eidos_ExecuteFunction_isFinite(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		if (x_value ->DimensionCount() == 1)
			result_SP = (std::isfinite(x_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{std::isfinite(x_value->FloatAtIndex(0, nullptr))});
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(std::isfinite(float_data[value_index]), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)isInfinite(float x)
EidosValue_SP Eidos_ExecuteFunction_isInfinite(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		if (x_value ->DimensionCount() == 1)
			result_SP = (std::isinf(x_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{(eidos_logical_t)std::isinf(x_value->FloatAtIndex(0, nullptr))});
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(std::isinf(float_data[value_index]), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)isNAN(float x)
EidosValue_SP Eidos_ExecuteFunction_isNAN(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		if (x_value ->DimensionCount() == 1)
			result_SP = (std::isnan(x_value->FloatAtIndex(0, nullptr)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{(eidos_logical_t)std::isnan(x_value->FloatAtIndex(0, nullptr))});
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(std::isnan(float_data[value_index]), value_index);
	}
	
	// Copy dimensions from whichever operand we chose at the beginning
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)log(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log(x_value->FloatAtIndex(0, nullptr))));
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(log(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_LOG_FLOAT);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_LOG_FLOAT) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = log(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)log10(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log10(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log10(x_value->FloatAtIndex(0, nullptr))));
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(log10(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_LOG10_FLOAT);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_LOG10_FLOAT) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = log10(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)log2(numeric x)
EidosValue_SP Eidos_ExecuteFunction_log2(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(log2(x_value->FloatAtIndex(0, nullptr))));
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(log2(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_LOG2_FLOAT);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_LOG2_FLOAT) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = log2(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric$)product(numeric x)
EidosValue_SP Eidos_ExecuteFunction_product(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t product = 1;
			double product_d = 1.0;
			bool fits_in_integer = true;
			
			// We do a tricky thing here.  We want to try to compute in integer, but switch to float if we overflow.
			// If we do overflow, we want to minimize numerical error by accumulating in integer for as long as we
			// can, and then throwing the integer accumulator over into the float accumulator only when it is about
			// to overflow.  We perform both computations in parallel, and use integer for the result if we can.
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t old_product = product;
				int64_t temp = int_data[value_index];
				
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
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double product = 1;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				product *= float_data[value_index];
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(product));
		}
	}
	
	return result_SP;
}

//	(float)round(float x)
EidosValue_SP Eidos_ExecuteFunction_round(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(round(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_ROUND);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_ROUND) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = round(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(*)setDifference(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setDifference(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setDifference): function setDifference() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidosObject_Class) && (class1 != gEidosObject_Class))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setDifference): function setDifference() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if (x_count == 0)
	{
		// If x is empty, the difference is the empty set
		if (class1 && (class1 != gEidosObject_Class))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if (y_count == 0)
	{
		// If y is empty, the difference is x, uniqued
		result_SP = UniqueEidosValue(x_value, false, true);
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_data1 = y_value->LogicalVector()->data();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_data0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (!logical_data0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (logical_data0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_data1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (!logical_data1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (logical_data1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		// NOLINTBEGIN(*-branch-clone) : intentional branch clones
		if (containsF1 && containsT1)
			result_SP = gStaticEidosValue_Logical_ZeroVec;
		else if (containsT0 && containsF0 && !containsT1 && !containsF1)
		{
			// CODE COVERAGE: This is dead code
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
		else if (containsT0 && !containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else if (containsF0 && !containsF1)
			result_SP = gStaticEidosValue_LogicalF;
		else
			result_SP = gStaticEidosValue_Logical_ZeroVec;
		// NOLINTEND(*-branch-clone)
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = gStaticEidosValue_Integer_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = gStaticEidosValue_Float_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &string0 = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::string &string1 = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = gStaticEidosValue_String_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = x_value->NewMatchingType();
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if (x_count == 1)
	{
		// If any element in y matches the element in x, the result is an empty vector
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int_data = y_value->IntVector()->data();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
				if (int0 == int_data[value_index])
					return gStaticEidosValue_Integer_ZeroVec;
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr);
			const double *float_data = y_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
			{
				double float1 = float_data[value_index];
				
				if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
					return gStaticEidosValue_Float_ZeroVec;
			}
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &string0 = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *y_value->StringVector();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
				if (string0 == string_vec[value_index])
					return gStaticEidosValue_String_ZeroVec;
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *obj0 = x_value->ObjectElementAtIndex(0, nullptr);
			EidosObject * const *object_vec = y_value->ObjectElementVector()->data();
			
			for (int value_index = 0; value_index < y_count; ++value_index)
				if (obj0 == object_vec[value_index])
					return x_value->NewMatchingType();
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if (y_count == 1)
	{
		// The result is x uniqued, minus the element in y if it matches
		result_SP = UniqueEidosValue(x_value, true, true);
		
		int result_count = result_SP->Count();
		
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int1 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_vec = result_SP->IntVector_Mutable();
			const int64_t *int_data = int_vec->data();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				if (int1 == int_data[value_index])
				{
					int_vec->erase_index(value_index);
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float1 = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_vec = result_SP->FloatVector_Mutable();
			double *float_data = float_vec->data();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
			{
				double float0 = float_data[value_index];
				
				if ((std::isnan(float1) && std::isnan(float0)) || (float1 == float0))
				{
					float_vec->erase_index(value_index);
					break;
				}
			}
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &string1 = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			std::vector<std::string> &string_vec = *result_SP->StringVector_Mutable();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				if (string1 == string_vec[value_index])
				{
					string_vec.erase(string_vec.begin() + value_index);
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			EidosValue_Object_vector *object_element_vec = result_SP->ObjectElementVector_Mutable();
			EidosObject * const *object_element_data = object_element_vec->data();
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				if (obj1 == object_element_data[value_index])
				{
					object_element_vec->erase_index(value_index);
					break;
				}
		}
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				int64_t value = int_data0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == int_data1[value_index1])
						break;
				
				if (value_index1 < y_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == int_data0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				int_result->push_int(value);
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data0 = x_value->FloatVector()->data();
			const double *float_data1 = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				double value0 = float_data0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
				{
					double value1 = float_data1[value_index1];
					
					if ((std::isnan(value0) && std::isnan(value1)) || (value0 == value1))
						break;
				}
				
				if (value_index1 < y_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
				{
					double value_scan = float_data0[scan_index];
					
					if ((std::isnan(value0) && std::isnan(value_scan)) || (value0 == value_scan))
						break;
				}
				
				if (scan_index < value_index0)
					continue;
				
				float_result->push_float(value0);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *y_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == string_vec1[value_index1])
						break;
				
				if (value_index1 < y_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == string_vec0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				string_result->PushString(value);
			}
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObject * const *object_vec0 = x_value->ObjectElementVector()->data();
			EidosObject * const *object_vec1 = y_value->ObjectElementVector()->data();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				EidosObject *value = object_vec0[value_index0];
				int value_index1, scan_index;
				
				// First check that the value does not exist in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == object_vec1[value_index1])
						break;
				
				if (value_index1 < y_count)
					continue;
				
				// Then check that we have not already handled the same value (uniquing)
				for (scan_index = 0; scan_index < value_index0; ++scan_index)
					if (value == object_vec0[scan_index])
						break;
				
				if (scan_index < value_index0)
					continue;
				
				object_result->push_object_element_CRR(value);
			}
		}
	}
	
	return result_SP;
}

//	(*)setIntersection(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setIntersection(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setIntersection): function setIntersection() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidosObject_Class) && (class1 != gEidosObject_Class))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setIntersection): function setIntersection() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if ((x_count == 0) || (y_count == 0))
	{
		// If either argument is empty, the intersection is the empty set
		if (class1 && (class1 != gEidosObject_Class))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_data1 = y_value->LogicalVector()->data();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_data0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (!logical_data0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (logical_data0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_data1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (!logical_data1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (logical_data1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		if (containsF0 && containsT0 && containsF1 && containsT1)
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
		else if (containsF0 && containsF1)
			result_SP = gStaticEidosValue_LogicalF;
		else if (containsT0 && containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else
			result_SP = gStaticEidosValue_Logical_ZeroVec;
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
			else
				result_SP = gStaticEidosValue_Integer_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
			else
				result_SP = gStaticEidosValue_Float_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &string0 = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::string &string1 = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
			else
				result_SP = gStaticEidosValue_String_ZeroVec;
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
			else
				result_SP = x_value->NewMatchingType();
		}
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// If either argument is singleton (but not both), handle that case with a fast check
		if (x_count == 1)
		{
			std::swap(x_count, y_count);
			std::swap(x_value, y_value);
		}
		
		// now x_count > 1, y_count == 1
		bool found_match = false;
		
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t value = y_value->IntAtIndex(0, nullptr);
			const int64_t *int_data = x_value->IntVector()->data();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
				if (value == int_data[scan_index])
				{
					found_match = true;
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double value0 = y_value->FloatAtIndex(0, nullptr);
			const double *float_data = x_value->FloatVector()->data();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
			{
				double value1 = float_data[scan_index];
				
				if ((std::isnan(value0) && std::isnan(value1)) || (value0 == value1))
				{
					found_match = true;
					break;
				}
			}
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &value = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *x_value->StringVector();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
				if (value == string_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *value = y_value->ObjectElementAtIndex(0, nullptr);
			EidosObject * const *object_vec = x_value->ObjectElementVector()->data();
			
			for (int scan_index = 0; scan_index < x_count; ++scan_index)
				if (value == object_vec[scan_index])
				{
					found_match = true;
					break;
				}
		}
		
		if (found_match)
			result_SP = y_value->CopyValues();
		else
			result_SP = x_value->NewMatchingType();
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				int64_t value = int_data0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == int_data1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == int_data0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							int_result->push_int(value);
						break;
					}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data0 = x_value->FloatVector()->data();
			const double *float_data1 = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				double value0 = float_data0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
				{
					double value1 = float_data1[value_index1];
					
					if ((std::isnan(value0) && std::isnan(value1)) || (value0 == value1))
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							double value_scan = float_data0[scan_index];
							
							if ((std::isnan(value0) && std::isnan(value_scan)) || (value0 == value_scan))
								break;
						}
						
						if (scan_index == value_index0)
							float_result->push_float(value0);
						break;
					}
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *y_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == string_vec1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == string_vec0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							string_result->PushString(value);
						break;
					}
			}
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObject * const *object_vec0 = x_value->ObjectElementVector()->data();
			EidosObject * const *object_vec1 = y_value->ObjectElementVector()->data();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (int value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				EidosObject *value = object_vec0[value_index0];
				
				// First check that the value also exists in y
				for (int value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == object_vec1[value_index1])
					{
						// Then check that we have not already handled the same value (uniquing)
						int scan_index;
						
						for (scan_index = 0; scan_index < value_index0; ++scan_index)
						{
							if (value == object_vec0[scan_index])
								break;
						}
						
						if (scan_index == value_index0)
							object_result->push_object_element_CRR(value);
						break;
					}
			}
		}
	}
	
	return result_SP;
}

//	(*)setSymmetricDifference(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setSymmetricDifference(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setSymmetricDifference): function setSymmetricDifference() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidosObject_Class) && (class1 != gEidosObject_Class))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setSymmetricDifference): function setSymmetricDifference() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if (x_count + y_count == 0)
	{
		if (class1 && (class1 != gEidosObject_Class))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if ((x_count == 1) && (y_count == 0))
	{
		result_SP = x_value->CopyValues();
	}
	else if ((x_count == 0) && (y_count == 1))
	{
		result_SP = y_value->CopyValues();
	}
	else if (x_count == 0)
	{
		result_SP = UniqueEidosValue(y_value, false, true);
	}
	else if (y_count == 0)
	{
		result_SP = UniqueEidosValue(x_value, false, true);
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_data1 = y_value->LogicalVector()->data();
		bool containsF0 = false, containsT0 = false, containsF1 = false, containsT1 = false;
		
		if (logical_data0[0])
		{
			containsT0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (!logical_data0[value_index])
				{
					containsF0 = true;
					break;
				}
		}
		else
		{
			containsF0 = true;
			
			for (int value_index = 1; value_index < x_count; ++value_index)
				if (logical_data0[value_index])
				{
					containsT0 = true;
					break;
				}
		}
		
		if (logical_data1[0])
		{
			containsT1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (!logical_data1[value_index])
				{
					containsF1 = true;
					break;
				}
		}
		else
		{
			containsF1 = true;
			
			for (int value_index = 1; value_index < y_count; ++value_index)
				if (logical_data1[value_index])
				{
					containsT1 = true;
					break;
				}
		}
		
		if ((containsF0 != containsF1) && (containsT0 != containsT1))
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
		else if ((containsF0 == containsF1) && (containsT0 == containsT1))
			result_SP = gStaticEidosValue_Logical_ZeroVec;
		else if (containsT0 != containsT1)
			result_SP = gStaticEidosValue_LogicalT;
		else // (containsF0 != containsF1)
			result_SP = gStaticEidosValue_LogicalF;
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// If both arguments are singleton, handle that case with a simple equality check
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = gStaticEidosValue_Integer_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{int0, int1});
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = gStaticEidosValue_Float_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{float0, float1});
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &string0 = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::string &string1 = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = gStaticEidosValue_String_ZeroVec;
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{string0, string1});
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = x_value->NewMatchingType();
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector({obj0, obj1}, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// We have one value that is a singleton, one that is a vector.  We'd like this case to be fast,
		// so that addition of a single element to a set is a fast operation.
		if (x_count == 1)
		{
			std::swap(x_count, y_count);
			std::swap(x_value, y_value);
		}
		
		// now x_count > 1, y_count == 1
		result_SP = UniqueEidosValue(x_value, true, true);
		
		int result_count = result_SP->Count();
		
		// result_SP is modifiable and is guaranteed to be a vector, so now the result is x uniqued,
		// minus the element in y if it matches, but plus the element in y if it does not match
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int1 = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_vec = result_SP->IntVector_Mutable();
			const int64_t *int_data = int_vec->data();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (int1 == int_data[value_index])
					break;
			
			if (value_index == result_count)
				int_vec->push_int(int1);
			else
				int_vec->erase_index(value_index);
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float1 = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_vec = result_SP->FloatVector_Mutable();
			double *float_data = float_vec->data();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
			{
				double float0 = float_data[value_index];
				
				if ((std::isnan(float0) && std::isnan(float1)) || (float1 == float0))
					break;
			}
			
			if (value_index == result_count)
				float_vec->push_float(float1);
			else
				float_vec->erase_index(value_index);
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &string1 = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			std::vector<std::string> &string_vec = *result_SP->StringVector_Mutable();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (string1 == string_vec[value_index])
					break;
			
			if (value_index == result_count)
				string_vec.emplace_back(string1);
			else
				string_vec.erase(string_vec.begin() + value_index);
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			EidosValue_Object_vector *object_element_vec = result_SP->ObjectElementVector_Mutable();
			EidosObject * const *object_element_data = object_element_vec->data();
			int value_index;
			
			for (value_index = 0; value_index < result_count; ++value_index)
				if (obj1 == object_element_data[value_index])
					break;
			
			if (value_index == result_count)
				object_element_vec->push_object_element_CRR(obj1);
			else
				object_element_vec->erase_index(value_index);
		}
	}
	else
	{
		// Both arguments have size >1, so we can use fast APIs for both.  Loop through x adding
		// unique values not in y, then loop through y adding unique values not in x.
		int value_index0, value_index1, scan_index;
		
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			result_SP = EidosValue_SP(int_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				int64_t value = int_data0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == int_data1[value_index1])
						break;
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == int_data0[scan_index])
							break;
					
					if (scan_index == value_index0)
						int_result->push_int(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				int64_t value = int_data1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
					if (value == int_data0[value_index0])
						break;
				
				if (value_index0 == x_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == int_data1[scan_index])
							break;
					
					if (scan_index == value_index1)
						int_result->push_int(value);
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_vec0 = x_value->FloatVector()->data();
			const double *float_vec1 = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			result_SP = EidosValue_SP(float_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				double value0 = float_vec0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
				{
					double float1 = float_vec1[value_index1];
					
					if ((std::isnan(value0) && std::isnan(float1)) || (value0 == float1))
						break;
				}
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
					{
						double value_scan = float_vec0[scan_index];
						
						if ((std::isnan(value0) && std::isnan(value_scan)) || (value0 == value_scan))
							break;
					}
					
					if (scan_index == value_index0)
						float_result->push_float(value0);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				double value1 = float_vec1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
				{
					double value0 = float_vec0[value_index0];
					
					if ((std::isnan(value1) && std::isnan(value0)) || (value1 == value0))
						break;
				}
				
				if (value_index0 == x_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
					{
						double value_scan = float_vec1[scan_index];
						
						if ((std::isnan(value1) && std::isnan(value_scan)) || (value1 == value_scan))
							break;
					}
					
					if (scan_index == value_index1)
						float_result->push_float(value1);
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *y_value->StringVector();
			EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
			result_SP = EidosValue_SP(string_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				std::string value = string_vec0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == string_vec1[value_index1])
						break;
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == string_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						string_result->PushString(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				std::string value = string_vec1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
					if (value == string_vec0[value_index0])
						break;
				
				if (value_index0 == x_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == string_vec1[scan_index])
							break;
					
					if (scan_index == value_index1)
						string_result->PushString(value);
				}
			}
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObject * const *object_vec0 = x_value->ObjectElementVector()->data();
			EidosObject * const *object_vec1 = y_value->ObjectElementVector()->data();
			EidosValue_Object_vector *object_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class());
			result_SP = EidosValue_SP(object_result);
			
			for (value_index0 = 0; value_index0 < x_count; ++value_index0)
			{
				EidosObject *value = object_vec0[value_index0];
				
				// First check that the value also exists in y
				for (value_index1 = 0; value_index1 < y_count; ++value_index1)
					if (value == object_vec1[value_index1])
						break;
				
				if (value_index1 == y_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index0; ++scan_index)
						if (value == object_vec0[scan_index])
							break;
					
					if (scan_index == value_index0)
						object_result->push_object_element_CRR(value);
				}
			}
			
			for (value_index1 = 0; value_index1 < y_count; ++value_index1)
			{
				EidosObject *value = object_vec1[value_index1];
				
				// First check that the value also exists in y
				for (value_index0 = 0; value_index0 < x_count; ++value_index0)
					if (value == object_vec0[value_index0])
						break;
				
				if (value_index0 == x_count)
				{
					// Then check that we have not already handled the same value (uniquing)
					for (scan_index = 0; scan_index < value_index1; ++scan_index)
						if (value == object_vec1[scan_index])
							break;
					
					if (scan_index == value_index1)
						object_result->push_object_element_CRR(value);
				}
			}
		}
	}
	
	return result_SP;
}

//	(*)setUnion(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_setUnion(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	EidosValue *y_value = p_arguments[1].get();
	EidosValueType y_type = y_value->Type();
	int y_count = y_value->Count();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setUnion): function setUnion() requires that both operands have the same type." << EidosTerminate(nullptr);
	
	EidosValueType arg_type = x_type;
	const EidosClass *class0 = nullptr, *class1 = nullptr;
	
	if (arg_type == EidosValueType::kValueObject)
	{
		class0 = ((EidosValue_Object *)x_value)->Class();
		class1 = ((EidosValue_Object *)y_value)->Class();
		
		if ((class0 != class1) && (class0 != gEidosObject_Class) && (class1 != gEidosObject_Class))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setUnion): function setUnion() requires that both operands of object type have the same class (or undefined class)." << EidosTerminate(nullptr);
	}
	
	if (x_count + y_count == 0)
	{
		if (class1 && (class1 != gEidosObject_Class))
			result_SP = y_value->NewMatchingType();
		else
			result_SP = x_value->NewMatchingType();
	}
	else if ((x_count == 1) && (y_count == 0))
	{
		result_SP = x_value->CopyValues();
	}
	else if ((x_count == 0) && (y_count == 1))
	{
		result_SP = y_value->CopyValues();
	}
	else if (arg_type == EidosValueType::kValueLogical)
	{
		// Because EidosValue_Logical works differently than other EidosValue types, this code can handle
		// both the singleton and non-singleton cases; LogicalVector() is always available
		const eidos_logical_t *logical_vec0 = x_value->LogicalVector()->data();
		const eidos_logical_t *logical_vec1 = y_value->LogicalVector()->data();
		bool containsF = false, containsT = false;
		
		if (((x_count > 0) && logical_vec0[0]) || ((y_count > 0) && logical_vec1[0]))
		{
			// We have a true value; look for a false value
			containsT = true;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (!logical_vec0[value_index])
				{
					containsF = true;
					break;
				}
			
			if (!containsF)
				for (int value_index = 0; value_index < y_count; ++value_index)
					if (!logical_vec1[value_index])
					{
						containsF = true;
						break;
					}
		}
		else
		{
			// We have a false value; look for a true value
			containsF = true;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (logical_vec0[value_index])
				{
					containsT = true;
					break;
				}
			
			if (!containsT)
				for (int value_index = 0; value_index < y_count; ++value_index)
					if (logical_vec1[value_index])
					{
						containsT = true;
						break;
					}
		}
		
		if (containsF && !containsT)
			result_SP = gStaticEidosValue_LogicalF;
		else if (containsT && !containsF)
			result_SP = gStaticEidosValue_LogicalT;
		else if (!containsT && !containsF)
			result_SP = gStaticEidosValue_Logical_ZeroVec;		// CODE COVERAGE: This is dead code
		else	// containsT && containsF
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(2);
			result_SP = EidosValue_SP(logical_result);
			
			logical_result->set_logical_no_check(false, 0);
			logical_result->set_logical_no_check(true, 1);
		}
	}
	else if (x_count == 0)
	{
		// x is zero-length, y is >1, so we just need to unique y
		result_SP = UniqueEidosValue(y_value, false, true);
	}
	else if (y_count == 0)
	{
		// y is zero-length, x is >1, so we just need to unique x
		result_SP = UniqueEidosValue(x_value, false, true);
	}
	else if ((x_count == 1) && (y_count == 1))
	{
		// Make a bit of an effort to produce a singleton result, while handling the singleton/singleton case
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t int0 = x_value->IntAtIndex(0, nullptr), int1 = y_value->IntAtIndex(0, nullptr);
			
			if (int0 == int1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{int0, int1});
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double float0 = x_value->FloatAtIndex(0, nullptr), float1 = y_value->FloatAtIndex(0, nullptr);
			
			if ((std::isnan(float0) && std::isnan(float1)) || (float0 == float1))
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{float0, float1});
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &string0 = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::string &string1 = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			
			if (string0 == string1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{string0, string1});
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *obj0 = x_value->ObjectElementAtIndex(0, nullptr), *obj1 = y_value->ObjectElementAtIndex(0, nullptr);
			
			if (obj0 == obj1)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(obj0, ((EidosValue_Object *)x_value)->Class()));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector({obj0, obj1}, ((EidosValue_Object *)x_value)->Class()));
		}
	}
	else if ((x_count == 1) || (y_count == 1))
	{
		// We have one value that is a singleton, one that is a vector.  We'd like this case to be fast,
		// so that addition of a single element to a set is a fast operation.
		if (x_count == 1)
		{
			std::swap(x_count, y_count);
			std::swap(x_value, y_value);
		}
		
		// now x_count > 1, y_count == 1
		result_SP = UniqueEidosValue(x_value, true, true);
		
		int result_count = result_SP->Count();
		
		// result_SP is modifiable and is guaranteed to be a vector, so now add y if necessary using the fast APIs
		if (arg_type == EidosValueType::kValueInt)
		{
			int64_t value = y_value->IntAtIndex(0, nullptr);
			const int64_t *int_data = result_SP->IntVector()->data();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == int_data[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				result_SP->IntVector_Mutable()->push_int(value);
		}
		else if (arg_type == EidosValueType::kValueFloat)
		{
			double value1 = y_value->FloatAtIndex(0, nullptr);
			const double *float_data = result_SP->FloatVector()->data();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				double value0 = float_data[scan_index];
				
				if ((std::isnan(value1) && std::isnan(value0)) || (value1 == value0))
					break;
			}
			
			if (scan_index == result_count)
				result_SP->FloatVector_Mutable()->push_float(value1);
		}
		else if (arg_type == EidosValueType::kValueString)
		{
			const std::string &value = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec = *result_SP->StringVector();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == string_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				result_SP->StringVector_Mutable()->emplace_back(value);
		}
		else if (arg_type == EidosValueType::kValueObject)
		{
			EidosObject *value = y_value->ObjectElementAtIndex(0, nullptr);
			EidosObject * const *object_vec = result_SP->ObjectElementVector()->data();
			int scan_index;
			
			for (scan_index = 0; scan_index < result_count; ++scan_index)
			{
				if (value == object_vec[scan_index])
					break;
			}
			
			if (scan_index == result_count)
				result_SP->ObjectElementVector_Mutable()->push_object_element_CRR(value);
		}
	}
	else
	{
		// We have two arguments which are both vectors of >1 value, so this is the base case.  We construct
		// a new EidosValue containing all elements from both arguments, and then call UniqueEidosValue() to unique it.
		// This code might look slow, but really the uniquing is O(N^2) and everything else is O(N), so since
		// we are in the vector/vector case here, it really isn't worth worrying about optimizing the O(N) part.
		result_SP = ConcatenateEidosValues(p_arguments, false, false);	// no NULL, no VOID
		result_SP = UniqueEidosValue(result_SP.get(), false, true);
	}
	
	return result_SP;
}

//	(float)sin(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sin(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sin(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(sin(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)sqrt(numeric x)
EidosValue_SP Eidos_ExecuteFunction_sqrt(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sqrt(x_value->FloatAtIndex(0, nullptr))));
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(sqrt(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_SQRT_FLOAT);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_SQRT_FLOAT) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = sqrt(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(numeric$)sum(lif x)
EidosValue_SP Eidos_ExecuteFunction_sum(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// BEWARE: this is called by Eidos_ExecuteFunction_mean(), which assumes that arguments match!
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_type == EidosValueType::kValueInt)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
		}
		else
#ifndef _OPENMP
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
			const int64_t *int_data = x_value->IntVector()->data();
			int64_t sum = 0;
			double sum_d = 0;
			bool fits_in_integer = true;
			
			// We do a tricky thing here.  We want to try to compute in integer, but switch to float if we overflow.
			// If we do overflow, we want to minimize numerical error by accumulating in integer for as long as we
			// can, and then throwing the integer accumulator over into the float accumulator only when it is about
			// to overflow.  We perform both computations in parallel, and use integer for the result if we can.
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t old_sum = sum;
				int64_t temp = int_data[value_index];
				
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
#else
		{
			// In the OpenMP case we want to follow fairly different logic, because dealing with catching the overflow
			// case across multiple threads seems excessively complex; instead we look for an overflow afterwards
			const int64_t *int_data = x_value->IntVector()->data();
			double sum_d = 0;

			EIDOS_THREAD_COUNT(gEidos_OMP_threads_SUM_INTEGER);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(int_data) reduction(+: sum_d) if(parallel:x_count >= EIDOS_OMPMIN_SUM_INTEGER) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
				sum_d += int_data[value_index];

			// 2^53 is the largest integer such that it and all smaller integers can be represented in double losslessly
			int64_t sum = (int64_t)sum_d;
			bool fits_in_integer = (((double)sum == sum_d) && (sum < 9007199254740992L) && (sum > -9007199254740992L));

			if (fits_in_integer)
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(sum));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum_d));
		}
#endif
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		if (x_count == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
		}
		else
		{
			// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
			const double *float_data = x_value->FloatVector()->data();
			double sum = 0;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_SUM_FLOAT);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data) reduction(+: sum) if(parallel:x_count >= EIDOS_OMPMIN_SUM_FLOAT) num_threads(thread_count)
			for (int value_index = 0; value_index < x_count; ++value_index)
				sum += float_data[value_index];
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum));
		}
	}
	else if (x_type == EidosValueType::kValueLogical)
	{
		// EidosValue_Logical does not have a singleton subclass, so we can always use the fast API
		const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
		int64_t sum = 0;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_SUM_LOGICAL);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(logical_data) reduction(+: sum) if(parallel:x_count >= EIDOS_OMPMIN_SUM_LOGICAL) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			sum += logical_data[value_index];
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(sum));
	}
	
	return result_SP;
}

//	(float$)sumExact(float x)
EidosValue_SP Eidos_ExecuteFunction_sumExact(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
	}
	else
	{
		// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		double sum = Eidos_ExactSum(float_data, x_count);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum));
	}
	
	return result_SP;
}

//	(float)tan(numeric x)
EidosValue_SP Eidos_ExecuteFunction_tan(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(tan(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(tan(x_value->FloatAtIndex(value_index, nullptr)), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(float)trunc(float x)
EidosValue_SP Eidos_ExecuteFunction_trunc(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(trunc(x_value->FloatAtIndex(0, nullptr))));
	}
	else
	{
		// We have x_count != 1 and x_value is guaranteed to be an EidosValue_Float, so we can use the fast API
		const double *float_data = x_value->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		double *float_result_data = float_result->data();
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_TRUNC);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(x_count) firstprivate(float_data, float_result_data) if(parallel:x_count >= EIDOS_OMPMIN_TRUNC) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result_data[value_index] = trunc(float_data[value_index]);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}










































































