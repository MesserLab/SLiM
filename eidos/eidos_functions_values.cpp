//
//  eidos_functions_values.cpp
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
#include "eidos_interpreter.h"
#include "eidos_rng.h"
#include "eidos_sorting.h"

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <utility>

#include "eidos_globals.h"
#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
#endif


// From stackoverflow: http://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf/
// I chose to use iFreilicht's answer, which requires C++11.  BCH 26 April 2016
#include <memory>
#include <iostream>
#include <string>
#include <cstdio>
#include <cinttypes>

template<typename ... Args>
std::string EidosStringFormat(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1;	// Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]); 
	snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1);				// We don't want the '\0' inside
}


// ************************************************************************************
//
//	vector construction functions
//
#pragma mark -
#pragma mark Vector conversion functions
#pragma mark -


//	(*)c(...)
EidosValue_SP Eidos_ExecuteFunction_c(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	if (p_arguments.size() == 0)
		result_SP = gStaticEidosValueNULL;	// c() returns NULL, by definition
	else
		result_SP = ConcatenateEidosValues(p_arguments, true, false);	// allow NULL but not VOID
	
	return result_SP;
}

//	(float)float(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_float(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_float): function float() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(element_count);
	result_SP = EidosValue_SP(float_result);
	
	for (int64_t value_index = 0; value_index < element_count; ++value_index)
		float_result->set_float_no_check(0.0, value_index);
	
	return result_SP;
}

//	(integer)integer(integer$ length, [integer$ fill1 = 0], [integer$ fill2 = 1], [Ni fill2Indices = NULL])
EidosValue_SP Eidos_ExecuteFunction_integer(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	EidosValue *fill1_value = p_arguments[1].get();
	EidosValue *fill2_value = p_arguments[2].get();
	EidosValue *fill2Indices_value = p_arguments[3].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	int64_t fill1 = fill1_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integer): function integer() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(element_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int64_t value_index = 0; value_index < element_count; ++value_index)
		int_result->set_int_no_check(fill1, value_index);
	
	if (fill2Indices_value->Type() == EidosValueType::kValueInt)
	{
		int64_t fill2 = fill2_value->IntAtIndex(0, nullptr);
		int64_t *result_data = int_result->data();
		int positions_count = fill2Indices_value->Count();
		
		if (positions_count == 1)
		{
			int64_t position = fill2Indices_value->IntAtIndex(0, nullptr);
			
			if ((position < 0) || (position >= element_count))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integer): function integer() requires positions in fill2Indices to be between 0 and length - 1 (" << position << " supplied)." << EidosTerminate(nullptr);
			
			result_data[position] = fill2;
		}
		else
		{
			const int64_t *positions_data = fill2Indices_value->IntVector()->data();
			
			for (int positions_index = 0; positions_index < positions_count; ++positions_index)
			{
				int64_t position = positions_data[positions_index];
				
				if ((position < 0) || (position >= element_count))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_integer): function integer() requires positions in fill2Indices to be between 0 and length - 1 (" << position << " supplied)." << EidosTerminate(nullptr);
				
				result_data[position] = fill2;
			}
		}
	}
	
	return result_SP;
}

//	(logical)logical(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_logical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_logical): function logical() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_Logical_ZeroVec;
	
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(element_count);
	result_SP = EidosValue_SP(logical_result);
	
	for (int64_t value_index = 0; value_index < element_count; ++value_index)
		logical_result->set_logical_no_check(false, value_index);
	
	return result_SP;
}

//	(object<Object>)object(void)
EidosValue_SP Eidos_ExecuteFunction_object(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	result_SP = gStaticEidosValue_Object_ZeroVec;
	
	return result_SP;
}

//	(*)rep(* x, integer$ count)
EidosValue_SP Eidos_ExecuteFunction_rep(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *count_value = p_arguments[1].get();
	
	int64_t rep_count = count_value->IntAtIndex(0, nullptr);
	
	if (rep_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rep): function rep() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << EidosTerminate(nullptr);
	
	// the return type depends on the type of the first argument, which will get replicated
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
		for (int value_idx = 0; value_idx < x_count; value_idx++)
			result->PushValueFromIndexOfEidosValue(value_idx, *x_value, nullptr);
	
	return result_SP;
}

