//
//  eidos_functions_matrices.cpp
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

#include <utility>
#include <vector>
#include <algorithm>


// ************************************************************************************
//
//	matrix and array functions
//

#pragma mark -
#pragma mark Matrix and array functions
#pragma mark -


//	(*)apply(* x, integer margin, string$ lambdaSource)
EidosValue_SP Eidos_ExecuteFunction_apply(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_dimcount = x_value->DimensionCount();
	const int64_t *x_dim = x_value->Dimensions();
	
	if (x_dimcount < 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): function apply() requires parameter x to be a matrix or array." << std::endl << "NOTE: The apply() function was renamed sapply() in Eidos 1.6, and a new function named apply() has been added; you may need to change this call to be a call to sapply() instead." << EidosTerminate(nullptr);
	
	// Determine the margins requested and check their validity
	EidosValue *margin_value = p_arguments[1].get();
	int margin_count = margin_value->Count();
	std::vector<int> margins;
	std::vector<int64_t> margin_sizes;
	
	if (margin_count <= 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): function apply() requires that margins be specified." << EidosTerminate(nullptr);
	
	for (int margin_index = 0; margin_index < margin_count; ++margin_index)
	{
		int64_t margin = margin_value->IntAtIndex(margin_index, nullptr);
		
		if ((margin < 0) || (margin >= x_dimcount))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): specified margin " << margin << " is out of range in function apply(); margin indices are zero-based, and thus must be from 0 to size(dim(x)) - 1." << EidosTerminate(nullptr);
		
		for (int margin_index_2 = 0; margin_index_2 < margin_index; ++margin_index_2)
		{
			int64_t margin_2 = margin_value->IntAtIndex(margin_index_2, nullptr);
			
			if (margin_2 == margin)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): specified margin " << margin << " was already specified to function apply(); a given margin may be specified only once." << EidosTerminate(nullptr);
		}
		
		margins.emplace_back((int)margin);
		margin_sizes.emplace_back(x_dim[margin]);
	}
	
	// Get the lambda string and cache its script
	EidosValue *lambda_value = p_arguments[2].get();
	EidosValue_String_singleton *lambda_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[2].get());
	EidosScript *script = (lambda_value_singleton ? lambda_value_singleton->CachedScript() : nullptr);
	
	// Errors in lambdas should be reported for the lambda script, not for the calling script,
	// if possible.  In the GUI this does not work well, however; there, errors should be
	// reported as occurring in the call to sapply().  Here we save off the current
	// error context and set up the error context for reporting errors inside the lambda,
	// in case that is possible; see how exceptions are handled below.
	EidosErrorContext error_context_save = gEidosErrorContext;
	
	// We try to do tokenization and parsing once per script, by caching the script inside the EidosValue_String_singleton instance
	if (!script)
	{
		script = new EidosScript(lambda_value->StringAtIndex(0, nullptr), -1);
		
		gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script, true};
		
		try
		{
			script->Tokenize();
			script->ParseInterpreterBlockToAST(false);
		}
		catch (...)
		{
			if (gEidosTerminateThrows)
				gEidosErrorContext = error_context_save;
			
			delete script;
			
			throw;
		}
		
		if (lambda_value_singleton)
			lambda_value_singleton->SetCachedScript(script);
	}
	
	std::vector<EidosValue_SP> results;
	
	gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script, true};
	
	try
	{
		EidosSymbolTable &symbols = p_interpreter.SymbolTable();									// use our own symbol table
		EidosFunctionMap &function_map = p_interpreter.FunctionMap();								// use our own function map
		EidosInterpreter interpreter(*script, symbols, function_map, p_interpreter.Context(), p_interpreter.ExecutionOutputStream(), p_interpreter.ErrorOutputStream());
		bool consistent_return_length = true;	// consistent across all values, including NULLs?
		int return_length = -1;					// what the consistent length is
		
		// Set up inclusion_indices and inclusion_counts vectors as a skeleton for each marginal subset below
		std::vector<std::vector<int64_t>> inclusion_indices;	// the chosen indices for each dimension
		std::vector<int> inclusion_counts;						// the number of chosen indices for each dimension
		
		inclusion_indices.reserve(x_dimcount);
		inclusion_counts.reserve(x_dimcount);
		
		for (int subset_index = 0; subset_index < x_dimcount; ++subset_index)
		{
			int dim_size = (int)x_dim[subset_index];
			std::vector<int64_t> indices;
			
			indices.reserve(dim_size);
			
			for (int dim_index = 0; dim_index < dim_size; ++dim_index)
				indices.emplace_back(dim_index);
			
			inclusion_counts.emplace_back((int)indices.size());
			inclusion_indices.emplace_back(indices);
		}
		
		for (int margin_index = 0; margin_index < margin_count; ++margin_index)
			inclusion_counts[margins[margin_index]] = 1;
		
		// Iterate through each index for the marginal dimensions, in order
		std::vector<int64_t> margin_counter(margin_count, 0);
		
		do
		{
			// margin_counter has values for each margin; generate a slice through x with them
			for (int margin_index = 0; margin_index < margin_count; ++margin_index)
			{
				int margin_dim = margins[margin_index];
				
				inclusion_indices[margin_dim].clear();
				inclusion_indices[margin_dim].emplace_back(margin_counter[margin_index]);
			}
			
			EidosValue_SP apply_value = x_value->Subset(inclusion_indices, true, nullptr);
			
			// Set the iterator variable "applyValue" to the value
			symbols.SetValueForSymbolNoCopy(gEidosID_applyValue, std::move(apply_value));
			
			// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
			// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
			// bugs that would otherwise not manifest.
			EidosValue_SP &&return_value_SP = interpreter.EvaluateInterpreterBlock(false, true);		// do not print output, return the last statement value
			
			if (return_value_SP->Type() == EidosValueType::kValueVOID)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): each iteration within apply() must return a non-void value." << EidosTerminate(nullptr);
			
			if (consistent_return_length)
			{
				int length = return_value_SP->Count();
				
				if (return_length == -1)
					return_length = length;
				else if (length != return_length)
					consistent_return_length = false;
			}
			
			results.emplace_back(return_value_SP);
			
			// increment margin_counter in the base system of margin_sizes
			int margin_counter_index = 0;
			
			do
			{
				if (++margin_counter[margin_counter_index] == margin_sizes[margin_counter_index])
				{
					margin_counter[margin_counter_index] = 0;
					margin_counter_index++;		// carry
				}
				else
					break;
			}
			while (margin_counter_index < margin_count);
			
			// if we carried out off the top, we are done iterating across all margins
			if (margin_counter_index == margin_count)
				break;
		}
		while (true);
		
		// We do not want a leftover applyValue symbol in the symbol table, so we remove it now
		symbols.RemoveValueForSymbol(gEidosID_applyValue);
		
		// Assemble all the individual results together, just as c() does
		result_SP = ConcatenateEidosValues(results, true, false);	// allow NULL but not VOID
		
		// Set the dimensions of the result.  If the returns from the lambda were not consistent in their
		// length and dimensions, we just return the plain vector without dimensions; we can't do anything
		// with it.  With returns of a consistent length n, (1) if n == 1, the return value will be a vector
		// if only one margin was used, or a matrix/array of dimension dim(x)[margin] if more than one
		// margin was used, (2) if n > 1, the return value will be an array of dimension c(n, dim(x)[margin]),
		// or (3) if n == 0, the return value will be a length zero vector of the appropriate type.  I think
		// we follow R's policy more or less exactly; that is my intent.
		if (consistent_return_length && (return_length > 0))
		{
			if (return_length == 1)
			{
				if (margin_count == 1)
				{
					// do nothing and return a plain vector
				}
				else
				{
					// dimensions of dim(x)[margin]; in other words, the sizes of the marginal dimensions of x, in the specified order of the margins
					result_SP->SetDimensions(margin_count, margin_sizes.data());
				}
			}
			else // return_length > 1)
			{
				// dimensions of c(n, dim(x)[margin]); in other words, n rows, and the marginal dim sizes after that
				int64_t *dims = (int64_t *)malloc((margin_count + 1) * sizeof(int64_t));
				if (!dims)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_apply): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				
				dims[0] = return_length;
				
				for (int dim_index = 0; dim_index < margin_count; ++dim_index)
					dims[dim_index + 1] = margin_sizes[dim_index];
				
				result_SP->SetDimensions(margin_count + 1, dims);
				
				free(dims);
			}
		}
	}
	catch (...)
	{
		// If exceptions throw, then we want to set up the error information to highlight the
		// sapply() that failed, since we can't highlight the actual error.  (If exceptions
		// don't throw, this catch block will never be hit; exit() will already have been called
		// and the error will have been reported from the context of the lambda script string.)
		if (gEidosTerminateThrows)
			gEidosErrorContext = error_context_save;
		
		if (!lambda_value_singleton)
			delete script;
		
		throw;
	}
	
	// Restore the normal error context in the event that no exception occurring within the lambda
	gEidosErrorContext = error_context_save;
	
	if (!lambda_value_singleton)
		delete script;
	
	return result_SP;
}

