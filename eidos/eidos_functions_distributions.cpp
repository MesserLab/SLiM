//
//  eidos_functions_distributions.cpp
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
#include "eidos_rng.h"

#include <vector>

#include "gsl_linalg.h"
#include "gsl_errno.h"
#include "gsl_cdf.h"


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
//	distribution draw / density functions
//
#pragma mark -
#pragma mark Distribution draw/density functions
#pragma mark -


//	(integer)findInterval(numeric x, numeric vec, [logical$ rightmostClosed = F], [logical$ allInside = F])
EidosValue_SP Eidos_ExecuteFunction_findInterval(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Switching to using binary search for this algorithm, but I want to keep the old code around in case it is needed
#define EIDOS_FIND_INTERVAL_USE_BINARY_SEARCH	1
	
	EidosValue *arg_x = p_arguments[0].get();
	EidosValue *arg_vec = p_arguments[1].get();
	EidosValue *arg_rightmostClosed = p_arguments[2].get();
	EidosValue *arg_allInside = p_arguments[3].get();
	
	EidosValueType x_type = arg_x->Type();
	int x_count = arg_x->Count();
	
	EidosValueType vec_type = arg_vec->Type();
	int vec_count = arg_vec->Count();
	
	if (vec_count == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_findInterval): findInterval() requires vec to be of length > 0." << EidosTerminate(nullptr);
	
	bool rightmostClosed = arg_rightmostClosed->LogicalAtIndex(0, nullptr);
	bool allInside = arg_allInside->LogicalAtIndex(0, nullptr);
	
#if !(EIDOS_FIND_INTERVAL_USE_BINARY_SEARCH)
	// Used by the old linear-search algorithm
	int initial_x_result = allInside ? 0 : -1;
#endif
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
	
	if (vec_type == EidosValueType::kValueInt)
	{
		// Get a raw pointer to vec's values
		const int64_t *vec_data = arg_vec->IsSingleton() ? &((EidosValue_Int_singleton *)arg_vec)->IntValue_Mutable() : ((EidosValue_Int_vector *)arg_vec)->data();
		
		// Check that vec is sorted
		for (int vec_index = 0; vec_index < vec_count - 1; ++vec_index)
			if (vec_data[vec_index] > vec_data[vec_index + 1])
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_findInterval): findInterval() requires vec to be sorted into non-decreasing order." << EidosTerminate(nullptr);
		
		// Branch on the type of arg_x
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *x_data = arg_x->IsSingleton() ? &((EidosValue_Int_singleton *)arg_x)->IntValue_Mutable() : ((EidosValue_Int_vector *)arg_x)->data();
			
			// Find intervals for integer vec, integer x
			
#if EIDOS_FIND_INTERVAL_USE_BINARY_SEARCH
			// binary search - testing indicates this is pretty much always as fast or faster than linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				int64_t x_value = x_data[x_index];
				long x_result;
				
				if (x_value < vec_data[0])
					x_result = (allInside ? 0 : -1);
				else if (x_value > vec_data[vec_count - 1])
					x_result = (allInside ? vec_count - 2 : vec_count - 1);
				else if (x_value == vec_data[vec_count - 1])
					x_result = ((rightmostClosed || allInside) ? vec_count - 2 : vec_count - 1);
				else
					x_result = (std::upper_bound(vec_data, vec_data + vec_count, x_value) - vec_data) - 1;
				
				int_result->set_int_no_check(x_result, x_index);
			}
#else
			// linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				int64_t x_value = x_data[x_index];
				int x_result = initial_x_result;
				
				if (x_value >= vec_data[0])
				{
					for (x_result = 0; x_result < vec_count - 1; ++x_result)
						if ((x_value >= vec_data[x_result]) && (x_value < vec_data[x_result + 1]))
							break;
					
					if (rightmostClosed && (x_result == vec_count - 1) && (x_value == vec_data[vec_count - 1]))
						x_result = vec_count - 2;
					if (allInside && (x_result > vec_count - 2))
						x_result = vec_count - 2;
				}
				
				int_result->set_int_no_check(x_result, x_index);
			}
#endif
		}
		else	// (x_type == EidosValueType::kValueFloat)
		{
			const double *x_data = arg_x->IsSingleton() ? &((EidosValue_Float_singleton *)arg_x)->FloatValue_Mutable() : ((EidosValue_Float_vector *)arg_x)->data();
			
			// Find intervals for integer vec, float x
			
#if EIDOS_FIND_INTERVAL_USE_BINARY_SEARCH
			// binary search - testing indicates this is pretty much always as fast or faster than linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				double x_value = x_data[x_index];
				long x_result;
				
				if (x_value < vec_data[0])
					x_result = (allInside ? 0 : -1);
				else if (x_value > vec_data[vec_count - 1])
					x_result = (allInside ? vec_count - 2 : vec_count - 1);
				else if (x_value == vec_data[vec_count - 1])
					x_result = ((rightmostClosed || allInside) ? vec_count - 2 : vec_count - 1);
				else
					x_result = (std::upper_bound(vec_data, vec_data + vec_count, x_value) - vec_data) - 1;
				
				int_result->set_int_no_check(x_result, x_index);
			}
#else
			// linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				double x_value = x_data[x_index];
				int x_result = initial_x_result;
				
				if (x_value >= vec_data[0])
				{
					for (x_result = 0; x_result < vec_count - 1; ++x_result)
						if ((x_value >= vec_data[x_result]) && (x_value < vec_data[x_result + 1]))
							break;
					
					if (rightmostClosed && (x_result == vec_count - 1) && (x_value == vec_data[vec_count - 1]))
						x_result = vec_count - 2;
					if (allInside && (x_result > vec_count - 2))
						x_result = vec_count - 2;
				}
				
				int_result->set_int_no_check(x_result, x_index);
			}
#endif
		}
	}
	else // (vec_type == EidosValueType::kValueFloat))
	{
		// Get a raw pointer to vec's values
		const double *vec_data = arg_vec->IsSingleton() ? &((EidosValue_Float_singleton *)arg_vec)->FloatValue_Mutable() : ((EidosValue_Float_vector *)arg_vec)->data();
		
		// Check that vec is sorted
		for (int vec_index = 0; vec_index < vec_count - 1; ++vec_index)
			if (vec_data[vec_index] > vec_data[vec_index + 1])
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_findInterval): findInterval() requires vec to be sorted into non-decreasing order." << EidosTerminate(nullptr);
		
		// Branch on the type of arg_x
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *x_data = arg_x->IsSingleton() ? &((EidosValue_Int_singleton *)arg_x)->IntValue_Mutable() : ((EidosValue_Int_vector *)arg_x)->data();
			
			// Find intervals for float vec, integer x
			
#if EIDOS_FIND_INTERVAL_USE_BINARY_SEARCH
			// binary search - testing indicates this is pretty much always as fast or faster than linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				int64_t x_value = x_data[x_index];
				long x_result;
				
				if (x_value < vec_data[0])
					x_result = (allInside ? 0 : -1);
				else if (x_value > vec_data[vec_count - 1])
					x_result = (allInside ? vec_count - 2 : vec_count - 1);
				else if (x_value == vec_data[vec_count - 1])
					x_result = ((rightmostClosed || allInside) ? vec_count - 2 : vec_count - 1);
				else
					x_result = (std::upper_bound(vec_data, vec_data + vec_count, x_value) - vec_data) - 1;
				
				int_result->set_int_no_check(x_result, x_index);
			}