//	(*)repEach(* x, integer count)
EidosValue_SP Eidos_ExecuteFunction_repEach(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *count_value = p_arguments[1].get();
	int count_count = count_value->Count();
	
	// the return type depends on the type of the first argument, which will get replicated
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	if (count_count == 1)
	{
		int64_t rep_count = count_value->IntAtIndex(0, nullptr);
		
		if (rep_count < 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires count to be greater than or equal to 0 (" << rep_count << " supplied)." << EidosTerminate(nullptr);
		
		for (int value_idx = 0; value_idx < x_count; value_idx++)
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfEidosValue(value_idx, *x_value, nullptr);
	}
	else if (count_count == x_count)
	{
		for (int value_idx = 0; value_idx < x_count; value_idx++)
		{
			int64_t rep_count = count_value->IntAtIndex(value_idx, nullptr);
			
			if (rep_count < 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires all elements of count to be greater than or equal to 0 (" << rep_count << " supplied)." << EidosTerminate(nullptr);
			
			for (int rep_idx = 0; rep_idx < rep_count; rep_idx++)
				result->PushValueFromIndexOfEidosValue(value_idx, *x_value, nullptr);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_repEach): function repEach() requires that parameter count's size() either (1) be equal to 1, or (2) be equal to the size() of its first argument." << EidosTerminate(nullptr);
	}
	
	return result_SP;
}

//	(*)sample(* x, integer$ size, [logical$ replace = F], [Nif weights = NULL])
EidosValue_SP Eidos_ExecuteFunction_sample(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int64_t sample_size = p_arguments[1]->IntAtIndex(0, nullptr);
	bool replace = p_arguments[2]->LogicalAtIndex(0, nullptr);
	EidosValue *weights_value = p_arguments[3].get();
	int x_count = x_value->Count();
	
	if (sample_size < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires a sample size >= 0 (" << sample_size << " supplied)." << EidosTerminate(nullptr);
	if (sample_size == 0)
	{
		result_SP = x_value->NewMatchingType();
		return result_SP;
	}
	
	if (x_count == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() provided with insufficient elements (0 supplied)." << EidosTerminate(nullptr);
	
	if (!replace && (x_count < sample_size))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() provided with insufficient elements (" << x_count << " supplied, " << sample_size << " needed)." << EidosTerminate(nullptr);
	
	// decide whether to use weights, if weights were supplied
	EidosValueType weights_type = weights_value->Type();
	int weights_count = weights_value->Count();
	
	if (weights_type == EidosValueType::kValueNULL)
	{
		weights_value = nullptr;
	}
	else
	{
		if (weights_count != x_count)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires x and weights to be the same length." << EidosTerminate(nullptr);
		
		if (weights_count == 1)
		{
			double weight = weights_value->FloatAtIndex(0, nullptr);
			
			if ((weight < 0.0) || std::isnan(weight))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << EidosStringForFloat(weight) << " supplied)." << EidosTerminate(nullptr);
			if (weight == 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
			
			// one weight, greater than zero; no need to use it, and this guarantees below that weights_value is non-singleton
			weights_value = nullptr;
		}
	}
	
	// if replace==F but we're only sampling one item, we might as well set replace=T, which chooses a simpler case below
	// at present this doesn't matter since sample_size == 1 is handled separately anyway, but it is a good inference to draw
	if (!replace && (sample_size == 1))
		replace = true;
	
	// full shuffle; optimized case for everything but std::string, which is difficult as usual
	// and is handled below, because gsl_ran_shuffle() can't move std::string safely
	if (!weights_value && !replace && (sample_size == x_count) && (sample_size != 1) && (x_type != EidosValueType::kValueString))
	{
		gsl_rng *main_thread_rng = EIDOS_GSL_RNG(omp_get_thread_num());
		
		result_SP = x_value->CopyValues();
		EidosValue *result = result_SP.get();
		
		switch (x_type)
		{
			case EidosValueType::kValueVOID: break;			// NOLINT(*-branch-clone) : intentional consecutive branches
			case EidosValueType::kValueNULL: break;
			case EidosValueType::kValueLogical:
				Eidos_ran_shuffle(main_thread_rng, result->LogicalVector_Mutable()->data(), x_count);
				break;
			case EidosValueType::kValueInt:
				Eidos_ran_shuffle(main_thread_rng, result->IntVector_Mutable()->data(), x_count);
				break;
			case EidosValueType::kValueFloat:
				Eidos_ran_shuffle(main_thread_rng, result->FloatVector_Mutable()->data(), x_count);
				break;
			case EidosValueType::kValueObject:
				Eidos_ran_shuffle(main_thread_rng, result->ObjectElementVector_Mutable()->data(), x_count);
				break;
			default:
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): (internal error) unsupported type in sample()" << EidosTerminate(nullptr);
		}
		
		return result_SP;
	}
	
	// several algorithms below use a buffer of indexes; we share that here as static locals
	// whenever sampling without replacement, we resize the buffer to the needed capacity here, too,
	// and initialize the buffer; all the code paths below use it in essentially the same way
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_ExecuteFunction_sample(): usage of statics");
	
	static int *index_buffer = nullptr;
	static int buffer_capacity = 0;
	bool needs_index_buffer = !replace;		// if we are sampling without replacement, we will need this buffer
	
	if (needs_index_buffer)
	{
		if (x_count > buffer_capacity)
		{
			buffer_capacity = x_count * 2;		// double whenever we go over capacity, to avoid reallocations
			if (index_buffer)
				free(index_buffer);
			index_buffer = (int *)malloc(buffer_capacity * sizeof(int));	// no need to realloc, we don't need the old data
			if (!index_buffer)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		}
		
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_SAMPLE_INDEX);
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_INDEX);
#pragma omp parallel for schedule(static) default(none) shared(index_buffer, x_count) if(x_count > EIDOS_OMPMIN_SAMPLE_INDEX) num_threads(thread_count)
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			index_buffer[value_index] = value_index;
		}
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_SAMPLE_INDEX);
	}
	
	// the algorithm used depends on whether weights were supplied
	if (weights_value)
	{
		gsl_rng *main_thread_rng = EIDOS_GSL_RNG(omp_get_thread_num());
		
		if (replace && ((x_count > 100) || (sample_size > 100)) && (sample_size > 1))
		{
			// a large sampling task with replacement and weights goes through an optimized code path here
			// so that we can optimize the code more deeply for the type of x_value, and parallelize
			
			// first we check and prepare the weights vector as doubles, so the GSL can work with it
			const double *weights_float = nullptr;
			double *weights_float_malloced = nullptr;
			double weights_sum = 0.0;
			
			if (weights_type == EidosValueType::kValueFloat)
			{
				weights_float = weights_value->FloatVector()->data();
				
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					double weight = weights_float[value_index];
					
					if ((weight < 0.0) || std::isnan(weight))
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << EidosStringForFloat(weight) << " supplied)." << EidosTerminate(nullptr);
					
					weights_sum += weight;
				}
			}
			else	// EidosValueType::kValueInt : convert the weights to doubles
			{
				const int64_t *weights_int = weights_value->IntVector()->data();
				
				weights_float_malloced = (double *)malloc(x_count * sizeof(double));
				
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					int64_t weight = weights_int[value_index];
					
					if (weight < 0)
					{
						free(weights_float_malloced);
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << weight << " supplied)." << EidosTerminate(nullptr);
					}
					
					weights_float_malloced[value_index] = weight;
					weights_sum += weight;
				}
				
				// weights_float_malloced will be freed below
				weights_float = weights_float_malloced;
			}
			
			if (weights_sum <= 0.0)
			{
				free(weights_float_malloced);
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
			}
			
			// prepare the GSL to draw from the discrete distribution
			gsl_ran_discrete_t *discrete_draw = gsl_ran_discrete_preproc(x_count, weights_float);
			
			// now treat each type separately
			if (x_type == EidosValueType::kValueInt)
			{
				const int64_t *int_data = x_value->IntVector()->data();
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(sample_size);
				int64_t *int_result_data = int_result->data();
				result_SP = EidosValue_SP(int_result);
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_WR_INT);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size) firstprivate(discrete_draw, int_data, int_result_data) if(sample_size >= EIDOS_OMPMIN_SAMPLE_WR_INT) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(static) nowait
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						int rose_index = (int)gsl_ran_discrete(rng, discrete_draw);
						
						int_result_data[samples_generated] = int_data[rose_index];
					}
				}
			}
			else if (x_type == EidosValueType::kValueFloat)
			{
				const double *float_data = x_value->FloatVector()->data();
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(sample_size);
				double *float_result_data = float_result->data();
				result_SP = EidosValue_SP(float_result);
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_WR_FLOAT);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size) firstprivate(discrete_draw, float_data, float_result_data) if(sample_size >= EIDOS_OMPMIN_SAMPLE_WR_FLOAT) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(static) nowait
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						int rose_index = (int)gsl_ran_discrete(rng, discrete_draw);
						
						float_result_data[samples_generated] = float_data[rose_index];
					}
				}
			}
			else if (x_type == EidosValueType::kValueObject)
			{
				EidosObject * const *object_data = x_value->ObjectElementVector()->data();
				const EidosClass *object_class = ((EidosValue_Object *)x_value)->Class();
				EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(object_class))->resize_no_initialize(sample_size);
				EidosObject **object_result_data = object_result->data();
				result_SP = EidosValue_SP(object_result);
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_WR_OBJECT);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size) firstprivate(discrete_draw, object_data, object_result_data) if(sample_size >= EIDOS_OMPMIN_SAMPLE_WR_OBJECT) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(static) nowait
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						int rose_index = (int)gsl_ran_discrete(rng, discrete_draw);
						EidosObject *object_element = object_data[rose_index];
						
						object_result_data[samples_generated] = object_element;
					}
				}
				
				if (object_class->UsesRetainRelease())
				{
					// Retain all of the objects chosen; this is not done in parallel because it would require locks
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						EidosObject *object_element = object_result_data[samples_generated];
						static_cast<EidosDictionaryRetained *>(object_element)->Retain();		// unsafe cast to avoid virtual function overhead
					}
				}
			}
			else
			{
				// This handles the logical and string cases
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					int rose_index = (int)gsl_ran_discrete(main_thread_rng, discrete_draw);
					
					result->PushValueFromIndexOfEidosValue(rose_index, *x_value, nullptr);
				}
			}
			
			gsl_ran_discrete_free(discrete_draw);
			
			if (weights_float_malloced)
				free(weights_float_malloced);
		}
		// handle the weights vector with separate cases for float and integer, so we can access it directly for speed
		else if (weights_type == EidosValueType::kValueFloat)
		{
			const double *weights_float = weights_value->FloatVector()->data();
			double weights_sum = 0.0;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				double weight = weights_float[value_index];
				
				if ((weight < 0.0) || std::isnan(weight))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << EidosStringForFloat(weight) << " supplied)." << EidosTerminate(nullptr);
				
				weights_sum += weight;
			}
			
			if (weights_sum <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
			
			if (sample_size == 1)
			{
				// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
				double rose = Eidos_rng_uniform(main_thread_rng) * weights_sum;
				double rose_sum = 0.0;
				int rose_index;
				
				for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
				{
					rose_sum += weights_float[rose_index];
					
					if (rose <= rose_sum)
						break;
				}
				
				return x_value->GetValueAtIndex(rose_index, nullptr);
			}
			else if (replace)
			{
				// with replacement, we can just do a series of independent draws
				// (note the large-task case is handled with the GSL above)
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					double rose = Eidos_rng_uniform(main_thread_rng) * weights_sum;
					double rose_sum = 0.0;
					int rose_index;
					
					for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_float[rose_index];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(rose_index, *x_value, nullptr);
				}
			}
			else
			{
				// without replacement, we remove each item after it is drawn, so brute force seems like the only way
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				// do the sampling
				int64_t contender_count = x_count;
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					if (weights_sum <= 0.0)
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
					
					double rose = Eidos_rng_uniform(main_thread_rng) * weights_sum;
					double rose_sum = 0.0;
					int rose_index;
					
					for (rose_index = 0; rose_index < contender_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_float[index_buffer[rose_index]];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(index_buffer[rose_index], *x_value, nullptr);
					
					// remove the sampled index since replace==F; note this algorithm is terrible if we are sampling
					// a large number of elements without replacement, with weights, but that seems unlikely to me...
					weights_sum -= weights_float[index_buffer[rose_index]];	// possible source of numerical error
					memmove(index_buffer + rose_index, index_buffer + rose_index + 1, (contender_count - rose_index - 1) * sizeof(int));
					--contender_count;
				}
			}
		}
		else if (weights_type == EidosValueType::kValueInt)
		{
			const int64_t *weights_int = weights_value->IntVector()->data();
			int64_t weights_sum = 0;
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				int64_t weight = weights_int[value_index];
				
				if (weight < 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() requires all weights to be non-negative (" << weight << " supplied)." << EidosTerminate(nullptr);
				
				weights_sum += weight;
				
				if (weights_sum < 0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): overflow of integer sum of weights in function sample(); the weights used are too large." << EidosTerminate(nullptr);
			}
			
			if (weights_sum <= 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
			
			if (sample_size == 1)
			{
				// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
				int64_t rose = (int64_t)ceil(Eidos_rng_uniform(main_thread_rng) * weights_sum);
				int64_t rose_sum = 0;
				int rose_index;
				
				for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
				{
					rose_sum += weights_int[rose_index];
					
					if (rose <= rose_sum)
						break;
				}
				
				return x_value->GetValueAtIndex(rose_index, nullptr);
			}
			else if (replace)
			{
				// with replacement, we can just do a series of independent draws
				// (note the large-task case is handled with the GSL above)
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					int64_t rose = (int64_t)ceil(Eidos_rng_uniform(main_thread_rng) * weights_sum);
					int64_t rose_sum = 0;
					int rose_index;
					
					for (rose_index = 0; rose_index < x_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_int[rose_index];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(rose_index, *x_value, nullptr);
				}
			}
			else
			{
				// without replacement, we remove each item after it is drawn, so brute force seems like the only way
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				// do the sampling
				int64_t contender_count = x_count;
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					if (weights_sum <= 0)
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): function sample() encountered weights summing to <= 0." << EidosTerminate(nullptr);
					
					int64_t rose = (int64_t)ceil(Eidos_rng_uniform(main_thread_rng) * weights_sum);
					int64_t rose_sum = 0;
					int rose_index;
					
					for (rose_index = 0; rose_index < contender_count - 1; ++rose_index)	// -1 so roundoff gives the result to the last contender
					{
						rose_sum += weights_int[index_buffer[rose_index]];
						
						if (rose <= rose_sum)
							break;
					}
					
					result->PushValueFromIndexOfEidosValue(index_buffer[rose_index], *x_value, nullptr);
					
					// remove the sampled index since replace==F; note this algorithm is terrible if we are sampling
					// a large number of elements without replacement, with weights, but that seems unlikely to me...
					weights_sum -= weights_int[index_buffer[rose_index]];
					memmove(index_buffer + rose_index, index_buffer + rose_index + 1, (contender_count - rose_index - 1) * sizeof(int));
					--contender_count;
				}
			}
		}
		else
		{
			// CODE COVERAGE: This is dead code
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sample): (internal error) weights vector must be type float or integer." << EidosTerminate(nullptr);
		}
	}
	else
	{
		// weights not supplied; use equal weights
		if (sample_size == 1)
		{
			// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
			gsl_rng *main_thread_rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
			return x_value->GetValueAtIndex((int)Eidos_rng_uniform_int(main_thread_rng, x_count), nullptr);
		}
		else if (replace)
		{
			// with replacement, we can just do a series of independent draws
			if (x_count == 1)
			{
				// If there is only one element to sample from, there is no need to draw elements
				// This case removes the possibility of x_value being singleton from the branches below
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					result->PushValueFromIndexOfEidosValue(0, *x_value, nullptr);
			}
			else if (x_type == EidosValueType::kValueInt)
			{
				const int64_t *int_data = x_value->IntVector()->data();
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(sample_size);
				int64_t *int_result_data = int_result->data();
				result_SP = EidosValue_SP(int_result);
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_R_INT);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size) firstprivate(int_data, int_result_data, x_count) if(sample_size >= EIDOS_OMPMIN_SAMPLE_R_INT) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(static) nowait
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						int32_t sample = Eidos_rng_uniform_int(rng, x_count);
						int_result_data[samples_generated] = int_data[sample];
					}
				}
			}
			else if (x_type == EidosValueType::kValueFloat)
			{
				const double *float_data = x_value->FloatVector()->data();
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(sample_size);
				double *float_result_data = float_result->data();
				result_SP = EidosValue_SP(float_result);
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_R_FLOAT);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size) firstprivate(float_data, float_result_data, x_count) if(sample_size >= EIDOS_OMPMIN_SAMPLE_R_FLOAT) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(static) nowait
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						int32_t sample = Eidos_rng_uniform_int(rng, x_count);
						float_result_data[samples_generated] = float_data[sample];
					}
				}
			}
			else if (x_type == EidosValueType::kValueObject)
			{
				EidosObject * const *object_data = x_value->ObjectElementVector()->data();
				const EidosClass *object_class = ((EidosValue_Object *)x_value)->Class();
				EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(object_class))->resize_no_initialize(sample_size);
				EidosObject **object_result_data = object_result->data();
				result_SP = EidosValue_SP(object_result);
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_R_OBJECT);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size) firstprivate(object_data, object_result_data, x_count) if(sample_size >= EIDOS_OMPMIN_SAMPLE_R_OBJECT) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(static) nowait
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						int32_t sample = Eidos_rng_uniform_int(rng, x_count);
						EidosObject *object_element = object_data[sample];
						object_result_data[samples_generated] = object_element;
					}
				}
				
				if (object_class->UsesRetainRelease())
				{
					// Retain all of the objects chosen; this is not done in parallel because it would require locks
					for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					{
						EidosObject *object_element = object_result_data[samples_generated];
						static_cast<EidosDictionaryRetained *>(object_element)->Retain();		// unsafe cast to avoid virtual function overhead
					}
				}
			}
			else
			{
				// This handles the logical and string cases
				gsl_rng *main_thread_rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
				result_SP = x_value->NewMatchingType();
				EidosValue *result = result_SP.get();
				
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
					result->PushValueFromIndexOfEidosValue((int)Eidos_rng_uniform_int(main_thread_rng, x_count), *x_value, nullptr);
			}
		}
		else
		{
			// get indices of x; we sample from this vector and then look up the corresponding EidosValue element
			// this is generally faster than gsl_ran_choose(), which is O(n) in x_count with a large constant factor;
			// we are O(n+m) in x_count and sample_size, but our constant factor is much, much smaller, because
			// gsl_ran_choose() does a gsl_rng_uniform() call for every element in x_value()!  We only do one
			// Eidos_rng_uniform_int() call per element in sample_size, at the price of a separate index buffer
			// and a lack of re-entrancy and thread-safety.  This is a *lot* faster for sample_size << x_count.
			gsl_rng *main_thread_rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
			result_SP = x_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			// do the sampling; this is not parallelized because of contention over index_buffer removals
			int64_t contender_count = x_count;
			
			for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
			{
				int rose_index = (int)Eidos_rng_uniform_int(main_thread_rng, (uint32_t)contender_count);
				result->PushValueFromIndexOfEidosValue(index_buffer[rose_index], *x_value, nullptr);
				index_buffer[rose_index] = index_buffer[--contender_count];
			}
		}
	}
	
	return result_SP;
}