// (*)array(* data, integer dim)
EidosValue_SP Eidos_ExecuteFunction_array(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *data_value = p_arguments[0].get();
	EidosValue *dim_value = p_arguments[1].get();
	
	int data_count = data_value->Count();
	int dim_count = dim_value->Count();
	
	if (dim_count < 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_array): function array() requires at least two dimensions (i.e., at least a matrix)" << EidosTerminate(nullptr);
	
	int64_t dim_product = 1;
	
	for (int dim_index = 0; dim_index < dim_count; ++dim_index)
	{
		int64_t dim = dim_value->IntAtIndex(dim_index, nullptr);
		
		if (dim < 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_array): function array() requires that all dimensions be >= 1." << EidosTerminate(nullptr);
		
		dim_product *= dim;
	}
	
	if (data_count != dim_product)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_array): function array() requires a data vector with a length equal to the product of the proposed dimensions." << EidosTerminate(nullptr);
	
	// construct the array from the data and dimensions
	result_SP = data_value->CopyValues();
	
	result_SP->SetDimensions(dim_count, dim_value->IntVector()->data());
	
	return result_SP;
}

// (*)cbind(...)
EidosValue_SP Eidos_ExecuteFunction_cbind(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// First check the type and class of the result; NULL may be mixed in, but otherwise all arguments must be the same type and class
	EidosValueType result_type = EidosValueType::kValueNULL;
	const EidosClass *result_class = gEidosObject_Class;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		EidosValueType arg_type = arg->Type();
		
		if (arg_type == EidosValueType::kValueNULL)
			continue;
		else if (result_type == EidosValueType::kValueNULL)
			result_type = arg_type;
		else if (arg_type != result_type)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() requires that all arguments be the same type (or NULL)." << EidosTerminate(nullptr);
		
		if (arg_type == EidosValueType::kValueObject)
		{
			EidosValue_Object *arg_object = (EidosValue_Object *)arg;
			const EidosClass *arg_class = arg_object->Class();
			
			if (arg_class == gEidosObject_Class)
				continue;
			else if (result_class == gEidosObject_Class)
				result_class = arg_class;
			else if (arg_class != result_class)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() requires that all object arguments be of the same class." << EidosTerminate(nullptr);
		}
	}
	
	if (result_type == EidosValueType::kValueNULL)
		return gStaticEidosValueNULL;
	
	// Next determine the dimensions of the result; every argument must be zero-size or a match for the dimensions we already have
	int64_t result_rows = 0;
	int64_t result_cols = 0;
	int64_t result_length = 0;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		int64_t arg_length = arg->Count();
		
		// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
		if (arg_length == 0)
			continue;
		
		int arg_dimcount = arg->DimensionCount();
		
		if ((arg_dimcount != 1) && (arg_dimcount != 2))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() requires that all arguments be vectors or matrices." << EidosTerminate(nullptr);
		
		const int64_t *arg_dims = arg->Dimensions();
		int64_t arg_nrow = (arg_dimcount == 1) ? arg_length : arg_dims[0];
		int64_t arg_ncol = (arg_dimcount == 1) ? 1 : arg_dims[1];
		
		if (result_rows == 0)
			result_rows = arg_nrow;
		else if (result_rows != arg_nrow)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_cbind): function cbind() mismatch among arguments in their number of rows." << EidosTerminate(nullptr);
		
		result_cols += arg_ncol;
		result_length += arg_length;
	}
	
	// Construct our result; this is simpler than rbind(), here we basically just concatenate the arguments directly...
	EidosValue_SP result_SP(nullptr);
	
	switch (result_type)
	{
		case EidosValueType::kValueVOID:	break;		// never hit	// NOLINT(*-branch-clone) : intentional consecutive branches
		case EidosValueType::kValueNULL:	break;		// never hit
		case EidosValueType::kValueLogical:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->reserve(result_length)); break;
		case EidosValueType::kValueInt:		result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve(result_length)); break;
		case EidosValueType::kValueFloat:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(result_length)); break;
		case EidosValueType::kValueString:	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector()); break;
		case EidosValueType::kValueObject:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(result_class))->reserve(result_length)); break;
	}
	
	EidosValue *result = result_SP.get();
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		int64_t arg_length = arg->Count();
		
		// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
		if (arg_length == 0)
			continue;
		
		for (int element_index = 0; element_index < arg_length; ++element_index)
			result->PushValueFromIndexOfEidosValue(element_index, *arg, nullptr);
	}
	
	const int64_t dim_buf[2] = {result_rows, result_cols};
	
	result->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (integer)dim(* x)