#else
			// linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				int64_t x_value = x_data[x_index];
				int x_result = initial_x_result;
				
				if (x_value >= vec_data[0])
				{
					for (x_result = 0; x_result < vec_count - 1; ++x_result)
						if ((x_value >= vec_data[x_result]) && (x_value < vec_data[x_result + 1]))
							break;
					
					if (rightmostClosed && (x_result == vec_count - 1) && (x_value == vec_data[vec_count - 1]))
						x_result = vec_count - 2;
					if (allInside && (x_result > vec_count - 2))
						x_result = vec_count - 2;
				}
				
				int_result->set_int_no_check(x_result, x_index);
			}
#endif
		}
		else	// (x_type == EidosValueType::kValueFloat)
		{
			const double *x_data = arg_x->IsSingleton() ? &((EidosValue_Float_singleton *)arg_x)->FloatValue_Mutable() : ((EidosValue_Float_vector *)arg_x)->data();
			
			// Find intervals for float vec, float x
			
#if EIDOS_FIND_INTERVAL_USE_BINARY_SEARCH
			// binary search - testing indicates this is pretty much always as fast or faster than linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				double x_value = x_data[x_index];
				long x_result;
				
				if (x_value < vec_data[0])
					x_result = (allInside ? 0 : -1);
				else if (x_value > vec_data[vec_count - 1])
					x_result = (allInside ? vec_count - 2 : vec_count - 1);
				else if (x_value == vec_data[vec_count - 1])
					x_result = ((rightmostClosed || allInside) ? vec_count - 2 : vec_count - 1);
				else
					x_result = (std::upper_bound(vec_data, vec_data + vec_count, x_value) - vec_data) - 1;
				
				int_result->set_int_no_check(x_result, x_index);
			}
#else
			// linear search
			for (int x_index = 0; x_index < x_count; ++x_index)
			{
				double x_value = x_data[x_index];
				int x_result = initial_x_result;
				
				if (x_value >= vec_data[0])
				{
					for (x_result = 0; x_result < vec_count - 1; ++x_result)
						if ((x_value >= vec_data[x_result]) && (x_value < vec_data[x_result + 1]))
							break;
					
					if (rightmostClosed && (x_result == vec_count - 1) && (x_value == vec_data[vec_count - 1]))
						x_result = vec_count - 2;
					if (allInside && (x_result > vec_count - 2))
						x_result = vec_count - 2;
				}
				
				int_result->set_int_no_check(x_result, x_index);
			}
#endif
		}
	}
	
	return EidosValue_SP(int_result);
}

//	(float)dmvnorm(float x, numeric mu, numeric sigma)
EidosValue_SP Eidos_ExecuteFunction_dmvnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_x = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	
	if (arg_x->Count() == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// matrix with n rows (one row per quantile vector) and k columns (one column per dimension)
	int dimension_count = arg_x->DimensionCount();
	int64_t num_quantiles;
	int d;
	
	if (dimension_count == 1)
	{
		num_quantiles = 1;
		d = arg_x->Count();
	}
	else if (dimension_count == 2)
	{
		const int64_t *dimensions = arg_x->Dimensions();
		num_quantiles = dimensions[0];
		d = (int)dimensions[1];
	}
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires x to be a vector containing a single quantile, or a matrix of quantiles." << EidosTerminate(nullptr);
	
	if (d <= 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires a Gaussian function dimensionality of >= 2 (use dnorm() for dimensionality of 1)." << EidosTerminate(nullptr);
	
	int mu_count = arg_mu->Count();
	int mu_dimcount = arg_mu->DimensionCount();
	int sigma_dimcount = arg_sigma->DimensionCount();
	const int64_t *sigma_dims = arg_sigma->Dimensions();
	
	if ((mu_dimcount != 1) || (mu_count != d))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires mu to be a plain vector of length k, where k is the number of dimensions for the multivariate Gaussian function (>= 2), matching the dimensionality of the quantile vectors in x." << EidosTerminate(nullptr);
	if (sigma_dimcount != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires sigma to be a matrix." << EidosTerminate(nullptr);
	if ((sigma_dims[0] != d) || (sigma_dims[1] != d))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires sigma to be a k x k matrix, where k is the number of dimensions for the multivariate Gaussian function (>= 2), matching the dimensionality of the quantile vectors in x." << EidosTerminate(nullptr);
	
	// Set up the GSL vectors
	gsl_vector *gsl_mu = gsl_vector_calloc(d);
	gsl_matrix *gsl_Sigma = gsl_matrix_calloc(d, d);
	gsl_matrix *gsl_L = gsl_matrix_calloc(d, d);
	gsl_vector *gsl_x = gsl_vector_calloc(d);
	gsl_vector *gsl_work = gsl_vector_calloc(d);
	
	if (!gsl_mu || !gsl_Sigma || !gsl_L || !gsl_x || !gsl_work)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	try {
		for (int dim_index = 0; dim_index < d; ++dim_index)
			gsl_vector_set(gsl_mu, dim_index, arg_mu->FloatAtIndex(dim_index, nullptr));
		
		for (int row_index = 0; row_index < d; ++row_index)
		{
			for (int col_index = 0; col_index < d; ++col_index)
			{
				double value = arg_sigma->FloatAtIndex(row_index + col_index * d, nullptr);
				
				if (std::isnan(value))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() does not allow sigma to contain NANs." << EidosTerminate(nullptr);	// oddly, GSL does not raise an error on this!
				
				gsl_matrix_set(gsl_Sigma, row_index, col_index, value);
			}
		}
		
		gsl_matrix_memcpy(gsl_L, gsl_Sigma);
	}
	catch (...) {
		gsl_vector_free(gsl_mu);
		gsl_matrix_free(gsl_Sigma);
		gsl_matrix_free(gsl_L);
		gsl_vector_free(gsl_x);
		gsl_vector_free(gsl_work);
		throw;
	}
	
	// Disable the GSL's default error handler, which calls abort().  Normally we run with that handler,
	// which is perhaps a bit risky, but we want to check for all error cases and avert them before we
	// ever call into the GSL; having the GSL raise when it encounters an error condition is kind of OK
	// because it should never ever happen.  But the GSL calls we will make in this function could hit
	// errors unpredictably, if for example it turns out that Sigma is not positive-definite.  So for
	// this stretch of code we disable the default handler and check for errors returned from the GSL.
	gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
	int gsl_err;
	
	// Do the draws, which involves a preliminary step of doing a Cholesky decomposition
	gsl_err = gsl_linalg_cholesky_decomp1(gsl_L);
	
	if (gsl_err)
	{
		gsl_set_error_handler(old_handler);
		
		// Clean up GSL stuff
		gsl_vector_free(gsl_mu);
		gsl_matrix_free(gsl_Sigma);
		gsl_matrix_free(gsl_L);
		gsl_vector_free(gsl_x);
		gsl_vector_free(gsl_work);
		
		if (gsl_err == GSL_EDOM)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): function dmvnorm() requires that sigma, the variance-covariance matrix, be positive-definite." << EidosTerminate(nullptr);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_linalg_cholesky_decomp1() function." << EidosTerminate(nullptr);
	}
	
	const double *float_data = arg_x->FloatVector()->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
	result_SP = EidosValue_SP(float_result);
	
	for (int64_t value_index = 0; value_index < num_quantiles; ++value_index)
	{
		double gsl_result;
		
		for (int dim_index = 0; dim_index < d; ++dim_index)
			gsl_vector_set(gsl_x, dim_index, *(float_data + value_index + dim_index * num_quantiles));
		
		gsl_err = gsl_ran_multivariate_gaussian_pdf (gsl_x, gsl_mu, gsl_L, &gsl_result, gsl_work);
		
		if (gsl_err)
		{
			gsl_set_error_handler(old_handler);
			
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_ran_multivariate_gaussian_pdf() function." << EidosTerminate(nullptr);
		}
		
		float_result->set_float_no_check(gsl_result, value_index);
	}
	
	// Clean up GSL stuff
	gsl_vector_free(gsl_mu);
	gsl_matrix_free(gsl_Sigma);
	gsl_matrix_free(gsl_L);
	gsl_vector_free(gsl_x);
	gsl_vector_free(gsl_work);
	
	gsl_set_error_handler(old_handler);
	
	return result_SP;
}

//	(float)dnorm(float x, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_dnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int num_quantiles = arg_quantile->Count();
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires mean to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		// Note that this case, EIDOS_OMPMIN_DNORM_1, running SINGLE-threaded, got about 20% slower from SLiM 4.0.1 to now.  I have looked into it
		// and have no idea why.  My best hypothesis is that it is due to a change in the compiler/toolchain treatment of this code.  The only code
		// change here is the addition of the omp pragma, and removing that makes no difference.  A profile shows nothing, reverting the whole
		// function shows nothing, doing an Archive build shows nothing.  Total mystery, without delving into the assembly code.  Such is life.
		if (sigma0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gaussian_pdf(arg_quantile->FloatAtIndex(0, nullptr) - mu0, sigma0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_DNORM_1);
#pragma omp parallel for schedule(static) default(none) shared(num_quantiles) firstprivate(float_data, float_result, mu0, sigma0) if(num_quantiles >= EIDOS_OMPMIN_DNORM_1) num_threads(thread_count)
			for (int value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_gaussian_pdf(float_data[value_index] - mu0, sigma0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		bool saw_error = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_DNORM_2);
#pragma omp parallel for schedule(static) default(none) shared(num_quantiles) firstprivate(float_data, float_result, mu_singleton, sigma_singleton, mu0, sigma0, arg_mu, arg_sigma) reduction(||: saw_error) if(num_quantiles >= EIDOS_OMPMIN_DNORM_2) num_threads(thread_count)
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(value_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(value_index, nullptr));
			
			if (sigma <= 0.0)
			{
				saw_error = true;
				continue;
			}
			
			float_result->set_float_no_check(gsl_ran_gaussian_pdf(float_data[value_index] - mu, sigma), value_index);
		}
		
		if (saw_error)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dnorm): function dnorm() requires sd > 0.0." << EidosTerminate(nullptr);
	}
	
	return result_SP;
}

