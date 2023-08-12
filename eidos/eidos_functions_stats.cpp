//
//  eidos_functions_stats.cpp
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


//	(float$)cor(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_cor(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	int count = x_value->Count();
	
	if (x_value->IsArray() || y_value->IsArray())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): function cor() does not currently support matrix/array arguments." << EidosTerminate(nullptr);
	if (count != y_value->Count())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cor): function cor() requires that x and y be the same size." << EidosTerminate(nullptr);
	
	if (count > 1)
	{
		// calculate means
		double mean_x = 0, mean_y = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			mean_x += x_value->FloatAtIndex(value_index, nullptr);
			mean_y += y_value->FloatAtIndex(value_index, nullptr);
		}
		
		mean_x /= count;
		mean_y /= count;
		
		// calculate sums of squares and products of differences
		double ss_x = 0, ss_y = 0, diff_prod = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			double dx = x_value->FloatAtIndex(value_index, nullptr) - mean_x;
			double dy = y_value->FloatAtIndex(value_index, nullptr) - mean_y;
			
			ss_x += dx * dx;
			ss_y += dy * dy;
			diff_prod += dx * dy;
		}
		
		// calculate correlation
		double cor = diff_prod / (sqrt(ss_x) * sqrt(ss_y));
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cor));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}

//	(float$)cov(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_cov(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	int count = x_value->Count();
	
	if (x_value->IsArray() || y_value->IsArray())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): function cov() does not currently support matrix/array arguments." << EidosTerminate(nullptr);
	if (count != y_value->Count())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cov): function cov() requires that x and y be the same size." << EidosTerminate(nullptr);
	
	if (count > 1)
	{
		// calculate means
		double mean_x = 0, mean_y = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			mean_x += x_value->FloatAtIndex(value_index, nullptr);
			mean_y += y_value->FloatAtIndex(value_index, nullptr);
		}
		
		mean_x /= count;
		mean_y /= count;
		
		// calculate covariance
		double cov = 0;
		
		for (int value_index = 0; value_index < count; ++value_index)
		{
			double temp_x = (x_value->FloatAtIndex(value_index, nullptr) - mean_x);
			double temp_y = (y_value->FloatAtIndex(value_index, nullptr) - mean_y);
			cov += temp_x * temp_y;
		}
		
		cov = cov / (count - 1);
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cov));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
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
			
			if (arg_count == 1)
			{
				if (arg_value->LogicalAtIndex(0, nullptr) == true)
					return gStaticEidosValue_LogicalT;
			}
			else
			{
				const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
					if (logical_data[value_index] == true)
						return gStaticEidosValue_LogicalT;
			}
		}
		
		result_SP = gStaticEidosValue_LogicalF;
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		int64_t max = p_arguments[first_nonempty_argument]->IntAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntAtIndex(0, nullptr);
				if (max < temp)
					max = temp;
			}
			else if (arg_count > 1)
			{
				const int64_t *int_data = arg_value->IntVector()->data();
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
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(max));
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		double max = p_arguments[first_nonempty_argument]->FloatAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatAtIndex(0, nullptr);
				
				// if there is a NAN the result is always NAN, so we don't need to scan further
				if (std::isnan(temp))
					return gStaticEidosValue_FloatNAN;
				
				if (max < temp)
					max = temp;
			}
			else
			{
				const double *float_data = arg_value->FloatVector()->data();
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
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(max));
	}
	else if (x_type == EidosValueType::kValueString)
	{
		const std::string *max = &(((EidosValue_String *)(p_arguments[first_nonempty_argument].get()))->StringRefAtIndex(0, nullptr));
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue_String *arg_value = (EidosValue_String *)(p_arguments[arg_index].get());
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				const std::string &temp = arg_value->StringRefAtIndex(0, nullptr);
				if (*max < temp)
					max = &temp;
			}
			else
			{
				const std::vector<std::string> &string_vec = *arg_value->StringVector();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
					if (*max < temp)
						max = &temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(*max));
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
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_value->FloatAtIndex(0, nullptr)));
	}
	else
	{
		// Call sum() to do the addition for us, since it takes exactly the same arguments; it will return numeric$ which we treat as float$
		// Note this means we inherit the parallelization/vectorization behavior of sum(); we have no separate benchmarks for mean()
		EidosValue_SP sum_value = Eidos_ExecuteFunction_sum(p_arguments, p_interpreter);
		double sum = sum_value->FloatAtIndex(0, nullptr);
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sum / x_count));
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
			
			if (arg_count == 1)
			{
				if (arg_value->LogicalAtIndex(0, nullptr) == false)
					return gStaticEidosValue_LogicalF;
			}
			else
			{
				const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
					if (logical_data[value_index] == false)
						return gStaticEidosValue_LogicalF;
			}
		}
		
		result_SP = gStaticEidosValue_LogicalT;
	}
	else if (x_type == EidosValueType::kValueInt)
	{
		int64_t min = p_arguments[first_nonempty_argument]->IntAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntAtIndex(0, nullptr);
				if (min > temp)
					min = temp;
			}
			else if (arg_count > 1)
			{
				const int64_t *int_data = arg_value->IntVector()->data();
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
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(min));
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		double min = p_arguments[first_nonempty_argument]->FloatAtIndex(0, nullptr);
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatAtIndex(0, nullptr);
				
				// if there is a NAN the result is always NAN, so we don't need to scan further
				if (std::isnan(temp))
					return gStaticEidosValue_FloatNAN;
				
				if (min > temp)
					min = temp;
			}
			else
			{
				const double *float_data = arg_value->FloatVector()->data();
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
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(min));
	}
	else if (x_type == EidosValueType::kValueString)
	{
		const std::string *min = &(((EidosValue_String *)(p_arguments[first_nonempty_argument].get()))->StringRefAtIndex(0, nullptr));
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue_String *arg_value = (EidosValue_String *)(p_arguments[arg_index].get());
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				const std::string &temp = arg_value->StringRefAtIndex(0, nullptr);
				if (*min > temp)
					min = &temp;
			}
			else
			{
				const std::vector<std::string> &string_vec = *arg_value->StringVector();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
					if (*min > temp)
						min = &temp;
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(*min));
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
			if (std::isnan(x_value->FloatAtIndex(0, nullptr)) || std::isnan(y_value->FloatAtIndex(0, nullptr)))
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
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			eidos_logical_t y_singleton_value = y_value->LogicalAtIndex(0, nullptr);
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] || y_singleton_value, value_index); // || is logical max
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntVector()->data();
			int64_t y_singleton_value = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data();
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
			const double * __restrict__ float0_data = x_value->FloatVector()->data();
			double y_singleton_value = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data();
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
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::string &y_singleton_value = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
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
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			const eidos_logical_t *logical1_data = y_value->LogicalVector()->data();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] || logical1_data[value_index], value_index); // || is logical max
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntVector()->data();
			const int64_t * __restrict__ int1_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data();
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
			const double * __restrict__ float0_data = x_value->FloatVector()->data();
			const double * __restrict__ float1_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data();
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
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::vector<std::string> &string1_vec = *y_value->StringVector();
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
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
			if (std::isnan(x_value->FloatAtIndex(0, nullptr)) || std::isnan(y_value->FloatAtIndex(0, nullptr)))
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
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			eidos_logical_t y_singleton_value = y_value->LogicalAtIndex(0, nullptr);
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] && y_singleton_value, value_index); // && is logical min
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntVector()->data();
			int64_t y_singleton_value = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data();
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
			const double * __restrict__ float0_data = x_value->FloatVector()->data();
			double y_singleton_value = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data();
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
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::string &y_singleton_value = ((EidosValue_String *)y_value)->StringRefAtIndex(0, nullptr);
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
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
			const eidos_logical_t *logical0_data = x_value->LogicalVector()->data();
			const eidos_logical_t *logical1_data = y_value->LogicalVector()->data();
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(logical_result);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				logical_result->set_logical_no_check(logical0_data[value_index] && logical1_data[value_index], value_index); // && is logical min
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t * __restrict__ int0_data = x_value->IntVector()->data();
			const int64_t * __restrict__ int1_data = y_value->IntVector()->data();
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			int64_t * __restrict__ int_result_data = int_result->data();
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
			const double * __restrict__ float0_data = x_value->FloatVector()->data();
			const double * __restrict__ float1_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			double * __restrict__ float_result_data = float_result->data();
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
			const std::vector<std::string> &string0_vec = *x_value->StringVector();
			const std::vector<std::string> &string1_vec = *y_value->StringVector();
			EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
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
			double prob = probs_value->FloatAtIndex(probs_index, nullptr);
			
			if ((prob < 0.0) || (prob > 1.0))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): function quantile() requires probabilities to be in [0, 1]." << EidosTerminate(nullptr);
			
			probs.emplace_back(prob);
		}
	}
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(probs_count);
	result_SP = EidosValue_SP(float_result);
	
	if (x_count == 1)
	{
		// All quantiles of a singleton are the value of the singleton; the probabilities don't matter as long as they're in range (checked above)
		double x_singleton = x_value->FloatAtIndex(0, nullptr);
		
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
			const double *float_data = x_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				if (std::isnan(float_data[value_index]))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_quantile): quantiles of NAN are undefined." << EidosTerminate(nullptr);
		}
		
		// Next we get an order vector for x, which can be integer or float
		std::vector<int64_t> order;
		
		if (x_type == EidosValueType::kValueInt)
			order = EidosSortIndexes(x_value->IntVector()->data(), x_count, true);
		else if (x_type == EidosValueType::kValueFloat)
			order = EidosSortIndexes(x_value->FloatVector()->data(), x_count, true);
		
		// Now loop over the requested probabilities and calculate them
		for (int probs_index = 0; probs_index < probs_count; ++probs_index)
		{
			double prob = probs[probs_index];
			double index = (x_count - 1) * prob;
			int64_t lo = (int64_t)std::floor(index);
			int64_t hi = (int64_t)std::ceil(index);
			
			double quantile = x_value->FloatAtIndex((int)order[lo], nullptr);
			if (lo != hi) {
				double h = index - lo;
				quantile *= (1.0 - h);
				quantile += h * x_value->FloatAtIndex((int)order[hi], nullptr);
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
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(2);
		result_SP = EidosValue_SP(int_result);
		
		int64_t max = p_arguments[first_nonempty_argument]->IntAtIndex(0, nullptr);
		int64_t min = max;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				int64_t temp = arg_value->IntAtIndex(0, nullptr);
				if (max < temp)
					max = temp;
				else if (min > temp)
					min = temp;
			}
			else
			{
				const int64_t *int_data = arg_value->IntVector()->data();
				
				for (int value_index = 0; value_index < arg_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (max < temp)
						max = temp;
					else if (min > temp)
						min = temp;
				}
			}
		}
		
		int_result->set_int_no_check(min, 0);
		int_result->set_int_no_check(max, 1);
	}
	else if (x_type == EidosValueType::kValueFloat)
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(2);
		result_SP = EidosValue_SP(float_result);
		
		double max = p_arguments[first_nonempty_argument]->FloatAtIndex(0, nullptr);
		double min = max;
		
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg_value = p_arguments[arg_index].get();
			int arg_count = arg_value->Count();
			
			if (arg_count == 1)
			{
				double temp = arg_value->FloatAtIndex(0, nullptr);
				
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
			else
			{
				const double *float_data = arg_value->FloatVector()->data();
				
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
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count > 1)
	{
		double mean = 0;
		double sd = 0;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			mean += x_value->FloatAtIndex(value_index, nullptr);
		
		mean /= x_count;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			double temp = (x_value->FloatAtIndex(value_index, nullptr) - mean);
			sd += temp * temp;
		}
		
		sd = sqrt(sd / (x_count - 1));
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(sd));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
	}
	
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
	
	const double *vec1 = x_value->FloatVector()->data();
	double pvalue = 0.0;
	
	if (y_type != EidosValueType::kValueNULL)
	{
		// This is the x & y case, which is a two-sample Welch's t-test
		if (y_count <= 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ttest): function ttest() requires enough elements in y to compute variance." << EidosTerminate(nullptr);
		
		const double *vec2 = y_value->FloatVector()->data();
		
		// Right now this function only provides a two-sample t-test; we could add an optional mu argument and make y optional in order to allow a one-sample test as well
		// If we got into that, we'd probably want to provide one-sided t-tests as well, yada yada...
		pvalue = Eidos_TTest_TwoSampleWelch(vec1, x_count, vec2, y_count, nullptr, nullptr);
	}
	else if (mu_type != EidosValueType::kValueNULL)
	{
		// This is the x & mu case, which is a one-sample t-test
		double mu = mu_value->FloatAtIndex(0, nullptr);
		
		pvalue = Eidos_TTest_OneSample(vec1, x_count, mu, nullptr);
	}
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(pvalue));
	
	return result_SP;
}

//	(float$)var(numeric x)
EidosValue_SP Eidos_ExecuteFunction_var(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_value->IsArray())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_var): function var() does not currently support a matrix/array argument." << EidosTerminate(nullptr);
	
	if (x_count > 1)
	{
		// calculate mean
		double mean = 0;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			mean += x_value->FloatAtIndex(value_index, nullptr);
		
		mean /= x_count;
		
		// calculate variance
		double var = 0;
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			double temp = (x_value->FloatAtIndex(value_index, nullptr) - mean);
			var += temp * temp;
		}
		
		var = var / (x_count - 1);
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(var));
	}
	else
	{
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}






