EidosValue_SP Eidos_ExecuteFunction_dim(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *data_value = p_arguments[0].get();
	int dim_count = data_value->DimensionCount();
	
	if (dim_count <= 1)
		return gStaticEidosValueNULL;
	
	const int64_t *dim_values = data_value->Dimensions();
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(dim_count);
	result_SP = EidosValue_SP(int_result);
	
	for (int dim_index = 0; dim_index < dim_count; ++dim_index)
		int_result->set_int_no_check(dim_values[dim_index], dim_index);
	
	return result_SP;
}

// (*)drop(* x)
EidosValue_SP Eidos_ExecuteFunction_drop(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	EidosValue *x_value = p_arguments[0].get();
	int source_dimcount = x_value->DimensionCount();
	const int64_t *source_dim = x_value->Dimensions();
	
	if (source_dimcount <= 1)
	{
		// x is already a vector, so just return it
		result_SP = EidosValue_SP(x_value);
	}
	else
	{
		// Count the number of dimensions that have a size != 1
		int needed_dim_count = 0;
		
		for (int dim_index = 0; dim_index < source_dimcount; dim_index++)
			if (source_dim[dim_index] > 1)
				needed_dim_count++;
		
		if (needed_dim_count == source_dimcount)
		{
			// No dimensions can be dropped, so do nothing
			result_SP = EidosValue_SP(x_value);
		}
		else if (needed_dim_count <= 1)
		{
			// A vector is all that is needed
			result_SP = x_value->CopyValues();
			
			result_SP->SetDimensions(1, nullptr);
		}
		else
		{
			// We need to drop some dimensions but still end up with a matrix or array
			result_SP = x_value->CopyValues();
			
			int64_t *dim_buf = (int64_t *)malloc(needed_dim_count * sizeof(int64_t));
			if (!dim_buf)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_drop): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
			int dim_buf_index = 0;
			
			for (int dim_index = 0; dim_index < source_dimcount; dim_index++)
				if (source_dim[dim_index] > 1)
					dim_buf[dim_buf_index++] = source_dim[dim_index];
			
			result_SP->SetDimensions(needed_dim_count, dim_buf);
			
			free(dim_buf);
		}
	}
	
	return result_SP;
}