//	(float)qnorm(float p, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_qnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_prob = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_probs = arg_prob->Count();
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_probs))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires mean to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_probs))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires sd to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_probs == 1)
		{
			const double float_prob = arg_prob->FloatAtIndex(0, nullptr);
			if (float_prob < 0.0 || float_prob > 1.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires 0.0 <= p <= 1.0 (" << EidosStringForFloat(float_prob) << " supplied)." << EidosTerminate(nullptr);

			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_cdf_gaussian_Pinv(float_prob, sigma0) + mu0));
		}
		else
		{
			const double *float_data = arg_prob->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_probs);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t value_index = 0; value_index < num_probs; ++value_index) {
				if (float_data[value_index] < 0.0 || float_data[value_index] > 1.0)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires 0.0 <= p <= 1.0 (" << EidosStringForFloat(float_data[value_index]) << " supplied)." << EidosTerminate(nullptr);
				float_result->set_float_no_check(gsl_cdf_gaussian_Pinv(float_data[value_index], sigma0) + mu0, value_index);
        }
		}
	}
	else
	{
		const double *float_data = arg_prob->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_probs);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_probs; ++value_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(value_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(value_index, nullptr));
		  if (float_data[value_index] < 0.0 || float_data[value_index] > 1.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires 0.0 <= p <= 1.0 (" << EidosStringForFloat(float_data[value_index]) << " supplied)." << EidosTerminate(nullptr);
			
			if (sigma <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_qnorm): function qnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_cdf_gaussian_Pinv(float_data[value_index], sigma) + mu, value_index);
		}
	}
	
	return result_SP;
}