//	(numeric)seq(numeric$ from, numeric$ to, [Nif$ by = NULL], [Ni$ length = NULL])
EidosValue_SP Eidos_ExecuteFunction_seq(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *from_value = p_arguments[0].get();
	EidosValueType from_type = from_value->Type();
	EidosValue *to_value = p_arguments[1].get();
	EidosValueType to_type = to_value->Type();
	EidosValue *by_value = p_arguments[2].get();
	EidosValueType by_type = by_value->Type();
	EidosValue *length_value = p_arguments[3].get();
	EidosValueType length_type = length_value->Type();
	
	if ((from_type == EidosValueType::kValueFloat) && !std::isfinite(from_value->FloatAtIndex(0, nullptr)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires a finite value for the 'from' parameter." << EidosTerminate(nullptr);
	if ((to_type == EidosValueType::kValueFloat) && !std::isfinite(to_value->FloatAtIndex(0, nullptr)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires a finite value for the 'to' parameter." << EidosTerminate(nullptr);
	if ((by_type != EidosValueType::kValueNULL) && (length_type != EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() may be supplied with either 'by' or 'length', but not both." << EidosTerminate(nullptr);
	
	if (length_type != EidosValueType::kValueNULL)
	{
		// A length value has been supplied, so we guarantee a vector of that length even if from==to
		int64_t length = length_value->IntAtIndex(0, nullptr);
		
		if (length <= 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires that length, if supplied, must be > 0." << EidosTerminate(nullptr);
		if (length > 10000000)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() cannot construct a sequence with more than 10000000 entries." << EidosTerminate(nullptr);
		
		if ((from_type == EidosValueType::kValueFloat) || (to_type == EidosValueType::kValueFloat))
		{
			// a float value was given, so we will generate a float sequence in all cases
			double first_value = from_value->FloatAtIndex(0, nullptr);
			double second_value = to_value->FloatAtIndex(0, nullptr);
			
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(length);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t seq_index = 0; seq_index < length; ++seq_index)
			{
				if (seq_index == 0)
					float_result->set_float_no_check(first_value, seq_index);
				else if (seq_index == length - 1)
					float_result->set_float_no_check(second_value, seq_index);
				else
					float_result->set_float_no_check(first_value + (second_value - first_value) * (seq_index / (double)(length - 1)), seq_index);
			}
		}
		else
		{
			// int values were given, so whether we generate a float sequence or an int sequence depends on whether length divides evenly
			int64_t first_value = from_value->IntAtIndex(0, nullptr);
			int64_t second_value = to_value->IntAtIndex(0, nullptr);
			
			if (length == 1)
			{
				// If a sequence of length 1 is requested, generate a single integer at the start
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_value));
			}
			else if ((second_value - first_value) % (length - 1) == 0)
			{
				// length divides evenly, so generate an integer sequence
				int64_t by = (second_value - first_value) / (length - 1);
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(length);
				result_SP = EidosValue_SP(int_result);
				
				for (int64_t seq_index = 0; seq_index < length; ++seq_index)
					int_result->set_int_no_check(first_value + by * seq_index, seq_index);
			}
			else
			{
				// length does not divide evenly, so generate a float sequence
				double by = (second_value - first_value) / (double)(length - 1);
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(length);
				result_SP = EidosValue_SP(float_result);
				
				for (int64_t seq_index = 0; seq_index < length; ++seq_index)
				{
					if (seq_index == 0)
						float_result->set_float_no_check(first_value, seq_index);
					else if (seq_index == length - 1)
						float_result->set_float_no_check(second_value, seq_index);
					else
						float_result->set_float_no_check(first_value + by * seq_index, seq_index);
				}
			}
		}
	}
	else
	{
		// Either a by value has been supplied, or we're using our default step
		if ((from_type == EidosValueType::kValueFloat) || (to_type == EidosValueType::kValueFloat) || (by_type == EidosValueType::kValueFloat))
		{
			// float return case
			double first_value = from_value->FloatAtIndex(0, nullptr);
			double second_value = to_value->FloatAtIndex(0, nullptr);
			double default_by = ((first_value < second_value) ? 1 : -1);
			double by = ((by_type != EidosValueType::kValueNULL) ? by_value->FloatAtIndex(0, nullptr) : default_by);
			
			if (by == 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires by != 0." << EidosTerminate(nullptr);
			if (!std::isfinite(by))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires a finite value for the 'by' parameter." << EidosTerminate(nullptr);
			if (((first_value < second_value) && (by < 0)) || ((first_value > second_value) && (by > 0)))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() by has incorrect sign." << EidosTerminate(nullptr);
			
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(int(1 + ceil((second_value - first_value) / by)));	// take a stab at a reserve size; might not be quite right, but no harm
			result_SP = EidosValue_SP(float_result);
			
			if (by > 0)
				for (double seq_value = first_value; seq_value <= second_value; seq_value += by)
					float_result->push_float(seq_value);
			else
				for (double seq_value = first_value; seq_value >= second_value; seq_value += by)
					float_result->push_float(seq_value);
		}
		else
		{
			// int return case
			int64_t first_value = from_value->IntAtIndex(0, nullptr);
			int64_t second_value = to_value->IntAtIndex(0, nullptr);
			int64_t default_by = ((first_value < second_value) ? 1 : -1);
			int64_t by = ((by_type != EidosValueType::kValueNULL) ? by_value->IntAtIndex(0, nullptr) : default_by);
			
			if (by == 0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() requires by != 0." << EidosTerminate(nullptr);
			if (((first_value < second_value) && (by < 0)) || ((first_value > second_value) && (by > 0)))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seq): function seq() by has incorrect sign." << EidosTerminate(nullptr);
			
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve((int)(1 + (second_value - first_value) / by));		// take a stab at a reserve size; might not be quite right, but no harm
			result_SP = EidosValue_SP(int_result);
			
			if (by > 0)
				for (int64_t seq_value = first_value; seq_value <= second_value; seq_value += by)
					int_result->push_int(seq_value);
			else
				for (int64_t seq_value = first_value; seq_value >= second_value; seq_value += by)
					int_result->push_int(seq_value);
		}
	}
	
	return result_SP;
}

//	(integer)seqAlong(* x)
EidosValue_SP Eidos_ExecuteFunction_seqAlong(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	// That might seem like an odd policy, since the sequence doesn't match the reality of the value,
	// but it follows R's behavior, and it gives one sequence-element per value-element.
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	int x_count = x_value->Count();
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < x_count; ++value_index)
		int_result->set_int_no_check(value_index, value_index);
	
	return result_SP;
}

//	(integer)seqLen(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_seqLen(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t length = length_value->IntAtIndex(0, nullptr);
	
	if (length < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_seqLen): function seqLen() requires length to be greater than or equal to 0 (" << length << " supplied)." << EidosTerminate(nullptr);
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(length);
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < length; ++value_index)
		int_result->set_int_no_check(value_index, value_index);
	
	return result_SP;
}

//	(string)string(integer$ length)
EidosValue_SP Eidos_ExecuteFunction_string(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	int64_t element_count = length_value->IntAtIndex(0, nullptr);
	
	if (element_count < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_string): function string() requires length to be greater than or equal to 0 (" << element_count << " supplied)." << EidosTerminate(nullptr);
	
	if (element_count == 0)
		return gStaticEidosValue_String_ZeroVec;
	
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)element_count);
	result_SP = EidosValue_SP(string_result);
	
	for (int64_t value_index = element_count; value_index > 0; --value_index)
		string_result->PushString(gEidosStr_empty_string);
	
	return result_SP;
}


// ************************************************************************************
//
//	value inspection/manipulation functions
//
#pragma mark -
#pragma mark Value inspection/manipulation functions
#pragma mark -