// (*)matrix(* data, [integer$ nrow = 1], [integer$ ncol = 1], [logical$ byrow = F])
EidosValue_SP Eidos_ExecuteFunction_matrix(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *data_value = p_arguments[0].get();
	EidosValue *nrow_value = p_arguments[1].get();
	EidosValue *ncol_value = p_arguments[2].get();
	EidosValue *byrow_value = p_arguments[3].get();
	
	int data_count = data_value->Count();
	bool nrow_null = (nrow_value->Type() == EidosValueType::kValueNULL);
	bool ncol_null = (ncol_value->Type() == EidosValueType::kValueNULL);
	
	int64_t nrow = nrow_null ? -1 : nrow_value->IntAtIndex(0, nullptr);
	int64_t ncol = ncol_null ? -1 : ncol_value->IntAtIndex(0, nullptr);
	
	if ((!nrow_null && (nrow <= 0)) || (!ncol_null && (ncol <= 0)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): dimension <= 0 requested, which is not allowed." << EidosTerminate(nullptr);
	if (data_count == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): matrix() cannot create a matrix with zero elements; matrix dimensions equal to zero are not allowed." << EidosTerminate(nullptr);
	
	if (nrow_null && ncol_null)
	{
		// return a one-column matrix, following R
		ncol = 1;
		nrow = data_count;
	}
	else if (nrow_null)
	{
		// try to infer a row count
		if (data_count % ncol == 0)
			nrow = data_count / ncol;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): function matrix() data size is not a multiple of the supplied column count." << EidosTerminate(nullptr);
	}
	else if (ncol_null)
	{
		// try to infer a column count
		if (data_count % nrow == 0)
			ncol = data_count / nrow;
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): function matrix() data size is not a multiple of the supplied row count." << EidosTerminate(nullptr);
	}
	else
	{
		nrow = nrow_value->IntAtIndex(0, nullptr);
		ncol = ncol_value->IntAtIndex(0, nullptr);
		
		if (data_count != nrow * ncol)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrix): function matrix() requires a data vector with a length equal to the product of the proposed number of rows and columns." << EidosTerminate(nullptr);
	}
	
	eidos_logical_t byrow = byrow_value->LogicalAtIndex(0, nullptr);
	
	if (byrow)
	{
		// The non-default case: use the values in data to fill rows one by one, requiring transposition here; singletons are easy, though
		if (data_count <= 1)
			result_SP = data_value->CopyValues();
		else
		{
			// non-singleton byrow case, so we need to actually transpose
			result_SP = data_value->NewMatchingType();
			
			EidosValue *result = result_SP.get();
			
			for (int64_t value_index = 0; value_index < data_count; ++value_index)
			{
				int64_t dest_col = (value_index / nrow);
				int64_t dest_row = (value_index % nrow);
				int64_t src_index = dest_col + dest_row * ncol;
				
				result->PushValueFromIndexOfEidosValue((int)src_index, *data_value, nullptr);
			}
		}
	}
	else
	{
		// The default case: use the values in data to fill columns one by one, mirroring the internal layout used by Eidos
		result_SP = data_value->CopyValues();
	}
	
	const int64_t dim_buf[2] = {nrow, ncol};
	
	result_SP->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (numeric)matrixMult(numeric x, numeric y)