//	(float)pnorm(float q, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_pnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_quantiles = arg_quantile->Count();
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires mean to be of length 1 or equal in length to q." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires sd to be of length 1 or equal in length to q." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (mu_singleton && sigma_singleton)
	{
		if (sigma0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_cdf_gaussian_P(arg_quantile->FloatAtIndex(0, nullptr) - mu0, sigma0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_cdf_gaussian_P(float_data[value_index] - mu0, sigma0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex(value_index, nullptr));
			double sigma = (sigma_singleton ? sigma0 : arg_sigma->FloatAtIndex(value_index, nullptr));
			
			if (sigma <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_pnorm): function pnorm() requires sd > 0.0 (" << EidosStringForFloat(sigma) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_cdf_gaussian_P(float_data[value_index] - mu, sigma), value_index);
		}
	}
	
	return result_SP;
}

//	(float)dbeta(float x, numeric alpha, numeric beta)
EidosValue_SP Eidos_ExecuteFunction_dbeta(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_alpha = p_arguments[1].get();
	EidosValue *arg_beta = p_arguments[2].get();
	int num_quantiles = arg_quantile->Count();
	int arg_alpha_count = arg_alpha->Count();
	int arg_beta_count = arg_beta->Count();
	bool alpha_singleton = (arg_alpha_count == 1);
	bool beta_singleton = (arg_beta_count == 1);
	
	if (!alpha_singleton && (arg_alpha_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires alpha to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	if (!beta_singleton && (arg_beta_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires beta to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	double alpha0 = (arg_alpha_count ? arg_alpha->FloatAtIndex(0, nullptr) : 0.0);
	double beta0 = (arg_beta_count ? arg_beta->FloatAtIndex(0, nullptr) : 0.0);
	
	if (alpha_singleton && beta_singleton)
	{
		if (!(alpha0 > 0.0))	// true for NaN
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha0) << " supplied)." << EidosTerminate(nullptr);
		if (!(beta0 > 0.0))		// true for NaN
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires beta > 0.0 (" << EidosStringForFloat(beta0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_beta_pdf(arg_quantile->FloatAtIndex(0, nullptr), alpha0, beta0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_beta_pdf(float_data[value_index], alpha0, beta0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double alpha = (alpha_singleton ? alpha0 : arg_alpha->FloatAtIndex(value_index, nullptr));
			double beta = (beta_singleton ? beta0 : arg_beta->FloatAtIndex(value_index, nullptr));
			
			if (!(alpha > 0.0))		// true for NaN
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha) << " supplied)." << EidosTerminate(nullptr);
			if (!(beta > 0.0))		// true for NaN
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dbeta): function dbeta() requires beta > 0.0 (" << EidosStringForFloat(beta) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_beta_pdf(float_data[value_index], alpha, beta), value_index);
		}
	}
	
	return result_SP;
}

//	(float)rbeta(integer$ n, numeric alpha, numeric beta)
EidosValue_SP Eidos_ExecuteFunction_rbeta(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_alpha = p_arguments[1].get();
	EidosValue *arg_beta = p_arguments[2].get();
	int arg_alpha_count = arg_alpha->Count();
	int arg_beta_count = arg_beta->Count();
	bool alpha_singleton = (arg_alpha_count == 1);
	bool beta_singleton = (arg_beta_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!alpha_singleton && (arg_alpha_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires alpha to be of length 1 or n." << EidosTerminate(nullptr);
	if (!beta_singleton && (arg_beta_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires beta to be of length 1 or n." << EidosTerminate(nullptr);
	
	double alpha0 = (arg_alpha_count ? arg_alpha->FloatAtIndex(0, nullptr) : 0.0);
	double beta0 = (arg_beta_count ? arg_beta->FloatAtIndex(0, nullptr) : 0.0);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (alpha_singleton && beta_singleton)
	{
		if (alpha0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha0) << " supplied)." << EidosTerminate(nullptr);
		if (beta0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires beta > 0.0 (" << EidosStringForFloat(beta0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_beta(rng, alpha0, beta0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_beta(rng, alpha0, beta0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double alpha = (alpha_singleton ? alpha0 : arg_alpha->FloatAtIndex(draw_index, nullptr));
			double beta = (beta_singleton ? beta0 : arg_beta->FloatAtIndex(draw_index, nullptr));
			
			if (alpha <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires alpha > 0.0 (" << EidosStringForFloat(alpha) << " supplied)." << EidosTerminate(nullptr);
			if (beta <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbeta): function rbeta() requires beta > 0.0 (" << EidosStringForFloat(beta) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_beta(rng, alpha, beta), draw_index);
		}
	}
	
	return result_SP;
}

//	(integer)rbinom(integer$ n, integer size, float prob)
EidosValue_SP Eidos_ExecuteFunction_rbinom(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_size = p_arguments[1].get();
	EidosValue *arg_prob = p_arguments[2].get();
	int arg_size_count = arg_size->Count();
	int arg_prob_count = arg_prob->Count();
	bool size_singleton = (arg_size_count == 1);
	bool prob_singleton = (arg_prob_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!size_singleton && (arg_size_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size to be of length 1 or n." << EidosTerminate(nullptr);
	if (!prob_singleton && (arg_prob_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires prob to be of length 1 or n." << EidosTerminate(nullptr);
	
	int size0 = (int)arg_size->IntAtIndex(0, nullptr);
	double probability0 = arg_prob->FloatAtIndex(0, nullptr);
	
	if (size_singleton && prob_singleton)
	{
		if (size0 < 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size >= 0 (" << size0 << " supplied)." << EidosTerminate(nullptr);
		if ((probability0 < 0.0) || (probability0 > 1.0) || std::isnan(probability0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires probability in [0.0, 1.0] (" << EidosStringForFloat(probability0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			if ((probability0 == 0.5) && (size0 == 1))
			{
				Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton((int64_t)Eidos_RandomBool(rng_state)));
			}
			else
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_binomial(rng, probability0, size0)));
			}
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			if ((probability0 == 0.5) && (size0 == 1))
			{
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_RBINOM_1);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result) if(num_draws >= EIDOS_OMPMIN_RBINOM_1) num_threads(thread_count)
				{
					Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
					
#pragma omp for schedule(dynamic, 1024) nowait
					for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
						int_result->set_int_no_check((int64_t)Eidos_RandomBool(rng_state), draw_index);
				}
			}
			else
			{
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_RBINOM_2);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result, probability0, size0) if(num_draws >= EIDOS_OMPMIN_RBINOM_2) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(dynamic, 1024) nowait
					for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
						int_result->set_int_no_check(gsl_ran_binomial(rng, probability0, size0), draw_index);
				}
			}
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		bool saw_error1 = false, saw_error2 = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_RBINOM_3);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result, size_singleton, prob_singleton, size0, probability0, arg_size, arg_prob) reduction(||: saw_error1) reduction(||: saw_error2) if(num_draws >= EIDOS_OMPMIN_RBINOM_3) num_threads(thread_count)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
#pragma omp for schedule(dynamic, 1024) nowait
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				int size = (size_singleton ? size0 : (int)arg_size->IntAtIndex((int)draw_index, nullptr));
				double probability = (prob_singleton ? probability0 : arg_prob->FloatAtIndex((int)draw_index, nullptr));
				
				if (size < 0)
				{
					saw_error1 = true;
					continue;
				}
				if ((probability < 0.0) || (probability > 1.0) || std::isnan(probability))
				{
					saw_error2 = true;
					continue;
				}
				
				int_result->set_int_no_check(gsl_ran_binomial(rng, probability, size), draw_index);
			}
		}
		
		if (saw_error1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires size >= 0." << EidosTerminate(nullptr);
		if (saw_error2)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbinom): function rbinom() requires probability in [0.0, 1.0]." << EidosTerminate(nullptr);
	}
	
	return result_SP;
}

//	(float)rcauchy(integer$ n, [numeric location = 0], [numeric scale = 1])
EidosValue_SP Eidos_ExecuteFunction_rcauchy(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_location = p_arguments[1].get();
	EidosValue *arg_scale = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_location_count = arg_location->Count();
	int arg_scale_count = arg_scale->Count();
	bool location_singleton = (arg_location_count == 1);
	bool scale_singleton = (arg_scale_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!location_singleton && (arg_location_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires location to be of length 1 or n." << EidosTerminate(nullptr);
	if (!scale_singleton && (arg_scale_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires scale to be of length 1 or n." << EidosTerminate(nullptr);
	
	double location0 = (arg_location_count ? arg_location->FloatAtIndex(0, nullptr) : 0.0);
	double scale0 = (arg_scale_count ? arg_scale->FloatAtIndex(0, nullptr) : 1.0);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (location_singleton && scale_singleton)
	{
		if (scale0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires scale > 0.0 (" << EidosStringForFloat(scale0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_cauchy(rng, scale0) + location0));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_cauchy(rng, scale0) + location0, draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double location = (location_singleton ? location0 : arg_location->FloatAtIndex(draw_index, nullptr));
			double scale = (scale_singleton ? scale0 : arg_scale->FloatAtIndex(draw_index, nullptr));
			
			if (scale <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rcauchy): function rcauchy() requires scale > 0.0 (" << EidosStringForFloat(scale) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_cauchy(rng, scale) + location, draw_index);
		}
	}
	
	return result_SP;
}

//	(integer)rdunif(integer$ n, [integer min = 0], [integer max = 1])
EidosValue_SP Eidos_ExecuteFunction_rdunif(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_min = p_arguments[1].get();
	EidosValue *arg_max = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_min_count = arg_min->Count();
	int arg_max_count = arg_max->Count();
	bool min_singleton = (arg_min_count == 1);
	bool max_singleton = (arg_max_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!min_singleton && (arg_min_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires min to be of length 1 or n." << EidosTerminate(nullptr);
	if (!max_singleton && (arg_max_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires max to be of length 1 or n." << EidosTerminate(nullptr);
	
	int64_t min_value0 = (arg_min_count ? arg_min->IntAtIndex(0, nullptr) : 0);
	int64_t max_value0 = (arg_max_count ? arg_max->IntAtIndex(0, nullptr) : 1);
	
	if (min_singleton && max_singleton)
	{
		uint64_t count0 = (max_value0 - min_value0) + 1;
		
		if (max_value0 < min_value0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires min <= max." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			if (count0 == 2)
			{
				Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(Eidos_RandomBool(rng_state) + min_value0));
			}
			else
			{
				Eidos_MT_State *mt = EIDOS_MT_RNG(omp_get_thread_num());
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(Eidos_rng_uniform_int_MT64(mt, count0) + min_value0));
			}
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			if (count0 == 2)
			{
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_RDUNIF_1);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result, min_value0) if(num_draws >= EIDOS_OMPMIN_RDUNIF_1) num_threads(thread_count)
				{
					Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
					
#pragma omp for schedule(dynamic, 1024) nowait
					for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
						int_result->set_int_no_check(Eidos_RandomBool(rng_state) + min_value0, draw_index);
				}
			}
			else
			{
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_RDUNIF_2);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result, min_value0, count0) if(num_draws >= EIDOS_OMPMIN_RDUNIF_2) num_threads(thread_count)
				{
					Eidos_MT_State *mt = EIDOS_MT_RNG(omp_get_thread_num());
					
#pragma omp for schedule(dynamic, 1024) nowait
					for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
						int_result->set_int_no_check(Eidos_rng_uniform_int_MT64(mt, count0) + min_value0, draw_index);
				}
			}
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		bool saw_error = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_RDUNIF_3);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result, min_singleton, max_singleton, min_value0, max_value0, arg_min, arg_max) reduction(||: saw_error) if(num_draws >= EIDOS_OMPMIN_RDUNIF_3) num_threads(thread_count)
		{
			Eidos_MT_State *mt = EIDOS_MT_RNG(omp_get_thread_num());
			
#pragma omp for schedule(dynamic, 1024) nowait
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				int64_t min_value = (min_singleton ? min_value0 : arg_min->IntAtIndex((int)draw_index, nullptr));
				int64_t max_value = (max_singleton ? max_value0 : arg_max->IntAtIndex((int)draw_index, nullptr));
				int64_t count = (max_value - min_value) + 1;
				
				if (max_value < min_value)
				{
					saw_error = true;
					continue;
				}
				
				int_result->set_int_no_check(Eidos_rng_uniform_int_MT64(mt, count) + min_value, draw_index);
			}
		}
		
		if (saw_error)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rdunif): function rdunif() requires min <= max." << EidosTerminate(nullptr);
	}
	
	return result_SP;
}

//	(float)dexp(float x, [numeric mu = 1])
EidosValue_SP Eidos_ExecuteFunction_dexp(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	int num_quantiles = arg_quantile->Count();
	int arg_mu_count = arg_mu->Count();
	bool mu_singleton = (arg_mu_count == 1);
	
	if (!mu_singleton && (arg_mu_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dexp): function dexp() requires mu to be of length 1 or equal in length to x." << EidosTerminate(nullptr);
	
	if (mu_singleton)
	{
		double mu0 = arg_mu->FloatAtIndex(0, nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_exponential_pdf(arg_quantile->FloatAtIndex(0, nullptr), mu0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			for (int value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_exponential_pdf(float_data[value_index], mu0), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mu = arg_mu->FloatAtIndex(value_index, nullptr);
			
			float_result->set_float_no_check(gsl_ran_exponential_pdf(float_data[value_index], mu), value_index);
		}
	}
	
	return result_SP;
}

//	(float)rexp(integer$ n, [numeric mu = 1])
EidosValue_SP Eidos_ExecuteFunction_rexp(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_mu_count = arg_mu->Count();
	bool mu_singleton = (arg_mu_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rexp): function rexp() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!mu_singleton && (arg_mu_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rexp): function rexp() requires mu to be of length 1 or n." << EidosTerminate(nullptr);
	
	if (mu_singleton)
	{
		double mu0 = arg_mu->FloatAtIndex(0, nullptr);
		
		if (num_draws == 1)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_exponential(rng, mu0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_REXP_1);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(float_result, mu0) if(num_draws >= EIDOS_OMPMIN_REXP_1) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
#pragma omp for schedule(static) nowait
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					float_result->set_float_no_check(gsl_ran_exponential(rng, mu0), draw_index);
			}
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_REXP_2);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(float_result, arg_mu) if(num_draws >= EIDOS_OMPMIN_REXP_2) num_threads(thread_count)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
#pragma omp for schedule(static) nowait
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				double mu = arg_mu->FloatAtIndex((int)draw_index, nullptr);
				
				float_result->set_float_no_check(gsl_ran_exponential(rng, mu), draw_index);
			}
		}
	}
	
	return result_SP;
}

//	(float)rf(integer$ n, numeric d1, numeric d2)
EidosValue_SP Eidos_ExecuteFunction_rf(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_d1 = p_arguments[1].get();
	EidosValue *arg_d2 = p_arguments[2].get();
	int arg_d1_count = arg_d1->Count();
	int arg_d2_count = arg_d2->Count();
	bool d1_singleton = (arg_d1_count == 1);
	bool d2_singleton = (arg_d2_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rf): function rf() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!d1_singleton && (arg_d1_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rf): function rf() requires d1 to be of length 1 or n." << EidosTerminate(nullptr);
	if (!d2_singleton && (arg_d2_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rf): function rf() requires d2 to be of length 1 or n." << EidosTerminate(nullptr);
	
	double d1_0 = (arg_d1_count ? arg_d1->FloatAtIndex(0, nullptr) : 0.0);
	double d2_0 = (arg_d2_count ? arg_d2->FloatAtIndex(0, nullptr) : 0.0);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (d1_singleton && d2_singleton)
	{
		if (d1_0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rf): function rf() requires d1 > 0.0 (" << EidosStringForFloat(d1_0) << " supplied)." << EidosTerminate(nullptr);
		if (d2_0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rf): function rf() requires d2 > 0.0 (" << EidosStringForFloat(d2_0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_fdist(rng, d1_0, d2_0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_fdist(rng, d1_0, d2_0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double d1 = (d1_singleton ? d1_0 : arg_d1->FloatAtIndex(draw_index, nullptr));
			double d2 = (d2_singleton ? d2_0 : arg_d2->FloatAtIndex(draw_index, nullptr));
			
			if (d1 <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rf): function rf() requires d1 > 0.0 (" << EidosStringForFloat(d1) << " supplied)." << EidosTerminate(nullptr);
			if (d2 <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rf): function rf() requires d2 > 0.0 (" << EidosStringForFloat(d2) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_fdist(rng, d1, d2), draw_index);
		}
	}
	
	return result_SP;
}

//	(float)dgamma(float x, numeric mean, numeric shape)
EidosValue_SP Eidos_ExecuteFunction_dgamma(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_quantile = p_arguments[0].get();
	EidosValue *arg_mean = p_arguments[1].get();
	EidosValue *arg_shape = p_arguments[2].get();
	int num_quantiles = arg_quantile->Count();
	int arg_mean_count = arg_mean->Count();
	int arg_shape_count = arg_shape->Count();
	bool mean_singleton = (arg_mean_count == 1);
	bool shape_singleton = (arg_shape_count == 1);
	
	if (!mean_singleton && (arg_mean_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires mean to be of length 1 or n." << EidosTerminate(nullptr);
	if (!shape_singleton && (arg_shape_count != num_quantiles))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires shape to be of length 1 or n." << EidosTerminate(nullptr);
	
	double mean0 = (arg_mean_count ? arg_mean->FloatAtIndex(0, nullptr) : 1.0);
	double shape0 = (arg_shape_count ? arg_shape->FloatAtIndex(0, nullptr) : 0.0);
	
	if (mean_singleton && shape_singleton)
	{
		if (!(shape0 > 0.0))	// true for NaN
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires shape > 0.0 (" << EidosStringForFloat(shape0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_quantiles == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gamma_pdf(arg_quantile->FloatAtIndex(0, nullptr), shape0, mean0/shape0)));
		}
		else
		{
			const double *float_data = arg_quantile->FloatVector()->data();
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
			result_SP = EidosValue_SP(float_result);
			
			double scale = mean0 / shape0;
			
			for (int64_t value_index = 0; value_index < num_quantiles; ++value_index)
				float_result->set_float_no_check(gsl_ran_gamma_pdf(float_data[value_index], shape0, scale), value_index);
		}
	}
	else
	{
		const double *float_data = arg_quantile->FloatVector()->data();
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_quantiles);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < num_quantiles; ++value_index)
		{
			double mean = (mean_singleton ? mean0 : arg_mean->FloatAtIndex(value_index, nullptr));
			double shape = (shape_singleton ? shape0 : arg_shape->FloatAtIndex(value_index, nullptr));
			
			if (!(shape > 0.0))		// true for NaN
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_dgamma): function dgamma() requires shape > 0.0 (" << EidosStringForFloat(shape) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_gamma_pdf(float_data[value_index], shape, mean / shape), value_index);
		}
	}
	
	return result_SP;
}

//	(float)rgamma(integer$ n, numeric mean, numeric shape)
EidosValue_SP Eidos_ExecuteFunction_rgamma(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_mean = p_arguments[1].get();
	EidosValue *arg_shape = p_arguments[2].get();
	int arg_mean_count = arg_mean->Count();
	int arg_shape_count = arg_shape->Count();
	bool mean_singleton = (arg_mean_count == 1);
	bool shape_singleton = (arg_shape_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!mean_singleton && (arg_mean_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires mean to be of length 1 or n." << EidosTerminate(nullptr);
	if (!shape_singleton && (arg_shape_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape to be of length 1 or n." << EidosTerminate(nullptr);
	
	double mean0 = (arg_mean_count ? arg_mean->FloatAtIndex(0, nullptr) : 1.0);
	double shape0 = (arg_shape_count ? arg_shape->FloatAtIndex(0, nullptr) : 0.0);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (mean_singleton && shape_singleton)
	{
		if (shape0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape > 0.0 (" << EidosStringForFloat(shape0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gamma(rng, shape0, mean0/shape0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			double scale = mean0 / shape0;
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_gamma(rng, shape0, scale), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double mean = (mean_singleton ? mean0 : arg_mean->FloatAtIndex(draw_index, nullptr));
			double shape = (shape_singleton ? shape0 : arg_shape->FloatAtIndex(draw_index, nullptr));
			
			if (shape <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgamma): function rgamma() requires shape > 0.0 (" << EidosStringForFloat(shape) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_gamma(rng, shape, mean / shape), draw_index);
		}
	}
	
	return result_SP;
}

//	(integer)rgeom(integer$ n, float p)
EidosValue_SP Eidos_ExecuteFunction_rgeom(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_p = p_arguments[1].get();
	int arg_p_count = arg_p->Count();
	bool p_singleton = (arg_p_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!p_singleton && (arg_p_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires p to be of length 1 or n." << EidosTerminate(nullptr);
	
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	// Note that there are two different definitions of the geometric distribution (see https://en.wikipedia.org/wiki/Geometric_distribution).
	// We follow R in using the definition that is supported on the set {0, 1, 2, 3, ...}.  Unfortunately, gsl_ran_geometric() uses the other
	// definition, which is supported on the set {1, 2, 3, ...}.  The GSL's version does not allow p==1.0, so we have to special-case that.
	// Otherwise, the result of our definition is equal to the result from the GSL's definition minus 1; the GSL uses the "shifted geometric".
	
	if (p_singleton)
	{
		double p0 = arg_p->FloatAtIndex(0, nullptr);
		
		if ((p0 <= 0.0) || (p0 > 1.0) || std::isnan(p0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires 0.0 < p <= 1.0 (" << EidosStringForFloat(p0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			if (p0 == 1.0)	// special-case p==1.0
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_geometric(rng, p0) - 1));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			if (p0 == 1.0)	// special-case p==1.0
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(0, draw_index);
			else
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(gsl_ran_geometric(rng, p0) - 1, draw_index);
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double p = arg_p->FloatAtIndex(draw_index, nullptr);
			
			if ((p <= 0.0) || (p >= 1.0) || std::isnan(p))
			{
				if (p == 1.0)	// special-case p==1.0; inside here to avoid an extra comparison per loop in the standard case
				{
					int_result->set_int_no_check(0, draw_index);
					continue;
				}
				
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rgeom): function rgeom() requires 0.0 < p <= 1.0 (" << EidosStringForFloat(p) << " supplied)." << EidosTerminate(nullptr);
			}
			
			int_result->set_int_no_check(gsl_ran_geometric(rng, p) - 1, draw_index);
		}
	}
	
	return result_SP;
}

//	(float)rlnorm(integer$ n, [numeric meanlog = 0], [numeric sdlog = 1])
EidosValue_SP Eidos_ExecuteFunction_rlnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_meanlog = p_arguments[1].get();
	EidosValue *arg_sdlog = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_meanlog_count = arg_meanlog->Count();
	int arg_sdlog_count = arg_sdlog->Count();
	bool meanlog_singleton = (arg_meanlog_count == 1);
	bool sdlog_singleton = (arg_sdlog_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!meanlog_singleton && (arg_meanlog_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires meanlog to be of length 1 or n." << EidosTerminate(nullptr);
	if (!sdlog_singleton && (arg_sdlog_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rlnorm): function rlnorm() requires sdlog to be of length 1 or n." << EidosTerminate(nullptr);
	
	double meanlog0 = (arg_meanlog_count ? arg_meanlog->FloatAtIndex(0, nullptr) : 0.0);
	double sdlog0 = (arg_sdlog_count ? arg_sdlog->FloatAtIndex(0, nullptr) : 1.0);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (meanlog_singleton && sdlog_singleton)
	{
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_lognormal(rng, meanlog0, sdlog0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_lognormal(rng, meanlog0, sdlog0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double meanlog = (meanlog_singleton ? meanlog0 : arg_meanlog->FloatAtIndex(draw_index, nullptr));
			double sdlog = (sdlog_singleton ? sdlog0 : arg_sdlog->FloatAtIndex(draw_index, nullptr));
			
			float_result->set_float_no_check(gsl_ran_lognormal(rng, meanlog, sdlog), draw_index);
		}
	}
	
	return result_SP;
}

// (float)rmvnorm(integer$ n, numeric mu, numeric sigma)
EidosValue_SP Eidos_ExecuteFunction_rmvnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *arg_n = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_draws = arg_n->IntAtIndex(0, nullptr);
	int mu_count = arg_mu->Count();
	int mu_dimcount = arg_mu->DimensionCount();
	int sigma_dimcount = arg_sigma->DimensionCount();
	const int64_t *sigma_dims = arg_sigma->Dimensions();
	int d = mu_count;
	
	if (num_draws < 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires n to be greater than or equal to 1 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	
	if ((mu_dimcount != 1) || (mu_count < 2))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires mu to be a plain vector of length k, where k is the number of dimensions for the multivariate Gaussian function (k must be >= 2)." << EidosTerminate(nullptr);
	if (sigma_dimcount != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires sigma to be a matrix." << EidosTerminate(nullptr);
	if ((sigma_dims[0] != d) || (sigma_dims[1] != d))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires sigma to be a k x k matrix, where k is the number of dimensions for the multivariate Gaussian function (k must be >= 2)." << EidosTerminate(nullptr);
	
	for (int row_index = 0; row_index < d; ++row_index)
		for (int col_index = 0; col_index < d; ++col_index)
			if (std::isnan(arg_sigma->FloatAtIndex(row_index + col_index * d, nullptr)))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() does not allow sigma to contain NANs." << EidosTerminate(nullptr);	// oddly, GSL does not raise an error on this!
	
	// Set up the GSL vectors
	gsl_vector *gsl_mu = gsl_vector_calloc(d);
	gsl_matrix *gsl_Sigma = gsl_matrix_calloc(d, d);
	gsl_matrix *gsl_L = gsl_matrix_calloc(d, d);
	gsl_vector *gsl_result = gsl_vector_calloc(d);
	
	if (!gsl_mu || !gsl_Sigma || !gsl_L || !gsl_result)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	for (int dim_index = 0; dim_index < d; ++dim_index)
		gsl_vector_set(gsl_mu, dim_index, arg_mu->FloatAtIndex(dim_index, nullptr));
	
	for (int row_index = 0; row_index < d; ++row_index)
		for (int col_index = 0; col_index < d; ++col_index)
			gsl_matrix_set(gsl_Sigma, row_index, col_index, arg_sigma->FloatAtIndex(row_index + col_index * d, nullptr));
	
	gsl_matrix_memcpy(gsl_L, gsl_Sigma);
	
	// Disable the GSL's default error handler, which calls abort().  Normally we run with that handler,
	// which is perhaps a bit risky, but we want to check for all error cases and avert them before we
	// ever call into the GSL; having the GSL raise when it encounters an error condition is kind of OK
	// because it should never ever happen.  But the GSL calls we will make in this function could hit
	// errors unpredictably, if for example it turns out that Sigma is not positive-definite.  So for
	// this stretch of code we disable the default handler and check for errors returned from the GSL.
	gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
	int gsl_err;
	
	// Do the draws, which involves a preliminary step of doing a Cholesky decomposition
	gsl_err = gsl_linalg_cholesky_decomp1(gsl_L);
	
	if (gsl_err)
	{
		gsl_set_error_handler(old_handler);
		
		// Clean up GSL stuff
		gsl_vector_free(gsl_mu);
		gsl_matrix_free(gsl_Sigma);
		gsl_matrix_free(gsl_L);
		gsl_vector_free(gsl_result);
		
		if (gsl_err == GSL_EDOM)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): function rmvnorm() requires that sigma, the variance-covariance matrix, be positive-definite." << EidosTerminate(nullptr);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_linalg_cholesky_decomp1() function." << EidosTerminate(nullptr);
	}
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws * d);
	result_SP = EidosValue_SP(float_result);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
	{
		gsl_err = gsl_ran_multivariate_gaussian(rng, gsl_mu, gsl_L, gsl_result);
		
		if (gsl_err)
		{
			gsl_set_error_handler(old_handler);
			
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rmvnorm): (internal error) an unknown error with code " << gsl_err << " occurred inside the GNU Scientific Library's gsl_ran_multivariate_gaussian() function." << EidosTerminate(nullptr);
		}
		
		for (int dim_index = 0; dim_index < d; ++dim_index)
			float_result->set_float_no_check(gsl_vector_get(gsl_result, dim_index), draw_index + dim_index * num_draws);
	}
	
	// Clean up GSL stuff
	gsl_vector_free(gsl_mu);
	gsl_matrix_free(gsl_Sigma);
	gsl_matrix_free(gsl_L);
	gsl_vector_free(gsl_result);
	
	gsl_set_error_handler(old_handler);
	
	// Set the dimensions of the result; we want one row per draw
	int64_t dim[2] = {num_draws, d};
	
	float_result->SetDimensions(2, dim);
	
	return result_SP;
}

//	(integer)rnbinom(integer$ n, numeric size, float prob)
EidosValue_SP Eidos_ExecuteFunction_rnbinom(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_size = p_arguments[1].get();
	EidosValue *arg_prob = p_arguments[2].get();
	int arg_size_count = arg_size->Count();
	int arg_prob_count = arg_prob->Count();
	bool size_singleton = (arg_size_count == 1);
	bool prob_singleton = (arg_prob_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnbinom): function rnbinom() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!size_singleton && (arg_size_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnbinom): function rnbinom() requires size to be of length 1 or n." << EidosTerminate(nullptr);
	if (!prob_singleton && (arg_prob_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnbinom): function rnbinom() requires prob to be of length 1 or n." << EidosTerminate(nullptr);
	
	double size0 = arg_size->FloatAtIndex(0, nullptr);
	double probability0 = arg_prob->FloatAtIndex(0, nullptr);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (size_singleton && prob_singleton)
	{
		if ((size0 < 0) || std::isnan(size0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnbinom): function rnbinom() requires size >= 0 (" << size0 << " supplied)." << EidosTerminate(nullptr);
		if ((probability0 <= 0.0) || (probability0 > 1.0) || std::isnan(probability0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnbinom): function rnbinom() requires probability in (0.0, 1.0] (" << EidosStringForFloat(probability0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_negative_binomial(rng, probability0, size0)));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				int_result->set_int_no_check(gsl_ran_negative_binomial(rng, probability0, size0), draw_index);
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double size = (size_singleton ? size0 : arg_size->FloatAtIndex(draw_index, nullptr));
			double probability = (prob_singleton ? probability0 : arg_prob->FloatAtIndex(draw_index, nullptr));
			
			if ((size < 0) || std::isnan(size))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnbinom): function rnbinom() requires size >= 0 (" << size << " supplied)." << EidosTerminate(nullptr);
			if ((probability <= 0.0) || (probability > 1.0) || std::isnan(probability))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnbinom): function rnbinom() requires probability in (0.0, 1.0] (" << EidosStringForFloat(probability) << " supplied)." << EidosTerminate(nullptr);
			
			int_result->set_int_no_check(gsl_ran_negative_binomial(rng, probability, size), draw_index);
		}
	}
	
	return result_SP;
}

//	(float)rnorm(integer$ n, [numeric mean = 0], [numeric sd = 1])
EidosValue_SP Eidos_ExecuteFunction_rnorm(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_mu = p_arguments[1].get();
	EidosValue *arg_sigma = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_mu_count = arg_mu->Count();
	int arg_sigma_count = arg_sigma->Count();
	bool mu_singleton = (arg_mu_count == 1);
	bool sigma_singleton = (arg_sigma_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!mu_singleton && (arg_mu_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires mean to be of length 1 or n." << EidosTerminate(nullptr);
	if (!sigma_singleton && (arg_sigma_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd to be of length 1 or n." << EidosTerminate(nullptr);
	
	double mu0 = (arg_mu_count ? arg_mu->FloatAtIndex(0, nullptr) : 0.0);
	double sigma0 = (arg_sigma_count ? arg_sigma->FloatAtIndex(0, nullptr) : 1.0);
	
	if (sigma_singleton && (sigma0 < 0.0))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd >= 0.0 (" << EidosStringForFloat(sigma0) << " supplied)." << EidosTerminate(nullptr);
	
	if (num_draws == 1)
	{
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_gaussian(rng, sigma0) + mu0));
	}
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
	result_SP = EidosValue_SP(float_result);
	
	if (mu_singleton && sigma_singleton)
	{
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_RNORM_1);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(float_result, sigma0, mu0) if(num_draws >= EIDOS_OMPMIN_RNORM_1) num_threads(thread_count)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
#pragma omp for schedule(static) nowait
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_gaussian(rng, sigma0) + mu0, draw_index);
		}
	}
	else if (sigma_singleton)	// && !mu_singleton
	{
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_RNORM_2);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(float_result, sigma0, arg_mu) if(num_draws >= EIDOS_OMPMIN_RNORM_2) num_threads(thread_count)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
#pragma omp for schedule(static) nowait
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				double mu = arg_mu->FloatAtIndex((int)draw_index, nullptr);
				
				float_result->set_float_no_check(gsl_ran_gaussian(rng, sigma0) + mu, draw_index);
			}
		}
	}
	else	// !sigma_singleton
	{
		bool saw_error = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_RNORM_3);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(float_result, mu_singleton, mu0, arg_mu, arg_sigma) reduction(||: saw_error) if(num_draws >= EIDOS_OMPMIN_RNORM_3) num_threads(thread_count)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
#pragma omp for schedule(static) nowait
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				double mu = (mu_singleton ? mu0 : arg_mu->FloatAtIndex((int)draw_index, nullptr));
				double sigma = arg_sigma->FloatAtIndex((int)draw_index, nullptr);
				
				if (sigma < 0.0)
				{
					saw_error = true;
					continue;
				}
				
				float_result->set_float_no_check(gsl_ran_gaussian(rng, sigma) + mu, draw_index);
			}
		}
		
		if (saw_error)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rnorm): function rnorm() requires sd >= 0.0." << EidosTerminate(nullptr);
	}
	
	return result_SP;
}

//	(integer)rpois(integer$ n, numeric lambda)
EidosValue_SP Eidos_ExecuteFunction_rpois(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_lambda = p_arguments[1].get();
	int arg_lambda_count = arg_lambda->Count();
	bool lambda_singleton = (arg_lambda_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!lambda_singleton && (arg_lambda_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda to be of length 1 or n." << EidosTerminate(nullptr);
	
	// Here we ignore USE_GSL_POISSON and always use the GSL.  This is because we don't know whether lambda (otherwise known as mu) is
	// small or large, and because we don't know what level of accuracy is demanded by whatever the user is doing with the deviates,
	// and so forth; it makes sense to just rely on the GSL for maximal accuracy and reliability.
	
	if (lambda_singleton)
	{
		double lambda0 = arg_lambda->FloatAtIndex(0, nullptr);
		
		if ((lambda0 <= 0.0) || std::isnan(lambda0))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda > 0.0 (" << EidosStringForFloat(lambda0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gsl_ran_poisson(rng, lambda0)));
		}
		else
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(int_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_RPOIS_1);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result, lambda0) if(num_draws >= EIDOS_OMPMIN_RPOIS_1) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
#pragma omp for schedule(static) nowait
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					int_result->set_int_no_check(gsl_ran_poisson(rng, lambda0), draw_index);
			}
		}
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(int_result);
		
		bool saw_error = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_RPOIS_2);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(int_result, arg_lambda) reduction(||: saw_error) if(num_draws >= EIDOS_OMPMIN_RPOIS_2) num_threads(thread_count)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
#pragma omp for schedule(dynamic, 1024) nowait
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			{
				double lambda = arg_lambda->FloatAtIndex((int)draw_index, nullptr);
				
				if ((lambda <= 0.0) || std::isnan(lambda))
				{
					saw_error = true;
					continue;
				}
				
				int_result->set_int_no_check(gsl_ran_poisson(rng, lambda), draw_index);
			}
		}
		
		if (saw_error)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rpois): function rpois() requires lambda > 0.0." << EidosTerminate(nullptr);
	}
	
	return result_SP;
}

//	(float)runif(integer$ n, [numeric min = 0], [numeric max = 1])
EidosValue_SP Eidos_ExecuteFunction_runif(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	EidosValue *arg_min = p_arguments[1].get();
	EidosValue *arg_max = p_arguments[2].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	int arg_min_count = arg_min->Count();
	int arg_max_count = arg_max->Count();
	bool min_singleton = (arg_min_count == 1);
	bool max_singleton = (arg_max_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!min_singleton && (arg_min_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min to be of length 1 or n." << EidosTerminate(nullptr);
	if (!max_singleton && (arg_max_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires max to be of length 1 or n." << EidosTerminate(nullptr);
	
	double min_value0 = (arg_min_count ? arg_min->FloatAtIndex(0, nullptr) : 0.0);
	double max_value0 = (arg_max_count ? arg_max->FloatAtIndex(0, nullptr) : 1.0);
	
	if (min_singleton && max_singleton && (min_value0 == 0.0) && (max_value0 == 1.0))
	{
		// With the default min and max, we can streamline quite a bit
		if (num_draws == 1)
		{
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(Eidos_rng_uniform(rng)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_RUNIF_1);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws, std::cout) firstprivate(float_result) if(num_draws >= EIDOS_OMPMIN_RUNIF_1) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
#pragma omp for schedule(static) nowait
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
					float_result->set_float_no_check(Eidos_rng_uniform(rng), draw_index);
			}
		}
	}
	else
	{
		double range0 = max_value0 - min_value0;
		
		if (min_singleton && max_singleton)
		{
			if (range0 < 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min < max." << EidosTerminate(nullptr);
			
			if (num_draws == 1)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(Eidos_rng_uniform(rng) * range0 + min_value0));
			}
			else
			{
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
				result_SP = EidosValue_SP(float_result);
				
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_RUNIF_2);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(float_result, range0, min_value0) if(num_draws >= EIDOS_OMPMIN_RUNIF_2) num_threads(thread_count)
				{
					gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
					
#pragma omp for schedule(static) nowait
					for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
						float_result->set_float_no_check(Eidos_rng_uniform(rng) * range0 + min_value0, draw_index);
				}
			}
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
			result_SP = EidosValue_SP(float_result);
			
			bool saw_error = false;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_RUNIF_3);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, num_draws) firstprivate(float_result, min_singleton, max_singleton, min_value0, max_value0, arg_min, arg_max) reduction(||: saw_error) if(num_draws >= EIDOS_OMPMIN_RUNIF_3) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
#pragma omp for schedule(static) nowait
				for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				{
					double min_value = (min_singleton ? min_value0 : arg_min->FloatAtIndex((int)draw_index, nullptr));
					double max_value = (max_singleton ? max_value0 : arg_max->FloatAtIndex((int)draw_index, nullptr));
					double range = max_value - min_value;
					
					if (range < 0.0)
					{
						saw_error = true;
						continue;
					}
					
					float_result->set_float_no_check(Eidos_rng_uniform(rng) * range + min_value, draw_index);
				}
			}
			
			if (saw_error)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_runif): function runif() requires min < max." << EidosTerminate(nullptr);
		}
	}
	
	return result_SP;
}