//	(logical$)all(logical x, ...)
EidosValue_SP Eidos_ExecuteFunction_all(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	result_SP = gStaticEidosValue_LogicalT;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		
		if (arg_value->Type() != EidosValueType::kValueLogical)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_all): function all() requires that all arguments be of type logical." << EidosTerminate(nullptr);
		
		int arg_count = arg_value->Count();
		const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
		
		for (int value_index = 0; value_index < arg_count; ++value_index)
			if (!logical_data[value_index])
			{
				result_SP = gStaticEidosValue_LogicalF;
				break;
			}
	}
	
	return result_SP;
}

//	(logical$)any(logical x, ...)
EidosValue_SP Eidos_ExecuteFunction_any(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	result_SP = gStaticEidosValue_LogicalF;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg_value = p_arguments[arg_index].get();
		
		if (arg_value->Type() != EidosValueType::kValueLogical)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_any): function any() requires that all arguments be of type logical." << EidosTerminate(nullptr);
		
		int arg_count = arg_value->Count();
		const eidos_logical_t *logical_data = arg_value->LogicalVector()->data();
		
		for (int value_index = 0; value_index < arg_count; ++value_index)
			if (logical_data[value_index])
			{
				result_SP = gStaticEidosValue_LogicalT;
				break;
			}
	}
	
	return result_SP;
}

//	(void)cat(* x, [string$ sep = " "], [logical$ error = F])
EidosValue_SP Eidos_ExecuteFunction_cat(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH catn() BELOW!
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValueType x_type = x_value->Type();
	std::string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	eidos_logical_t use_error_stream = p_arguments[2]->LogicalAtIndex(0, nullptr);
	std::ostream &output_stream = (use_error_stream ? p_interpreter.ErrorOutputStream() : p_interpreter.ExecutionOutputStream());
	
	for (int value_index = 0; value_index < x_count; ++value_index)
	{
		if (value_index > 0)
			output_stream << separator;
		
		if (x_type == EidosValueType::kValueObject)
			output_stream << *x_value->ObjectElementAtIndex(value_index, nullptr);
		else
			output_stream << x_value->StringAtIndex(value_index, nullptr);
	}
	
	return gStaticEidosValueVOID;
}

//	(void)catn([* x = ""], [string$ sep = " "], [logical$ error = F])
EidosValue_SP Eidos_ExecuteFunction_catn(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH cat() ABOVE!
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValueType x_type = x_value->Type();
	std::string separator = p_arguments[1]->StringAtIndex(0, nullptr);
	eidos_logical_t use_error_stream = p_arguments[2]->LogicalAtIndex(0, nullptr);
	std::ostream &output_stream = (use_error_stream ? p_interpreter.ErrorOutputStream() : p_interpreter.ExecutionOutputStream());
	
	for (int value_index = 0; value_index < x_count; ++value_index)
	{
		if (value_index > 0)
			output_stream << separator;
		
		if (x_type == EidosValueType::kValueObject)
			output_stream << *x_value->ObjectElementAtIndex(value_index, nullptr);
		else
			output_stream << x_value->StringAtIndex(value_index, nullptr);
	}
	
	output_stream << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(string)format(string$ format, numeric x)
EidosValue_SP Eidos_ExecuteFunction_format(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *format_value = p_arguments[0].get();
	std::string format = format_value->StringAtIndex(0, nullptr);
	EidosValue *x_value = p_arguments[1].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	// Check the format string for correct syntax.  We have to be pretty careful about what we pass on to C++, both
	// for robustness and for security.  We allow the standard flags (+- #0), an integer field width (but not *), and
	// an integer precision (but not *).  For integer x we allow %d %i %o %x %X, for float x we allow %f %F %e %E %g %G;
	// other conversion specifiers are not allowed.  We do not allow a length modifier; we supply the correct length
	// modifier ourselves, which is platform-dependent.  We allow the format to be embedded within a longer string,
	// as usual, for convenience, but only one % specifier may exist within the format string.
	int length = (int)format.length();
	int pos = 0;
	int conversion_specifier_pos = -1;
	char conv_ch = ' ';
	bool flag_plus = false, flag_minus = false, flag_space = false, flag_pound = false, flag_zero = false;
	
	while (pos < length)
	{
		if (format[pos] == '%')
		{
			if ((pos + 1 < length) && (format[pos + 1] == '%'))
			{
				// skip over %% escapes
				pos += 2;
			}
			else if (conversion_specifier_pos != -1)
			{
				// we already saw a format specifier
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); only one % escape is allowed." << EidosTerminate(nullptr);
			}
			else
			{
				// other uses of % must be the format specifier, which we now parse
				
				// skip the %
				++pos;
				
				// skip over the optional +- #0 flags
				while (pos < length)
				{
					char flag = format[pos];
					
					if (flag == '+')
					{
						if (flag_plus)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '+' specified more than once." << EidosTerminate(nullptr);
						
						flag_plus = true;
						++pos;	// skip the '+'
					}
					else if (flag == '-')
					{
						if (flag_minus)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '-' specified more than once." << EidosTerminate(nullptr);
						
						flag_minus = true;
						++pos;	// skip the '-'
					}
					else if (flag == ' ')
					{
						if (flag_space)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag ' ' specified more than once." << EidosTerminate(nullptr);
						
						flag_space = true;
						++pos;	// skip the ' '
					}
					else if (flag == '#')
					{
						if (flag_pound)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '#' specified more than once." << EidosTerminate(nullptr);
						
						flag_pound = true;
						++pos;	// skip the '#'
					}
					else if (flag == '0')
					{
						if (flag_zero)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); flag '0' specified more than once." << EidosTerminate(nullptr);
						
						flag_zero = true;
						++pos;	// skip the '0'
					}
					else
					{
						// not a flag character, so we are done with our optional flags
						break;
					}
				}
				
				// skip over the optional field width; eat a [1-9] followed by any number of [0-9]
				if (pos < length)
				{
					char fieldwidth_ch = format[pos];
					
					if ((fieldwidth_ch >= '1') && (fieldwidth_ch <= '9'))
					{
						// skip the leading digit
						++pos;
						
						while (pos < length)
						{
							fieldwidth_ch = format[pos];
							
							if ((fieldwidth_ch >= '0') && (fieldwidth_ch <= '9'))
								++pos;	// skip the digit
							else
								break;
						}
					}
				}
				
				// skip the optional precision specifier, a '.' followed by an integer
				if ((pos < length) && (format[pos] == '.'))
				{
					// skip the leading '.'
					++pos;
					
					while (pos < length)
					{
						char precision_ch = format[pos];
						
						if ((precision_ch >= '0') && (precision_ch <= '9'))
							++pos;	// skip the digit
						else
							break;
					}
				}
				
				// now eat the required conversion specifier
				if (pos < length)
				{
					conv_ch = format[pos];
					
					conversion_specifier_pos = pos;
					++pos;
					
					if ((conv_ch == 'd') || (conv_ch == 'i') || (conv_ch == 'o') || (conv_ch == 'x') || (conv_ch == 'X'))
					{
						if (x_type != EidosValueType::kValueInt)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type integer." << EidosTerminate(nullptr);
					}
					else if ((conv_ch == 'f') || (conv_ch == 'F') || (conv_ch == 'e') || (conv_ch == 'E') || (conv_ch == 'g') || (conv_ch == 'G'))
					{
						if (x_type != EidosValueType::kValueFloat)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' requires an argument of type float." << EidosTerminate(nullptr);
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); conversion specifier '" << conv_ch << "' not supported." << EidosTerminate(nullptr);
					}
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); missing conversion specifier after '%'." << EidosTerminate(nullptr);
				}
			}
		}
		else
		{
			// Skip over all other characters
			++pos;
		}
	}
	
	// Fix the format string to have the correct length modifier.  This is an issue only for integer; for float, the
	// default is double anyway so we're fine.  For integer, the correct format strings are defined by <cinttypes>:
	// PRId64, PRIi64, PRIo64, PRIx64, and PRIX64.
	if (x_type == EidosValueType::kValueInt)
	{
		std::string new_conv_string;
		
		if (conv_ch == 'd')
			new_conv_string = PRId64;
		else if (conv_ch == 'i')
			new_conv_string = PRIi64;
		else if (conv_ch == 'o')
			new_conv_string = PRIo64;
		else if (conv_ch == 'x')
			new_conv_string = PRIx64;
		else if (conv_ch == 'X')
			new_conv_string = PRIX64;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): (internal error) bad format string in function format(); conversion specifier '" << conv_ch << "' not recognized." << EidosTerminate(nullptr);		// CODE COVERAGE: This is dead code
		
		format.replace(conversion_specifier_pos, 1, new_conv_string);
	}
	
	// Check for possibilities that produce undefined behavior according to the C++11 standard
	if (flag_pound && ((conv_ch == 'd') || (conv_ch == 'i')))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_format): bad format string in function format(); the flag '#' may not be used with the conversion specifier '" << conv_ch << "'." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		// singleton case
		std::string result_string;
		
		if (x_type == EidosValueType::kValueInt)
			result_string = EidosStringFormat(format, x_value->IntAtIndex(0, nullptr));
		else if (x_type == EidosValueType::kValueFloat)
			result_string = EidosStringFormat(format, x_value->FloatAtIndex(0, nullptr));
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
	}
	else
	{
		// non-singleton x vector, with a singleton format vector
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
		result_SP = EidosValue_SP(string_result);
		
		if (x_type == EidosValueType::kValueInt)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(EidosStringFormat(format, x_value->IntAtIndex(value_index, nullptr)));
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			for (int value_index = 0; value_index < x_count; ++value_index)
				string_result->PushString(EidosStringFormat(format, x_value->FloatAtIndex(value_index, nullptr)));
		}
	}
	
	return result_SP;
}

//	(logical$)identical(* x, * y)
EidosValue_SP Eidos_ExecuteFunction_identical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	
	return IdenticalEidosValues(x_value, y_value) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
}