EidosValue_SP Eidos_ExecuteFunction_matrixMult(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *y_value = p_arguments[1].get();
	
	if (x_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): function matrixMult() x is not a matrix." << EidosTerminate(nullptr);
	if (y_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): function matrixMult() y is not a matrix." << EidosTerminate(nullptr);
	
	EidosValueType x_type = x_value->Type();
	EidosValueType y_type = y_value->Type();
	
	if (x_type != y_type)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): function matrixMult() requires that x and y are the same type." << EidosTerminate(nullptr);
	
	const int64_t *x_dim = x_value->Dimensions();
	int64_t x_rows = x_dim[0];
	int64_t x_cols = x_dim[1];
	int64_t x_length = x_rows * x_cols;
	const int64_t *y_dim = y_value->Dimensions();
	int64_t y_rows = y_dim[0];
	int64_t y_cols = y_dim[1];
	int64_t y_length = y_rows * y_cols;
	
	if (x_cols != y_rows)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): in function matrixMult(), x and y are not conformable." << EidosTerminate(nullptr);
	
	EidosValue_SP result_SP(nullptr);
	int64_t result_rows = x_rows;
	int64_t result_cols = y_cols;
	int64_t result_length = result_rows * result_cols;
	
	// Do the multiplication.  We split out singleton cases here so that the big general case can run fast.
	// We also split by integer integer versus float, because we have to in order to multiply.  Finally,
	// in the integer cases we have to check for overflows; see EidosInterpreter::Evaluate_Mult().
	
	if ((x_length == 1) && (y_length == 1))
	{
		// a 1x1 vector multiplied by a 1x1 vector
		if (x_type == EidosValueType::kValueInt)
		{
			int64_t x_singleton = x_value->IntAtIndex(0, nullptr);
			int64_t y_singleton = y_value->IntAtIndex(0, nullptr);
			int64_t multiply_result;
			bool overflow = Eidos_mul_overflow(x_singleton, y_singleton, &multiply_result);
			
			if (overflow)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(multiply_result));
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			double x_singleton = x_value->FloatAtIndex(0, nullptr);
			double y_singleton = y_value->FloatAtIndex(0, nullptr);
			
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_singleton * y_singleton));
		}
	}
	else if (x_length == 1)
	{
		// a 1x1 vector multiplied by a row vector
		if (x_type == EidosValueType::kValueInt)
		{
			int64_t x_singleton = x_value->IntAtIndex(0, nullptr);
			const int64_t *y_data = y_value->IntVector()->data();
			EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t y_index = 0; y_index < y_length; ++y_index)
			{
				int64_t y_operand = y_data[y_index];
				int64_t multiply_result;
				bool overflow = Eidos_mul_overflow(x_singleton, y_operand, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
				
				result->set_int_no_check(multiply_result, y_index);
			}
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			double x_singleton = x_value->FloatAtIndex(0, nullptr);
			const double *y_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t y_index = 0; y_index < y_length; ++y_index)
				result->set_float_no_check(x_singleton * y_data[y_index], y_index);
		}
	}
	else if (y_length == 1)
	{
		// a column vector multiplied by a 1x1 vector
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *x_data = x_value->IntVector()->data();
			int64_t y_singleton = y_value->IntAtIndex(0, nullptr);
			EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t x_index = 0; x_index < x_length; ++x_index)
			{
				int64_t x_operand = x_data[x_index];
				int64_t multiply_result;
				bool overflow = Eidos_mul_overflow(x_operand, y_singleton, &multiply_result);
				
				if (overflow)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
				
				result->set_int_no_check(multiply_result, x_index);
			}
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			const double *x_data = x_value->FloatVector()->data();
			double y_singleton = y_value->FloatAtIndex(0, nullptr);
			EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t x_index = 0; x_index < x_length; ++x_index)
				result->set_float_no_check(x_data[x_index] * y_singleton, x_index);
		}
	}
	else
	{
		// this is the general case; we have non-singleton matrices for both x and y, so we can divide by integer/float and use direct access
		if (x_type == EidosValueType::kValueInt)
		{
			const int64_t *x_data = x_value->IntVector()->data();
			const int64_t *y_data = y_value->IntVector()->data();
			EidosValue_Int_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t result_col_index = 0; result_col_index < result_cols; ++result_col_index)
			{
				for (int64_t result_row_index = 0; result_row_index < result_rows; ++result_row_index)
				{
					int64_t result_index = result_col_index * result_rows + result_row_index;
					int64_t summation_result = 0;
					
					for (int64_t product_index = 0; product_index < x_cols; ++product_index)	// x_cols == y_rows; this is the dimension that matches, n x m * m x p
					{
						int64_t x_row = result_row_index;
						int64_t x_col = product_index;
						int64_t x_index = x_col * x_rows + x_row;
						int64_t y_row = product_index;
						int64_t y_col = result_col_index;
						int64_t y_index = y_col * y_rows + y_row;
						int64_t x_operand = x_data[x_index];
						int64_t y_operand = y_data[y_index];
						int64_t multiply_result;
						bool overflow = Eidos_mul_overflow(x_operand, y_operand, &multiply_result);
						
						if (overflow)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer multiplication overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
						
						int64_t add_result;
						bool overflow2 = Eidos_add_overflow(summation_result, multiply_result, &add_result);
						
						if (overflow2)
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_matrixMult): integer addition overflow in function matrixMult(); you may wish to cast the matrices to float with asFloat() before multiplying." << EidosTerminate(nullptr);
						
						summation_result = add_result;
					}
					
					result->set_int_no_check(summation_result, result_index);
				}
			}
		}
		else // (x_type == EidosValueType::kValueFloat)
		{
			const double *x_data = x_value->FloatVector()->data();
			const double *y_data = y_value->FloatVector()->data();
			EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(result_length);
			result_SP = EidosValue_SP(result);
			
			for (int64_t result_col_index = 0; result_col_index < result_cols; ++result_col_index)
			{
				for (int64_t result_row_index = 0; result_row_index < result_rows; ++result_row_index)
				{
					int64_t result_index = result_col_index * result_rows + result_row_index;
					double summation_result = 0;
					
					for (int64_t product_index = 0; product_index < x_cols; ++product_index)	// x_cols == y_rows; this is the dimension that matches, n x m * m x p
					{
						int64_t x_row = result_row_index;
						int64_t x_col = product_index;
						int64_t x_index = x_col * x_rows + x_row;
						int64_t y_row = product_index;
						int64_t y_col = result_col_index;
						int64_t y_index = y_col * y_rows + y_row;
						double x_operand = x_data[x_index];
						double y_operand = y_data[y_index];
						
						summation_result += x_operand * y_operand;
					}
					
					result->set_float_no_check(summation_result, result_index);
				}
			}
		}
	}
	
	const int64_t dim_buf[2] = {result_rows, result_cols};
	
	result_SP->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (integer$)ncol(* x)