//	(float)rweibull(integer$ n, numeric lambda, numeric k)
EidosValue_SP Eidos_ExecuteFunction_rweibull(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	EidosValue *arg_lambda = p_arguments[1].get();
	EidosValue *arg_k = p_arguments[2].get();
	int arg_lambda_count = arg_lambda->Count();
	int arg_k_count = arg_k->Count();
	bool lambda_singleton = (arg_lambda_count == 1);
	bool k_singleton = (arg_k_count == 1);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	if (!lambda_singleton && (arg_lambda_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda to be of length 1 or n." << EidosTerminate(nullptr);
	if (!k_singleton && (arg_k_count != num_draws))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k to be of length 1 or n." << EidosTerminate(nullptr);
	
	double lambda0 = (arg_lambda_count ? arg_lambda->FloatAtIndex(0, nullptr) : 0.0);
	double k0 = (arg_k_count ? arg_k->FloatAtIndex(0, nullptr) : 0.0);
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (lambda_singleton && k_singleton)
	{
		if (lambda0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda > 0.0 (" << EidosStringForFloat(lambda0) << " supplied)." << EidosTerminate(nullptr);
		if (k0 <= 0.0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k > 0.0 (" << EidosStringForFloat(k0) << " supplied)." << EidosTerminate(nullptr);
		
		if (num_draws == 1)
		{
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gsl_ran_weibull(rng, lambda0, k0)));
		}
		else
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
			result_SP = EidosValue_SP(float_result);
			
			for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
				float_result->set_float_no_check(gsl_ran_weibull(rng, lambda0, k0), draw_index);
		}
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize((int)num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int draw_index = 0; draw_index < num_draws; ++draw_index)
		{
			double lambda = (lambda_singleton ? lambda0 : arg_lambda->FloatAtIndex(draw_index, nullptr));
			double k = (k_singleton ? k0 : arg_k->FloatAtIndex(draw_index, nullptr));
			
			if (lambda <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires lambda > 0.0 (" << EidosStringForFloat(lambda) << " supplied)." << EidosTerminate(nullptr);
			if (k <= 0.0)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rweibull): function rweibull() requires k > 0.0 (" << EidosStringForFloat(k) << " supplied)." << EidosTerminate(nullptr);
			
			float_result->set_float_no_check(gsl_ran_weibull(rng, lambda, k), draw_index);
		}
	}
	
	return result_SP;
}









