//	(*)ifelse(logical test, * trueValues, * falseValues)
EidosValue_SP Eidos_ExecuteFunction_ifelse(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *test_value = p_arguments[0].get();
	int test_count = test_value->Count();
	const eidos_logical_t *logical_vec = (*test_value->LogicalVector()).data();
	
	EidosValue *trueValues_value = p_arguments[1].get();
	EidosValueType trueValues_type = trueValues_value->Type();
	int trueValues_count = trueValues_value->Count();
	
	EidosValue *falseValues_value = p_arguments[2].get();
	EidosValueType falseValues_type = falseValues_value->Type();
	int falseValues_count = falseValues_value->Count();
	
	if (trueValues_type != falseValues_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): function ifelse() requires arguments 2 and 3 to be the same type (" << trueValues_type << " and " << falseValues_type << " supplied)." << EidosTerminate(nullptr);
	
	if ((trueValues_count == test_count) && (falseValues_count == test_count))
	{
		// All three are equal counts, so we can do the whole thing in parallel
		if (test_count > 1)
		{
			// Use direct access to make this fast
			if (trueValues_type == EidosValueType::kValueLogical)
			{
				const eidos_logical_t *true_vec = trueValues_value->LogicalVector()->data();
				const eidos_logical_t *false_vec = falseValues_value->LogicalVector()->data();
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					logical_result->set_logical_no_check(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index], value_index);
				
				result_SP = logical_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueInt)
			{
				const int64_t *true_data = trueValues_value->IntVector()->data();
				const int64_t *false_data = falseValues_value->IntVector()->data();
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					int_result->set_int_no_check(logical_vec[value_index] ? true_data[value_index] : false_data[value_index], value_index);
				
				result_SP = int_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueFloat)
			{
				const double *true_data = trueValues_value->FloatVector()->data();
				const double *false_data = falseValues_value->FloatVector()->data();
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					float_result->set_float_no_check(logical_vec[value_index] ? true_data[value_index] : false_data[value_index], value_index);
				
				result_SP = float_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueString)
			{
				const std::vector<std::string> &true_vec = (*trueValues_value->StringVector());
				const std::vector<std::string> &false_vec = (*falseValues_value->StringVector());
				EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
				EidosValue_String_vector *string_result = string_result_SP->Reserve(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					string_result->PushString(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index]);
				
				result_SP = string_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueObject)
			{
				const EidosClass *trueValues_class = ((EidosValue_Object *)trueValues_value)->Class();
				const EidosClass *falseValues_class = ((EidosValue_Object *)falseValues_value)->Class();
				
				if (trueValues_class != falseValues_class)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): objects of different types cannot be mixed in function ifelse()." << EidosTerminate(nullptr);
				
				EidosObject * const *true_vec = trueValues_value->ObjectElementVector()->data();
				EidosObject * const *false_vec = falseValues_value->ObjectElementVector()->data();
				EidosValue_Object_vector_SP object_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(trueValues_class));
				EidosValue_Object_vector *object_result = object_result_SP->resize_no_initialize_RR(test_count);
				
				if (object_result->UsesRetainRelease())
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_no_previous_RR(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index], value_index);
				}
				else
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_NORR(logical_vec[value_index] ? true_vec[value_index] : false_vec[value_index], value_index);
				}
				
				result_SP = object_result_SP;
			}
		}
		
		if (!result_SP)
		{
			// General case
			result_SP = trueValues_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int value_index = 0; value_index < test_count; ++value_index)
			{
				if (logical_vec[value_index])
					result->PushValueFromIndexOfEidosValue(value_index, *trueValues_value, nullptr);
				else
					result->PushValueFromIndexOfEidosValue(value_index, *falseValues_value, nullptr);
			}
		}
	}
	else if ((trueValues_count == 1) && (falseValues_count == 1))
	{
		// trueValues and falseValues are both singletons, so we can prefetch both values
		if (test_count > 1)
		{
			// Use direct access to make this fast
			if (trueValues_type == EidosValueType::kValueLogical)
			{
				eidos_logical_t true_value = trueValues_value->LogicalAtIndex(0, nullptr);
				eidos_logical_t false_value = falseValues_value->LogicalAtIndex(0, nullptr);
				EidosValue_Logical_SP logical_result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
				EidosValue_Logical *logical_result = logical_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					logical_result->set_logical_no_check(logical_vec[value_index] ? true_value : false_value, value_index);
				
				result_SP = logical_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueInt)
			{
				int64_t true_value = trueValues_value->IntAtIndex(0, nullptr);
				int64_t false_value = falseValues_value->IntAtIndex(0, nullptr);
				EidosValue_Int_vector_SP int_result_SP = EidosValue_Int_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
				EidosValue_Int_vector *int_result = int_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					int_result->set_int_no_check(logical_vec[value_index] ? true_value : false_value, value_index);
				
				result_SP = int_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueFloat)
			{
				double true_value = trueValues_value->FloatAtIndex(0, nullptr);
				double false_value = falseValues_value->FloatAtIndex(0, nullptr);
				EidosValue_Float_vector_SP float_result_SP = EidosValue_Float_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
				EidosValue_Float_vector *float_result = float_result_SP->resize_no_initialize(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					float_result->set_float_no_check(logical_vec[value_index] ? true_value : false_value, value_index);
				
				result_SP = float_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueString)
			{
				const std::string &true_value = ((EidosValue_String *)trueValues_value)->StringRefAtIndex(0, nullptr);
				const std::string &false_value = ((EidosValue_String *)falseValues_value)->StringRefAtIndex(0, nullptr);
				EidosValue_String_vector_SP string_result_SP = EidosValue_String_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
				EidosValue_String_vector *string_result = string_result_SP->Reserve(test_count);
				
				for (int value_index = 0; value_index < test_count; ++value_index)
					string_result->PushString(logical_vec[value_index] ? true_value : false_value);
				
				result_SP = string_result_SP;
			}
			else if (trueValues_type == EidosValueType::kValueObject)
			{
				const EidosClass *trueValues_class = ((EidosValue_Object *)trueValues_value)->Class();
				const EidosClass *falseValues_class = ((EidosValue_Object *)falseValues_value)->Class();
				
				if (trueValues_class != falseValues_class)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): objects of different types cannot be mixed in function ifelse()." << EidosTerminate(nullptr);
				
				EidosObject *true_value = trueValues_value->ObjectElementAtIndex(0, nullptr);
				EidosObject *false_value = falseValues_value->ObjectElementAtIndex(0, nullptr);
				EidosValue_Object_vector_SP object_result_SP = EidosValue_Object_vector_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(trueValues_class));
				EidosValue_Object_vector *object_result = object_result_SP->resize_no_initialize_RR(test_count);
				
				if (object_result->UsesRetainRelease())
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_no_previous_RR(logical_vec[value_index] ? true_value : false_value, value_index);
				}
				else
				{
					for (int value_index = 0; value_index < test_count; ++value_index)
						object_result->set_object_element_no_check_NORR(logical_vec[value_index] ? true_value : false_value, value_index);
				}
				
				result_SP = object_result_SP;
			}
		}
		
		if (!result_SP)
		{
			// General case; this is hit when (trueValues_count == falseValues_count == 1) && (test_count == 0), since the
			// test_count > 1 case is handled directly above and the test_count == 1 case is further above.
			result_SP = trueValues_value->NewMatchingType();
			EidosValue *result = result_SP.get();
			
			for (int value_index = 0; value_index < test_count; ++value_index)
			{
				// CODE COVERAGE: The interior of the loop here is actually dead code; see above.
				if (logical_vec[value_index])
					result->PushValueFromIndexOfEidosValue(0, *trueValues_value, nullptr);
				else
					result->PushValueFromIndexOfEidosValue(0, *falseValues_value, nullptr);
			}
		}
	}
	else if ((trueValues_count == test_count) && (falseValues_count == 1))
	{
		// vector trueValues, singleton falseValues; I suspect this case is less common so I'm deferring optimization
		result_SP = trueValues_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < test_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(value_index, *trueValues_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(0, *falseValues_value, nullptr);
		}
	}
	else if ((trueValues_count == 1) && (falseValues_count == test_count))
	{
		// singleton trueValues, vector falseValues; I suspect this case is less common so I'm deferring optimization
		result_SP = trueValues_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		for (int value_index = 0; value_index < test_count; ++value_index)
		{
			if (logical_vec[value_index])
				result->PushValueFromIndexOfEidosValue(0, *trueValues_value, nullptr);
			else
				result->PushValueFromIndexOfEidosValue(value_index, *falseValues_value, nullptr);
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_ifelse): function ifelse() requires that trueValues and falseValues each be either of length 1, or equal in length to test." << EidosTerminate(nullptr);
	}
	
	// Dimensionality of the result always matches that of the test parameter; this is R's policy and it makes sense
	result_SP->CopyDimensionsFromValue(test_value);
	
	return result_SP;
}

//	(integer)match(* x, * table)
EidosValue_SP Eidos_ExecuteFunction_match(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	EidosValue *table_value = p_arguments[1].get();
	EidosValueType table_type = table_value->Type();
	int table_count = table_value->Count();
	
	if (x_type != table_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_match): function match() requires arguments x and table to be the same type." << EidosTerminate(nullptr);
	
	if (x_type == EidosValueType::kValueNULL)
	{
		result_SP = gStaticEidosValue_Integer_ZeroVec;
		return result_SP;
	}
	
	if ((x_count == 1) && (table_count == 1))
	{
		// Handle singleton matching separately, to allow the use of the fast vector API below
		if (x_type == EidosValueType::kValueLogical)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->LogicalAtIndex(0, nullptr) == table_value->LogicalAtIndex(0, nullptr) ? 0 : -1));
		else if (x_type == EidosValueType::kValueInt)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr) == table_value->IntAtIndex(0, nullptr) ? 0 : -1));
		else if (x_type == EidosValueType::kValueFloat)
		{
			double f0 = x_value->FloatAtIndex(0, nullptr), f1 = table_value->FloatAtIndex(0, nullptr);
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(((std::isnan(f0) && std::isnan(f1)) || (f0 == f1)) ? 0 : -1));
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string &s0 = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::string &s1 = ((EidosValue_String *)table_value)->StringRefAtIndex(0, nullptr);
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(s0 == s1 ? 0 : -1));
		}
		else if (x_type == EidosValueType::kValueObject)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->ObjectElementAtIndex(0, nullptr) == table_value->ObjectElementAtIndex(0, nullptr) ? 0 : -1));
	}
	else if (x_count == 1)	// && (table_count != 1)
	{
		int table_index;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t value0 = x_value->LogicalAtIndex(0, nullptr);
			const eidos_logical_t *logical_data1 = table_value->LogicalVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == logical_data1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t value0 = x_value->IntAtIndex(0, nullptr);
			const int64_t *int_data1 = table_value->IntVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == int_data1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double value0 = x_value->FloatAtIndex(0, nullptr);
			const double *float_data1 = table_value->FloatVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
			{
				double f1 = float_data1[table_index];
				
				if ((std::isnan(value0) && std::isnan(f1)) || (value0 == f1))
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string &value0 = ((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec1 = *table_value->StringVector();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == string_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		else // if (x_type == EidosValueType::kValueObject)
		{
			EidosObject *value0 = x_value->ObjectElementAtIndex(0, nullptr);
			EidosObject * const *objelement_vec1 = table_value->ObjectElementVector()->data();
			
			for (table_index = 0; table_index < table_count; ++table_index)
				if (value0 == objelement_vec1[table_index])
				{
					result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(table_index));
					break;
				}
		}
		
		if (table_index == table_count)
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1));
	}
	else if (table_count == 1)	// && (x_count != 1)
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t value1 = table_value->LogicalAtIndex(0, nullptr);
			const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(logical_data0[value_index] == value1 ? 0 : -1, value_index);
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t value1 = table_value->IntAtIndex(0, nullptr);
			const int64_t *int_data0 = x_value->IntVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(int_data0[value_index] == value1 ? 0 : -1, value_index);
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double value1 = table_value->FloatAtIndex(0, nullptr);
			const double *float_data0 = x_value->FloatVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				double f0 = float_data0[value_index];
				
				int_result->set_int_no_check(((std::isnan(f0) && std::isnan(value1)) || (f0 == value1)) ? 0 : -1, value_index);
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string &value1 = ((EidosValue_String *)table_value)->StringRefAtIndex(0, nullptr);
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(string_vec0[value_index] == value1 ? 0 : -1, value_index);
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObject *value1 = table_value->ObjectElementAtIndex(0, nullptr);
			EidosObject * const *objelement_vec0 = x_value->ObjectElementVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
				int_result->set_int_no_check(objelement_vec0[value_index] == value1 ? 0 : -1, value_index);
		}
	}
	else						// ((x_count != 1) && (table_count != 1))
	{
		// We can use the fast vector API; we want match() to be very fast since it is a common bottleneck
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		int64_t *int_result_data = int_result->data();
		result_SP = EidosValue_SP(int_result);
		
		int table_index;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			const eidos_logical_t *logical_data0 = x_value->LogicalVector()->data();
			const eidos_logical_t *logical_data1 = table_value->LogicalVector()->data();
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				for (table_index = 0; table_index < table_count; ++table_index)
					if (logical_data0[value_index] == logical_data1[table_index])
						break;
				
				int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
			}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data0 = x_value->IntVector()->data();
			const int64_t *int_data1 = table_value->IntVector()->data();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table to speed up lookups from O(N) to O(1)