EidosValue_SP Eidos_ExecuteFunction_ncol(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *data_value = p_arguments[0].get();
	int dim_count = data_value->DimensionCount();
	
	if (dim_count < 2)
		return gStaticEidosValueNULL;
	
	const int64_t *dim_values = data_value->Dimensions();
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(dim_values[1]));
}

// (integer$)nrow(* x)
EidosValue_SP Eidos_ExecuteFunction_nrow(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *data_value = p_arguments[0].get();
	int dim_count = data_value->DimensionCount();
	
	if (dim_count < 2)
		return gStaticEidosValueNULL;
	
	const int64_t *dim_values = data_value->Dimensions();
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(dim_values[0]));
}

// (*)rbind(...)
EidosValue_SP Eidos_ExecuteFunction_rbind(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// First check the type and class of the result; NULL may be mixed in, but otherwise all arguments must be the same type and class
	EidosValueType result_type = EidosValueType::kValueNULL;
	const EidosClass *result_class = gEidosObject_Class;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		EidosValueType arg_type = arg->Type();
		
		if (arg_type == EidosValueType::kValueNULL)
			continue;
		else if (result_type == EidosValueType::kValueNULL)
			result_type = arg_type;
		else if (arg_type != result_type)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() requires that all arguments be the same type (or NULL)." << EidosTerminate(nullptr);
		
		if (arg_type == EidosValueType::kValueObject)
		{
			EidosValue_Object *arg_object = (EidosValue_Object *)arg;
			const EidosClass *arg_class = arg_object->Class();
			
			if (arg_class == gEidosObject_Class)
				continue;
			else if (result_class == gEidosObject_Class)
				result_class = arg_class;
			else if (arg_class != result_class)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() requires that all object arguments be of the same class." << EidosTerminate(nullptr);
		}
	}
	
	if (result_type == EidosValueType::kValueNULL)
		return gStaticEidosValueNULL;
	
	// Next determine the dimensions of the result; every argument must be zero-size or a match for the dimensions we already have
	int64_t result_rows = 0;
	int64_t result_cols = 0;
	int64_t result_length = 0;
	
	for (int arg_index = 0; arg_index < argument_count; ++arg_index)
	{
		EidosValue *arg = p_arguments[arg_index].get();
		int64_t arg_length = arg->Count();
		
		// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
		if (arg_length == 0)
			continue;
		
		int arg_dimcount = arg->DimensionCount();
		
		if ((arg_dimcount != 1) && (arg_dimcount != 2))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() requires that all arguments be vectors or matrices." << EidosTerminate(nullptr);
		
		const int64_t *arg_dims = arg->Dimensions();
		int64_t arg_nrow = (arg_dimcount == 1) ? 1 : arg_dims[0];
		int64_t arg_ncol = (arg_dimcount == 1) ? arg_length : arg_dims[1];
		
		if (result_cols == 0)
			result_cols = arg_ncol;
		else if (result_cols != arg_ncol)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_rbind): function rbind() mismatch among arguments in their number of columns." << EidosTerminate(nullptr);
		
		result_rows += arg_nrow;
		result_length += arg_length;
	}
	
	// Construct our result; for each column, we scan through our arguments and append rows from that column; not very efficient, but general...
	EidosValue_SP result_SP(nullptr);
	
	switch (result_type)
	{
		case EidosValueType::kValueVOID:	break;		// never hit	// NOLINT(*-branch-clone) : intentional consecutive branches
		case EidosValueType::kValueNULL:	break;		// never hit
		case EidosValueType::kValueLogical:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->reserve(result_length)); break;
		case EidosValueType::kValueInt:		result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve(result_length)); break;
		case EidosValueType::kValueFloat:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(result_length)); break;
		case EidosValueType::kValueString:	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector()); break;
		case EidosValueType::kValueObject:	result_SP = EidosValue_SP((new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(result_class))->reserve(result_length)); break;
	}
	
	EidosValue *result = result_SP.get();
	
	for (int col_index = 0; col_index < result_cols; ++col_index)
	{
		for (int arg_index = 0; arg_index < argument_count; ++arg_index)
		{
			EidosValue *arg = p_arguments[arg_index].get();
			int64_t arg_length = arg->Count();
			
			// skip over zero-length arguments, including NULL; zero-length vectors must match in type (above) but are otherwise ignored
			if (arg_length == 0)
				continue;
			
			int arg_dimcount = arg->DimensionCount();
			
			if (arg_dimcount == 1)
			{
				// vector; take the nth value to fill the nth column in the result
				result->PushValueFromIndexOfEidosValue(col_index, *arg, nullptr);
			}
			else
			{
				// matrix; take the whole nth column of the matrix to fill the nth column in the result
				const int64_t *arg_dims = arg->Dimensions();
				int64_t arg_nrow = arg_dims[0];
				
				for (int64_t row_index = 0; row_index < arg_nrow; ++row_index)
					result->PushValueFromIndexOfEidosValue((int)(col_index * arg_nrow + row_index), *arg, nullptr);
			}
		}
	}
	
	const int64_t dim_buf[2] = {result_rows, result_cols};
	
	result->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (*)t(* x)
EidosValue_SP Eidos_ExecuteFunction_t(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	EidosValue *x_value = p_arguments[0].get();
	
	if (x_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_t): in function t() x is not a matrix." << EidosTerminate(nullptr);
	
	const int64_t *source_dim = x_value->Dimensions();
	int64_t source_rows = source_dim[0];
	int64_t source_cols = source_dim[1];
	int64_t dest_rows = source_cols;
	int64_t dest_cols = source_rows;
	
	result_SP = x_value->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	for (int64_t col_index = 0; col_index < dest_cols; ++col_index)
	{
		for (int64_t row_index = 0; row_index < dest_rows; ++row_index)
		{
			//int64_t dest_index = col_index * dest_rows + row_index;
			int64_t source_index = row_index * source_rows + col_index;
			
			result->PushValueFromIndexOfEidosValue((int)source_index, *x_value, nullptr);
		}
	}
	
	const int64_t dim_buf[2] = {dest_rows, dest_cols};
	
	result->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (logical)lowerTri(* x, [logical$ diag = F])
EidosValue_SP Eidos_ExecuteFunction_lowerTri(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// contributed by Nick O'Brien (@nobrien97)
	
	EidosValue *x_value = p_arguments[0].get();
	eidos_logical_t diag = p_arguments[1].get()->LogicalAtIndex(0, nullptr);
	
	if (x_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_lowerTri): in function lowerTri() x is not a matrix." << EidosTerminate(nullptr);
	
	const int64_t *dim = x_value->Dimensions();
	int64_t nrows = dim[0];
	int64_t ncols = dim[1];
	
	// Create new empty logical matrix
	EidosValue_Logical *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(nrows * ncols);
	EidosValue_SP result_SP(result);
	
	// Iterate through result matrix and set lower triangle to T, remaining values to F
	// If we want the diagonal elements included, we need to include them in our condition
	for (int64_t row_index = 0; row_index < nrows; ++row_index)
	{
		for (int64_t col_index = 0; col_index < ncols; ++col_index)
		{
			// Get 1D index from rows/cols
			int64_t index = col_index * nrows + row_index;
			
			bool in_triangle;
			
			if (row_index > col_index)
				in_triangle = true;
			else if (diag && (row_index == col_index))
				in_triangle = true;
			else
				in_triangle = false;
			
			result->set_logical_no_check(in_triangle, (int)index);
		}
	}
	
	// Apply dimension attributes and return
	const int64_t dim_buf[2] = {nrows, ncols};
	result->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (logical)upperTri(* x, [logical$ diag = F])
EidosValue_SP Eidos_ExecuteFunction_upperTri(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// contributed by Nick O'Brien (@nobrien97)
	
	EidosValue *x_value = p_arguments[0].get();
	eidos_logical_t diag = p_arguments[1].get()->LogicalAtIndex(0, nullptr);
	
	if (x_value->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_upperTri): in function upperTri() x is not a matrix." << EidosTerminate(nullptr);
	
	const int64_t *dim = x_value->Dimensions();
	int64_t nrows = dim[0];
	int64_t ncols = dim[1];
	
	// Create new empty logical matrix
	EidosValue_Logical *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(nrows * ncols);
	EidosValue_SP result_SP(result);
	
	// Iterate through result matrix and set upper triangle to T, remaining values to F
	// If we want the diagonal elements included, we need to include them in our condition
	for (int64_t row_index = 0; row_index < nrows; ++row_index)
	{
		for (int64_t col_index = 0; col_index < ncols; ++col_index)
		{
			// Get 1D index from rows/cols
			int64_t index = col_index * nrows + row_index;

			bool in_triangle;
			
			if (row_index < col_index)
				in_triangle = true;
			else if (diag && (row_index == col_index))
				in_triangle = true;
			else
				in_triangle = false;
			
			result->set_logical_no_check(in_triangle, (int)index);
		}
	}
	
	// Apply dimension attributes and return
	const int64_t dim_buf[2] = {nrows, ncols};
	result->SetDimensions(2, dim_buf);
	
	return result_SP;
}

// (*)diag([* x = 1], [integer$ nrow], [integer$ ncol])
EidosValue_SP Eidos_ExecuteFunction_diag(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// contributed by Nick O'Brien (@nobrien97)
	
	/* 
		Four return modes depending on the form of inputs (matching R behaviour)
		1: x is a matrix - return the diagonal elements of x (nrow and ncol cannot be specified in this mode)
	 	2: x is missing and nrow and/or ncol is given - return an identity matrix with dimensions nrow * ncol, nrow*nrow, 1*ncol
		3: x is a singleton vector and is the only input - return an identity matrix with dimensions equal to the length of x
		4: x is a numeric or logical vector - return a matrix with the given diagonal entries and 0/F in the off diagonals
	*/
	
	EidosValue *x_value = p_arguments[0].get();
	EidosValue *nrow_value = p_arguments[1].get();
	EidosValue *ncol_value = p_arguments[2].get();
	int64_t x_count = x_value->Count();
	EidosValueType x_type = x_value->Type();
	bool nrow_null = (nrow_value->Type() == EidosValueType::kValueNULL);
	bool ncol_null = (ncol_value->Type() == EidosValueType::kValueNULL);
	
	if (x_value->DimensionCount() > 2)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_diag): in function diag() x must be a vector or a matrix." << EidosTerminate(nullptr);
	
	// 1: If x is a matrix we return the diagonals
	if (x_value->DimensionCount() == 2)
	{	
		if (!nrow_null || !ncol_null) 	
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_diag): in function diag() nrow and ncol must be NULL when x is a matrix." << EidosTerminate(nullptr);
		
		// Setup output
		EidosValue_SP result_SP = x_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		const int64_t *source_dim = x_value->Dimensions();
		int64_t source_nrow = source_dim[0];
		int64_t source_ncol= source_dim[1];
		int64_t max_diag_index = std::min(source_nrow, source_ncol);
		
		// Iterate over diagonals: number of diagonals is the minimum of nrows and ncols 
		for (int64_t diag_index = 0; diag_index < max_diag_index; ++diag_index)
		{
			// Convert to 1D: because row == col, we can use diag_index in place of both
			int64_t source_index = diag_index * source_nrow + diag_index;
			
			result->PushValueFromIndexOfEidosValue((int)source_index, *x_value, nullptr);	
		}
		
		return result_SP;
	}
	
	// 2: If x is 1 and nrow is non-NULL, return an identity matrix of size nrow (by ncol, if specified)
	if ((x_type == EidosValueType::kValueInt) && (x_count == 1) && (x_value->IntAtIndex(0, nullptr) == 1) && !nrow_null)
	{
		int64_t nrow = nrow_value->IntAtIndex(0, nullptr);
		int64_t ncol = (ncol_null ? nrow : ncol_value->IntAtIndex(0, nullptr));
		
		if ((nrow < 1) || (ncol < 1))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_diag): in function diag() when an identity matrix is being generated, both dimensions of that matrix must be >= 1." << EidosTerminate(nullptr);
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(nrow * ncol);
		EidosValue_SP result_SP(int_result);
		
		for (int64_t col_index = 0; col_index < ncol; ++col_index)
			for (int64_t row_index = 0; row_index < nrow; ++row_index)
				int_result->set_int_no_check((row_index == col_index) ? 1 : 0, col_index * nrow + row_index);
		
		const int64_t dim_buf[2] = {nrow, ncol};
		int_result->SetDimensions(2, dim_buf);
		
		return result_SP;
	}
	
	// 3: If x is a singleton integer, nrow/ncol must not be set, and a square identity matrix of size x is returned
	if ((x_type == EidosValueType::kValueInt) && (x_count == 1) && nrow_null && ncol_null)
	{
		int64_t size = x_value->IntAtIndex(0, nullptr);
		
		if (size < 1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_diag): in function diag() when x specificies an identity matrix size, that size must be >= 1." << EidosTerminate(nullptr);
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(size * size);
		EidosValue_SP result_SP(int_result);
		
		for (int64_t col_index = 0; col_index < size; ++col_index)
			for (int64_t row_index = 0; row_index < size; ++row_index)
				int_result->set_int_no_check((row_index == col_index) ? 1 : 0, col_index * size + row_index);
		
		const int64_t dim_buf[2] = {size, size};
		int_result->SetDimensions(2, dim_buf);
		
		return result_SP;
	}
	
	// 4: If x is a logical/integer/float vector of length >= 2, use the values of x for the diagonal
	if (((x_type == EidosValueType::kValueLogical) || (x_type == EidosValueType::kValueInt) || (x_type == EidosValueType::kValueFloat)) && (x_count >= 2))
	{
		int64_t nrow = (nrow_null ? x_count : nrow_value->IntAtIndex(0, nullptr));
		int64_t ncol = (ncol_null ? nrow : ncol_value->IntAtIndex(0, nullptr));		// it is weird that the default is nrow, not x_count, but this mirrors R's behavior
		int64_t max_diag_index = std::min(nrow, ncol);
		
		if (max_diag_index != x_count)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_diag): in function diag(), when values for the diagonal are supplied in x, those values may not be truncated or recycled by the dimensions specified with nrow and ncol." << EidosTerminate(nullptr);
		
		// define default value to copy to off-diagonals
		EidosValue_SP default_value;
		
		switch (x_type)
		{
		case EidosValueType::kValueLogical:		default_value = gStaticEidosValue_LogicalF;		break;
		case EidosValueType::kValueFloat:		default_value = gStaticEidosValue_Float0;		break;
		case EidosValueType::kValueInt:			default_value = gStaticEidosValue_Integer0;		break;
		default:																				break;
		}
		
		EidosValue_SP result_SP = x_value->NewMatchingType();
		EidosValue *result = result_SP.get();
		
		// Initialise result matrix and fill in diagonals with x values
		for (int64_t col_index = 0; col_index < ncol; ++col_index)
		{
			for (int64_t row_index = 0; row_index < nrow; ++row_index)
			{
				// Set diagonal to the corresponding value of x, other values to F/0/0.0
				if (row_index == col_index)
					result->PushValueFromIndexOfEidosValue((int)col_index, *x_value, nullptr);
				else
					result->PushValueFromIndexOfEidosValue(0, *default_value, nullptr);
			}
		}
		
		// Apply dimension attributes and return
		const int64_t dim_buf[2] = {nrow, ncol};
		result->SetDimensions(2, dim_buf);
		
		return result_SP;		
	}
	
	EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_diag): diag() requires one of four specific input parameter patterns; see the documentation." << EidosTerminate(nullptr);
}













