#if EIDOS_ROBIN_HOOD_HASHING
				robin_hood::unordered_flat_map<int64_t, int64_t> fromValueToIndex;
				//typedef robin_hood::pair<int64_t, int64_t> MAP_PAIR;
#elif STD_UNORDERED_MAP_HASHING
				std::unordered_map<int64_t, int64_t> fromValueToIndex;
				//typedef std::pair<int64_t, int64_t> MAP_PAIR;
#endif
				
				try {
					for (table_index = 0; table_index < table_count; ++table_index)
						fromValueToIndex.emplace(int_data1[table_index], table_index);	// does nothing if the key is already in the map
				} catch (...) {
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_match): (internal error) function match() encountered a raise from its internal hash table (kValueInt); please report this." << EidosTerminate(nullptr);
				}
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MATCH_INT);
#pragma omp parallel for schedule(static) default(none) shared(x_count, fromValueToIndex) firstprivate(int_data0, int_result_data) if(x_count >= EIDOS_OMPMIN_MATCH_INT) num_threads(thread_count)
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(int_data0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result_data[value_index] = find_index;
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
						if (int_data0[value_index] == int_data1[table_index])
							break;
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			const double *float_data0 = x_value->FloatVector()->data();
			const double *float_data1 = table_value->FloatVector()->data();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table to speed up lookups from O(N) to O(1)
				// we have to use a custom comparator so that NAN==NAN is true, so that NAN gets matched correctly
				auto equal = [](const double& l, const double& r) { if (std::isnan(l) && std::isnan(r)) return true; return l == r; };
#if EIDOS_ROBIN_HOOD_HASHING
				robin_hood::unordered_flat_map<double, int64_t, robin_hood::hash<double>, decltype(equal)> fromValueToIndex(0, robin_hood::hash<double>{}, equal);
				//typedef robin_hood::pair<double, int64_t> MAP_PAIR;
#elif STD_UNORDERED_MAP_HASHING
				std::unordered_map<double, int64_t, std::hash<double>, decltype(equal)> fromValueToIndex(0, std::hash<double>{}, equal);
				//typedef std::pair<double, int64_t> MAP_PAIR;
#endif
				
				try {
					for (table_index = 0; table_index < table_count; ++table_index)
						fromValueToIndex.emplace(float_data1[table_index], table_index);	// does nothing if the key is already in the map
				} catch (...) {
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_match): (internal error) function match() encountered a raise from its internal hash table (kValueFloat); please report this." << EidosTerminate(nullptr);
				}
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MATCH_FLOAT);
#pragma omp parallel for schedule(static) default(none) shared(x_count, fromValueToIndex) firstprivate(float_data0, int_result_data) if(x_count >= EIDOS_OMPMIN_MATCH_FLOAT) num_threads(thread_count)
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(float_data0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result_data[value_index] = find_index;
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
					{
						double f0 = float_data0[value_index], f1 = float_data1[table_index];
						
						if ((std::isnan(f0) && std::isnan(f1)) || (f0 == f1))	// need to make NAN match NAN
							break;
					}
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::vector<std::string> &string_vec0 = *x_value->StringVector();
			const std::vector<std::string> &string_vec1 = *table_value->StringVector();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table to speed up lookups from O(N) to O(1)
#if EIDOS_ROBIN_HOOD_HASHING
				robin_hood::unordered_flat_map<std::string, int64_t> fromValueToIndex;
				//typedef robin_hood::pair<std::string, int64_t> MAP_PAIR;
#elif STD_UNORDERED_MAP_HASHING
				std::unordered_map<std::string, int64_t> fromValueToIndex;
				//typedef std::pair<std::string, int64_t> MAP_PAIR;
#endif
				
				try {
					for (table_index = 0; table_index < table_count; ++table_index)
						fromValueToIndex.emplace(string_vec1[table_index], table_index);	// does nothing if the key is already in the map
				} catch (...) {
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_match): (internal error) function match() encountered a raise from its internal hash table (kValueString); please report this." << EidosTerminate(nullptr);
				}
				
				// Note that if string_vec0 were firstprivate, OpenMP would copy the data, NOT the reference!!!
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MATCH_STRING);
#pragma omp parallel for schedule(static) default(none) shared(x_count, fromValueToIndex, string_vec0) firstprivate(int_result_data) if(x_count >= EIDOS_OMPMIN_MATCH_STRING) num_threads(thread_count)
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(string_vec0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result_data[value_index] = find_index;
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
						if (string_vec0[value_index] == string_vec1[table_index])
							break;
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
		else if (x_type == EidosValueType::kValueObject)
		{
			EidosObject * const *objelement_vec0 = x_value->ObjectElementVector()->data();
			EidosObject * const *objelement_vec1 = table_value->ObjectElementVector()->data();
			
			if ((x_count >= 500) && (table_count >= 5))		// a guess based on timing data; will be platform-dependent and dataset-dependent
			{
				// use a hash table to speed up lookups from O(N) to O(1)
#if EIDOS_ROBIN_HOOD_HASHING
				robin_hood::unordered_flat_map<EidosObject *, int64_t> fromValueToIndex;
				//typedef robin_hood::pair<EidosObject *, int64_t> MAP_PAIR;
#elif STD_UNORDERED_MAP_HASHING
				std::unordered_map<EidosObject *, int64_t> fromValueToIndex;
				//typedef std::pair<EidosObject *, int64_t> MAP_PAIR;
#endif
				
				try {
					for (table_index = 0; table_index < table_count; ++table_index)
						fromValueToIndex.emplace(objelement_vec1[table_index], table_index);	// does nothing if the key is already in the map
				} catch (...) {
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_match): (internal error) function match() encountered a raise from its internal hash table (kValueObject); please report this." << EidosTerminate(nullptr);
				}
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_MATCH_OBJECT);
#pragma omp parallel for schedule(static) default(none) shared(x_count, fromValueToIndex) firstprivate(objelement_vec0, int_result_data) if(x_count >= EIDOS_OMPMIN_MATCH_OBJECT) num_threads(thread_count)
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					auto find_iter = fromValueToIndex.find(objelement_vec0[value_index]);
					int64_t find_index = (find_iter == fromValueToIndex.end()) ? -1 : find_iter->second;
					
					int_result_data[value_index] = find_index;
				}
			}
			else
			{
				// brute-force lookup, since the problem probably isn't big enough to merit building a hash table
				for (int value_index = 0; value_index < x_count; ++value_index)
				{
					for (table_index = 0; table_index < table_count; ++table_index)
						if (objelement_vec0[value_index] == objelement_vec1[table_index])
							break;
					
					int_result->set_int_no_check(table_index == table_count ? -1 : table_index, value_index);
				}
			}
		}
	}
	
	return result_SP;
}

//	(integer)order(+ x, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_order(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		// This handles all the zero-length cases by returning integer(0)
		result_SP = gStaticEidosValue_Integer_ZeroVec;
	}
	else if (x_count == 1)
	{
		// This handles all the singleton cases by returning 0
		result_SP = gStaticEidosValue_Integer0;
	}
	else
	{
		// Here we handle the vector cases, which can be done with direct access
		EidosValueType x_type = x_value->Type();
		bool ascending = p_arguments[1]->LogicalAtIndex(0, nullptr);
		std::vector<int64_t> order;
		
		if (x_type == EidosValueType::kValueLogical)
			order = EidosSortIndexes(x_value->LogicalVector()->data(), x_count, ascending);
		else if (x_type == EidosValueType::kValueInt)
			order = EidosSortIndexes(x_value->IntVector()->data(), x_count, ascending);
		else if (x_type == EidosValueType::kValueFloat)
			order = EidosSortIndexes(x_value->FloatVector()->data(), x_count, ascending);
		else if (x_type == EidosValueType::kValueString)
			order = EidosSortIndexes(*x_value->StringVector(), ascending);
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(order));
		result_SP = EidosValue_SP(int_result);
	}
	
	return result_SP;
}

//	(string$)paste(..., [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_paste(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH paste0() BELOW!
	size_t argument_count = p_arguments.size();
	std::string separator = p_arguments[argument_count - 1]->StringAtIndex(0, nullptr);
	std::string result_string;
	
	// SLiM 3.5 breaks backward compatibility for paste() because the second argument, which would have been interpreted as "sep="
	// before, now gets eaten by the ellipsis unless it is explicitly named.  Here we try to issue a useful warning about this,
	// for the strings that seem like the most likely to be used as separators.
	if ((argument_count == 3) && (separator == " ") &&
		((p_arguments[1]->Type() == EidosValueType::kValueString) && (p_arguments[1]->Count() == 1)))
	{
		std::string pseudosep = p_arguments[1]->StringAtIndex(0, nullptr);	// perhaps intended as sep, and now sep=" " has been used as a default?
		
		if ((pseudosep == "") || (pseudosep == " ") || (pseudosep == "\t") || (pseudosep == "\n") || (pseudosep == ",") || (pseudosep == ", ") || (pseudosep == " , ") || (pseudosep == ";") || (pseudosep == "; ") || (pseudosep == " ; "))
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ErrorOutputStream() << R"V0G0N(#WARNING (Eidos_ExecuteFunction_paste): function paste() changed its semantics in Eidos 2.5 (SLiM 3.5).  The second argument here is no longer interpreted to be a separator string; if you want those semantics, use 'sep=' to name the second argument, as in 'paste(1:5, sep=",");'.  That is the way to regain backward compatibility.  If, on the other hand, you do not intend the second argument here to be a separator string, you can get rid of this warning by appending the second argument using the + operator instead.  For example, you would transform 'x = paste(1:5, ",");' into 'x = paste(1:5) + " ,";'.  You can also use suppressWarnings() to avoid this warning message.)V0G0N" << std::endl;
		}
	}
	
	for (size_t argument_index = 0; argument_index < argument_count - 1; ++argument_index)
	{
		EidosValue *x_value = p_arguments[argument_index].get();
		int x_count = x_value->Count();
		EidosValueType x_type = x_value->Type();
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			if (!((value_index == 0) && (argument_index == 0)))
				result_string.append(separator);
			
			if (x_type == EidosValueType::kValueObject)
			{
				std::ostringstream oss;
				
				oss << *x_value->ObjectElementAtIndex(value_index, nullptr);
				
				result_string.append(oss.str());
			}
			else
				result_string.append(x_value->StringAtIndex(value_index, nullptr));
		}
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
}

//	(string$)paste0(...)
EidosValue_SP Eidos_ExecuteFunction_paste0(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	// SYNCH WITH paste() ABOVE!
	size_t argument_count = p_arguments.size();
	std::string result_string;
	
	for (size_t argument_index = 0; argument_index < argument_count; ++argument_index)
	{
		EidosValue *x_value = p_arguments[argument_index].get();
		int x_count = x_value->Count();
		EidosValueType x_type = x_value->Type();
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			if (x_type == EidosValueType::kValueObject)
			{
				std::ostringstream oss;
				
				oss << *x_value->ObjectElementAtIndex(value_index, nullptr);
				
				result_string.append(oss.str());
			}
			else
				result_string.append(x_value->StringAtIndex(value_index, nullptr));
		}
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(result_string));
}

//	(void)print(* x, [logical$ error = F])
EidosValue_SP Eidos_ExecuteFunction_print(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	eidos_logical_t use_error_stream = p_arguments[1]->LogicalAtIndex(0, nullptr);
	std::ostream &output_stream = (use_error_stream ? p_interpreter.ErrorOutputStream() : p_interpreter.ExecutionOutputStream());
	
	output_stream << *x_value << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(integer)rank(numeric x, [string$ tiesMethod = "average"])
EidosValue_SP Eidos_ExecuteFunction_rank(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *tiesMethod_value = p_arguments[1].get();
	int x_count = x_value->Count();
	
	// figure out how we will resolve ties
	typedef enum {
		kTiesAverage,		// produces a result of type float, unlike all the others
		kTiesFirst,
		kTiesLast,
		kTiesRandom,		// not currently supported, but supported in R
		kTiesMax,
		kTiesMin
	} TiesMethod;
	
	std::string tiesMethod_string = tiesMethod_value->StringAtIndex(0, nullptr);
	TiesMethod tiesMethod;
	
	if (tiesMethod_string == "average")
		tiesMethod = TiesMethod::kTiesAverage;
	else if (tiesMethod_string == "first")
		tiesMethod = TiesMethod::kTiesFirst;
	else if (tiesMethod_string == "last")
		tiesMethod = TiesMethod::kTiesLast;
	else if (tiesMethod_string == "random")
		tiesMethod = TiesMethod::kTiesRandom;
	else if (tiesMethod_string == "max")
		tiesMethod = TiesMethod::kTiesMax;
	else if (tiesMethod_string == "min")
		tiesMethod = TiesMethod::kTiesMin;
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rank): function rank() requires tiesMethod to be 'average', 'first', 'last', 'random', 'max', or 'min'." << EidosTerminate(nullptr);
	
	if (tiesMethod == TiesMethod::kTiesRandom)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rank): tiesMethod == 'random' is not currently supported." << EidosTerminate(nullptr);
	
	if (x_count == 0)
	{
		// This handles all the zero-length cases by returning float(0) or integer(0)
		if (tiesMethod == TiesMethod::kTiesAverage)
			result_SP = gStaticEidosValue_Float_ZeroVec;
		else
			result_SP = gStaticEidosValue_Integer_ZeroVec;
	}
	else if (x_count == 1)
	{
		// This handles all the singleton cases by returning 1.0 or 1
		if (tiesMethod == TiesMethod::kTiesAverage)
			result_SP = gStaticEidosValue_Float1;
		else
			result_SP = gStaticEidosValue_Integer1;
	}
	else
	{
		// Here we handle the vector cases, which can be done with direct access
		EidosValue_Float_vector *float_result = nullptr;
		EidosValue_Int_vector *int_result = nullptr;
		EidosValueType x_type = x_value->Type();
		
		if (tiesMethod == TiesMethod::kTiesAverage)
		{
			float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(float_result);
		}
		else
		{
			int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
			result_SP = EidosValue_SP(int_result);
		}
		
		// Handle integer and float; note that this is unrelated to the type of the result!
		if (x_type == EidosValueType::kValueInt)
		{
			std::vector<std::pair<int64_t, size_t>> pairs;
			
			{
				// construct our vector of pairs: std::pair<original x value, index in x>
				const int64_t *int_data = x_value->IntVector()->data();
				
				for (int index = 0; index < x_count; ++index)
					pairs.emplace_back(int_data[index], index);
				
				// sort by the original x value; we use a stable sort if needed by the ties method
				if ((tiesMethod == TiesMethod::kTiesFirst) || (tiesMethod == TiesMethod::kTiesLast))
					std::stable_sort(pairs.begin(), pairs.end(), [](const std::pair<int64_t, size_t> &l, const std::pair<int64_t, size_t> &r) { return l.first < r.first; });
				else
					std::sort(pairs.begin(), pairs.end(), [](const std::pair<int64_t, size_t> &l, const std::pair<int64_t, size_t> &r) { return l.first < r.first; });
			}
			
			// handle shared ranks one at a time, starting with rank 1
			for (int run_start = 0; run_start < x_count; )
			{
				// look for runs of equal x values, which get handled as a block
				int64_t run_value = pairs[run_start].first;
				int run_end;
				
				for (run_end = run_start + 1; run_end < x_count; ++run_end)
					if (pairs[run_end].first != run_value)
						break;
				run_end--;
				
				// the run ranges from run_start to run_end
				switch (tiesMethod)
				{
					case TiesMethod::kTiesAverage:
					{
						double rank = (run_end + run_start) / 2.0;
						
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							float_result->set_float_no_check((double)rank + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesFirst:
					{
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)run_pos + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesLast:
					{
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)(run_end - (run_pos - run_start)) + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesRandom:
					{
						// not currently supported, errors out above
						break;
					}
					case TiesMethod::kTiesMax:
					{
						int64_t rank = run_end;
						
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)rank + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesMin:
					{
						int64_t rank = run_start;
						
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)rank + 1, pairs[run_pos].second);
						break;
					}
				}
				
				// go to the next element to handle the next rank
				run_start = run_end + 1;
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			std::vector<std::pair<double, size_t>> pairs;
			
			{
				// construct our vector of pairs: std::pair<original x value, index in x>
				const double *float_data = x_value->FloatVector()->data();
				
				for (int index = 0; index < x_count; ++index)
					pairs.emplace_back(float_data[index], index);
				
				// sort by the original x value; we use a stable sort if needed by the ties method
				if ((tiesMethod == TiesMethod::kTiesFirst) || (tiesMethod == TiesMethod::kTiesLast))
					std::stable_sort(pairs.begin(), pairs.end(), [](const std::pair<double, size_t> &l, const std::pair<double, size_t> &r) { return l.first < r.first; });
				else
					std::sort(pairs.begin(), pairs.end(), [](const std::pair<double, size_t> &l, const std::pair<double, size_t> &r) { return l.first < r.first; });
			}
			
			// handle shared ranks one at a time, starting with rank 1
			for (int run_start = 0; run_start < x_count; )
			{
				// look for runs of equal x values, which get handled as a block
				double run_value = pairs[run_start].first;
				int run_end;
				
				for (run_end = run_start + 1; run_end < x_count; ++run_end)
					if (pairs[run_end].first != run_value)
						break;
				run_end--;
				
				// the run ranges from run_start to run_end
				switch (tiesMethod)
				{
					case TiesMethod::kTiesAverage:
					{
						double rank = (run_end + run_start) / 2.0;
						
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							float_result->set_float_no_check((double)rank + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesFirst:
					{
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)run_pos + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesLast:
					{
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)(run_end - (run_pos - run_start)) + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesRandom:
					{
						// not currently supported, errors out above
						break;
					}
					case TiesMethod::kTiesMax:
					{
						int64_t rank = run_end;
						
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)rank + 1, pairs[run_pos].second);
						break;
					}
					case TiesMethod::kTiesMin:
					{
						int64_t rank = run_start;
						
						for (int run_pos = run_start; run_pos <= run_end; ++run_pos)
							int_result->set_int_no_check((int64_t)rank + 1, pairs[run_pos].second);
						break;
					}
				}
				
				// go to the next element to handle the next rank
				run_start = run_end + 1;
			}
		}
	}
	
	return result_SP;
}

//	(*)rev(* x)
EidosValue_SP Eidos_ExecuteFunction_rev(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int value_index = x_count - 1; value_index >= 0; --value_index)
		result->PushValueFromIndexOfEidosValue(value_index, *x_value, nullptr);
	
	return result_SP;
}

//	(integer$)size(* x)
//	(integer$)length(* x)
EidosValue_SP Eidos_ExecuteFunction_size_length(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->Count()));
	
	return result_SP;
}

//	(+)sort(+ x, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_sort(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter __attribute__((unused)) &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int value_index = 0; value_index < x_count; ++value_index)
		result->PushValueFromIndexOfEidosValue(value_index, *x_value, nullptr);
	
	result->Sort(p_arguments[1]->LogicalAtIndex(0, nullptr));
	
	return result_SP;
}

//	(object)sortBy(object x, string$ property, [logical$ ascending = T])
EidosValue_SP Eidos_ExecuteFunction_sortBy(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(((EidosValue_Object *)x_value)->Class()))->resize_no_initialize_RR(x_count);
	result_SP = EidosValue_SP(object_result);
	
	if (object_result->UsesRetainRelease())
	{
		for (int value_index = 0; value_index < x_count; ++value_index)
			object_result->set_object_element_no_check_no_previous_RR(x_value->ObjectElementAtIndex(value_index, nullptr), value_index);
	}
	else
	{
		for (int value_index = 0; value_index < x_count; ++value_index)
			object_result->set_object_element_no_check_NORR(x_value->ObjectElementAtIndex(value_index, nullptr), value_index);
	}
	
	object_result->SortBy(p_arguments[1]->StringAtIndex(0, nullptr), p_arguments[2]->LogicalAtIndex(0, nullptr));
	
	return result_SP;
}

//	(void)str(* x, [logical$ error = F])
EidosValue_SP Eidos_ExecuteFunction_str(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	eidos_logical_t use_error_stream = p_arguments[1]->LogicalAtIndex(0, nullptr);
	std::ostream &output_stream = (use_error_stream ? p_interpreter.ErrorOutputStream() : p_interpreter.ExecutionOutputStream());
	
	x_value->PrintStructure(output_stream, 2);
	output_stream << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(integer)tabulate(integer bin, [Ni$ maxbin = NULL])
EidosValue_SP Eidos_ExecuteFunction_tabulate(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *bin_value = p_arguments[0].get();
	int value_count = bin_value->Count();			// the name "bin_count" is just too confusing
	
	EidosValue *maxbin_value = p_arguments[1].get();
	EidosValueType maxbin_type = maxbin_value->Type();
	
	// set up to work with either a singleton or a non-singleton vector
	int64_t singleton_value = (value_count == 1) ? bin_value->IntAtIndex(0, nullptr) : 0;
	const int64_t *int_data = (value_count == 1) ? &singleton_value : bin_value->IntVector()->data();
	
	// determine maxbin
	int64_t maxbin;
	
	if (maxbin_type == EidosValueType::kValueNULL)
	{
		maxbin = 0;		// note that if the parallel loop runs, this gets reinitialized to the most negative number!
		
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_TABULATE_MAXBIN);
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_TABULATE_MAXBIN);
#pragma omp parallel for schedule(static) default(none) shared(value_count) firstprivate(int_data) reduction(max: maxbin) if(value_count >= EIDOS_OMPMIN_TABULATE_MAXBIN) num_threads(thread_count)
		for (int value_index = 0; value_index < value_count; ++value_index)
		{
			int64_t value = int_data[value_index];
			if (value > maxbin)
				maxbin = value;
		}
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_TABULATE_MAXBIN);
	}
	else
	{
		maxbin = maxbin_value->IntAtIndex(0, nullptr);
	}
	
	if (maxbin < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_tabulate): function tabulate() requires maxbin to be greater than or equal to 0." << EidosTerminate(nullptr);
	
	// set up the result vector and zero it out
	int64_t num_bins = maxbin + 1;
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_bins);
	int64_t *result_data = int_result->data();
	result_SP = EidosValue_SP(int_result);
	
	for (int bin_index = 0; bin_index < num_bins; ++bin_index)
		result_data[bin_index] = 0;
	
	// do the tabulation
#ifdef _OPENMP
	if ((value_count > EIDOS_OMPMIN_TABULATE) && (gEidosNumThreads > 1))
	{
		// Our custom OpenMP implementation has some extra overhead that we want to avoid when running single-threaded
		// We make completely separate tallies in each thread, and then do a reduction at the end into result_data.
		// I tried some other approaches – per-thread locks, and atomic updates – and they were much slower.
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_TABULATE);
#pragma omp parallel default(none) shared(value_count, num_bins) firstprivate(int_data, result_data) num_threads(thread_count) // if(EIDOS_OMPMIN_TABULATE) is above
		{
			int64_t *perthread_tallies = (int64_t *)calloc(num_bins, sizeof(int64_t));
			
#pragma omp for schedule(dynamic, 1024) nowait
			for (int value_index = 0; value_index < value_count; ++value_index)
			{
				int64_t value = int_data[value_index];
				
				if ((value >= 0) && (value < num_bins))
				{
					// I tried using per-bin locks instead, but the locking overhead was huge.
					perthread_tallies[value]++;
				}
			}
			
#pragma omp critical
			{
				// Given the nowait on the for loop above, there is some hope that the threads won't stack up too badly here
				for (int bin_index = 0; bin_index < num_bins; ++bin_index)
					result_data[bin_index] += perthread_tallies[bin_index];
			}
			
			free(perthread_tallies);
		}
	}
	else
#endif
	{
		// Non-parallel implementation
		for (int value_index = 0; value_index < value_count; ++value_index)
		{
			int64_t value = int_data[value_index];
			
			if ((value >= 0) && (value <= maxbin))
				result_data[value]++;
		}
	}
	return result_SP;
}

//	(*)unique(* x, [logical$ preserveOrder = T])
EidosValue_SP Eidos_ExecuteFunction_unique(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	return UniqueEidosValue(p_arguments[0].get(), false, p_arguments[1]->LogicalAtIndex(0, nullptr));
}

//	(integer)which(logical x)
EidosValue_SP Eidos_ExecuteFunction_which(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
	EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
	result_SP = EidosValue_SP(int_result);
	
	for (int value_index = 0; value_index < x_count; ++value_index)
		if (logical_data[value_index])
			int_result->push_int(value_index);
	
	return result_SP;
}

//	(integer$)whichMax(+ x)
EidosValue_SP Eidos_ExecuteFunction_whichMax(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		int first_index = 0;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t max = x_value->LogicalAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					eidos_logical_t temp = logical_data[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t max = x_value->IntAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const int64_t *int_data = x_value->IntVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double max = x_value->FloatAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
				const double *float_data = x_value->FloatVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					double temp = float_data[value_index];
					if (max < temp) { max = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string *max = &((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_String_vector; we can use the fast API
				const std::vector<std::string> &string_vec = *x_value->StringVector();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
					if (*max < temp) { max = &temp; first_index = value_index; }
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_index));
	}
	
	return result_SP;
}

//	(integer$)whichMin(+ x)
EidosValue_SP Eidos_ExecuteFunction_whichMin(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	int x_count = x_value->Count();
	
	if (x_count == 0)
	{
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		int first_index = 0;
		
		if (x_type == EidosValueType::kValueLogical)
		{
			eidos_logical_t min = x_value->LogicalAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const eidos_logical_t *logical_data = x_value->LogicalVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					eidos_logical_t temp = logical_data[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueInt)
		{
			int64_t min = x_value->IntAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Int_vector; we can use the fast API
				const int64_t *int_data = x_value->IntVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					int64_t temp = int_data[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueFloat)
		{
			double min = x_value->FloatAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_Float_vector; we can use the fast API
				const double *float_data = x_value->FloatVector()->data();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					double temp = float_data[value_index];
					if (min > temp) { min = temp; first_index = value_index; }
				}
			}
		}
		else if (x_type == EidosValueType::kValueString)
		{
			const std::string *min = &((EidosValue_String *)x_value)->StringRefAtIndex(0, nullptr);
			
			if (x_count > 1)
			{
				// We have x_count != 1, so the type of x_value must be EidosValue_String_vector; we can use the fast API
				const std::vector<std::string> &string_vec = *x_value->StringVector();
				
				for (int value_index = 1; value_index < x_count; ++value_index)
				{
					const std::string &temp = string_vec[value_index];
					if (*min > temp) { min = &temp; first_index = value_index; }
				}
			}
		}
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(first_index));
	}
	
	return result_SP;
}


// ************************************************************************************
//
//	value type testing/coercion functions
//
#pragma mark -
#pragma mark Value type testing/coercion functions
#pragma mark -


//	(float)asFloat(+ x)
EidosValue_SP Eidos_ExecuteFunction_asFloat(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
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
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			float_result->set_float_no_check(x_value->FloatAtIndex(value_index, nullptr), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(integer)asInteger(+ x)
EidosValue_SP Eidos_ExecuteFunction_asInteger(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->IntAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			int_result->set_int_no_check(x_value->IntAtIndex(value_index, nullptr), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)asLogical(+ x)
EidosValue_SP Eidos_ExecuteFunction_asLogical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if ((x_count == 1) && (x_value->DimensionCount() == 1))
	{
		// Use the global constants, but only if we do not have to impose a dimensionality upon the value below
		result_SP = (x_value->LogicalAtIndex(0, nullptr) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			logical_result->set_logical_no_check(x_value->LogicalAtIndex(value_index, nullptr), value_index);
		
		result_SP->CopyDimensionsFromValue(x_value);
	}
	
	return result_SP;
}

//	(string)asString(+ x)
EidosValue_SP Eidos_ExecuteFunction_asString(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	if ((x_count == 0) && (x_value->Type() == EidosValueType::kValueNULL))
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_NULL));
	}
	else if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(x_value->StringAtIndex(0, nullptr)));
	}
	else
	{
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
		result_SP = EidosValue_SP(string_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			string_result->PushString(x_value->StringAtIndex(value_index, nullptr));
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(string$)elementType(* x)
EidosValue_SP Eidos_ExecuteFunction_elementType(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(x_value->ElementType()));
	
	return result_SP;
}

//	(logical$)isFloat(* x)
EidosValue_SP Eidos_ExecuteFunction_isFloat(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueFloat) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isInteger(* x)
EidosValue_SP Eidos_ExecuteFunction_isInteger(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueInt) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isLogical(* x)
EidosValue_SP Eidos_ExecuteFunction_isLogical(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueLogical) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isNULL(* x)
EidosValue_SP Eidos_ExecuteFunction_isNULL(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueNULL) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isObject(* x)
EidosValue_SP Eidos_ExecuteFunction_isObject(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueObject) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(logical$)isString(* x)
EidosValue_SP Eidos_ExecuteFunction_isString(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValueType x_type = x_value->Type();
	
	result_SP = (x_type == EidosValueType::kValueString) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	
	return result_SP;
}

//	(string$)type(* x)
EidosValue_SP Eidos_ExecuteFunction_type(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(StringForEidosValueType(x_value->Type())));
	
	return result_SP;
}











































